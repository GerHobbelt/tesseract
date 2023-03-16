
#include "debugpixa.h"
#include "image.h"
#include "tprintf.h"
#include "tesseractclass.h"

#include <allheaders.h>

#include <string>
#include <vector>
#include <chrono>  // chrono::system_clock
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip> // put_time


#if defined(HAVE_MUPDF)
#include "mupdf/fitz.h"
#endif

#undef TESSERACT_DISABLE_DEBUG_FONTS 
#define TESSERACT_DISABLE_DEBUG_FONTS 1

namespace tesseract {

  DebugPixa::DebugPixa(Tesseract* tesseract_ref)
    : tesseract_(tesseract_ref)
  {
    pixa_ = pixaCreate(0);
#ifdef TESSERACT_DISABLE_DEBUG_FONTS
    fonts_ = NULL;
#else
    fonts_ = bmfCreate(nullptr, 14);
#endif

    // set up the root info section:
    active_step_index = -1;
    PushNextSection("");
  }

  // If the filename_ has been set and there are any debug images, they are
  // written to the set filename_.
  DebugPixa::~DebugPixa() {
    pixaDestroy(&pixa_);
    bmfDestroy(&fonts_);
  }

  // Adds the given pix to the set of pages in the PDF file, with the given
  // caption added to the top.
  void DebugPixa::AddPix(const Image& pix, const char* caption) {
    int depth = pixGetDepth(pix);
#ifdef TESSERACT_DISABLE_DEBUG_FONTS
    pixaAddPix(pixa_, pix, L_COPY);
#else
    int color = depth < 8 ? 1 : (depth > 8 ? 0x00ff0000 : 0x80);
    Image pix_debug =
      pixAddSingleTextblock(pix, fonts_, caption, color, L_ADD_BELOW, nullptr);

    pixaAddPix(pixa_, pix_debug, L_INSERT);
#endif
    captions.push_back(caption);
  }

  void DebugPixa::AddPix(Image& pix, const char* caption, bool keep_a_copy) {
    int depth = pixGetDepth(pix);
#ifdef TESSERACT_DISABLE_DEBUG_FONTS
    pixaAddPix(pixa_, pix, keep_a_copy ? L_COPY : L_INSERT);
#else
    int color = depth < 8 ? 1 : (depth > 8 ? 0x00ff0000 : 0x80);
    Image pix_debug =
      pixAddSingleTextblock(pix, fonts_, caption, color, L_ADD_BELOW, nullptr);
    if (!keep_a_copy)
      pix.destroy();
    pixaAddPix(pixa_, pix_debug, L_INSERT);
#endif
    captions.push_back(caption);
  }


  // Return true when one or more images have been collected.
  bool DebugPixa::HasPix() const {
    return (pixaGetCount(pixa_) > 0);
  }

  void DebugPixa::PushNextSection(std::string title)
  {
    // sibling; but accept only one root!
    if (active_step_index < 0)
    {
      PushSubordinateSection(title);
      return;
    }
    ASSERT0(steps.size() >= 1);
    ASSERT0(active_step_index < steps.size());
    auto& prev_step = steps[active_step_index];
    int prev_level = prev_step.level;
    // accept only one root, so if root is 'active' again...
    if (prev_level == 0)
    {
      PushSubordinateSection(title);
      return;
    }

    PrepNextSection(prev_level, title);
  }

  void DebugPixa::PushSubordinateSection(std::string title)
  {
    // child (or root!)
    int prev_level = -1;
    if (active_step_index >= 0)
    {
      auto& prev_step = steps[active_step_index];
      prev_level = prev_step.level;
    }

    // child
    PrepNextSection(prev_level + 1, title);
  }

  void DebugPixa::PrepNextSection(int level, std::string title)
  {
    auto& step_ref = steps.emplace_back();
    // sibling
    step_ref.level = level;
    step_ref.title = title;
    step_ref.first_info_chunk = info_chunks.size();

    active_step_index = steps.size() - 1;

    auto& info_ref = info_chunks.emplace_back();
    info_ref.first_image_index = captions.size();     // neat way to get the number of images: every image comes with its own caption
  }

