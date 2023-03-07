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

#include <allheaders.h>
#include <tesseract/baseapi.h>
#include "dict.h"
#if defined(USE_OPENCL)
#  include "openclwrapper.h" // for OpenclDevice
#endif
#include <tesseract/renderer.h>
#include "simddetect.h"
#include "tprintf.h" // for tprintf
#include "tlog.h"

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
  char buf[2048] = "ERROR: ";

  if (module != nullptr) {
    snprintf(buf, sizeof(buf), "ERROR: %s: ", module);
  }
  size_t off = strlen(buf);
  vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
  buf[sizeof(buf) - 1] = 0;			// make sure string is NUL terminated under all circumstances.
  tprintf("{}\n", buf);
}

static void Win32WarningHandler(const char *module, const char *fmt, va_list ap) {
  char buf[2048] = "WARNING: ";

  if (module != nullptr) {
    snprintf(buf, sizeof(buf), "WARNING: %s: ", module);
  }
  size_t off = strlen(buf);
  vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
  buf[sizeof(buf) - 1] =
      0;  // make sure string is NUL terminated under all circumstances.
  tprintf("{}\n", buf);
}

#  endif /* HAVE_TIFFIO_H */

class AutoWin32ConsoleOutputCP {
public:
  explicit AutoWin32ConsoleOutputCP(UINT codeCP) {
    oldCP_ = GetConsoleOutputCP();
    SetConsoleOutputCP(codeCP);
  }
  ~AutoWin32ConsoleOutputCP() {
    SetConsoleOutputCP(oldCP_);
  }

private:
  UINT oldCP_;
};

static AutoWin32ConsoleOutputCP autoWin32ConsoleOutputCP(CP_UTF8);

#endif // _WIN32

static void PrintVersionInfo() {
  char *versionStrP;

  tprintf("tesseract {}\n", tesseract::TessBaseAPI::Version());

  versionStrP = getLeptonicaVersion();
  tprintf("  {}\n", versionStrP);
  lept_free(versionStrP);

  versionStrP = getImagelibVersions();
  tprintf("  {}\n", versionStrP);
  lept_free(versionStrP);

#ifdef USE_OPENCL
  cl_platform_id platform[4];
  cl_uint num_platforms;

  tprintf(" OpenCL info:\n");
  if (clGetPlatformIDs(4, platform, &num_platforms) == CL_SUCCESS) {
    tprintf("  Found {} platform(s).\n", num_platforms);
    for (unsigned n = 0; n < num_platforms; n++) {
      char info[256];
      if (clGetPlatformInfo(platform[n], CL_PLATFORM_NAME, 256, info, 0) == CL_SUCCESS) {
        tprintf("  Platform {} name: {}\n", n + 1, info);
      }
      if (clGetPlatformInfo(platform[n], CL_PLATFORM_VERSION, 256, info, 0) == CL_SUCCESS) {
        tprintf("  Version: {}\n", info);
      }
      cl_device_id devices[2];
      cl_uint num_devices;
      if (clGetDeviceIDs(platform[n], CL_DEVICE_TYPE_ALL, 2, devices, &num_devices) == CL_SUCCESS) {
        tprintf("  Found {} device(s).\n", num_devices);
        for (unsigned i = 0; i < num_devices; ++i) {
          if (clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 256, info, 0) == CL_SUCCESS) {
            tprintf("    Device {} name: {}\n", i + 1, info);
          }
        }
      }
    }
  }
#endif
#if defined(HAVE_NEON) || defined(__aarch64__)
  if (tesseract::SIMDDetect::IsNEONAvailable())
    tprintf(" Found NEON\n");
#else
  if (tesseract::SIMDDetect::IsAVX512BWAvailable()) {
    tprintf(" Found AVX512BW\n");
  }
  if (tesseract::SIMDDetect::IsAVX512FAvailable()) {
    tprintf(" Found AVX512F\n");
  }
  if (tesseract::SIMDDetect::IsAVX512VNNIAvailable()) {
    tprintf(" Found AVX512VNNI\n");
  }
  if (tesseract::SIMDDetect::IsAVX2Available()) {
    tprintf(" Found AVX2\n");
  }
  if (tesseract::SIMDDetect::IsAVXAvailable()) {
    tprintf(" Found AVX\n");
  }
  if (tesseract::SIMDDetect::IsFMAAvailable()) {
    tprintf(" Found FMA\n");
  }
  if (tesseract::SIMDDetect::IsSSEAvailable()) {
    tprintf(" Found SSE4.1\n");
  }
