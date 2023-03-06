#ifndef TESSERACT_CCSTRUCT_DEBUGPIXA_H_
#define TESSERACT_CCSTRUCT_DEBUGPIXA_H_

#include "image.h"
#include "tprintf.h"

#include <allheaders.h>

#include <string>
#include <vector>

#if defined(HAVE_MUPDF)
#include "mupdf/fitz.h"
#endif

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

    // Return true when one or more images have been collected.
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

    void WriteHTML(const char* filename) {
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
    }\n\
  </style>\n\
</head>\n\
<body>\n\
<article>\n\
<h1>Tesseract diagnostic image set</h1>\n"
        );

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

          const char* pixfname = fz_basename(fn.c_str());
          auto w = pixGetWidth(pixs);
          auto h = pixGetHeight(pixs);
          auto depth = pixGetDepth(pixs);
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

          fprintf(html, "<section>\n\
  <h2>image #%02d: %s</h2>\n\
  <figure>\n\
    <img src = \"%s\" >\n\
    <figcaption>size: %d x %d px; %s</figcaption>\n\
  </figure>\n\
  <p>%s</p>\n\
</section>\n",
            i + 1, caption.c_str(), pixfname, (int)w, (int)h, depth_str, captions[i].c_str());

          pixDestroy(&pixs);
        }
        //pixaClear(pixa_);

        fprintf(html, "</body>\n\
</html>\n"
);
        fclose(html);
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
