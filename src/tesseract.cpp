/**********************************************************************
 * File:        tesseract.cpp
 * Description: Main program for merge of tess and editor.
 * Author:      Ray Smith
 *
 * (C) Copyright 1992, Hewlett-Packard Ltd.
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 *
 **********************************************************************/

// Include automatically generated configuration file if running autoconf
#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#include <cerrno> // for errno
#if defined(__USE_GNU)
#  include <cfenv> // for feenableexcept
#endif
#include <climits> // for INT_MIN, INT_MAX
#include <cstdlib> // for std::getenv
#include <iostream>
#include <map>    // for std::map
#include <memory> // std::unique_ptr
#include <sstream> // std::ostringstream

#include <leptonica/allheaders.h>
#if LIBLEPT_MINOR_VERSION > 82
#include <leptonica/pix_internal.h>
#endif
#include <tesseract/baseapi.h>
#include <tesseract/renderer.h>
#include "simddetect.h"
#include "tesseractclass.h" // for AnyTessLang
#include "tprintf.h" // for tprintf
#include "tlog.h"
#include "pathutils.h"
#include "imagedata.h" // DocumentData

#ifdef _OPENMP
#  include <omp.h>
#endif

#if defined(HAVE_LIBARCHIVE)
#  include <archive.h>
#endif
#if defined(HAVE_LIBCURL)
#  include <curl/curl.h>
#endif

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#  include <fcntl.h>
#  include <io.h>
#  if defined(HAVE_TIFFIO_H)

#    include <tiffio.h>

using namespace tesseract;

static void Win32ErrorHandler(const char *module, const char *fmt, va_list ap) {
  char buf[2048] = "";

  if (module != nullptr) {
    snprintf(buf, sizeof(buf), "%s: ", module);
  }
  size_t off = strlen(buf);
  vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
  buf[sizeof(buf) - 1] = 0;			// make sure string is NUL terminated under all circumstances.
  tprintError("{}\n", buf);
}

static void Win32WarningHandler(const char *module, const char *fmt, va_list ap) {
  char buf[2048] = "";

  if (module != nullptr) {
    snprintf(buf, sizeof(buf), "%s: ", module);
  }
  size_t off = strlen(buf);
  vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
  buf[sizeof(buf) - 1] =
      0;  // make sure string is NUL terminated under all circumstances.
  tprintWarn("{}\n", buf);
}

#  endif /* HAVE_TIFFIO_H */

#endif // _WIN32

static void PrintVersionInfo() {
  const char *versionStrP;

  tprintInfo("tesseract {}\n", tesseract::TessBaseAPI::Version());

  versionStrP = getLeptonicaVersion();
  tprintInfo("  {}\n", versionStrP);
  stringDestroy(&versionStrP);

  versionStrP = getImagelibVersions();
  tprintInfo("  {}\n", versionStrP);
  stringDestroy(&versionStrP);

#if defined(HAVE_NEON) || defined(__aarch64__)
  if (tesseract::SIMDDetect::IsNEONAvailable())
    tprintInfo(" Found NEON\n");
#else
  if (tesseract::SIMDDetect::IsAVX512BWAvailable()) {
    tprintInfo(" Found AVX512BW\n");
  }
  if (tesseract::SIMDDetect::IsAVX512FAvailable()) {
    tprintInfo(" Found AVX512F\n");
  }
  if (tesseract::SIMDDetect::IsAVX512VNNIAvailable()) {
    tprintInfo(" Found AVX512VNNI\n");
  }
  if (tesseract::SIMDDetect::IsAVX2Available()) {
    tprintInfo(" Found AVX2\n");
  }
  if (tesseract::SIMDDetect::IsAVXAvailable()) {
    tprintInfo(" Found AVX\n");
  }
  if (tesseract::SIMDDetect::IsFMAAvailable()) {
    tprintInfo(" Found FMA\n");
  }
  if (tesseract::SIMDDetect::IsSSEAvailable()) {
    tprintInfo(" Found SSE4.1\n");
  }
#endif
#ifdef _OPENMP
  tprintDebug(" Found OpenMP {}\n", _OPENMP);
#endif
#if defined(HAVE_LIBARCHIVE)
#  if ARCHIVE_VERSION_NUMBER >= 3002000
  tprintInfo(" Found {}\n", archive_version_details());
#  else
  tprintInfo(" Found {}\n", archive_version_string());
#  endif // ARCHIVE_VERSION_NUMBER
#endif   // HAVE_LIBARCHIVE
#if defined(HAVE_LIBCURL)
  tprintInfo(" Found {}\n", curl_version());
#endif
  tprintInfo("\n");
}

static void PrintHelpForPSM() {
  const char *msg =
      "Page segmentation modes:\n"
      "  0    Orientation and script detection (OSD) only.\n"
      "  1    Automatic page segmentation with OSD.\n"
      "  2    Automatic page segmentation, but no OSD, nor OCR.\n"
      "  3    Fully automatic page segmentation, but no OSD. (Default)\n"
      "  4    Assume a single column of text of variable sizes.\n"
      "  5    Assume a single uniform block of vertically aligned text.\n"
      "  6    Assume a single uniform block of text.\n"
      "  7    Treat the image as a single text line.\n"
      "  8    Treat the image as a single word.\n"
      "  9    Treat the image as a single word in a circle.\n"
      " 10    Treat the image as a single character.\n"
      " 11    Sparse text. Find as much text as possible in no particular order.\n"
      " 12    Sparse text with OSD.\n"
      " 13    Raw line. Treat the image as a single text line,\n"
      "       bypassing hacks that are Tesseract-specific.\n"
      "\n";

#if DISABLED_LEGACY_ENGINE
  const char *disabled_osd_msg = "\nNOTE: The OSD modes are currently disabled.\n";
  tprintInfo("{}{}", msg, disabled_osd_msg);
#else
  tprintInfo("{}", msg);
#endif
}

