/**********************************************************************
 * File:        stringrenderer.cpp
 * Description: Class for rendering UTF-8 text to an image, and retrieving
 *              bounding boxes around each grapheme cluster.
 * Author:      Ranjith Unnikrishnan
 *
 * (C) Copyright 2013, Google Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **********************************************************************/

#include <tesseract/preparation.h> // compiler config, etc.

#include "stringrenderer.h"

#include <leptonica/allheaders.h> // from leptonica
#include <leptonica/pix_internal.h> 
#include <tesseract/baseapi.h> // for TessBaseAPI
#include "boxchar.h"
#include "fileio.h"
#include "helpers.h" // for TRand
#include "ligature_table.h"
#include "../unicharset/normstrngs.h"
#include "tlog.h"

#include <tesseract/unichar.h>

#if defined(PANGO_ENABLE_ENGINE) && defined(HAS_LIBICU)

//#include "pango/pango-font.h"
//#include "pango/pango-glyph-item.h"
#include "unicode/uchar.h" // from libicu

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <regex>
#include <sstream> // for std::stringstream
#include <utility>
#include <vector>

#define DISABLE_HEAP_LEAK_CHECK

namespace tesseract {

static const int kDefaultOutputResolution = 300;

// Word joiner (U+2060) inserted after letters in ngram mode, as per
// recommendation in http://unicode.org/reports/tr14/ to avoid line-breaks at
// hyphens and other non-alpha characters.
static const char *kWordJoinerUTF8 = "\u2060";

static bool IsCombiner(int ch) {
  const int char_type = u_charType(ch);
  return ((char_type == U_NON_SPACING_MARK) || (char_type == U_ENCLOSING_MARK) ||
          (char_type == U_COMBINING_SPACING_MARK));
}

static std::string EncodeAsUTF8(const char32 ch32) {
  UNICHAR uni_ch(ch32);
  return std::string(uni_ch.utf8(), uni_ch.utf8_len());
}

// Returns true with probability 'prob'.
static bool RandBool(const double prob, TRand *rand) {
  if (prob == 1.0) {
    return true;
  }
  if (prob == 0.0) {
    return false;
  }
  return rand->UnsignedRand(1.0) < prob;
}

/* static */
static Image CairoARGB32ToPixFormat(cairo_surface_t *surface) {
  if (cairo_image_surface_get_format(surface) != CAIRO_FORMAT_ARGB32) {
    tprintError("Unexpected surface format {}\n", int(cairo_image_surface_get_format(surface)));
    return nullptr;
  }
  const int width = cairo_image_surface_get_width(surface);
  const int height = cairo_image_surface_get_height(surface);
  Image pix = pixCreate(width, height, 32);
  int byte_stride = cairo_image_surface_get_stride(surface);

  for (int i = 0; i < height; ++i) {
    memcpy(reinterpret_cast<unsigned char *>(pixGetData(pix) + i * pixGetWpl(pix)) + 1,
           cairo_image_surface_get_data(surface) + i * byte_stride,
           byte_stride - ((i == height - 1) ? 1 : 0));
  }
  return pix;
}

StringRenderer::StringRenderer(const std::string &font_desc, int page_width, int page_height)
    : font_(font_desc)
    , page_width_(page_width)
    , page_height_(page_height)
    , h_margin_(50)
    , v_margin_(50)
    , pen_color_{0.0, 0.0, 0.0}
    , char_spacing_(0)
    , leading_(0)
    , vertical_text_(false)
    , gravity_hint_strong_(false)
    , render_fullwidth_latin_(false)
    , underline_start_prob_(0)
    , underline_continuation_prob_(0)
    , underline_style_(PANGO_UNDERLINE_SINGLE)
    , drop_uncovered_chars_(true)
    , strip_unrenderable_words_(false)
    , add_ligatures_(false)
    , output_word_boxes_(false)
    , surface_(nullptr)
    , cr_(nullptr)
    , layout_(nullptr)
    , start_box_(0)
    , start_line_box_(0)
    , page_(0)
    , box_padding_(0)
    , page_boxes_(nullptr)
    , total_chars_(0)
    , font_index_(0)
    , last_offset_(0) {
  set_resolution(kDefaultOutputResolution);
  set_font(font_desc);
}

bool StringRenderer::set_font(const std::string &desc) {
  bool success = font_.ParseFontDescriptionName(desc);
  font_.set_resolution(resolution_);
  return success;
}

void StringRenderer::set_resolution(const int resolution) {
  resolution_ = resolution;
  font_.set_resolution(resolution);
}

void StringRenderer::set_underline_start_prob(const double frac) {
  underline_start_prob_ = std::min(std::max(frac, 0.0), 1.0);
}

void StringRenderer::set_underline_continuation_prob(const double frac) {
  underline_continuation_prob_ = std::min(std::max(frac, 0.0), 1.0);
}

StringRenderer::~StringRenderer() {
  ClearBoxes();
  FreePangoCairo();
}

void StringRenderer::InitPangoCairo() {
  FreePangoCairo();
  surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, page_width_, page_height_);
  cr_ = cairo_create(surface_);
  {
    DISABLE_HEAP_LEAK_CHECK;
    layout_ = pango_cairo_create_layout(cr_);
  }

  if (vertical_text_) {
    PangoContext *context = pango_layout_get_context(layout_);
    pango_context_set_base_gravity(context, PANGO_GRAVITY_EAST);
    if (gravity_hint_strong_) {
      pango_context_set_gravity_hint(context, PANGO_GRAVITY_HINT_STRONG);
    }
    pango_layout_context_changed(layout_);
  }

  SetLayoutProperties();
}