  void DebugPixa::PopSection()
  {
    int idx = active_step_index;
    ASSERT0(steps.size() >= 1);
    ASSERT0(active_step_index >= 0);
    ASSERT0(active_step_index < steps.size());

    // return to parent
    auto& step = steps[idx];
    auto level = step.level - 1;    // level we seek
    for (idx--; idx >= 0; idx--)
    {
      auto& prov_step = steps[idx];
      if (prov_step.level == level)
      {
        // bingo!
        active_step_index = idx;

        // now all we need is a fresh info_chunk:
        auto& info_ref = info_chunks.emplace_back();
        info_ref.first_image_index = captions.size();     // neat way to get the number of images: every image comes with its own caption
        return;
      }
    }
    // when we get here, we're already sitting at root, so nothing changes
  }

#if 0
  // Sets the destination filename and enables images to be written to a PDF
  // on destruction.
  void DebugPixa::WritePDF(const char* filename) {
    if (HasPix()) {
      // TODO: add the captions to the PDF as well, but in TEXT format, not as part of the pix (i.e. not using the bitmap `fonts_`)

      pixaConvertToPdf(pixa_, 300, 1.0f, 0, 0, "AllDebugImages", filename);
      //pixaClear(pixa_);
    }
  }
#endif

  static char* strnrpbrk(char* base, const char* breakset, size_t len)
  {
    for (size_t i = len; i > 0; ) {
      if (strchr(breakset, base[--i]))
        return base + i;
    }
    return nullptr;
  }

  static std::string check_unknown(const std::string& s, const char* default_value = "(unknown / nil)") {
    if (s.empty())
      return default_value;
    return s;
  }

  static void SanitizeCaptionForFilenamePart(std::string& str) {
    auto len = str.size();
    char* s = str.data();
    char* d = s;
    char* base = s;

    for (int i = 0; i < len; i++, s++) {
      switch (*s) {
      case ' ':
      case '\n':
      case ':':
      case '=':
      case '`':
      case '\'':
      case '"':
      case '~':
      case '?':
      case '*':
      case '|':
      case '&':
      case '<':
      case '>':
      case '{':
      case '}':
      case '\\':
      case '/':
        if (d > base && d[-1] != '.')
          *d++ = '.';
        continue;

      default:
        if (isprint(*s)) {
          *d++ = *s;
        }
        else {
          if (d > base && d[-1] != '.')
            *d++ = '.';
        }
        continue;
      }
    }

    if (d > base && d[-1] == '.')
      d--;

    len = d - base;

    // heuristics: shorten the part to a sensible length
    while (len > 40) {
      char* pnt = strnrpbrk(base, ".-_", len);
      if (!pnt)
        break;

      len = pnt - base;
    }

    // hard clip if heuristic didn't deliver:
    if (len > 40) {
      len = 40;
    }

    str.resize(len);
  }

#if 0
  void DebugPixa::WritePNGs(const char* filename) {
    if (HasPix()) {
      const char* ext = strrchr(filename, '.');
      std::string partname(filename);
      partname = partname.substr(0, ext - filename);
      int counter = 0;
      const char* label = NULL;
      int n = pixaGetCount(pixa_);

      for (int i = 0; i < n; i++) {
        counter++;
        char in[40];
        snprintf(in, 40, ".img%04d", counter);
        std::string caption = captions[i];
        SanitizeCaptionForFilenamePart(caption);
        const char* cprefix = (caption.empty() ? "" : ".");
        std::string fn(partname + in + cprefix + caption + /* ext */ ".png");

        auto pixs = pixaGetPix(pixa_, i, L_CLONE);
        if (pixs == nullptr) {
          L_ERROR("pixs[%d] not retrieved\n", __func__, i);
          continue;
        }
        pixWrite(fn.c_str(), pixs, IFF_PNG);
        pixDestroy(&pixs);
      }
      //pixaClear(pixa_);
    }
  }
#endif

  static inline int FADE(int val, const int factor) {
    return (val * factor + 255 * (256 - factor)) >> 8 /* div 256 */;
  }

  static inline int MIX(int val1, int val2, const int factor) {
    return (val2 * factor + val1 * (256 - factor)) >> 8 /* div 256 */;
  }