#if !DISABLED_LEGACY_ENGINE
static void PrintHelpForOEM() {
  const char *msg =
      "OCR Engine modes:\n"
      "  0    Legacy engine only.\n"
      "  1    Neural nets LSTM engine only.\n"
      "  2    Legacy + LSTM engines.\n"
      "  3    Default, based on what is available.\n"
      "\n";

  tprintInfo("{}", msg);
}
#endif // !DISABLED_LEGACY_ENGINE

static const char* basename(const char* path)
{
  size_t i;
  size_t len = strlen(path);
  for (i = strcspn(path, ":/\\"); i < len; i = strcspn(path, ":/\\"))
  {
    path = path + i + 1;
    len -= i + 1;
  }
  return path;
}

static void PrintHelpExtra(const char *program) {
  program = basename(program);
  tprintInfo(
      "Usage:\n"
      "  {} --help | --help-extra | --help-psm | "
#if !DISABLED_LEGACY_ENGINE
      "--help-oem | "
#endif
      "--version\n"
      "  {} --list-langs [--tessdata-dir <path>]\n"
#if !DISABLED_LEGACY_ENGINE
      "  {} --print-fonts-table [options...] [<configfile>...]\n"
#endif  // !DISABLED_LEGACY_ENGINE
      "  {} --print-parameters [options...] [<configfile>...]\n"
      "  {} info [<trainingfile>...]\n"
      "  {} unpack [<file>...]\n"
      "  {} version\n"
      "  {} <imagename>|<imagelist>|stdin <outputbase>|stdout [options...] "
      "[<configfile>...]\n"
      "\n"
      "OCR options:\n"
      "  --tessdata-dir PATH   Specify the location of tessdata path.\n"
      "  --user-words PATH     Specify the location of user words file.\n"
      "                        (Same as: -c user_words_file=PATH)\n"
      "  --user-patterns PATH  Specify the location of user patterns file.\n"
      "                        (Same as: -c user_patterns_file=PATH)\n"
      "  --dpi VALUE           Specify DPI for input image.\n"
      "  --loglevel LEVEL      Specify logging level. LEVEL can be\n"
      "                        ALL, TRACE, DEBUG, INFO, WARN, ERROR, FATAL or OFF.\n"
      "  --rectangle RECT      Specify rectangle(s) used for OCR.\n"
      "                        format: l173t257w2094h367[+l755t815w594h820[...]]\n"
      "  -l LANG[+LANG]        Specify language(s) used for OCR.\n"
      "  -c VAR=VALUE          Set value for config variables.\n"
      "                        Multiple -c arguments are allowed.\n"
      "  --psm NUM             Specify page segmentation mode.\n"
#if !DISABLED_LEGACY_ENGINE
      "  --oem NUM             Specify OCR Engine mode.\n"
#endif
      "  --visible-pdf-image PATH\n"
      "                        Specify path to source page image which will be\n"
      "                        used as image underlay in PDF output.\n"
      "                        (page rendered then as image + OCR text hidden overlay)\n"
      "  --config PATH         Specify the location of config file(s).\n"
      "                        You can specify multiple config files by issuing this\n"
      "                        option multiple times, once for each config file.\n"
      "                        When this option is used, it is assumed no further\n"
      "                        config files will be listed at the end of the command line.\n"
      "  --outputbase PATH     Specify the output base path (with possible filename\n"
      "                        at the end); this is equivalent to specifying the base path\n"
      "                        as an independent argument, but this option is useful\n"
      "                        when specifying multiple image input files: then those do not\n"
      "                        have to be followed by the base path at the end of the list.\n"
      "\n"
      "NOTE: These options must occur before any configfile.\n"
      "\n",
      program, program, program, program, program, program, program
#if !DISABLED_LEGACY_ENGINE
      , program
#endif  // !DISABLED_LEGACY_ENGINE
  );

  PrintHelpForPSM();
#if !DISABLED_LEGACY_ENGINE
  PrintHelpForOEM();
#endif

  tprintInfo(
      "Commands:\n"
      "\n"
      "  {} info [<trainingfile>...]\n"
      "                        Prints info about the trainingfile(s), whether they are\n"
      "                        LSTM (tesseract v4/v5) or Legacy (tesseract v3)\n"
      "\n"
      "  {} unpack [<file>...]\n"
      "                        Unpack training archives into transcription text files\n"
      "                        and image scans (pictures)\n"
      "\n"
      "  {} version\n"
      "                        Alias for '--version'.\n"
      "\n"
      "Stand-alone {} options:\n"
      "  -h, --help            Show minimal help message.\n"
      "  --help-extra          Show extra help for advanced users.\n"
      "  --help-psm            Show page segmentation modes.\n"
#if !DISABLED_LEGACY_ENGINE
      "  --help-oem            Show OCR Engine modes.\n"
#endif
      "  -v, --version         Show version information.\n"
      "  --rectangle           Specify rectangle(s) used for OCR.\n"
      "  --list-langs          List available languages for tesseract engine.\n"
#if !DISABLED_LEGACY_ENGINE
      "  --print-fonts-table   Print tesseract fonts table.\n"
#endif  // !DISABLED_LEGACY_ENGINE
      "  --print-parameters    Print tesseract parameters.\n"
      "\n"
      "You may also use the 'help' command as an alias for '--help' like this:\n"
      "  {} help\n"
      "or"
      "  {} help <section>\n"
      "where section is one of:\n"
      "  extra, oem, psm\n"
      "\n",
      program, program, program, program, program, program);
}