void StringRenderer::SetLayoutProperties() {
  std::string font_desc = font_.DescriptionName();
  // Specify the font via a description name
  PangoFontDescription *desc = pango_font_description_from_string(font_desc.c_str());
  // Assign the font description to the layout
  pango_layout_set_font_description(layout_, desc);
  pango_font_description_free(desc); // free the description
  pango_cairo_context_set_resolution(pango_layout_get_context(layout_), resolution_);

  int max_width = page_width_ - 2 * h_margin_;
  int max_height = page_height_ - 2 * v_margin_;
  tprintDebug("max_width = {}, max_height = {}\n", max_width, max_height);
  if (vertical_text_) {
    using std::swap;
    swap(max_width, max_height);
  }
  pango_layout_set_width(layout_, max_width * PANGO_SCALE);
  // Ultra-wide Thai strings need to wrap at char level.
  pango_layout_set_wrap(layout_, PANGO_WRAP_WORD_CHAR);

  // Adjust character spacing
  PangoAttrList *attr_list = pango_attr_list_new();
  if (char_spacing_) {
    PangoAttribute *spacing_attr = pango_attr_letter_spacing_new(char_spacing_ * PANGO_SCALE);
    spacing_attr->start_index = 0;
    spacing_attr->end_index = static_cast<guint>(-1);
    pango_attr_list_change(attr_list, spacing_attr);
  }

  if (add_ligatures_) {
    set_features("liga, clig, dlig, hlig");
    PangoAttribute *feature_attr = pango_attr_font_features_new(features_.c_str());
    pango_attr_list_change(attr_list, feature_attr);
  }

  pango_layout_set_attributes(layout_, attr_list);
  pango_attr_list_unref(attr_list);
  // Adjust line spacing
  if (leading_) {
    pango_layout_set_spacing(layout_, leading_ * PANGO_SCALE);
  }
}

void StringRenderer::FreePangoCairo() {
  if (layout_) {
    g_object_unref(layout_);
    layout_ = nullptr;
  }
  if (cr_) {
    cairo_destroy(cr_);
    cr_ = nullptr;
  }
  if (surface_) {
    cairo_surface_destroy(surface_);
    surface_ = nullptr;
  }
}

void StringRenderer::SetWordUnderlineAttributes(const std::string &page_text) {
  if (underline_start_prob_ == 0) {
    return;
  }
  PangoAttrList *attr_list = pango_layout_get_attributes(layout_);

  const char *text = page_text.c_str();
  size_t offset = 0;
  TRand rand;
  bool started_underline = false;
  PangoAttribute *und_attr = nullptr;

  while (offset < page_text.length()) {
    offset += SpanUTF8Whitespace(text + offset);
    if (offset == page_text.length()) {
      break;
    }

    int word_start = offset;
    int word_len = SpanUTF8NotWhitespace(text + offset);
    offset += word_len;
    if (started_underline) {
      // Should we continue the underline to the next word?
      if (RandBool(underline_continuation_prob_, &rand)) {
        // Continue the current underline to this word.
        und_attr->end_index = word_start + word_len;
      } else {
        // Otherwise end the current underline attribute at the end of the
        // previous word.
        pango_attr_list_insert(attr_list, und_attr);
        started_underline = false;
        und_attr = nullptr;
      }
    }
    if (!started_underline && RandBool(underline_start_prob_, &rand)) {
      // Start a new underline attribute
      und_attr = pango_attr_underline_new(underline_style_);
      und_attr->start_index = word_start;
      und_attr->end_index = word_start + word_len;
      started_underline = true;
    }
  }
  // Finish the current underline attribute at the end of the page.
  if (started_underline) {
    und_attr->end_index = page_text.length();
    pango_attr_list_insert(attr_list, und_attr);
  }
}

// Returns offset in utf8 bytes to first page.
int StringRenderer::FindFirstPageBreakOffset(const char *text, int text_length) {
  if (!text_length) {
    return 0;
  }
  const int max_height = (page_height_ - 2 * v_margin_);
  const int max_width = (page_width_ - 2 * h_margin_);
  const int max_layout_height = vertical_text_ ? max_width : max_height;

  UNICHAR::const_iterator it = UNICHAR::begin(text, text_length);
  const UNICHAR::const_iterator it_end = UNICHAR::end(text, text_length);
  const int kMaxUnicodeBufLength = 15000;
  for (int i = 0; i < kMaxUnicodeBufLength && it < it_end; ++it, ++i) {
    ;
  }
  int buf_length = it.utf8_data() - text;
  tprintInfo("len = {}  buf_len = {}\n", text_length, buf_length);
  pango_layout_set_text(layout_, text, buf_length);

  PangoLayoutIter *line_iter = nullptr;
  { // Fontconfig caches some info here that is not freed before exit.
    DISABLE_HEAP_LEAK_CHECK;
    line_iter = pango_layout_get_iter(layout_);
  }
  bool first_page = true;
  int page_top = 0;
  int offset = buf_length;
  do {
    // Get bounding box of the current line
    PangoRectangle line_ink_rect;
    pango_layout_iter_get_line_extents(line_iter, &line_ink_rect, nullptr);
    pango_extents_to_pixels(&line_ink_rect, nullptr);
    PangoLayoutLine *line = pango_layout_iter_get_line_readonly(line_iter);
    if (first_page) {
      page_top = line_ink_rect.y;
      first_page = false;
    }
    int line_bottom = line_ink_rect.y + line_ink_rect.height;
    if (line_bottom - page_top > max_layout_height) {
      offset = line->start_index;
      tprintInfo("Found offset = {}\n", offset);
      break;
    }
  } while (pango_layout_iter_next_line(line_iter));
  pango_layout_iter_free(line_iter);
  return offset;
}

const std::vector<BoxChar *> &StringRenderer::GetBoxes() const {
  return boxchars_;
}

const std::vector<BoxChar *> &StringRenderer::GetLineBoxes() const {
  return line_boxchars_;
}

Boxa *StringRenderer::GetPageBoxes() const {
  return page_boxes_;
}

void StringRenderer::RotatePageBoxes(float rotation) {
  BoxChar::RotateBoxes(rotation, page_width_ / 2, page_height_ / 2, start_box_, boxchars_.size(),
                       &boxchars_);
  BoxChar::RotateBoxes(rotation, page_width_ / 2, page_height_ / 2, start_line_box_, line_boxchars_.size(),
                     &line_boxchars_);
  BoxChar::RotateBaseline(rotation, page_width_ / 2, page_height_ / 2, start_line_box_, line_boxchars_.size(),
                     &line_boxchars_);
}