#endif
#ifdef _OPENMP
  tprintf(" Found OpenMP {}\n", _OPENMP);
#endif
#if defined(HAVE_LIBARCHIVE)
#  if ARCHIVE_VERSION_NUMBER >= 3002000
  tprintf(" Found {}\n", archive_version_details());
#  else
  tprintf(" Found {}\n", archive_version_string());
#  endif // ARCHIVE_VERSION_NUMBER
#endif   // HAVE_LIBARCHIVE
#if defined(HAVE_LIBCURL)
  tprintf(" Found {}\n", curl_version());
#endif
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
      " 11    Sparse text. Find as much text as possible in no"
      " particular order.\n"
      " 12    Sparse text with OSD.\n"
      " 13    Raw line. Treat the image as a single text line,\n"
      "       bypassing hacks that are Tesseract-specific.\n";

#if DISABLED_LEGACY_ENGINE
  const char *disabled_osd_msg = "\nNOTE: The OSD modes are currently disabled.\n";
  tprintf("{}{}", msg, disabled_osd_msg);
#else
  tprintf("{}", msg);
#endif
}

#if !DISABLED_LEGACY_ENGINE
static void PrintHelpForOEM() {
  const char *msg =
      "OCR Engine modes:\n"
      "  0    Legacy engine only.\n"
      "  1    Neural nets LSTM engine only.\n"
      "  2    Legacy + LSTM engines.\n"
      "  3    Default, based on what is available.\n";

  tprintf("{}", msg);
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
  tprintf(
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
      "\n"
      "NOTE: These options must occur before any configfile.\n"
      "",
      program, program, program, program, program, program, program
#if !DISABLED_LEGACY_ENGINE
      , program
#endif  // !DISABLED_LEGACY_ENGINE
  );

  PrintHelpForPSM();
#if !DISABLED_LEGACY_ENGINE
  tprintf("\n");
  PrintHelpForOEM();
#endif

  tprintf(
      "\n"
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
      "",
      program, program, program, program, program, program);
}

static void PrintHelpMessage(const char *program) {
  program = basename(program);
  tprintf(
      "Usage:\n"
      "  {} --help | --help-extra | --version\n"
      "  {} help [section]\n"
      "  {} --list-langs\n"
      "  {} --print-parameters\n"
      "  {} <imagename> <outputbase> [options...] [<configfile>...]\n"
      "\n"
      "OCR options:\n"
      "  -l LANG[+LANG]        Specify language(s) used for OCR.\n"
      "NOTE: These options must occur before any configfile.\n"
      "\n"
      "Stand-alone {} options:\n"
      "  --help                Show this help message.\n"
      "  --help-extra          Show extra help for advanced users.\n"
      "  --version             Show version information.\n"
      "  --list-langs          List available languages for tesseract engine.\n"
      "  --print-parameters    Print tesseract parameters.\n",
      program, program, program, program, program, program);
}

static bool SetVariablesFromCLArgs(tesseract::TessBaseAPI &api, int argc, const char** argv) {
  bool success = true;
  char opt1[256], opt2[255];
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      strncpy(opt1, argv[i + 1], 255);
      opt1[255] = '\0';
      char *p = strchr(opt1, '=');
      if (!p) {
        tprintf("ERROR: Missing '=' in configvar assignment for '{}'\n", opt1);
        success = false;
        break;
      }
      *p = 0;
      strncpy(opt2, strchr(argv[i + 1], '=') + 1, sizeof(opt2) - 1);
      opt2[254] = 0;
      ++i;

      if (!api.SetVariable(opt1, opt2)) {
        tprintf("ERROR: Could not set option: {}={}\n", opt1, opt2);
      }
    }
  }
  return success;
}