static void PrintHelpMessage(const char *program) {
  program = basename(program);
  tprintInfo(
      "Usage:\n"
      "  {} --help | --help-extra | --version\n"
      "  {} help [section]\n"
      "  {} --list-langs\n"
      "  {} --print-parameters\n"
      "  {} <imagename> <outputbase> [options...] [<configfile>...]\n"
      "\n"
      "OCR options:\n"
      "  --rectangle           Specify rectangle(s) used for OCR.\n"
      "  -l LANG[+LANG]        Specify language(s) used for OCR.\n"
      "NOTE: These options must occur before any configfile.\n"
      "\n"
      "Stand-alone {} options:\n"
      "  --help                Show this help message.\n"
      "  --help-extra          Show extra help for advanced users.\n"
      "  --version             Show version information.\n"
      "  --list-langs          List available languages for tesseract engine.\n"
      "  --print-parameters    Print tesseract parameters.\n"
	  "\n",
      program, program, program, program, program, program);
}

static bool SetVariablesFromCLArgs(tesseract::TessBaseAPI &api,
                                   const std::vector<std::string> &vars,
                                   const std::vector<std::string> &values)
{
  bool success = true;
  int len = vars.size();
  for (int i = 0; i < len; i++) {
    const std::string &var = vars[i];
    const std::string &value = values[i];

    if (!api.SetVariable(var.c_str(), value.c_str())) {
      tprintError("Could not set option: {}={}\n", var, value);
      success = false;
    }
  }
  return success;
}

static void PrintLangsList(tesseract::TessBaseAPI &api) {
  std::vector<std::string> languages;
  api.GetAvailableLanguagesAsVector(&languages);
  tprintInfo("List of available languages in \"{}\" ({}):\n",
         api.GetDatapath(), languages.size());
  for (const auto &language : languages) {
    tprintInfo("{}\n", language);
  }
  tprintInfo("\n");
}

/**
 * We have 2 possible sources of pagesegmode: a config file and
 * the command line. For backwards compatibility reasons, the
 * default in tesseract is tesseract::PSM_SINGLE_BLOCK, but the
 * default for this program is tesseract::PSM_AUTO. We will let
 * the config file take priority, so the command-line default
 * can take priority over the tesseract default, so we use the
 * value from the command line only if the retrieved mode
 * is still tesseract::PSM_SINGLE_BLOCK, indicating no change
 * in any config file. Therefore the only way to force
 * tesseract::PSM_SINGLE_BLOCK is from the command line.
 * It would be simpler if we could set the value before Init,
 * but that doesn't work.
 */
static void FixPageSegMode(tesseract::TessBaseAPI &api, tesseract::PageSegMode pagesegmode) {
  if (api.GetPageSegMode() == tesseract::PSM_SINGLE_BLOCK) {
    api.SetPageSegMode(pagesegmode);
  }
}

static void InfoTraineddata(const std::vector<std::string> &filenames) {
  for (const std::string &filename : filenames) {
    tesseract::TessdataManager mgr;
    if (!mgr.is_loaded() && !mgr.Init(filename.c_str())) {
      tprintError("Error opening data file {}\n", filename);
    } else {
      if (mgr.IsLSTMAvailable()) {
        tprintInfo("{} - LSTM\n", filename);
      }
      if (mgr.IsBaseAvailable()) {
        tprintInfo("{} - legacy\n", filename);
      }
    }
  }
  tprintInfo("\n");
}

static void UnpackFiles(const std::vector<std::string> &filenames) {
  for (const std::string &filename : filenames) {
    tprintInfo("Extracting {}\n", filename);
    tesseract::DocumentData images(filename);
    if (!images.LoadDocument(filename.c_str(), 0, 0, nullptr)) {
      tprintError("Failed to read training data from {}!\n", filename);
      continue;
    }
    tprintInfo("Extracted:\n");
    tprintInfo("  {} pages\n", images.NumPages());
    tprintInfo("  {} size\n", images.PagesSize());

    for (int page = 0; page < images.NumPages(); page++) {
      std::string basename = filename;
      basename = basename.erase(basename.size() - 6);
      std::ostringstream stream;
      stream << basename << '_' << page;
      const tesseract::ImageData* image = images.GetPage(page);
      tprintInfo("document page #{}: image file: {}\n", page, image->imagefilename());

      const char* transcription = image->transcription().c_str();
      std::string gt_filename = stream.str() + ".gt.txt";
      FILE* f = fopen(gt_filename.c_str(), "wb");
      if (f == nullptr) {
        tprintError("Writing ground truth transcription to file '{}' for document page #{} failed\n", gt_filename, page);
        continue;
      }
      fprintf(f, "%s\n", transcription);
      fclose(f);
      tprintInfo("Ground truth transcription for document page #{}: {}\n", page, transcription);

      Pix* pix = image->GetPix();
      std::string image_filename = stream.str() + ".png";
      if (pixWrite(image_filename.c_str(), pix, IFF_PNG) != 0) {
        tprintError("Writing {} failed\n", image_filename);
      }
      pixDestroy(&pix);

      const auto& boxes = image->boxes();
      const TBOX& box = boxes[0];
      box.print();
      const auto &box_texts = image->box_texts();
      tprintInfo("gt: {}\n", box_texts[0]);
    }
  }
}

