
#include "debugpixa.h"
#include "image.h"
#include "tprintf.h"

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

  DebugPixa::DebugPixa() {
    pixa_ = pixaCreate(0);
#ifdef TESSERACT_DISABLE_DEBUG_FONTS
    fonts_ = NULL;
#else
    fonts_ = bmfCreate(nullptr, 14);
#endif
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

  // Sets the destination filename and enables images to be written to a PDF
  // on destruction.
  void DebugPixa::WritePDF(const char* filename) {
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

  static void write_one_pix_for_html(FILE* html, int counter, const char* img_filename, const Image& pix, const char* title, const char* description, const Image* original_image)
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
      //auto w2 = pixGetWidth(*original_image);
      //auto h2 = pixGetHeight(*original_image);
      //pixSetAll(dstpix);
      //pixBlendBackgroundToColor(dstpix, *original_image, nullptr, 0xff000000, 1.0, 0, 255);
      //pixMultiplyByColor(dstpix, *original_image, nullptr, 0xff000000);

      int ow, oh, od;
      pixGetDimensions(*original_image, &ow, &oh, &od);

      Image toplayer = pixConvertTo32(pix);
      Image botlayer = pixConvertTo32(*original_image);

      if (w != ow || h != oh)
      {
        toplayer = pixScale(toplayer, ow * 1.0f / w, oh * 1.0f / h);
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
          
          int rvals, gvals, bvals;
          extractRGBValues(lines[j], &rvals, &gvals, &bvals);

          int rvald, gvald, bvald;
          extractRGBValues(lined[j], &rvald, &gvald, &bvald);

          // R
          rvald = rvald * 0.2 + 255 * 0.8;
          if (rvals < rvald)
            rvald = rvals;

          // G
          gvald = gvald * 0.7 + 255 * 0.3;
          if (gvals < gvald)
            gvald = gvals;

          // B
          bvald = bvald * 0.7 + 255 * 0.3;
          if (bvals < bvald)
            bvald = bvals;

          // A
          //avald = 0;

          composeRGBPixel(rvald, gvald, bvald, lined + j);
        }
      }
      //pixCopyResolution(pixd, pixs);
      //pixCopyInputFormat(pixd, pixs);

#if 0
      for (i = 0; i < h2; i++) {
        auto line = data + i * wpl;
        for (j = 0; j < w; j++) {
          int rval, gval, bval;
          extractRGBValues(line[j], &rval, &gval, &bval);
          nrval = (l_int32) (frval * rval + 0.5);
          ngval = (l_int32) (fgval * gval + 0.5);
          nbval = (l_int32) (fbval * bval + 0.5);
          composeRGBPixel(nrval, ngval, nbval, line + j);
        }
      }
#endif

      pixWrite(img_filename, botlayer, IFF_PNG);

      botlayer.destroy();
      toplayer.destroy();
    }

    fprintf(html, "<section>\n\
  <h2>image #%02d: %s</h2>\n\
  <figure>\n\
    <img src = \"%s\" >\n\
    <figcaption>size: %d x %d px; %s</figcaption>\n\
  </figure>\n\
  <p>%s</p>\n\
</section>\n",
    counter, title, pixfname, (int) w, (int) h, depth_str, description);
  }

  void DebugPixa::WriteHTML(const char* filename, const Image& original_image) {
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
<h1>Tesseract diagnostic image set</h1>\n\
<p>tesseract run @ %s</p>\n",
        now_str.c_str());

      {
        std::string fn(partname + ".img-original.png");

        write_one_pix_for_html(html, 0, fn.c_str(), original_image, "original image", "The original image as registered with the Tesseract instance.", nullptr);
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
        write_one_pix_for_html(html, counter, fn.c_str(), pixs, caption.c_str(), captions[i].c_str(), &original_image);

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