static void PrintLangsList(tesseract::TessBaseAPI &api) {
  std::vector<std::string> languages;
  api.GetAvailableLanguagesAsVector(&languages);
  tprintf("List of available languages in \"{}\" ({}):\n",
         api.GetDatapath(), languages.size());
  for (const auto &language : languages) {
    tprintf("{}\n", language);
  }
  api.End();
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

static bool checkArgValues(int arg, const char *mode, int count) {
  if (arg >= count || arg < 0) {
    tprintf("ERROR: Invalid {} value, please enter a number between 0-{}\n", mode, count - 1);
    return false;
  }
  return true;
}

//#include <filesystem>
#include <sstream>      // std::ostringstream
#include "imagedata.h"  // DocumentData

static void InfoTraineddata(const char** filenames) {
  const char* filename;
  while ((filename = *filenames++) != nullptr) {
    tesseract::TessdataManager mgr;
    if (!mgr.is_loaded() && !mgr.Init(filename)) {
      tprintf("Error: Error opening data file {}\n", filename);
    } else {
      if (mgr.IsLSTMAvailable()) {
        tprintf("{} - LSTM\n", filename);
      }
      if (mgr.IsBaseAvailable()) {
        tprintf("{} - legacy\n", filename);
      }
    }
  }
}

static void UnpackFiles(const char** filenames) {
  const char* filename;
  while ((filename = *filenames++) != nullptr) {
    tprintf("Extracting {}\n", filename);
    tesseract::DocumentData images(filename);
    if (!images.LoadDocument(filename, 0, 0, nullptr)) {
      tprintf("ERROR: Failed to read training data from {}!\n", filename);
      continue;
    }
#if 0
    tprintf("{} pages\n", images.NumPages());
    tprintf("{} size\n", images.PagesSize());
#endif
    for (int page = 0; page < images.NumPages(); page++) {
      std::string basename = filename;
      basename = basename.erase(basename.size() - 6);
      std::ostringstream stream;
      stream << basename << '_' << page;
      const tesseract::ImageData* image = images.GetPage(page);
#if 0
      const char* imagefilename = image->imagefilename().c_str();
      tprintf("fn: {}\n", imagefilename);
#endif
      const char* transcription = image->transcription().c_str();
      std::string gt_filename = stream.str() + ".gt.txt";
      FILE* f = fopen(gt_filename.c_str(), "wb");
      if (f == nullptr) {
        tprintf("ERROR: Writing {} failed\n", gt_filename);
        continue;
      }
      fprintf(f, "%s\n", transcription);
      fclose(f);
#if 0
      tprintf("gt page {}: {}\n", page, transcription);
#endif
      Pix* pix = image->GetPix();
      std::string image_filename = stream.str() + ".png";
      if (pixWrite(image_filename.c_str(), pix, IFF_PNG) != 0) {
        tprintf("ERROR: Writing {} failed\n", image_filename);
      }
      pixDestroy(&pix);
#if 0
      const GenericVector<TBOX>& boxes = image->boxes();
      const TBOX& box = boxes[0];
      box.print();
      const GenericVector<STRING>& box_texts = image->box_texts();
      tprintf("gt: {}\n", box_texts[0]);
#endif
    }
  }
}

namespace std {
namespace filesystem {
  bool exists(const char* filename);
}
}

bool std::filesystem::exists(const char* filename) {
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
  return _access(filename, 0) == 0;
#else
  return access(filename, 0) == 0;
#endif
}

// NOTE: arg_i is used here to avoid ugly *i so many times in this function
static bool ParseArgs(int argc, const char** argv, const char **lang, const char **image,
                      const char **outputbase, const char **datapath, l_int32 *dpi,
                      bool *list_langs,
                      const char **visible_pdf_image_file,
                      bool *print_parameters, bool *print_fonts_table,
                      std::vector<std::string> *vars_vec, std::vector<std::string> *vars_values,
                      l_int32 *arg_i, tesseract::PageSegMode *pagesegmode,
                      tesseract::OcrEngineMode *enginemode) {
  int i = 1;
  if (i < argc) {
    const char* verb = argv[i];
    if (verb[0] != '-' && !std::filesystem::exists(verb)) {
      i++;
      if (strcmp(verb, "help") == 0) {
        if (i < argc) {
          if (strcmp(argv[i], "extra") == 0) {
            PrintHelpExtra(argv[0]);
#if !DISABLED_LEGACY_ENGINE
          } else if ((strcmp(argv[i], "oem") == 0)) {
            PrintHelpForOEM();
#endif
          } else if ((strcmp(argv[i], "psm") == 0)) {
            PrintHelpForPSM();
          } else {
            tprintf("ERROR: No help available for {}\n", argv[i]);
          }
        } else {
          PrintHelpMessage(argv[0]);
        }
      } else if (strcmp(verb, "info") == 0) {
        InfoTraineddata(argv + i);
      } else if (strcmp(verb, "unpack") == 0) {
        UnpackFiles(argv + i);
      } else if (strcmp(verb, "version") == 0) {
        PrintVersionInfo();
      } else {
        tprintf("ERROR: Unknown action: {}\n", verb);
      }
      return true;
    }
  }
  bool noocr = false;
  for (i = 1; i < argc && (*outputbase == nullptr || argv[i][0] == '-'); i++) {
    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
      PrintHelpMessage(argv[0]);
      noocr = true;
    } else if (strcmp(argv[i], "--help-extra") == 0) {
      PrintHelpExtra(argv[0]);
      noocr = true;
    } else if ((strcmp(argv[i], "--help-psm") == 0)) {
      PrintHelpForPSM();
      noocr = true;
#if !DISABLED_LEGACY_ENGINE
    } else if ((strcmp(argv[i], "--help-oem") == 0)) {
      PrintHelpForOEM();
      noocr = true;
#endif
    } else if ((strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--version") == 0)) {
      PrintVersionInfo();
      noocr = true;
    } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
      *lang = argv[i + 1];
      ++i;
    } else if (strcmp(argv[i], "--tessdata-dir") == 0 && i + 1 < argc) {
      *datapath = argv[i + 1];
      ++i;
    } else if (strcmp(argv[i], "--dpi") == 0 && i + 1 < argc) {
      *dpi = atoi(argv[i + 1]);
      ++i;
    } else if (strcmp(argv[i], "--loglevel") == 0 && i + 1 < argc) {
      // Allow the log levels which are used by log4cxx.
      std::string loglevel_string = argv[++i];
      std::transform(loglevel_string.cbegin(), loglevel_string.cend(),
                   loglevel_string.begin(), // write to the same location
                   [](unsigned char c) { return std::toupper(c); });

      static const std::map<const std::string, int> loglevels {
        {"ALL", INT_MIN},
        {"TRACE", 5000},
        {"DEBUG", 10000},
        {"INFO", 20000},
        {"WARN", 30000},
        {"ERROR", 40000},
        {"FATAL", 50000},
        {"OFF", INT_MAX},
      };
      try {
        auto loglevel = loglevels.at(loglevel_string);
        FLAGS_tlog_level = loglevel;
      } catch (const std::out_of_range &e) {
		    (void)e;		// unused variable
        // TODO: Allow numeric argument?
        tprintf("ERROR: Unsupported --loglevel {}\n", loglevel_string);
        return false;
      }
    } else if (strcmp(argv[i], "--user-words") == 0 && i + 1 < argc) {
      vars_vec->push_back("user_words_file");
      vars_values->push_back(argv[i + 1]);
      ++i;
    } else if (strcmp(argv[i], "--user-patterns") == 0 && i + 1 < argc) {
      vars_vec->push_back("user_patterns_file");
      vars_values->push_back(argv[i + 1]);
      ++i;
    } else if (strcmp(argv[i], "--list-langs") == 0) {
      noocr = true;
      *list_langs = true;
    } else if (strcmp(argv[i], "--psm") == 0 && i + 1 < argc) {
      if (!checkArgValues(atoi(argv[i + 1]), "PSM", tesseract::PSM_COUNT)) {
        return false;
      }
      *pagesegmode = static_cast<tesseract::PageSegMode>(atoi(argv[i + 1]));
      ++i;
    } else if (strcmp(argv[i], "--oem") == 0 && i + 1 < argc) {
#if !DISABLED_LEGACY_ENGINE
      int oem = atoi(argv[i + 1]);
      if (!checkArgValues(oem, "OEM", tesseract::OEM_COUNT)) {
        return false;
      }
      *enginemode = static_cast<tesseract::OcrEngineMode>(oem);
#endif
      ++i;
    } else if (strcmp(argv[i], "--print-parameters") == 0) {
      noocr = true;
      *print_parameters = true;
#if !DISABLED_LEGACY_ENGINE
    } else if (strcmp(argv[i], "--print-fonts-table") == 0) {
      noocr = true;
      *print_fonts_table = true;
#endif  // !DISABLED_LEGACY_ENGINE
    } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      // handled properly after api init
      ++i;
    } else if (strcmp(argv[i], "--visible-pdf-image") == 0 && i + 1 < argc) {
      *visible_pdf_image_file = argv[i + 1];
      ++i;
    } else if (*image == nullptr) {
      *image = argv[i];
      i++;
      if (i == argc) {
        fprintf(stderr, "Error, missing outputbase command line argument\n");
        return false;
      }
      // outputbase follows image, don't allow options at that position.
      *outputbase = argv[i];
    } else {
      // Unexpected argument.
      tprintf("ERROR: Unknown command line argument '{}'\n", argv[i]);
      return false;
    }
  }

  *arg_i = i;

  if (*pagesegmode == tesseract::PSM_OSD_ONLY) {
    // OSD = orientation and script detection.
    if (*lang != nullptr && strcmp(*lang, "osd")) {
      // If the user explicitly specifies a language (other than osd)
      // or a script, only orientation can be detected.
      tprintf("WARNING: Detects only orientation with -l {}\n", *lang);
    } else {
      // That mode requires osd.traineddata to detect orientation and script.
      *lang = "osd";
    }
  }

  if (*outputbase == nullptr && noocr == false) {
    PrintHelpMessage(argv[0]);
    return false;
  }

  return true;
}