typedef enum {
  NO_CMD = 0,
  HELP_BASIC = 0x01,
  HELP_EXTRA = 0x02,
  HELP_OEM = 0x04,
  HELP_PSM = 0x08,
  INFO = 0x10,
  UNPACK = 0x20,
  VERSION = 0x40,
  DO_OCR = 0x80,
  LIST_LANGUAGES = 0x100,
  PRINT_PARAMETERS = 0x200,
  PRINT_FONTS_TABLE = 0x400,

  WE_ARE_BUGGERED = 0x8000
} CommandVerb;

// Return a CommandVerb mix + parsed arguments in vectors
static int ParseArgs(int argc, const char** argv,
                     ParamsVector *vars_vec,
                      std::vector<std::string>* path_args) {
  bool dash_dash = false;
  int i;
  int cmd = NO_CMD;
  for (i = 1; i < argc; i++) {
    const char *verb = argv[i];
    ASSERT0(verb != nullptr);
    if (verb[0] != '-' || dash_dash) {
      if (cmd == NO_CMD) {
        if (strcmp(verb, "help") == 0) {
          const char *subverb = argv[i + 1];
          if (subverb && subverb[0] != '-') {
            if (strcmp(subverb, "extra") == 0) {
              cmd = HELP_EXTRA;
#if !DISABLED_LEGACY_ENGINE
            } else if ((strcmp(subverb, "oem") == 0)) {
              cmd = HELP_OEM;
#endif
            } else if ((strcmp(subverb, "psm") == 0)) {
              cmd = HELP_PSM;
            } else {
              tprintError(
                  "No help available for '{}'.\n"
                  "Did you mean 'extra', 'oem' or 'psm'?\n",
                  subverb);
              return WE_ARE_BUGGERED;
            }
            i++;
          } else {
            cmd = HELP_BASIC;
          }
          continue;
        } else if (strcmp(verb, "info") == 0) {
          cmd = INFO;
          continue;
        } else if (strcmp(verb, "unpack") == 0) {
          cmd = UNPACK;
          continue;
        } else if (strcmp(verb, "version") == 0) {
          cmd = VERSION;
          continue;
        } else if (!fs::exists(verb)) {
          tprintError("Unknown action: '{}'\n", verb);
          return WE_ARE_BUGGERED;
        }
        // fall through
      }

      if (cmd == NO_CMD)
        cmd = DO_OCR;
      path_args->push_back(verb);
    } else if (cmd != NO_CMD && strcmp(verb, "-") == 0) {
      // stdin/stdout path spec: treat as command path parameter
      path_args->push_back(verb);
    } else if ((strcmp(verb, "-h") == 0) ||
               (strcmp(verb, "--help") == 0)) {
      cmd |= HELP_BASIC;
    } else if (strcmp(verb, "--help-extra") == 0) {
      cmd |= HELP_EXTRA;
    } else if ((strcmp(verb, "--help-psm") == 0)) {
      cmd |= HELP_PSM;
#if !DISABLED_LEGACY_ENGINE
    } else if ((strcmp(verb, "--help-oem") == 0)) {
      cmd |= HELP_OEM;
#endif
    } else if ((strcmp(verb, "-v") == 0) || (strcmp(verb, "--version") == 0)) {
      cmd |= VERSION;
    } else if (strcmp(verb, "-l") == 0 && i + 1 < argc) {
      StringParam *p = vars_vec->find<StringParam>("languages_to_try");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--tessdata-dir") == 0 && i + 1 < argc) {
      StringParam *p = vars_vec->find<StringParam>("tessdata_path");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--dpi") == 0 && i + 1 < argc) {
      IntParam *p = vars_vec->find<IntParam>("user_defined_dpi");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--loglevel") == 0 && i + 1 < argc) {
      IntParam *p = vars_vec->find<IntParam>("loglevel");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--user-words") == 0 && i + 1 < argc) {
      StringParam *p = vars_vec->find<StringParam>("user_words_file");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--user-patterns") == 0 && i + 1 < argc) {
      StringParam *p = vars_vec->find<StringParam>("user_patterns_file");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--list-langs") == 0) {
      cmd |= LIST_LANGUAGES;
    } else if (strcmp(verb, "--rectangle") == 0 && i + 1 < argc) {
      StringParam *p = vars_vec->find<StringParam>("reactangles_to_process");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--psm") == 0 && i + 1 < argc) {
      IntParam *p = vars_vec->find<IntParam>("tessedit_pageseg_mode");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--oem") == 0 && i + 1 < argc) {
      IntParam *p = vars_vec->find<IntParam>("tessedit_ocr_engine_mode");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--print-parameters") == 0) {
      cmd |= PRINT_PARAMETERS;
#if !DISABLED_LEGACY_ENGINE
    } else if (strcmp(verb, "--print-fonts-table") == 0) {
      cmd |= PRINT_FONTS_TABLE;
#endif  // !DISABLED_LEGACY_ENGINE
    } else if (strcmp(verb, "-c") == 0 && i + 1 < argc) {
      // handled properly after api init
      const char *var_stmt = argv[i + 1];
      ++i;
      const char *p = strchr(var_stmt, '=');
      if (!p) {
        tprintError("Missing '=' in '-c' configvar assignment statement: '{}'\n", var_stmt);
        return false;
      }
      std::string name(var_stmt, p - var_stmt);
      Param *v = vars_vec->find(name.c_str(), ANY_TYPE_PARAM);
      v->set_value(p + 1);
    } else if (strcmp(verb, "--visible-pdf-image") == 0 && i + 1 < argc) {
      IntParam *p = vars_vec->find<IntParam>("visible_pdf_image_path");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--config") == 0 && i + 1 < argc) {
      StringParam *p = vars_vec->find<StringParam>("config_files");
      std::string cfs = p->value();
      if (!cfs.empty())
        cfs += ';';       // separator suitable for all platforms AFAIAC.
      cfs += argv[i + 1];
      p->set_value(cfs);
      ++i;
    } else if (strcmp(verb, "--outputbase") == 0 && i + 1 < argc) {
      StringParam *p = vars_vec->find<StringParam>("output_base_path");
      p->set_value(argv[i + 1]);
      ++i;
    } else if (strcmp(verb, "--") == 0) {
      dash_dash = true;
    } else {
      // Unexpected argument.
      tprintError("Unknown command line argument '{}'\n", verb);
      return WE_ARE_BUGGERED;
    }
  }

  return cmd;
}