void StringRenderer::ClearBoxes() {
  for (auto &boxchar : boxchars_) {
    delete boxchar;
  }
  for (auto &boxchar : line_boxchars_) {
    delete boxchar;
  }
  line_boxchars_.clear();
  boxchars_.clear();
  boxaDestroy(&page_boxes_);
}

std::string StringRenderer::GetBoxesStr() {
  BoxChar::PrepareToWrite(&boxchars_);
  return BoxChar::GetTesseractBoxStr(page_height_, boxchars_);
}

void StringRenderer::WriteAllBoxes(const std::string &filename) {
  BoxChar::PrepareToWrite(&boxchars_);
  BoxChar::WriteTesseractBoxFile(filename, page_height_, boxchars_);
}

// Returns cluster strings in logical order.
bool StringRenderer::GetClusterStrings(std::vector<std::string> *cluster_text) {
  std::map<int, std::string> start_byte_to_text;
  PangoLayoutIter *run_iter = pango_layout_get_iter(layout_);
  const char *full_text = pango_layout_get_text(layout_);
  do {
    PangoLayoutRun *run = pango_layout_iter_get_run_readonly(run_iter);
    if (!run) {
      // End of line nullptr run marker
      tprintInfo("Found end of line marker\n");
      continue;
    }
    PangoGlyphItemIter cluster_iter;
    gboolean have_cluster;
    for (have_cluster = pango_glyph_item_iter_init_start(&cluster_iter, run, full_text);
         have_cluster; have_cluster = pango_glyph_item_iter_next_cluster(&cluster_iter)) {
      const int start_byte_index = cluster_iter.start_index;
      const int end_byte_index = cluster_iter.end_index;
      std::string text =
          std::string(full_text + start_byte_index, end_byte_index - start_byte_index);
      if (IsUTF8Whitespace(text.c_str())) {
        tprintInfo("Found whitespace\n");
        text = " ";
      }
      tprintInfo("start_byte={} end_byte={} : '{}'\n", start_byte_index, end_byte_index, text);
      if (add_ligatures_) {
        // Make sure the output box files have ligatured text in case the font
        // decided to use an unmapped glyph.
        text = LigatureTable::Get()->AddLigatures(text, nullptr);
      }
      start_byte_to_text[start_byte_index] = std::move(text);
    }
  } while (pango_layout_iter_next_run(run_iter));
  pango_layout_iter_free(run_iter);

  cluster_text->clear();
  for (auto it = start_byte_to_text.begin(); it != start_byte_to_text.end(); ++it) {
    cluster_text->push_back(it->second);
  }
  return !cluster_text->empty();
}

void StringRenderer::WriteAllBoxesPagebyPage(const std::string &filename, bool multipage, bool create_boxfiles, bool create_page) {
  BoxChar::PrepareToWrite(&boxchars_);
  if (!multipage) {
    std::vector<BoxChar *> page_boxchars;
    if (create_boxfiles) {
      page_boxchars.reserve(boxchars_.size());
      auto page_index = boxchars_[0]->page();
      for (auto boxe : boxchars_) {
        if (boxe->page() != page_index) {   
          auto page_filename = filename + (std::string) "." + std::to_string(page_index);
          BoxChar::WriteTesseractBoxFile(page_filename + (std::string) ".box" , page_height_, page_boxchars);
          page_index = boxe->page();
          page_boxchars.clear();
          // Skip empty lines from predecessor page
          if (boxe->ch() == "\t") continue;
        }
        page_boxchars.push_back(boxe);
      }
      if (!page_boxchars.empty()) {
        auto page_filename = filename + (std::string) "." + std::to_string(page_index);
        BoxChar::WriteTesseractBoxFile(page_filename + (std::string) ".box" , page_height_, page_boxchars);
        page_boxchars.clear();
      }
    }
    if (create_page) {
      page_boxchars.reserve(line_boxchars_.size());
      auto page_index = line_boxchars_[0]->page();
      for (auto boxe : line_boxchars_) {
        if (boxe->page() != page_index) {   
          auto page_filename = filename + (std::string) "." + std::to_string(page_index);
          WriteTesseractBoxAsPAGEFile(page_filename + (std::string) ".xml", page_boxchars);
          page_index = boxe->page();
          page_boxchars.clear();
        }
        page_boxchars.push_back(boxe);
      }
      if (!page_boxchars.empty()) {
        auto page_filename = filename + (std::string) "." + std::to_string(page_index);
        WriteTesseractBoxAsPAGEFile(page_filename + (std::string) ".xml", page_boxchars);
        page_boxchars.clear();
      }
    }
  } else {
    BoxChar::WriteTesseractBoxFile(filename + (std::string) ".box", page_height_, boxchars_);
  }
}

///
/// Write coordinates in the form of a points to a stream
///
static void 
AddPointsToPAGE(Pta *pts, std::stringstream &str) {
  int num_pts;
  int x, y;
  
  str <<"<Coords points=\"";
    num_pts = ptaGetCount(pts);
    for (int p = 0; p < num_pts; ++p) {
      ptaGetIPt(pts, p, &x, &y);
      if (p!=0) str << " ";
      str << (l_uint32) x << "," << (l_uint32) y;
    }
    str << "\"/>\n";
}

///
/// Directly write baseline information as baseline points a stream
///
static void 
AddBaselinePtsToPAGE(Pta *baseline_pts, std::stringstream &str) {
  int num_pts;
  int x, y;
  
  str <<"<Baseline points=\"";
    num_pts = ptaGetCount(baseline_pts);
    for (int p = 0; p < num_pts; ++p) {
      ptaGetIPt(baseline_pts, p, &x, &y);
      if (p!=0) str << " ";
      str << (l_uint32) x << "," << (l_uint32) y;
    }
    str << "\"/>\n";
}

