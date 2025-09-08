#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <iostream>
#include <tesseract/renderer.h>
#include "tesseractclass.h"

namespace tesseract {

std::string JsonEscape(const char *text) {
  std::string ret;
  const char *ptr;
  for (ptr = text; *ptr; ptr++) {
    switch (*ptr) {
      case '"':
        ret += "\\\"";
        break;
      case '\\':
        ret += "\\\\";
        break;
      case '\b':
        ret += "\\b";
        break;
      case '\f':
        ret += "\\f";
        break;
      case '\n':
        ret += "\\n";
        break;
      case '\r':
        ret += "\\r";
        break;
      case '\t':
        ret += "\\t";
        break;
      default:
        ret += *ptr;
    }
  }
  return ret;
}


static void AddBoxToJson(const tesseract::ResultIterator* it, tesseract::PageIteratorLevel level, std::stringstream& json_str) {
  int left, top, right, bottom;
  it->BoundingBox(level, &left, &top, &right, &bottom);
  json_str << "{ \"x0\": " << left << ", \"y0\": " << top << ", \"x1\": " << right << ", \"y1\": " << bottom << " }";
}

static void AddBaselineCoordsToJson(const tesseract::ResultIterator* it, tesseract::PageIteratorLevel level, std::stringstream& json_str) {
  int left, top, right, bottom;
  it->BoundingBox(level, &left, &top, &right, &bottom);

  int x1, y1, x2, y2;
  if (!it->Baseline(level, &x1, &y1, &x2, &y2)) {
    return;
  }

  json_str << ",\n              \"baseline\": { " << "\"x0\": " << x1 << ", \"y0\": " << y1 << ", \"x1\": " << x2 << ", \"y1\": " << y2 << " }";
}

/**
 * Make a JSON-formatted string with JSON from the internal
 * data structures.
 * page_number is 0-based but will appear in the output as 1-based.
 * Image name/input_file_ can be set by SetInputName before calling
 * GetJSONText
 * STL removed from original patch submission and refactored by rays.
 * Returned string must be freed with the delete [] operator.
 */
char *TessBaseAPI::GetJSONText(int page_number) {
  return GetJSONText(nullptr, page_number);
}

/**
 * Make a JSON-formatted string with JSON from the internal
 * data structures.
 * page_number is 0-based but will appear in the output as 1-based.
 * Image name/input_file_ can be set by SetInputName before calling
 * GetJSONText
 * STL removed from original patch submission and refactored by rays.
 * Returned string must be freed with the delete [] operator.
 */
char* TessBaseAPI::GetJSONText(ETEXT_DESC* monitor, int page_number) {
  if (tesseract_ == nullptr ||
      (page_res_ == nullptr && Recognize(monitor) < 0)) {
    return nullptr;
  }

  std::stringstream json_str;
  json_str.imbue(std::locale::classic());
  json_str.precision(8);
  json_str << "{\n  \"page_id\": " << page_number + 1 << ",\n  \"blocks\": [";

  bool first_word = true;
  bool first_block = true;

  std::unique_ptr<ResultIterator> res_it(GetIterator());
  while (!res_it->Empty(tesseract::RIL_BLOCK)) {

    if (res_it->Empty(RIL_WORD)) {
      res_it->Next(RIL_WORD);
      continue;
    }

    if (res_it->IsAtBeginningOf(tesseract::RIL_BLOCK)) {

      // Skip non-text blocks.
      // In addition to generally not being useful to the user,
      // non-text blocks can cause major performance issues
      // for some images where they greatly outnumber the text blocks.
      if (!PTIsTextType(res_it->BlockType())) {
        res_it->Next(tesseract::RIL_BLOCK);
        continue;
      }

      if (!first_block) json_str << ",";
      first_block = false;
      json_str << "\n    {\n      \"bbox\": ";
      AddBoxToJson(res_it.get(), tesseract::RIL_BLOCK, json_str);

      if (recognition_done_) {
        const std::unique_ptr<const char[]> grapheme(
            res_it->GetUTF8Text(tesseract::RIL_BLOCK));
        json_str << ",\n      \"text\": \"" << JsonEscape(grapheme.get()).c_str() << "\"";
        json_str << ",\n      \"confidence\": " 
                << static_cast<int>(res_it->Confidence(tesseract::RIL_BLOCK));
      } else {
        json_str << ",\n      \"text\": null";
        json_str << ",\n      \"confidence\": null";
      }

      json_str << ",\n      \"blocktype\": " 
              << static_cast<int>(res_it->BlockType());

      json_str << ",\n      \"paragraphs\": [";
    }
    if (res_it->IsAtBeginningOf(tesseract::RIL_PARA)) {

      json_str << "\n        {\n          \"bbox\": ";
      AddBoxToJson(res_it.get(), tesseract::RIL_PARA, json_str);

      if (recognition_done_) {
        const std::unique_ptr<const char[]> grapheme(
            res_it->GetUTF8Text(tesseract::RIL_PARA));
        json_str << ",\n          \"text\": \"" << JsonEscape(grapheme.get()).c_str() << "\"";
        json_str << ",\n          \"confidence\": " 
                << static_cast<int>(res_it->Confidence(tesseract::RIL_PARA));
      } else {
        json_str << ",\n          \"text\": null";
        json_str << ",\n          \"confidence\": null";
      }

      json_str << ",\n          \"is_ltr\": " 
              << static_cast<int>(res_it->ParagraphIsLtr());

      json_str << ",\n          \"lines\": [";
    }
    if (res_it->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) {

      json_str << "\n            {\n              \"bbox\": ";
      AddBoxToJson(res_it.get(), tesseract::RIL_TEXTLINE, json_str);

      if (recognition_done_) {
        const std::unique_ptr<const char[]> grapheme(
            res_it->GetUTF8Text(tesseract::RIL_TEXTLINE));
        json_str << ",\n              \"text\": \"" << JsonEscape(grapheme.get()).c_str() << "\"";
        json_str << ",\n              \"confidence\": " 
                << static_cast<int>(res_it->Confidence(tesseract::RIL_TEXTLINE));
      } else {
        json_str << ",\n              \"text\": null";
        json_str << ",\n              \"confidence\": null";
      }

      float row_height, descenders, ascenders;
      res_it->RowAttributes(&row_height, &descenders, &ascenders);

      json_str << ",\n              \"rowAttributes\": {";
      json_str << "\n                \"rowHeight\": " << row_height;
      // Descenders is reported as a negative within Tesseract internally so we need to flip it.
      // The positive version is intuitive, and matches what is reported in the hOCR output.
      json_str << ",\n                \"descenders\": " << -descenders;
      json_str << ",\n                \"ascenders\": " << ascenders;
      json_str << "\n              }";

      AddBaselineCoordsToJson(res_it.get(), tesseract::RIL_TEXTLINE, json_str);
      json_str << ",\n              \"words\": [";
      first_word = true;
    }

    bool last_word_in_line = res_it->IsAtFinalElement(RIL_TEXTLINE, RIL_WORD);
    bool last_word_in_para = res_it->IsAtFinalElement(RIL_PARA, RIL_WORD);
    bool last_word_in_block = res_it->IsAtFinalElement(RIL_BLOCK, RIL_WORD);

    if (!first_word) json_str << ",";
    json_str << "\n                {\n                  \"bbox\": ";
    AddBoxToJson(res_it.get(), tesseract::RIL_WORD, json_str);

    if (recognition_done_) {
      const std::unique_ptr<const char[]> grapheme_word(
        res_it->GetUTF8Text(tesseract::RIL_WORD));
      json_str << ",\n                  \"text\": \"" << JsonEscape(grapheme_word.get()).c_str() << "\",";
      json_str << "\n                  \"confidence\": " << static_cast<int>(res_it->Confidence(tesseract::RIL_WORD));
    } else {
      json_str << ",\n                  \"text\": null,";
      json_str << "\n                  \"confidence\": null";
    }

    tesseract::WordChoiceIterator wc(*res_it);
    int wc_cnt = 0;
    json_str << ",\n                  \"choices\": [";
    do {
      const char *choice = wc.GetUTF8Text();
      if (choice != nullptr) {
        if (wc_cnt > 0) json_str << ",";
        wc_cnt++;
        json_str << "\n                    {\n";
        json_str << "                      \"text\": \"" << JsonEscape(choice).c_str() << "\",";
        json_str << "\n                      \"confidence\": " << static_cast<int>(wc.Confidence());
        json_str << "\n                   }";
      }
    } while (recognition_done_ && wc.Next());
    if (wc_cnt > 0) {
      json_str << "\n                  ]";
    } else {
      json_str << "]";
    }

    bool bold, italic, underlined, monospace, serif, smallcaps;
    int pointsize, font_id;
    const char* font_name =
        res_it->WordFontAttributes(&bold, &italic, &underlined, &monospace,
                                   &serif, &smallcaps, &pointsize, &font_id);
    json_str << ",\n                  \"font_name\": \"" << (font_name ? font_name : "") << "\"";

    // Add symbols array
    // This needs to happen last, as it will advance the iterator to the next word.
    json_str << ",\n                  \"symbols\": [";
    
    bool first_char = true;
    do {
      if (!first_char) json_str << ",";
      json_str << "\n";
      json_str << "                    {\n";
      json_str << "                      \"bbox\": ";
      AddBoxToJson(res_it.get(), tesseract::RIL_SYMBOL, json_str);

      if (recognition_done_) {
        const std::unique_ptr<const char[]> grapheme(
            res_it->GetUTF8Text(tesseract::RIL_SYMBOL));
        json_str << ",\n                      \"text\": \"" << JsonEscape(grapheme.get()).c_str() << "\"";
        json_str << ",\n                      \"confidence\": " 
                << static_cast<int>(res_it->Confidence(tesseract::RIL_SYMBOL));
      } else {
        json_str << ",\n                      \"text\": null";
        json_str << ",\n                      \"confidence\": null";
      }

      json_str << ",\n                      \"is_superscript\": "
              << static_cast<int>(res_it->SymbolIsSuperscript());
      json_str << ",\n                      \"is_subscript\": "
              << static_cast<int>(res_it->SymbolIsSubscript());
      json_str << ",\n                      \"is_dropcap\": "
              << static_cast<int>(res_it->SymbolIsDropcap());

      json_str        << "\n                    }";
      first_char = false;
      
      res_it->Next(tesseract::RIL_SYMBOL);
    } while (!res_it->Empty(tesseract::RIL_BLOCK) && 
             !res_it->IsAtBeginningOf(tesseract::RIL_WORD));

    json_str << "\n                  ]";
    json_str << "\n                }";
    first_word = false;

    // Close any ending block/paragraph/textline.
    if (last_word_in_line) {
      json_str << "\n              ]\n            }";
      if (!last_word_in_para) {
        json_str << ",";
      }
    }
    if (last_word_in_para) {
      json_str << "\n          ]\n        }";
      if (!last_word_in_block) {
        json_str << ",";
      }
    }
    if (last_word_in_block) {
      json_str << "\n      ]\n    }";
    }

  }

  json_str << "\n  ]\n}\n";

  const std::string &text = json_str.str();
  char *result = new char[text.length() + 1];
  strcpy(result, text.c_str());
  return result;

}


/**********************************************************************
 * JSON Text Renderer interface implementation
 **********************************************************************/
TessJsonRenderer::TessJsonRenderer(const char *outputbase)
    : TessResultRenderer(outputbase, "json") {
}

bool TessJsonRenderer::BeginDocumentHandler() {
  AppendString("{\n  \"version\": \"" TESSERACT_VERSION_STR "\",\n");
  AppendString("  \"pages\": [\n");

  return true;
}

bool TessJsonRenderer::AddImageHandler(TessBaseAPI *api) {
  const std::unique_ptr<const char[]> json(api->GetJSONText(imagenum()));
  if (json == nullptr) {
    return false;
  }

  AppendString(json.get());

  return true;
}

bool TessJsonRenderer::EndDocumentHandler() {
  AppendString("  ]\n}\n");

  return true;
}

} // namespace tesseract