  static void write_one_pix_for_html(FILE* html, int counter, const char* img_filename, const Image& pix, const char* title, const char* description, Pix* original_image)
  {
    const char* pixfname = fz_basename(img_filename);
    int w, h, depth;
    pixGetDimensions(pix, &w, &h, &depth);
    const char* depth_str = ([depth]() {
      switch (depth) {
      default:
        return "unidentified color depth (probably color paletted)";
      case 1:
        return "monochrome (binary)";
      case 32:
        return "full color + alpha";
      case 24:
        return "full color";
      case 8:
        return "color palette (256 colors)";
      case 4:
        return "color palette (16 colors)";
      }
    })();

    if (original_image == nullptr) {
      pixWrite(img_filename, pix, IFF_PNG);
    }
    else {
      int ow, oh, od;
      pixGetDimensions(original_image, &ow, &oh, &od);

      Image toplayer = pixConvertTo32(pix);
      Image botlayer = pixConvertTo32(original_image);

      if (w != ow || h != oh)
      {
        // smaller images are generally masks, etc. and we DO NOT want to be confused by the smoothness
        // introduced by regular scaling, so we apply brutal sampled scale then:
        if (w < ow && h < oh) {
          toplayer = pixScaleBySamplingWithShift(toplayer, ow * 1.0f / w, oh * 1.0f / h, 0.0f, 0.0f);
        }
        else if (w > ow && h > oh) {
          // the new image has been either scaled up vs. the original OR a border was added (TODO)
          //
          // for now, we simply apply regular smooth scaling
          toplayer = pixScale(toplayer, ow * 1.0f / w, oh * 1.0f / h);
        }
        else {
          // non-uniform scaling...
          ASSERT0(!"Should never get here! Non-uniform scaling of images collected in DebugPixa!");
          toplayer = pixScale(toplayer, ow * 1.0f / w, oh * 1.0f / h);
        }
      }

      auto datas = pixGetData(toplayer);
      auto datad = pixGetData(botlayer);
      auto wpls = pixGetWpl(toplayer);
      auto wpld = pixGetWpl(botlayer);
      int i, j;
      for (i = 0; i < oh; i++) {
        auto lines = (datas + i * wpls);
        auto lined = (datad + i * wpld);
        for (j = 0; j < ow; j++) {
          // if top(SRC) is black, use that.
          // if top(SRC) is white, and bot(DST) isn't, color bot(DST) red and use that.
          // if top(SRC) is white, and bot(DST) is white, use white.

          // constant fade factors:
          const int red_factor = 0.1 * 256;
          const int green_factor = 0.5 * 256;
          const int blue_factor = 0.5 * 256;
          const int base_mix_factor = 0.90 * 256;
          const int bottom_mix_factor = 0.085 * 256;

          int rvals, gvals, bvals;
          extractRGBValues(lines[j], &rvals, &gvals, &bvals);

          int rvald, gvald, bvald;
          extractRGBValues(lined[j], &rvald, &gvald, &bvald);

          // R
          int rval = FADE(rvald, red_factor);
          if (rvals < rval)
            rval = MIX(rvals, rval, bottom_mix_factor);
          else
            rval = MIX(rvals, rval, base_mix_factor);

          // G
          int gval = FADE(gvald, green_factor);
          if (gvals < gval)
            gval = MIX(gvals, gval, bottom_mix_factor);
          else
            gval = MIX(gvals, gval, base_mix_factor);

          // B
          int bval = FADE(bvald, blue_factor);
          if (bvals < bval)
            bval = MIX(bvals, bval, bottom_mix_factor);
          else
            bval = MIX(bvals, bval, base_mix_factor);

          // A
          //avald = 0;

          composeRGBPixel(rval, gval, bval, lined + j);
        }
      }
      //pixCopyResolution(pixd, pixs);
      //pixCopyInputFormat(pixd, pixs);

      pixWrite(img_filename, botlayer, IFF_PNG);

      botlayer.destroy();
      toplayer.destroy();
    }

    fprintf(html, "<section>\n\
  <h2>image #%02d: %s</h2>\n\
  <figure>\n\
    <img src=\"%s\" >\n\
    <figcaption>size: %d x %d px; %s</figcaption>\n\
  </figure>\n\
  <p>%s</p>\n\
</section>\n",
      counter, title,
      pixfname,
      (int) w, (int) h, depth_str,
      description
    );
  }