void StringRenderer::WriteTesseractBoxAsPAGEFile(const std::string &filename, const std::vector<BoxChar *> &boxes){
  float x_min, y_min, x_max, y_max;
  std::stringstream page_str;
  std::stringstream line_str;

  page_str << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
    "<PcGts xmlns=\"http://schema.primaresearch.org/PAGE/gts/pagecontent/2019-07-15\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:schemaLocation=\"http://schema.primaresearch.org/PAGE/gts/pagecontent/2019-07-15 http://schema.primaresearch.org/PAGE/gts/pagecontent/2019-07-15/pagecontent.xsd\">\n"
    "\t<Metadata>\n"
    "\t\t<Creator>Tesseract - " << TESSERACT_VERSION_STR << " (Text2Image)</Creator>\n";
  
  // If gmtime conversion is problematic maybe l_getFormattedDate can be used here
  //const char *datestr = l_getFormattedDate();
  std::time_t now= std::time(nullptr);
  std::tm* now_tm= std::gmtime(&now);
  char mbstr[100];
  std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%dT%H:%M:%S", now_tm);
  
  page_str << "\t\t<Created>" << mbstr << "</Created>\n"
      << "\t\t<LastChange>" << mbstr << "</LastChange>\n"
      << "\t</Metadata>\n";

  page_str << "\t<Page "
           <<"imageFilename=\""<< filename;
  page_str << "\" " 
           << "imageWidth=\"" << page_width_ << "\" " 
           << "imageHeight=\"" << page_height_ << "\" "
           << "type=\"content\">\n";

  page_str << "\t\t<TextRegion id=\"r_0\" "
      << "custom=\""<< "readingOrder {index:0;}\">\n";
  Pta* all_polygon_pts = ptaCreate(0);
  std::string all_line_text = "";
  for (auto boxe : boxes) {
    Pta* line_polygon_pts = ptaCreate(0);
    line_str << "\t\t\t<TextLine id=\"r_0_0\" readingDirection=";
    if (boxe->rtl_index()) line_str << "\"right-to-left\" ";
    else line_str << "\"left-to-right\" ";
    line_str << "custom=\""<< "readingOrder {index:0;}\">\n";
    const Box *bbox = boxe->box();
    line_str << "\t\t\t\t";
    AddBaselinePtsToPAGE(boxe->baseline(), line_str);
    ptaAddPt(line_polygon_pts, bbox->x, bbox->y);
    ptaAddPt(line_polygon_pts, bbox->x+bbox->w, bbox->y);
    ptaAddPt(line_polygon_pts, bbox->x+bbox->w, bbox->y+bbox->h);
    ptaAddPt(line_polygon_pts, bbox->x, bbox->y+bbox->h);
    line_str << "\t\t\t\t";
    AddPointsToPAGE(line_polygon_pts, line_str);
    ptaJoin(all_polygon_pts, line_polygon_pts, 0, -1);
    std::string line_text = boxe->ch();
    all_line_text = all_line_text + line_text + "\n";
    line_str << "\t\t\t\t<TextEquiv index=\"1\">\n"
    << "\t\t\t\t\t<Unicode>" << line_text.c_str() << "</Unicode>\n"
    << "\t\t\t\t</TextEquiv>\n";
    line_str << "\t\t\t</TextLine>\n";
    ptaDestroy(&line_polygon_pts);
  }
  page_str << "\t\t\t<Coords points=\"";
  ptaGetMinMax(all_polygon_pts, &x_min, &y_min, &x_max, &y_max);
  page_str << (l_uint32) x_min << "," << (l_uint32) y_min;
  page_str << " " << (l_uint32) x_max << "," << (l_uint32) y_min;
  page_str << " " << (l_uint32) x_max << "," << (l_uint32) y_max;
  page_str << " " << (l_uint32) x_min << "," << (l_uint32) y_max;
  page_str << "\"/>\n";
  page_str << line_str.str();
  page_str << "\t\t\t\t<TextEquiv index=\"1\">\n"
  << "\t\t\t\t\t<Unicode>" << all_line_text.c_str() << "</Unicode>\n"
  << "\t\t\t\t</TextEquiv>\n";
  page_str << "\t\t</TextRegion>"; 
  line_str.str("");
  page_str << "\t\t</Page>\n</PcGts>\n";
  File::WriteStringToFileOrDie(page_str.str(), filename);
}


// Merges an array of BoxChars into words based on the identification of
// BoxChars containing the space character as inter-word separators.
//
// Sometime two adjacent characters in the sequence may be detected as lying on
// different lines based on their spatial positions. This may be the result of a
// newline character at end of the last word on a line in the source text, or of
// a discretionary line-break created by Pango at intra-word locations like
// hyphens. When this is detected the word is split at that location into
// multiple BoxChars. Otherwise, each resulting BoxChar will contain a word and
// its bounding box.
static void MergeBoxCharsToWords(std::vector<BoxChar *> *boxchars) {
  std::vector<BoxChar *> result;
  bool started_word = false;
  for (auto &boxchar : *boxchars) {
    if (boxchar->ch() == " " || boxchar->box() == nullptr) {
      result.push_back(boxchar);
      boxchar = nullptr;
      started_word = false;
      continue;
    }

    if (!started_word) {
      // Begin new word
      started_word = true;
      result.push_back(boxchar);
      boxchar = nullptr;
    } else {
      BoxChar *last_boxchar = result.back();
      // Compute bounding box union
      const Box *box = boxchar->box();
      int32_t box_x;
      int32_t box_y;
      int32_t box_w;
      int32_t box_h;
      boxGetGeometry(const_cast<Box *>(box), &box_x, &box_y, &box_w, &box_h);
      Box *last_box = last_boxchar->mutable_box();
      int32_t last_box_x;
      int32_t last_box_y;
      int32_t last_box_w;
      int32_t last_box_h;
      boxGetGeometry(last_box, &last_box_x, &last_box_y, &last_box_w, &last_box_h);
      int left = std::min(last_box_x, box_x);
      int right = std::max(last_box_x + last_box_w, box_x + box_w);
      int top = std::min(last_box_y, box_y);
      int bottom = std::max(last_box_y + last_box_h, box_y + box_h);
      // Conclude that the word was broken to span multiple lines based on the
      // size of the merged bounding box in relation to those of the individual
      // characters seen so far.
      if (right - left > last_box_w + 5 * box_w) {
        tprintInfo("Found line break after '{}'", last_boxchar->ch());
        // Insert a fake interword space and start a new word with the current
        // boxchar.
        result.push_back(new BoxChar(" ", 1));
        result.push_back(boxchar);
        boxchar = nullptr;
        continue;
      }
      // Append to last word
      last_boxchar->mutable_ch()->append(boxchar->ch());
      boxSetGeometry(last_box, left, top, right - left, bottom - top);
      delete boxchar;
      boxchar = nullptr;
    }
  }
  boxchars->swap(result);
}