static void PreloadRenderers(tesseract::TessBaseAPI &api,
                             std::vector<std::unique_ptr<TessResultRenderer>> &renderers,
                             tesseract::PageSegMode pagesegmode, const char *outputbase) {
  if (pagesegmode == tesseract::PSM_OSD_ONLY) {
#if !DISABLED_LEGACY_ENGINE
    renderers.push_back(std::make_unique<tesseract::TessOsdRenderer>(outputbase));
#endif // !DISABLED_LEGACY_ENGINE
  } else {
    bool error = false;
    bool b;
    api.GetBoolVariable("tessedit_create_hocr", &b);
    if (b) {
      bool font_info;
      api.GetBoolVariable("hocr_font_info", &font_info);
      auto renderer = std::make_unique<tesseract::TessHOcrRenderer>(outputbase, font_info);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create hOCR output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_alto", &b);
    if (b) {
      auto renderer = std::make_unique<tesseract::TessAltoRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create ALTO output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_page", &b);
    if (b) {
      auto renderer = std::make_unique<tesseract::TessPAGERenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create ALTO output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_tsv", &b);
    if (b) {
      bool font_info;
      api.GetBoolVariable("hocr_font_info", &font_info);
      auto renderer = std::make_unique<tesseract::TessTsvRenderer>(outputbase, font_info);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create TSV output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_pdf", &b);
    if (b) {
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
      if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        tprintf("ERROR: Cannot set STDIN to binary: {}", strerror(errno));
#endif // WIN32
      bool textonly;
      api.GetBoolVariable("textonly_pdf", &textonly);
      auto renderer = std::make_unique<tesseract::TessPDFRenderer>(outputbase, api.GetDatapath(), textonly);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create PDF output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_write_unlv", &b);
    if (b) {
      api.SetVariable("unlv_tilde_crunching", "true");
      auto renderer = std::make_unique<tesseract::TessUnlvRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create UNLV output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_lstmbox", &b);
    if (b) {
      auto renderer = std::make_unique<tesseract::TessLSTMBoxRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create LSTM BOX output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_boxfile", &b);
    if (b) {
      auto renderer = std::make_unique<tesseract::TessBoxTextRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create BOX output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_wordstrbox", &b);
    if (b) {
      auto renderer = std::make_unique<tesseract::TessWordStrBoxRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create WordStr BOX output file: {}\n", strerror(errno));
        error = true;
      }
    }

    api.GetBoolVariable("tessedit_create_txt", &b);
    if (b || (!error && renderers.empty())) {
      // Create text output if no other output was requested
      // even if text output was not explicitly requested unless
      // there was an error.
      auto renderer = std::make_unique<tesseract::TessTextRenderer>(outputbase);
      if (renderer->happy()) {
        renderers.push_back(std::move(renderer));
      } else {
        tprintf("ERROR: Could not create TXT output file: {}\n", strerror(errno));
      }
    }
  }

  // Null-out the renderers that are
  // added to the root, and leave the root in the vector.
  for (size_t r = 1; r < renderers.size(); ++r) {
    renderers[0]->insert(renderers[r].get());
    renderers[r].release(); // at the moment insert() is owning
  }
}

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

  const char *lang = nullptr;
  const char *image = nullptr;
  const char *visible_image_file = nullptr;
  const char *outputbase = nullptr;
  const char *datapath = nullptr;
  const char *visible_pdf_image_file = nullptr;
  bool list_langs = false;
  bool print_parameters = false;
  bool print_fonts_table = false;
  l_int32 dpi = 0;
  int arg_i = 1;
  int ret_val = EXIT_SUCCESS;

  tesseract::PageSegMode pagesegmode = tesseract::PSM_AUTO;
#if DISABLED_LEGACY_ENGINE
  auto enginemode = tesseract::OEM_LSTM_ONLY;
#else
  tesseract::OcrEngineMode enginemode = tesseract::OEM_DEFAULT;
#endif
  std::vector<std::string> vars_vec;
  std::vector<std::string> vars_values;

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

  if (!ParseArgs(argc, argv, &lang, &image, &outputbase, &datapath, &dpi, &list_langs,
                 &visible_pdf_image_file,
                 &print_parameters, &print_fonts_table, &vars_vec, &vars_values, &arg_i,
                 &pagesegmode, &enginemode)) {
    return EXIT_FAILURE;
  }

  bool in_recognition_mode = !list_langs && !print_parameters && !print_fonts_table;

  if (lang == nullptr && in_recognition_mode) {
    // Set default language model if none was given and a model file is needed.
    lang = "eng";
  }

  if (image == nullptr && in_recognition_mode) {
    return EXIT_SUCCESS;
  }

  // Call GlobalDawgCache here to create the global DawgCache object before
  // the TessBaseAPI object. This fixes the order of destructor calls:
  // first TessBaseAPI must be destructed, DawgCache must be the last object.
  tesseract::Dict::GlobalDawgCache();

  {
  TessBaseAPI api;

  api.SetOutputName(outputbase);

  int config_count = argc - arg_i;
  const int init_failed = api.InitFull(datapath, lang, enginemode, (config_count > 0 ? &(argv[arg_i]) : nullptr), config_count,
                                   &vars_vec, &vars_values, false);

  if (!SetVariablesFromCLArgs(api, argc, argv)) {
    return EXIT_FAILURE;
  }

  // SIMD settings might be overridden by config variable.
  tesseract::SIMDDetect::Update();

  if (list_langs) {
    PrintLangsList(api);
    return EXIT_SUCCESS;
  }

  if (init_failed) {
    tprintf("ERROR: Could not initialize tesseract.\n");
    return EXIT_FAILURE;
  }

  if (print_parameters) {
	tprintf("Tesseract parameters:\n");
    api.PrintVariables();
    api.End();
    return EXIT_SUCCESS;
  }

#if !DISABLED_LEGACY_ENGINE
  if (print_fonts_table) {
    tprintf("Tesseract fonts table:\n");
    api.PrintFontsTable();
    api.End();
    return EXIT_SUCCESS;
  }
#endif  // !DISABLED_LEGACY_ENGINE

  // record the currently active input image path as soon as possible:
  // this path is also used to construct the destination path for 
  // various debug output files.
  api.SetInputName(image);

  FixPageSegMode(api, pagesegmode);

  if (dpi) {
    auto dpi_string = std::to_string(dpi);
    api.SetVariable("user_defined_dpi", dpi_string.c_str());
  }

  if (visible_pdf_image_file) {
    api.SetVisibleImageFilename(visible_pdf_image_file);
  }

  if (pagesegmode == tesseract::PSM_AUTO_ONLY) {
    Pix *pixs = pixRead(image);
    if (!pixs) {
      tprintf("ERROR: Leptonica can't process input file: {}\n", image);
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
      tprintf(
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

#if DISABLED_LEGACY_ENGINE
  auto cur_psm = api.GetPageSegMode();
  auto osd_warning = std::string("");
  if (cur_psm == tesseract::PSM_OSD_ONLY) {
    const char *disabled_osd_msg =
        "\nERROR: The page segmentation mode 0 (OSD Only) is currently "
        "disabled.\n\n";
    tprintf("{}",  disabled_osd_msg);
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

  if (debug_all)
  {
      api.SetVariable("textord_tabfind_show_images", "Y");
      api.SetVariable("textord_tabfind_show_vlines", "Y");

#if !GRAPHICS_DISABLED
      api.SetVariable("textord_tabfind_show_initial_partitions", "Y");
      api.SetVariable("textord_tabfind_show_reject_blobs", "Y");
      api.SetVariable("textord_tabfind_show_partitions", "2");
      api.SetVariable("textord_tabfind_show_columns", "Y");
      api.SetVariable("textord_tabfind_show_blocks", "Y");
#endif

      api.SetVariable("textord_noise_debug", "Y");
      api.SetVariable("textord_oldbl_debug", "N");  // very noisy output
      api.SetVariable("textord_baseline_debug", "Y");
      api.SetVariable("textord_debug_block", "9");
      api.SetVariable("textord_debug_bugs", "9");
      api.SetVariable("textord_debug_tabfind", "0" /* "9" */);  // very noisy output

      api.SetVariable("textord_debug_baselines", "Y");
      api.SetVariable("textord_debug_blob", "Y");
      api.SetVariable("textord_debug_blob", "Y");
      api.SetVariable("textord_debug_pitch_metric", "Y");
      api.SetVariable("textord_debug_pitch_test", "Y");
      api.SetVariable("textord_debug_printable", "Y");
      api.SetVariable("textord_debug_xheights", "Y");
      api.SetVariable("textord_debug_xheights", "Y");

      api.SetVariable("tessedit_create_hocr", "Y");
      api.SetVariable("tessedit_create_alto", "Y");
      api.SetVariable("tessedit_create_page", "Y");
      api.SetVariable("tessedit_create_tsv", "Y");
      api.SetVariable("tessedit_create_pdf", "Y");
      api.SetVariable("textonly_pdf", "n");
      api.SetVariable("tessedit_write_unlv", "Y");
      api.SetVariable("tessedit_create_lstmbox", "Y");
      api.SetVariable("tessedit_create_boxfile", "Y");
      api.SetVariable("tessedit_create_wordstrbox", "Y");
      api.SetVariable("tessedit_create_txt", "Y");

      api.SetVariable("tessedit_dump_choices", "Y");
      api.SetVariable("tessedit_dump_pageseg_images", "Y");

      api.SetVariable("tessedit_write_images", "Y");

      api.SetVariable("tessedit_adaption_debug", "Y");
      api.SetVariable("tessedit_debug_block_rejection", "Y");
      api.SetVariable("tessedit_debug_doc_rejection", "Y");
      api.SetVariable("tessedit_debug_fonts", "Y");
      api.SetVariable("tessedit_debug_quality_metrics", "Y");

      api.SetVariable("tessedit_rejection_debug", "Y");
      api.SetVariable("tessedit_timing_debug", "Y");

      api.SetVariable("tessedit_bigram_debug", "Y");

      api.SetVariable("tess_debug_lstm", "N");  // LSTM debug output is extremely noisy

      api.SetVariable("debug_noise_removal", "Y");


      api.SetVariable("classify_debug_level", "1" /* "9" */);  // LSTM debug output is extremely noisy
      api.SetVariable("classify_learning_debug_level", "9");
      api.SetVariable("classify_debug_character_fragments", "Y");
      api.SetVariable("classify_enable_adaptive_debugger", "Y");
      //api.SetVariable("classify_learn_debug_str", "????????????????");
      api.SetVariable("matcher_debug_separate_windows", "Y");
      api.SetVariable("matcher_debug_flags", "Y");
      api.SetVariable("matcher_debug_level", "3");

      api.SetVariable("multilang_debug_level", "3");

      api.SetVariable("paragraph_debug_level", "3");

      api.SetVariable("segsearch_debug_level", "3");

      api.SetVariable("stopper_debug_level", "3");

      api.SetVariable("superscript_debug", "Y");

      api.SetVariable("crunch_debug", "Y");

      api.SetVariable("dawg_debug_level", "3");

      api.SetVariable("debug_fix_space_level", "9");
      api.SetVariable("debug_x_ht_level", "3");
      //api.SetVariable("debug_file", "xxxxxxxxxxxxxxxxx");
      //api.SetVariable("debug_output_path", "xxxxxxxxxxxxxx");

      api.SetVariable("hyphen_debug_level", "3");

      api.SetVariable("language_model_debug_level", "3");

      api.SetVariable("tosp_debug_level", "3");

      api.SetVariable("wordrec_debug_level", "3");

      api.SetVariable("word_to_debug", "Y");

      api.SetVariable("scribe_save_grey_rotated_image", "Y");
      api.SetVariable("scribe_save_binary_rotated_image", "Y");
      api.SetVariable("scribe_save_original_rotated_image", "Y");

      api.SetVariable("hocr_font_info", "Y");
      api.SetVariable("hocr_char_boxes", "Y");
      api.SetVariable("hocr_images", "Y");

      api.SetVariable("thresholding_debug", "Y");

      api.SetVariable("preprocess_graynorm_mode", "0"); // 0..3

      api.SetVariable("tessedit_bigram_debug", "Y");

      api.SetVariable("wordrec_debug_blamer", "Y");

      api.SetVariable("devanagari_split_debugimage", "Y");
      api.SetVariable("devanagari_split_debuglevel", "3");

      api.SetVariable("gapmap_debug", "Y");

      api.SetVariable("poly_debug", "N");  // very noisy output

      api.SetVariable("edges_debug", "Y");

      api.SetVariable("ambigs_debug_level", "3");

      api.SetVariable("applybox_debug", "Y");

      api.SetVariable("bidi_debug", "Y");

      api.SetVariable("chop_debug", "Y");

      api.SetVariable("debug_baseline_fit", "1"); // 0..3
      api.SetVariable("debug_baseline_y_coord", "-2000");

      api.SetVariable("showcase_threshold_methods", "Y");

      api.SetVariable("debug_write_unlv", "Y");
      api.SetVariable("debug_line_finding", "Y");
      api.SetVariable("debug_image_normalization", "Y");
      api.SetVariable("debug_do_not_use_scrollview_app", "Y");

      api.SetVariable("interactive_display_mode", "Y");

      api.SetVariable("debug_display_page", "Y");
      api.SetVariable("debug_display_page_blocks", "Y");
      api.SetVariable("debug_display_page_baselines", "Y");
  }

  std::vector<std::unique_ptr<TessResultRenderer>> renderers;

  if (in_training_mode) {
    renderers.push_back(nullptr);
  } else if (outputbase != nullptr) {
    PreloadRenderers(api, renderers, pagesegmode, outputbase);
  }

  if (!renderers.empty()) {
#if DISABLED_LEGACY_ENGINE
    if (!osd_warning.empty()) {
      tprintf("{}", osd_warning);
    }
#endif
    bool succeed = api.ProcessPages(image, nullptr, 0, renderers[0].get());
    if (!succeed) {
      tprintf("ERROR: Error during page processing. File: {}\n", image);
      ret_val = EXIT_FAILURE;
    }
  }
  }

  if (ret_val == EXIT_SUCCESS) {
    api.ReportParamsUsageStatistics();
  }
  }
  // ^^^ end of scope for the Tesseract api instance
  // --> cache occupancy is removed, so the next call will succeed without fail (due to internal sanity checks)

  tesseract::Dict::CleanGlobalDawgCache();

  return ret_val;
}