  void DebugPixa::WriteHTML(const char* filename) {
    if (HasPix()) {
      const char* ext = strrchr(filename, '.');
      std::string partname(filename);
      partname = partname.substr(0, ext - filename);
      int counter = 0;
      const char* label = NULL;

      FILE* html = fopen(filename, "w");
      if (!html) {
        tprintf("ERROR: cannot open diagnostics HTML output file %s: %s\n", filename, strerror(errno));
        return;
      }

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
        std::string now_str = ss.str();

        std::ostringstream languages;
        int num_subs = tesseract_->num_sub_langs();
        if (num_subs > 0) {
          languages << "<p>Language";
          if (num_subs > 1)
            languages << "s";
          languages << ": ";
          int i;
          for (i = 0; i < num_subs - 1; ++i) {
            languages << tesseract_->get_sub_lang(i)->lang << " + ";
          }
          languages << tesseract_->get_sub_lang(i)->lang << "</p>";
        }

      fprintf(html, "<html>\n\
<head>\n\
  <title>Tesseract diagnostic image set</title>\n\
  <link rel=\"stylesheet\" href=\"https://unpkg.com/normalize.css@8.0.1/normalize.css\" >\n\
  <link rel=\"stylesheet\" href=\"https://unpkg.com/modern-normalize@1.1.0/modern-normalize.css\" >\n\
  <style>\n\
    html {\n\
      margin: 1em 2em;\n\
    }\n\
    h2 {\n\
          margin-top: 4em;\n\
          border-top: 1px solid grey;\n\
          padding-top: 1em;\n\
    }\n\
    img {\n\
      border: solid #b0cfff .5em;\n\
      max-width: 70em;\n\
      margin-left: auto;\n\
      margin-right: auto;\n\
      display: block;\n\
    }\n\
    figcaption {\n\
      background-color: #325180;\n\
      color: #fff;\n\
      font-style: italic;\n\
      padding: .2em;\n\
      text-align: center;\n\
    }\n\
    figure {\n\
      max-width: 70em;\n\
      margin-left: 0;\n\
      background-color: #c5d5ed;\n\
    }\n\
  </style>\n\
</head>\n\
<body>\n\
<article>\n\
<h1>Tesseract diagnostic image set</h1>\n\
<p>tesseract run @ %s</p>\n\
<p>Input image file path: %s</p>\n\
<p>Output base: %s</p>\n\
<p>Input image path: %s</p>\n\
<p>Primary Language: %s</p>\n\
%s\
<p>Language Data Path Prefix: %s</p>\n\
<p>Data directory: %s</p>\n\
<p>Main directory: %s</p>\n\
",
        now_str.c_str(),
        check_unknown(tesseract_->input_file_path).c_str(),
        check_unknown(tesseract_->imagebasename).c_str(),
        check_unknown(tesseract_->imagefile).c_str(),
        tesseract_->lang.c_str(),
        languages.str().c_str(),
        check_unknown(tesseract_->language_data_path_prefix).c_str(),
        check_unknown(tesseract_->datadir).c_str(),
        check_unknown(tesseract_->directory).c_str()
      );

      {
        std::string fn(partname + ".img-original.png");

        write_one_pix_for_html(html, 0, fn.c_str(), tesseract_->pix_original(), "original image", "The original image as registered with the Tesseract instance.", nullptr);
      }

      int n = pixaGetCount(pixa_);

      for (int i = 0; i < n; i++) {
        counter++;
        char in[40];
        snprintf(in, 40, ".img%04d", counter);
        std::string caption = captions[i];
        SanitizeCaptionForFilenamePart(caption);
        const char* cprefix = (caption.empty() ? "" : ".");
        std::string fn(partname + in + cprefix + caption + /* ext */ ".png");

        Image pixs = pixaGetPix(pixa_, i, L_CLONE);
        if (pixs == nullptr) {
          L_ERROR("pixs[%d] not retrieved\n", __func__, i);
          continue;
        }
        write_one_pix_for_html(html, counter, fn.c_str(), pixs, caption.c_str(), captions[i].c_str(), tesseract_->pix_original());

        pixs.destroy();
      }
      //pixaClear(pixa_);

      fprintf(html, "</body>\n\
</html>\n"
);
      fclose(html);
    }
  }

  void DebugPixa::Clear()
  {
    pixaClear(pixa_);
    captions.clear();
  }

} // namespace tesseract