void StringRenderer::ComputeClusterBoxes() {
  const char *text = pango_layout_get_text(layout_);
  PangoLayoutIter *cluster_iter = pango_layout_get_iter(layout_);

  // Do a first pass to store cluster start indexes.
  std::vector<int> cluster_start_indices;
  do {
    cluster_start_indices.push_back(pango_layout_iter_get_index(cluster_iter));
    tprintDebug("Added {}\n", cluster_start_indices.back());
  } while (pango_layout_iter_next_cluster(cluster_iter));
  pango_layout_iter_free(cluster_iter);
  cluster_start_indices.push_back(strlen(text));
  tprintDebug("Added last index {}\n", cluster_start_indices.back());
  // Sort the indices and create a map from start to end indices.
  std::sort(cluster_start_indices.begin(), cluster_start_indices.end());
  std::map<int, int> cluster_start_to_end_index;
  for (size_t i = 0; i + 1 < cluster_start_indices.size(); ++i) {
    cluster_start_to_end_index[cluster_start_indices[i]] = cluster_start_indices[i + 1];
  }

  // Iterate to get line information: text, bbox and baseline
  PangoLayoutIter *line_iter = pango_layout_get_iter(layout_);
  do {
    PangoRectangle ink_rect, logical_rect;
    PangoLayoutLine* pango_line;
    pango_line = pango_layout_iter_get_line(line_iter);
    
    // Get text content
    std::string line_text = std::string(text + pango_line->start_index, pango_line->length);
    if (add_ligatures_) {
      // Make sure the output box files have ligatured text in case the font
      // decided to use an unmapped glyph.
      line_text = LigatureTable::Get()->AddLigatures(line_text, nullptr);
    }
    // Trim whitespaces
    line_text = std::regex_replace(line_text, std::regex("^[ \t]+|[ \t\n]+$"), "$1");

    bool rtl = false;
    if (line_text.size() == 0) continue;

    char32 ch = UNICHAR::UTF8ToUTF32(line_text.c_str())[0];
    UCharDirection dir = u_charDirection(ch);
    if (dir == U_RIGHT_TO_LEFT || dir == U_RIGHT_TO_LEFT_ARABIC || dir == U_RIGHT_TO_LEFT_ISOLATE) {
      rtl = true;
    }

    // Get bounding box 
    pango_layout_iter_get_line_extents(line_iter, &ink_rect, &logical_rect);
    pango_extents_to_pixels(&ink_rect, nullptr);
    pango_extents_to_pixels(&logical_rect, nullptr);

    // Get baseline
    int baseline = (pango_layout_iter_get_baseline(line_iter)/PANGO_SCALE)+ h_margin_;


    if (box_padding_) {
      ink_rect.x = std::max(0, ink_rect.x + v_margin_ - box_padding_);
      if ((ink_rect.width+ink_rect.x+2*box_padding_) < page_width_) ink_rect.width += 2 * box_padding_;
      logical_rect.y = std::max(0, logical_rect.y + h_margin_ - box_padding_);
      if ((ink_rect.height+ink_rect.y+2*box_padding_) < page_width_) ink_rect.height += 2 * box_padding_;
      logical_rect.height += 2 * box_padding_;
    } else {
      ink_rect.x = std::max(0, ink_rect.x + v_margin_ - 6);
      if ((ink_rect.width+ink_rect.x+2*6) < page_width_) ink_rect.width += 2*6;
      logical_rect.y = std::max(0, logical_rect.y + h_margin_  - 2);
      if ((ink_rect.height+ink_rect.y+2*2) < page_width_) ink_rect.height += 2*2;
    }

    //line_text = std::regex_replace(line_text, std::regex("\n+$"), "$1");
    // Store information if text is not empty
    auto *line_boxchar = new BoxChar(line_text.c_str(), line_text.size());
    line_boxchar->set_page(page_);
    line_boxchar->AddBox(ink_rect.x, logical_rect.y, ink_rect.width, logical_rect.height);
    line_boxchar->AddBaselinePt(ink_rect.x, baseline);
    line_boxchar->AddBaselinePt(ink_rect.x + ink_rect.width, baseline);
    line_boxchar->set_rtl_index(rtl);
    line_boxchars_.push_back(line_boxchar);
  } while (pango_layout_iter_next_line(line_iter));
  // Fix for vertical text
  if (vertical_text_) {
    const double rotation = -pango_gravity_to_rotation(
    pango_context_get_base_gravity(pango_layout_get_context(layout_)));
    BoxChar::RotateBoxes(rotation, page_width_ / 2, page_height_ / 2, start_line_box_, line_boxchars_.size(),
                         &line_boxchars_);
    BoxChar::RotateBaseline(rotation, page_width_ / 2, page_height_ / 2, start_line_box_, line_boxchars_.size(),
                         &line_boxchars_);
    BoxChar::TranslateBoxesAndBaseline((page_width_-page_height_)/2, (page_width_-page_height_)/2, start_line_box_, 
                         line_boxchars_.size(), &line_boxchars_);
  }
  //CorrectBoxPositionsToLayout(&line_boxchars_);
  // Iterate again to compute cluster boxes and their text with the obtained
  // cluster extent information.
  cluster_iter = pango_layout_get_iter(layout_);
  // Store BoxChars* sorted by their byte start positions
  std::map<int, BoxChar *> start_byte_to_box;
  do {
    PangoRectangle cluster_rect;
    pango_layout_iter_get_cluster_extents(cluster_iter, &cluster_rect, nullptr);
    pango_extents_to_pixels(&cluster_rect, nullptr);
    const int start_byte_index = pango_layout_iter_get_index(cluster_iter);
    const int end_byte_index = cluster_start_to_end_index[start_byte_index];
    std::string cluster_text =
        std::string(text + start_byte_index, end_byte_index - start_byte_index);
    if (!cluster_text.empty() && cluster_text[0] == '\n') {
      tprintInfo("Skipping newlines at start of text.\n");
      continue;
    }
    if (!cluster_rect.width || !cluster_rect.height || IsUTF8Whitespace(cluster_text.c_str())) {
      tprintInfo("Skipping whitespace with boxdim ({},{}) '{}'\n", cluster_rect.width,
           cluster_rect.height, cluster_text);
      auto *boxchar = new BoxChar(" ", 1);
      boxchar->set_page(page_);
      start_byte_to_box[start_byte_index] = boxchar;
      continue;
    }
    // Prepare a boxchar for addition at this byte position.
    tprintDebug("[{} {}], {}, {} : start_byte={} end_byte={} : '{}'\n", cluster_rect.x, cluster_rect.y,
         cluster_rect.width, cluster_rect.height, start_byte_index, end_byte_index,
         cluster_text);
    ASSERT_HOST_MSG(cluster_rect.width, "cluster_text:{}  start_byte_index:{}\n",
                    cluster_text, start_byte_index);
    ASSERT_HOST_MSG(cluster_rect.height, "cluster_text:{}  start_byte_index:{}\n",
                    cluster_text, start_byte_index);
    if (box_padding_) {
      cluster_rect.x = std::max(0, cluster_rect.x - box_padding_);
      cluster_rect.width += 2 * box_padding_;
      cluster_rect.y = std::max(0, cluster_rect.y - box_padding_);
      cluster_rect.height += 2 * box_padding_;
    }
    if (add_ligatures_) {
      // Make sure the output box files have ligatured text in case the font
      // decided to use an unmapped glyph.
      cluster_text = LigatureTable::Get()->AddLigatures(cluster_text, nullptr);
    }
    auto *boxchar = new BoxChar(cluster_text.c_str(), cluster_text.size());
    boxchar->set_page(page_);
    boxchar->AddBox(cluster_rect.x, cluster_rect.y, cluster_rect.width, cluster_rect.height);
    start_byte_to_box[start_byte_index] = boxchar;
  } while (pango_layout_iter_next_cluster(cluster_iter));
  pango_layout_iter_free(cluster_iter);

  // There is a subtle bug in the cluster text reported by the PangoLayoutIter
  // on ligatured characters (eg. The word "Lam-Aliph" in arabic). To work
  // around this, we use text reported using the PangoGlyphIter which is
  // accurate.
  // TODO(ranjith): Revisit whether this is still needed in newer versions of
  // pango.
  std::vector<std::string> cluster_text;
  if (GetClusterStrings(&cluster_text)) {
    ASSERT_HOST(cluster_text.size() == start_byte_to_box.size());
    int ind = 0;
    for (auto it = start_byte_to_box.begin(); it != start_byte_to_box.end(); ++it, ++ind) {
      it->second->mutable_ch()->swap(cluster_text[ind]);
    }
  }

  // Append to the boxchars list in byte order.
  std::vector<BoxChar *> page_boxchars;
  page_boxchars.reserve(start_byte_to_box.size());
  std::string last_ch;
  for (auto it = start_byte_to_box.begin(); it != start_byte_to_box.end(); ++it) {
    if (it->second->ch() == kWordJoinerUTF8) {
      // Skip zero-width joiner characters (ZWJs) here.
      delete it->second;
    } else {
      page_boxchars.push_back(it->second);
    }
  }
  CorrectBoxPositionsToLayout(&page_boxchars);

  if (render_fullwidth_latin_) {
    for (auto &it : start_byte_to_box) {
      // Convert fullwidth Latin characters to their halfwidth forms.
      std::string half(ConvertFullwidthLatinToBasicLatin(it.second->ch()));
      it.second->mutable_ch()->swap(half);
    }
  }

  // Merge the character boxes into word boxes if we are rendering n-grams.
  if (output_word_boxes_) {
    MergeBoxCharsToWords(&page_boxchars);
  }

  boxchars_.insert(boxchars_.end(), page_boxchars.begin(), page_boxchars.end());

  // Compute the page bounding box
  Box *page_box = nullptr;
  Boxa *all_boxes = nullptr;
  for (auto &page_boxchar : page_boxchars) {
    if (page_boxchar->box() == nullptr) {
      continue;
    }
    if (all_boxes == nullptr) {
      all_boxes = boxaCreate(0);
    }
    boxaAddBox(all_boxes, page_boxchar->mutable_box(), L_CLONE);
  }
  if (all_boxes != nullptr) {
    boxaGetExtent(all_boxes, nullptr, nullptr, &page_box);
    boxaDestroy(&all_boxes);
    if (page_boxes_ == nullptr) {
      page_boxes_ = boxaCreate(0);
    }
    boxaAddBox(page_boxes_, page_box, L_INSERT);
  }
}