static bool PreloadRenderers(tesseract::TessBaseAPI &api,
                             std::vector<std::unique_ptr<TessResultRenderer>> &renderers,
                             tesseract::PageSegMode pagesegmode, const char *outputbase) {
  bool error = false;
  if (pagesegmode == tesseract::PSM_OSD_ONLY) {
#if !DISABLED_LEGACY_ENGINE
    auto renderer = std::make_unique<tesseract::TessOsdRenderer>(outputbase);
    if (renderer->happy()) {
      renderers.push_back(std::move(renderer));
    } else {
      tprintError("Could not create OSD output file: {}\n",
              strerror(errno));
      error = true;
    }
#endif // !DISABLED_LEGACY_ENGINE
  } else {
    bool b;
    Tesseract &tess = api.tesseract();

    b = tess.tessedit_create_hocr;
    if (b) {
      bool font_info = tess.hocr_font_info;
      auto renderer = std::make_unique<tesseract::TessHOcrRenderer>(outputbase, font_info);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create hOCR output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_alto;
    if (b) {
      auto renderer = std::make_unique<tesseract::TessAltoRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create ALTO output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_page_xml;
    if (b) {
      auto renderer = std::make_unique<tesseract::TessPAGERenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create PAGE output file: %s\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_tsv;
    if (b) {
      bool lang_info = tess.tsv_lang_info;
      auto renderer = std::make_unique<tesseract::TessTsvRenderer>(outputbase, lang_info);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create TSV output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_pdf;
    if (b) {
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
      if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        tprintError("Cannot set STDIN to binary: {}", strerror(errno));
#endif // WIN32
      bool textonly = tess.textonly_pdf;
      auto renderer = std::make_unique<tesseract::TessPDFRenderer>(outputbase, api.GetDatapath(), textonly);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create PDF output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_write_unlv;
    if (b) {
      b = tess.unlv_tilde_crunching.set_value(true);
      auto renderer = std::make_unique<tesseract::TessUnlvRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create UNLV output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_lstmbox;
    if (b) {
      auto renderer = std::make_unique<tesseract::TessLSTMBoxRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create LSTM BOX output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_boxfile;
    if (b) {
      auto renderer = std::make_unique<tesseract::TessBoxTextRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create BOX output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_wordstrbox;
    if (b) {
      auto renderer = std::make_unique<tesseract::TessWordStrBoxRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create WordStr BOX output file: {}\n", strerror(errno));
        error = true;
      }
    }

    b = tess.tessedit_create_txt;
    if (b) {
      // Create text output if no other output was requested
      // even if text output was not explicitly requested unless
      // there was an error.
      auto renderer = std::make_unique<tesseract::TessTextRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintError("Could not create TXT output file: {}\n", strerror(errno));
        error = true;
      }
    }
  }

  if (!error) {
    // Null-out the renderers that are
    // added to the root, and leave the root in the vector.
    if (renderers.size() > 1) {
      for (size_t r = 1; r < renderers.size(); ++r) {
        renderers[0]->insert(renderers[r].get());
        renderers[r].release(); // at the moment insert() is owning
      }
      size_t l = 1;
#if !defined(NO_ASSERTIONS)
      for (size_t l = renderers.size(); l > 0; --l) {
        if (renderers[l - 1].get() != nullptr)
          break;
      }
#endif
      ASSERT0(l == 1);
      renderers.resize(l);
    }
  }

  return error;
}

namespace tesseract {

static inline auto format_as(Orientation o) {
  return fmt::underlying(o);
}

static inline auto format_as(TextlineOrder o) {
  return fmt::underlying(o);
}

static inline auto format_as(WritingDirection d) {
  return fmt::underlying(d);
}

}

static void SetupDebugAllPreset(TessBaseAPI &api)
{
  if (debug_all) {
    api.SetupDebugAllPreset();
  }
}

#if 0
static void pause_key(void) {
  (void)fgetc(stdin);
}
#endif

  /**********************************************************************
 *  main()
 *
 **********************************************************************/

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_main(int argc, const char** argv)
#endif
{
#if defined(__USE_GNU) && defined(HAVE_FEENABLEEXCEPT)
  // Raise SIGFPE.
#  if defined(__clang__)
  // clang creates code which causes some FP exceptions, so don't enable those.
  feenableexcept(FE_DIVBYZERO);
#  else
  feenableexcept(FE_DIVBYZERO | FE_OVERFLOW | FE_INVALID);
#  endif
#endif

#if 0
  atexit(pause_key);
#endif

  (void)tesseract::SetConsoleModeToUTF8();

  //const char *datapath = nullptr;
  //const char *visible_pdf_image_path = nullptr;
  int ret_val = EXIT_SUCCESS;

  //std::vector<std::string> vars_vec;
  //std::vector<std::string> vars_values;
  std::vector<std::string> path_params;

  if (std::getenv("LEPT_MSG_SEVERITY")) {
    // Get Leptonica message level from environment variable.
    setMsgSeverity(L_SEVERITY_EXTERNAL);
  } else {
#if defined(NDEBUG)
    // Disable debugging and informational messages from Leptonica.
    setMsgSeverity(L_SEVERITY_ERROR);
#else
    // Allow Leptonica to yak in debug builds.
    setMsgSeverity(DEFAULT_SEVERITY);
#endif
  }

#if defined(HAVE_TIFFIO_H) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
  /* Show libtiff errors and warnings on console (not in GUI). */
  TIFFSetErrorHandler(Win32ErrorHandler);
  TIFFSetWarningHandler(Win32WarningHandler);
#endif // HAVE_TIFFIO_H && _WIN32

  TessBaseAPI api;
  Tesseract &tess = api.tesseract();
  // get the list of available parameters for both Tesseract instances and as globals:
  // that's the superset we accept at the command line.
    auto& parlst = tess.params_collective();
    ParamsVector args_muster = parlst.flattened_copy();

  int cmd = ParseArgs(argc, argv, &args_muster, &path_params);
  if (cmd == WE_ARE_BUGGERED) {
    return EXIT_FAILURE;
  }

  if (cmd == NO_CMD) {
    PrintHelpMessage(argv[0]);
    return 7;
  }

  if (cmd & HELP_EXTRA) {
    PrintHelpExtra(argv[0]);
    cmd &= ~(HELP_OEM | HELP_PSM | HELP_BASIC);
  }
#if !DISABLED_LEGACY_ENGINE
  if (cmd & HELP_OEM) {
    PrintHelpForOEM();
  }
#endif
  if (cmd & HELP_PSM) {
    PrintHelpForPSM();
  }
  if (cmd & HELP_BASIC) {
    PrintHelpMessage(argv[0]);
  }
  if (cmd & VERSION) {
    PrintVersionInfo();
  }

  if (cmd & INFO) {
    InfoTraineddata(path_params);
  }

  int hlpcmds = cmd & (HELP_EXTRA | HELP_OEM | HELP_PSM | HELP_BASIC | INFO | LIST_LANGUAGES | PRINT_PARAMETERS | PRINT_FONTS_TABLE);
  cmd &= ~(HELP_EXTRA | HELP_OEM | HELP_PSM | HELP_BASIC | INFO | LIST_LANGUAGES | PRINT_PARAMETERS | PRINT_FONTS_TABLE);
  if (hlpcmds && cmd) {
    tprintError("Cannot mix non-help commands with help commands or any others. Action is aborted.\n");
    return EXIT_FAILURE;
  }
  if (cmd != UNPACK && cmd != DO_OCR) {
    tprintError("Cannot mix unpack and ocr commands. Action is aborted.\n");
    return EXIT_FAILURE;
  }

  if (cmd == UNPACK) {
    UnpackFiles(path_params);
  } else {
    assert(cmd == DO_OCR);

    // pull the path_params[] set apart into three different parts:
    // - source image(s) / imagelist (text) files
    // - target == output base name
    // - config files, to be loaded after tesseract instance is initialized
    //
    // 
    // Locate the outputbase (directory) argument in the parsed-from-CLI set:
    // the ones that follow it should be viable config files, while all preceding it
    // should be viable image files (or image *list* files); we wish to support
    // processing multiple image files in one run, so it's not *obvious* index '1' any more!

    if (path_params.size() < 1) {
      tprintError("Missing source image file as command line argument\n");
      return EXIT_FAILURE;
    }

    // First check if we may expect config files at the end of the list; if so,
    // collect them.
    StringParam *cfgs = args_muster.find<StringParam>("config_files");
    bool expect_cfg_files_at_end_of_list = !(cfgs && !cfgs->value().empty());

    StringParam *obp = args_muster.find<StringParam>("output_base_path");
    bool outputbasepath_is_specified = !(obp && !obp->value().empty());

    if (expect_cfg_files_at_end_of_list) {
      int min_expected_non_cfg_files = (outputbasepath_is_specified ? 1 : 2);

      while (path_params.size() << 1 > min_expected_non_cfg_files) {
        std::string cfgpath = path_params.back();
        if (fs::exists(cfgpath) && !fs::is_directory(cfgpath)) {
          path_params.pop_back();
          ParamsVectorSet muster_set({args_muster});
          if (ParamUtils::ReadParamsFile(cfgpath, muster_set, PARAM_VALUE_IS_SET_BY_CONFIGFILE)) {
            return EXIT_FAILURE;
          }
        }
      }

      // this parameter MAY have been set in one of those config files so better to recheck:
      obp = args_muster.find<StringParam>("output_base_path");
      outputbasepath_is_specified = !(obp && !obp->value().empty());
    }

    // second: grab the output_base_path if we haven't already:
    if (!outputbasepath_is_specified) {
      if (path_params.size() < 2) {
        tprintError("Error, missing outputbase command line argument\n");
        return EXIT_FAILURE;
      }

      std::string outpath = path_params.back();
      path_params.pop_back();
      obp = args_muster.find<StringParam>("output_base_path");
      obp->set_value(outpath);
    }

    // ... and now our path_params list is all images and image-list files.
    // Recognizing the latter (vs. older text-based image file formats) takes
    // a little heuristic, which we'll employ now to transform those into
    // an (updated) list of image files...
    if (api.ExpandImagelistFilesInSet(&path_params, args_muster) < 0) {
      return EXIT_FAILURE;
    }





      if (!SetVariablesFromCLArgs(api, args_muster)) {
        return EXIT_FAILURE;
      }

      // make sure the debug_all preset is set up BEFORE any command-line arguments
      // direct tesseract to set some arbitrary parameters just below,
      // for otherwise those `-c xyz=v` commands may be overruled by the
      // debug_all preset!
      if (debug_all) {
        api.SetupDebugAllPreset();

#if 0
        // repeat the `-c var=val` load as debug_all MAY have overwritten some of
        // these user-specified settings in the call above.
        if (!SetVariablesFromCLArgs(api, vars_vec, vars_values)) {
          return EXIT_FAILURE;
        }
#endif
      }

      Tesseract &tess = api.tesseract();

#if 0
      //source_image
      lang = api.languages_to_try;
      tesseract::PageSegMode pagesegmode = tesseract::PSM_AUTO;
      tesseract::OcrEngineMode enginemode = tesseract::OEM_DEFAULT;
      pagesegmode = api.tessedit_pageseg_mode;
      enginemode = api.tessedit_ocr_engine_mode;
#endif

  if (tess.tessedit_pageseg_mode == tesseract::PSM_OSD_ONLY) {
    // OSD = orientation and script detection.
    if (!tess.languages_to_try.empty() && strcmp(tess.languages_to_try.c_str(), "osd") != 0) {
      // If the user explicitly specifies a language (other than osd)
      // or a script, only orientation can be detected.
      tprintWarn("Detects only orientation with -l {}\n", tess.languages_to_try);
    } else {
      // That mode requires osd.traineddata to detect orientation and script.
      tess.languages_to_try = "osd";
    }

    assert(tess.languages_to_try.empty());
#if 0
    if (tess.languages_to_try.empty()) {
      // Set default language model if none was given and a model file is
      // needed.
      tess.languages_to_try = "eng";
    }
#endif
  }

#if 0
  // Call GlobalDawgCache here to create the global DawgCache object before
  // the TessBaseAPI object. This fixes the order of destructor calls:
  // first TessBaseAPI must be destructed, DawgCache must be the last object.
  tesseract::Dict::GlobalDawgCache();
#endif

#if 0
    api.SetOutputName(outputbase ::: output_base_path);
#endif

    const int init_failed = api.InitFull(datapath, path_params, args_muster);

    // SIMD settings might be overridden by config variable.
    tesseract::SIMDDetect::Update();

    if (hlpcmds & LIST_LANGUAGES) {
      PrintLangsList(api);
    }

    if (hlpcmds & PRINT_PARAMETERS) {
      tprintDebug("Tesseract parameters:\n");
      api.PrintVariables();
    }

#if !DISABLED_LEGACY_ENGINE
    if (hlpcmds & PRINT_FONTS_TABLE) {
      tprintDebug("Tesseract fonts table:\n");
      api.PrintFontsTable();
    }
#endif // !DISABLED_LEGACY_ENGINE

    if (init_failed) {
      tprintError("Could not initialize tesseract.\n");
      return EXIT_FAILURE;
    }

    ASSERT_HOST(&api.tesseract() != nullptr);

    // if we've done all we had to do, it's time to go bye bye.
    if (!cmd) {
      api.End();
      return EXIT_SUCCESS;
    }

    if (!tess.reactangles_to_process.empty()) {
      BOXA *rects = Tesseract::ParseRectsString(tess.reactangles_to_process.c_str());

      Pix* pixs = pixRead(image);
      if (!pixs) {
        tprintError("Cannot open input file: {}\n", image);
        return EXIT_FAILURE;
      }

      api.SetImage(pixs);

      std::string outfile = std::string(outputbase) + std::string(".txt");
      FILE* fout = NULL;

      if (strcmp(outputbase, "stdout") != 0) {
        fout = fopen(outfile.c_str(), "wb");
      }
      else {
        fout = stdout;
      }

      if (fout == NULL) {
        tprintError("Cannot open output file: {}\n", outfile);
        pixDestroy(&pixs);
        return EXIT_FAILURE;
      }

      // for each rectangle...
      bool errored = false;
      auto n = boxaGetCount(rects);
      for (int boxidx = 0; boxidx < n; boxidx++) {
        l_int32 top, left, width, height, bottom, right;
        errored |= (0 != boxaGetBoxGeometry(rects, boxidx, &left, &top, &width, &height));

        bottom = top + height;
        right = left + width;

        // clamp this rectangle
        if (left < 0) {
          left = 0;
        }

        if (top < 0) {
          top = 0;
        }

        if (right > pixs->w) {
          width = pixs->w - left;
        }

        if (bottom > pixs->h) {
          height = pixs->h - top;
        }

        if (width > 0 && height > 0) {
          char *utf8 = NULL;

          api.SetRectangle(left, top, width, height);
          utf8 = api.GetUTF8Text();
          if (utf8) {
            fwrite(utf8, 1, strlen(utf8), fout);
            delete[] utf8;
            utf8 = NULL;
          }
        } else {
          tprintError("incorrect rectangle size/shape as it doesn't sit on the page image (width: {}, height: {}) as this is what we got after clipping it with the source image: (left: {}, top: {}, width: {}, height: {})\n", pixs->w, pixs->h, left, top, width, height);
          errored = true;
        }
      }
      boxaDestroy(&rects);
      fclose(fout);
      pixDestroy(&pixs);
      return errored ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    // record the currently active input image path as soon as possible:
    // this path is also used to construct the destination path for 
    // various debug output files.
    api.SetInputName(image);

    FixPageSegMode(api, pagesegmode);

    if (tess.visible_pdf_image_path) {
      api.SetVisibleImageFilename(tess.visible_pdf_image_path);
    }

    if (pagesegmode == tesseract::PSM_AUTO_ONLY) {
      Pix* pixs = pixRead(image);
      if (!pixs) {
        tprintError("Leptonica can't process input file: {}\n", image);
        return 2;
      }

      api.SetImage(pixs);

      tesseract::Orientation orientation;
      tesseract::WritingDirection direction;
      tesseract::TextlineOrder order;
      float deskew_angle;

      const std::unique_ptr<const tesseract::PageIterator> it(api.AnalyseLayout());
      if (it) {
        // TODO: Implement output of page segmentation, see documentation
        // ("Automatic page segmentation, but no OSD, or OCR").
        it->Orientation(&orientation, &direction, &order, &deskew_angle);
        tprintDebug(
            "Orientation: {}\nWritingDirection: {}\nTextlineOrder: {}\n"
            "Deskew angle: {}\n",
            orientation, direction, order, deskew_angle);
      } else {
        ret_val = EXIT_FAILURE;
      }

      pixDestroy(&pixs);
    }
    else {
      // Set in_training_mode to true when using one of these configs:
      // ambigs.train, box.train, box.train.stderr, linebox, rebox, lstm.train.
      // In this mode no other OCR result files are written.
      bool b = false;
      bool in_training_mode = (api.GetBoolVariable("tessedit_ambigs_training", &b) && b) ||
        (api.GetBoolVariable("tessedit_resegment_from_boxes", &b) && b) ||
        (api.GetBoolVariable("tessedit_make_boxes_from_boxes", &b) && b) ||
        (api.GetBoolVariable("tessedit_train_line_recognizer", &b) && b);

      if (api.GetPageSegMode() == tesseract::PSM_OSD_ONLY) {
        if (!api.tesseract().AnyTessLang()) {
          fprintf(stderr, "Error, OSD requires a model for the legacy engine\n");
          return EXIT_FAILURE;
        }
      }
#if DISABLED_LEGACY_ENGINE
      auto cur_psm = api.GetPageSegMode();
      auto osd_warning = std::string("");
      if (cur_psm == tesseract::PSM_OSD_ONLY) {
        const char* disabled_osd_msg =
          "\nERROR: The page segmentation mode 0 (OSD Only) is currently "
          "disabled.\n\n";
        tprintDebug("{}", disabled_osd_msg);
        return EXIT_FAILURE;
      } else if (cur_psm == tesseract::PSM_AUTO_OSD) {
        api.SetPageSegMode(tesseract::PSM_AUTO);
        osd_warning +=
          "\nWARNING: The page segmentation mode 1 (Auto+OSD) is currently "
          "disabled. "
          "Using PSM 3 (Auto) instead.\n\n";
      } else if (cur_psm == tesseract::PSM_SPARSE_TEXT_OSD) {
        api.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
        osd_warning +=
          "\nWARNING: The page segmentation mode 12 (Sparse text + OSD) is "
          "currently disabled. "
          "Using PSM 11 (Sparse text) instead.\n\n";
      }
#endif // DISABLED_LEGACY_ENGINE

      std::vector<std::unique_ptr<TessResultRenderer>> renderers;

      bool succeed = true;

      if (in_training_mode) {
        renderers.push_back(nullptr);
      } else if (outputbase != nullptr) {
        succeed &= !PreloadRenderers(api, renderers, pagesegmode, outputbase);
        if (succeed && renderers.empty()) {
          // default: TXT + HOCR renderer
          api.SetupDefaultPreset();
          succeed &= !PreloadRenderers(api, renderers, pagesegmode, outputbase);
        }
      }

      if (!renderers.empty()) {
#if DISABLED_LEGACY_ENGINE
        if (!osd_warning.empty()) {
          tprintDebug("{}", osd_warning);
        }
#endif

        succeed &= api.ProcessPages(image, nullptr, 0, renderers[0].get());

        if (!succeed) {
          tprintError("Error during page processing. File: {}\n", image);
          ret_val = EXIT_FAILURE;
        }
      }
    }

    if (ret_val == EXIT_SUCCESS && verbose_process) {
      api.ReportParamsUsageStatistics();
    }

    // TODO? write/flush log output
    api.Clear();
  }
  // ^^^ end of scope for the Tesseract `api` instance
  // --> cache occupancy is removed, so the next call will succeed without fail (due to internal sanity checks)

  TessBaseAPI::ClearPersistentCache();

  return ret_val;
}

