#ifndef TESSERACT_CCSTRUCT_DEBUGPIXA_H_
#define TESSERACT_CCSTRUCT_DEBUGPIXA_H_

#include "image.h"

#include <allheaders.h>

#include <string>
#include <vector>

namespace tesseract {

  // Class to hold a Pixa collection of debug images with captions and save them
  // to a PDF file.
  class DebugPixa {
  public:
    // TODO(rays) add another constructor with size control.
    DebugPixa() {
      pixa_ = pixaCreate(0);
#ifdef TESSERACT_DISABLE_DEBUG_FONTS
      fonts_ = NULL;
#else
      fonts_ = bmfCreate(nullptr, 14);
#endif
    }
    // If the filename_ has been set and there are any debug images, they are
    // written to the set filename_.
    ~DebugPixa() {
      pixaDestroy(&pixa_);
      bmfDestroy(&fonts_);
    }

    // Adds the given pix to the set of pages in the PDF file, with the given
    // caption added to the top.
    void AddPix(const Image& pix, const char* caption, bool keep_a_copy = true) {
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

    // Return true wheen one or more images have been collected.
    bool HasPix() {
      return (pixaGetCount(pixa_) > 0);
    }

    // Sets the destination filename and enables images to be written to a PDF
    // on destruction.
    void WritePDF(const char* filename) {
      if (HasPix()) {
        // TODO: add the captions to the PDF as well, but in TEXT format, not as part of the pix (i.e. not using the bitmap `fonts_`)

        pixaConvertToPdf(pixa_, 300, 1.0f, 0, 0, "AllDebugImages", filename);
        //pixaClear(pixa_);
      }
    }

    static char* strnrpbrk(char* base, const char* breakset, size_t len)
    {
      for (size_t i = len; i > 0; ) {
        if (strchr(breakset, base[--i]))
          return base + i;
      }
      return nullptr;
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

    void WritePNGs(const char* filename) {
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

    void Clear()
    {
      pixaClear(pixa_);
      captions.clear();
    }

  private:
    // The collection of images to put in the PDF.
    Pixa* pixa_;
    // The fonts used to draw text captions.
    L_Bmf* fonts_;
    // the captions for each image
    std::vector<std::string> captions;
  };

} // namespace tesseract

#endif // TESSERACT_CCSTRUCT_DEBUGPIXA_H_