void StringRenderer::CorrectBoxPositionsToLayout(std::vector<BoxChar *> *boxchars) {
  if (vertical_text_) {
    const double rotation = -pango_gravity_to_rotation(
        pango_context_get_base_gravity(pango_layout_get_context(layout_)));
    BoxChar::TranslateBoxes(page_width_ - h_margin_, v_margin_, boxchars);
    BoxChar::RotateBoxes(rotation, page_width_ - h_margin_, v_margin_, 0, boxchars->size(),
                         boxchars);
  } else {
    BoxChar::TranslateBoxes(h_margin_, v_margin_, boxchars);
  }
}

int StringRenderer::StripUnrenderableWords(std::string *utf8_text) const {
  std::string output_text;
  std::string unrenderable_words;
  const char *text = utf8_text->c_str();
  size_t offset = 0;
  int num_dropped = 0;
  while (offset < utf8_text->length()) {
    int space_len = SpanUTF8Whitespace(text + offset);
    output_text.append(text + offset, space_len);
    offset += space_len;
    if (offset == utf8_text->length()) {
      break;
    }

    int word_len = SpanUTF8NotWhitespace(text + offset);
    if (font_.CanRenderString(text + offset, word_len)) {
      output_text.append(text + offset, word_len);
    } else {
      ++num_dropped;
      unrenderable_words.append(text + offset, word_len);
      unrenderable_words.append(" ");
    }
    offset += word_len;
  }
  utf8_text->swap(output_text);

  if (num_dropped > 0) {
    tprintInfo("Stripped {} unrenderable word(s): '{}'\n", num_dropped, unrenderable_words.c_str());
  }
  return num_dropped;
}

int StringRenderer::RenderToGrayscaleImage(const char *text, int text_length, Image *pix) {
  Image orig_pix = nullptr;
  int offset = RenderToImage(text, text_length, &orig_pix);
  if (orig_pix) {
    *pix = pixConvertTo8(orig_pix, false);
    orig_pix.destroy();
  }
  return offset;
}

int StringRenderer::RenderToBinaryImage(const char *text, int text_length, int threshold,
                                        Image *pix) {
  Image orig_pix = nullptr;
  int offset = RenderToImage(text, text_length, &orig_pix);
  if (orig_pix) {
    Image gray_pix = pixConvertTo8(orig_pix, false);
    orig_pix.destroy();
    *pix = pixThresholdToBinary(gray_pix, threshold);
    gray_pix.destroy();
  } else {
    *pix = orig_pix;
  }
  return offset;
}

// Add word joiner (WJ) characters between adjacent non-space characters except
// immediately before a combiner.
/* static */
std::string StringRenderer::InsertWordJoiners(const std::string &text) {
  std::string out_str;
  const UNICHAR::const_iterator it_end = UNICHAR::end(text.c_str(), text.length());
  for (UNICHAR::const_iterator it = UNICHAR::begin(text.c_str(), text.length()); it < it_end;
       ++it) {
    // Add the symbol to the output string.
    out_str.append(it.utf8_data(), it.utf8_len());
    // Check the next symbol.
    UNICHAR::const_iterator next_it = it;
    ++next_it;
    bool next_char_is_boundary = (next_it == it_end || *next_it == ' ');
    bool next_char_is_combiner = (next_it == it_end) ? false : IsCombiner(*next_it);
    if (*it != ' ' && *it != '\n' && !next_char_is_boundary && !next_char_is_combiner) {
      out_str += kWordJoinerUTF8;
    }
  }
  return out_str;
}

// Convert halfwidth Basic Latin characters to their fullwidth forms.
std::string StringRenderer::ConvertBasicLatinToFullwidthLatin(const std::string &str) {
  std::string full_str;
  const UNICHAR::const_iterator it_end = UNICHAR::end(str.c_str(), str.length());
  for (UNICHAR::const_iterator it = UNICHAR::begin(str.c_str(), str.length()); it < it_end; ++it) {
    // Convert printable and non-space 7-bit ASCII characters to
    // their fullwidth forms.
    if (IsInterchangeValid7BitAscii(*it) && isprint(*it) && !isspace(*it)) {
      // Convert by adding 0xFEE0 to the codepoint of 7-bit ASCII.
      char32 full_char = *it + 0xFEE0;
      full_str.append(EncodeAsUTF8(full_char));
    } else {
      full_str.append(it.utf8_data(), it.utf8_len());
    }
  }
  return full_str;
}

// Convert fullwidth Latin characters to their halfwidth forms.
std::string StringRenderer::ConvertFullwidthLatinToBasicLatin(const std::string &str) {
  std::string half_str;
  UNICHAR::const_iterator it_end = UNICHAR::end(str.c_str(), str.length());
  for (UNICHAR::const_iterator it = UNICHAR::begin(str.c_str(), str.length()); it < it_end; ++it) {
    char32 half_char = FullwidthToHalfwidth(*it);
    // Convert fullwidth Latin characters to their halfwidth forms
    // only if halfwidth forms are printable and non-space 7-bit ASCII.
    if (IsInterchangeValid7BitAscii(half_char) && isprint(half_char) && !isspace(half_char)) {
      half_str.append(EncodeAsUTF8(half_char));
    } else {
      half_str.append(it.utf8_data(), it.utf8_len());
    }
  }
  return half_str;
}

// Returns offset to end of text substring rendered in this method.
int StringRenderer::RenderToImage(const char *text, int text_length, Image *pix) {
  if (pix && *pix) {
    pix->destroy();
  }
  InitPangoCairo();

  const int page_offset = FindFirstPageBreakOffset(text, text_length);
  if (!page_offset) {
    return 0;
  }
  start_box_ = boxchars_.size();
  start_line_box_ = line_boxchars_.size();

  if (!vertical_text_) {
    // Translate by the specified margin
    cairo_translate(cr_, h_margin_, v_margin_);
  } else {
    // Vertical text rendering is achieved by a two-step process of first
    // performing regular horizontal layout with character orientation set to
    // EAST, and then translating and rotating the layout before rendering onto
    // the desired image surface. The settings required for the former step are
    // done within InitPangoCairo().
    //
    // Translate to the top-right margin of page
    cairo_translate(cr_, page_width_ - h_margin_, v_margin_);
    // Rotate the layout
    double rotation = -pango_gravity_to_rotation(
        pango_context_get_base_gravity(pango_layout_get_context(layout_)));
    tprintInfo("Rotating by {} radians\n", rotation);
    cairo_rotate(cr_, rotation);
    pango_cairo_update_layout(cr_, layout_);
  }
  std::string page_text(text, page_offset);
  if (render_fullwidth_latin_) {
    // Convert Basic Latin to their fullwidth forms.
    page_text = ConvertBasicLatinToFullwidthLatin(page_text);
  }
  if (strip_unrenderable_words_) {
    StripUnrenderableWords(&page_text);
  }
  if (drop_uncovered_chars_ && !font_.CoversUTF8Text(page_text.c_str(), page_text.length())) {
    int num_dropped = font_.DropUncoveredChars(&page_text);
    if (num_dropped) {
      tprintWarn("Dropped {} uncovered characters\n", num_dropped);
    }
  }
  if (add_ligatures_) {
    // Add ligatures wherever possible, including custom ligatures.
    page_text = LigatureTable::Get()->AddLigatures(page_text, &font_);
  }
  if (underline_start_prob_ > 0) {
    SetWordUnderlineAttributes(page_text);
  }

  pango_layout_set_text(layout_, page_text.c_str(), page_text.length());

  if (pix) {
    // Set a white background for the target image surface.
    cairo_set_source_rgb(cr_, 1.0, 1.0, 1.0); // sets drawing colour to white
    // Fill the surface with the active colour (if you don't do this, you will
    // be given a surface with a transparent background to draw on)
    cairo_paint(cr_);
    // Set the ink color to black
    cairo_set_source_rgb(cr_, pen_color_[0], pen_color_[1], pen_color_[2]);
    // If the target surface or transformation properties of the cairo instance
    // have changed, update the pango layout to reflect this
    pango_cairo_update_layout(cr_, layout_);
    {
      DISABLE_HEAP_LEAK_CHECK; // for Fontconfig
      // Draw the pango layout onto the cairo surface
      pango_cairo_show_layout(cr_, layout_);
    }
    *pix = CairoARGB32ToPixFormat(surface_);
  }
  ComputeClusterBoxes();
  FreePangoCairo();
  // Update internal state variables.
  ++page_;
  return page_offset;
}

// Render a string to an image, returning it as an 8 bit pix.  Behaves as
// RenderString, except that it ignores the font set at construction and works
// through all the fonts, returning 0 until they are exhausted, at which point
// it returns the value it should have returned all along, but no pix this time.
// Fonts that don't contain a given proportion of the characters in the string
// get skipped.
// Fonts that work each get rendered and the font name gets added
// to the image.
// NOTE that no boxes are produced by this function.
//
// Example usage: To render a null terminated char-array "txt"
//
// int offset = 0;
// do {
//   Image pix;
//   offset += renderer.RenderAllFontsToImage(min_proportion, txt + offset,
//                                            strlen(txt + offset), nullptr,
//                                            &pix);
//   ...
// } while (offset < strlen(text));
//
int StringRenderer::RenderAllFontsToImage(double min_coverage, const char *text, int text_length,
                                          std::string *font_used, Image *image) {
  *image = nullptr;
  // Select a suitable font to render the title with.
  const char kTitleTemplate[] = "%s : %d hits = %.2f%%, raw = %d = %.2f%%";
  std::string title_font;
  if (!FontUtils::SelectFont(kTitleTemplate, strlen(kTitleTemplate), &title_font, nullptr)) {
    tprintWarn("Could not find a font to render image title with!\n");
    title_font = "Arial";
  }
  title_font += " 8";
  tprintInfo("Selected title font: {}\n", title_font);
  if (font_used) {
    font_used->clear();
  }

  std::string orig_font = font_.DescriptionName();
  if (char_map_.empty()) {
    total_chars_ = 0;
    // Fill the hash table and use that for computing which fonts to use.
    for (UNICHAR::const_iterator it = UNICHAR::begin(text, text_length);
         it < UNICHAR::end(text, text_length); ++it) {
      ++total_chars_;
      ++char_map_[*it];
    }
    tprintDebug("Total chars = {}\n", total_chars_);
  }
  const std::vector<std::string> &all_fonts = FontUtils::ListAvailableFonts();

  for (size_t i = font_index_; i < all_fonts.size(); ++i) {
    ++font_index_;
    int raw_score = 0;
    int ok_chars = FontUtils::FontScore(char_map_, all_fonts[i], &raw_score, nullptr);
    if (ok_chars > 0 && ok_chars >= total_chars_ * min_coverage) {
      set_font(all_fonts[i]);
      int offset = RenderToBinaryImage(text, text_length, 128, image);
      ClearBoxes(); // Get rid of them as they are garbage.
      const int kMaxTitleLength = 1024;
      char title[kMaxTitleLength];
      // warning C4774: 'snprintf' : format string expected in argument 3 is not a string literal
      snprintf(title, kMaxTitleLength, kTitleTemplate, all_fonts[i].c_str(), ok_chars,
               100.0 * ok_chars / total_chars_, raw_score, 100.0 * raw_score / char_map_.size());
      tprintDebug("{}\n", title);
      // This is a good font! Store the offset to return once we've tried all
      // the fonts.
      if (offset) {
        last_offset_ = offset;
        if (font_used) {
          *font_used = all_fonts[i];
        }
      }
      // Add the font to the image.
      set_font(title_font);
      v_margin_ /= 8;
      Image title_image = nullptr;
      RenderToBinaryImage(title, strlen(title), 128, &title_image);
      *image |= title_image;
      title_image.destroy();

      v_margin_ *= 8;
      set_font(orig_font);
      // We return the real offset only after cycling through the list of fonts.
      return 0;
    } else {
      tprintDebug("Font {} failed with {} hits = {}%%\n", all_fonts[i].c_str(), ok_chars,
              100.0 * ok_chars / total_chars_);
    }
  }
  font_index_ = 0;
  char_map_.clear();
  return last_offset_ == 0 ? -1 : last_offset_;
}

} // namespace tesseract

#endif
