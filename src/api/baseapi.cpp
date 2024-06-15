/**********************************************************************
 * File:        baseapi.cpp
 * Description: Simple API for calling tesseract.
 * Author:      Ray Smith
 *
 * (C) Copyright 2006, Google Inc.
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

#define _USE_MATH_DEFINES // for M_PI

// Include automatically generated configuration file if running autoconf.
#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#include <tesseract/debugheap.h>
#include "boxword.h"    // for BoxWord
#include "coutln.h"     // for C_OUTLINE_IT, C_OUTLINE_LIST
#include "dawg_cache.h" // for DawgCache
#include "dict.h"       // for Dict
#include "elst.h"       // for ELIST_ITERATOR, ELISTIZE, ELISTIZEH
#include <leptonica/environ.h>    // for l_uint8
#include "equationdetect.h" // for EquationDetect, destructor of equ_detect_
#include "errcode.h" // for ASSERT_HOST
#include "helpers.h" // for IntCastRounded, chomp_string, copy_string
#include "host.h"    // for MAX_PATH
#include "imagedata.h" // for ImageData, DocumentData
#include <leptonica/imageio.h> // for IFF_TIFF_G4, IFF_TIFF, IFF_TIFF_G3, ...
#if !DISABLED_LEGACY_ENGINE
#  include "intfx.h" // for INT_FX_RESULT_STRUCT
#endif
#include "mutableiterator.h" // for MutableIterator
#include "normalis.h"        // for kBlnBaselineOffset, kBlnXHeight
#include "pageres.h"         // for PAGE_RES_IT, WERD_RES, PAGE_RES, CR_DE...
#include "paragraphs.h"      // for DetectParagraphs
#include "global_params.h"
#include "pdblock.h"         // for PDBLK
#include "points.h"          // for FCOORD
#include "polyblk.h"         // for POLY_BLOCK
#include "rect.h"            // for TBOX
#include "stepblob.h"        // for C_BLOB_IT, C_BLOB, C_BLOB_LIST
#include "tessdatamanager.h" // for TessdataManager, kTrainedDataSuffix
#include "tesseractclass.h"  // for Tesseract
#include "tprintf.h"         // for tprintf
#include "werd.h"            // for WERD, WERD_IT, W_FUZZY_NON, W_FUZZY_SP
#include "tabletransfer.h"   // for detected tables from tablefind.h
#include "thresholder.h"     // for ImageThresholder
#include "winutils.h"
#include "colfind.h"         // for param globals
#include "oldbasel.h"        // for param globals
#include "tovars.h"          // for param globals
#include "makerow.h"         // for param globals
#include "topitch.h"         // for param globals
#include "polyaprx.h"        // for param globals
#include "edgblob.h"         // for param globals
#include "pathutils.h"       // for fs namespace

#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>       // for ETEXT_DESC
#include <tesseract/osdetect.h>       // for OSResults, OSBestResult, OrientationId...
#include <tesseract/renderer.h>       // for TessResultRenderer
#include <tesseract/resultiterator.h> // for ResultIterator
#include <parameters/parameters.h>    // for Param, ..., ParamVectorSet class definitions
#include <tesseract/assert.h>

#include <cmath>    // for round, M_PI
#include <cstdint>  // for int32_t
#include <cstring>  // for strcmp, strcpy
#include <filesystem> // for path
#include <fstream>  // for size_t
#include <iostream> // for std::cin
#include <locale>   // for std::locale::classic
#include <memory>   // for std::unique_ptr
#include <set>      // for std::pair
#include <sstream>  // for std::stringstream
#include <vector>   // for std::vector
#include <cfloat>

#include <leptonica/allheaders.h> // for pixDestroy, boxCreate, boxaAddBox, box...
#ifdef HAVE_LIBCURL
#  include <curl/curl.h>
#endif

#ifdef __linux__
#  include <csignal> // for sigaction, SA_RESETHAND, SIGBUS, SIGFPE
#endif

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#  include <fcntl.h>
#  include <io.h>
#else
#  include <dirent.h> // for closedir, opendir, readdir, DIR, dirent
#  include <libgen.h>
#  include <sys/stat.h> // for stat, S_IFDIR
#  include <sys/types.h>
#  include <unistd.h>
#endif // _WIN32


namespace tesseract {

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

BOOL_VAR(stream_filelist, false, "Stream a filelist from stdin.");
BOOL_VAR(show_threshold_images, false, "Show grey/binary 'thresholded' (pre-processed) images.");
STRING_VAR(document_title, "", "Title of output document (used for hOCR and PDF output).");
#ifdef HAVE_LIBCURL
INT_VAR(curl_timeout, 0, "Timeout for curl in seconds.");
STRING_VAR(curl_cookiefile, "", "File with cookie data for curl");
#endif
INT_VAR(debug_all, 0, "Turn on all the debugging features. Set to '2' or higher for extreme verbose debug diagnostics output.");
BOOL_VAR(debug_misc, false, "Turn on miscellaneous debugging features.");
#if !GRAPHICS_DISABLED
BOOL_VAR(scrollview_support, false, "Turn ScrollView support on/off. When turned OFF, the OCR process executes a little faster but almost all graphical feedback/diagnostics features will have been disabled.");
#endif
BOOL_VAR(verbose_process, false, "Print descriptive messages reporting which steps are taken during the OCR process. This may help non-expert users to better grasp what is happening under the hood and which stages of the OCR process take up time.");
STRING_VAR(vars_report_file, "+", "Filename/path to write the 'Which -c variables were used' report. File may be 'stdout', '1' or '-' to be output to stdout. File may be 'stderr', '2' or '+' to be output to stderr. Empty means no report will be produced.");
BOOL_VAR(report_all_variables, true, "When reporting the variables used (via 'vars_report_file') also report all *unused* variables, hence the report will always list *all* available variables.");
DOUBLE_VAR(allowed_image_memory_capacity, ImageCostEstimate::get_max_system_allowance(), "Set maximum memory allowance for image data: this will be used as part of a sanity check for oversized input images.");
BOOL_VAR(two_pass, false, "Enable double analysis: this will analyse every image twice. Once with the given page segmentation mode (typically 3), and then once with a single block page segmentation mode. The second run runs on a modified image where any earlier blocks are turned black, causing Tesseract to skip them for the second analysis. Currently two pages are output for a single image, so this is clearly a hack, but it's not as computationally intensive as running two full runs. (In fact, it might add as little as ~10% overhead, depending on the input image)   WARNING: This will probably break weird non-filepath file input patterns like \"-\" for stdin, or things that resolve using libcurl.");


/** Minimum sensible image size to be worth running Tesseract. */
const int kMinRectSize = 10;
/** Character returned when Tesseract couldn't recognize anything. */
const char kTesseractReject = '~';
/** Character used by UNLV error counter as a reject. */
const char kUNLVReject = '~';
/** Character used by UNLV as a suspect marker. */
const char kUNLVSuspect = '^';
/**
 * Temp file used for storing current parameters before applying retry values.
 */
static const char *kOldVarsFile = "failed_vars.txt";

#if !DISABLED_LEGACY_ENGINE

static const char kUnknownFontName[] = "UnknownFont";

static STRING_VAR(classify_font_name, kUnknownFontName,
                  "Default font name to be used in training.");

// Finds the name of the training font and returns it in fontname, by cutting
// it out based on the expectation that the filename is of the form:
// /path/to/dir/[lang].[fontname].exp[num]
// The [lang], [fontname] and [num] fields should not have '.' characters.
// If the global parameter classify_font_name is set, its value is used instead.
static void ExtractFontName(const char* filename, std::string* fontname) {
  *fontname = classify_font_name;
  if (*fontname == kUnknownFontName) {
    // filename is expected to be of the form [lang].[fontname].exp[num]
    // The [lang], [fontname] and [num] fields should not have '.' characters.
    const char *basename = strrchr(filename, '/');
    const char *firstdot = strchr(basename ? basename : filename, '.');
    const char *lastdot  = strrchr(filename, '.');
    if (firstdot != lastdot && firstdot != nullptr && lastdot != nullptr) {
      ++firstdot;
      *fontname = firstdot;
      fontname->resize(lastdot - firstdot);
    }
  }
}
#endif

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

/* Add all available languages recursively.
 */
static void addAvailableLanguages(const std::string &datadir, const std::string &base,
                                  std::vector<std::string> *langs) {
  auto base2 = base;
  if (!base2.empty()) {
    base2 += "/";
  }
  const size_t extlen = sizeof(kTrainedDataSuffix);
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
  const auto kTrainedDataSuffixUtf16 = winutils::Utf8ToUtf16(kTrainedDataSuffix);

  WIN32_FIND_DATAW data;
  HANDLE handle = FindFirstFileW(winutils::Utf8ToUtf16((datadir + base2 + "*").c_str()).c_str(), &data);
  if (handle != INVALID_HANDLE_VALUE) {
    BOOL result = TRUE;
    for (; result;) {
      wchar_t *name = data.cFileName;
      // Skip '.', '..', and hidden files
      if (name[0] != '.') {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
          addAvailableLanguages(datadir, base2 + winutils::Utf16ToUtf8(name), langs);
        } else {
          size_t len = wcslen(name);
          if (len > extlen && name[len - extlen] == '.' &&
              wcscmp(&name[len - extlen + 1], kTrainedDataSuffixUtf16.c_str()) == 0) {
            name[len - extlen] = '\0';
            langs->push_back(base2 + winutils::Utf16ToUtf8(name));
          }
        }
      }
      result = FindNextFileW(handle, &data);
    }
    FindClose(handle);
  }
#else // _WIN32
  DIR *dir = opendir((datadir + base).c_str());
  if (dir != nullptr) {
    dirent *de;
    while ((de = readdir(dir))) {
      char *name = de->d_name;
      // Skip '.', '..', and hidden files
      if (name[0] != '.') {
        struct stat st;
        if (stat((datadir + base2 + name).c_str(), &st) == 0 && (st.st_mode & S_IFDIR) == S_IFDIR) {
          addAvailableLanguages(datadir, base2 + name, langs);
        } else {
          size_t len = strlen(name);
          if (len > extlen && name[len - extlen] == '.' &&
              strcmp(&name[len - extlen + 1], kTrainedDataSuffix) == 0) {
            name[len - extlen] = '\0';
            langs->push_back(base2 + name);
          }
        }
      }
    }
    closedir(dir);
  }
#endif
}


TessBaseAPI::TessBaseAPI()
    : tesseract_(nullptr)
#if !DISABLED_LEGACY_ENGINE
      ,
      osd_tesseract_(nullptr),
      equ_detect_(nullptr)
#endif
      ,
      reader_(nullptr),
      // thresholder_ is initialized to nullptr here, but will be set before use
      // by: A constructor of a derived API or created
      // implicitly when used in InternalResetImage.
      thresholder_(nullptr),
      paragraph_models_(nullptr),
      block_list_(nullptr),
      page_res_(nullptr),
      pix_visible_image_(nullptr),
      last_oem_requested_(OEM_DEFAULT),
      recognition_done_(false),
      rect_left_(0),
      rect_top_(0),
      rect_width_(0),
      rect_height_(0),
      image_width_(0),
      image_height_(0) {
  // make sure the debug_all preset is set up BEFORE any command-line arguments
  // direct tesseract to set some arbitrary parameters just below,
  // for otherwise those `-c xyz=v` commands may be overruled by the
  // debug_all preset!
  debug_all.set_on_modify_handler([this](decltype(debug_all) &target,
                                         const int32_t old_value,
                                         int32_t &new_value,
                                         const int32_t default_value,
                                         ParamSetBySourceType source_type,
                                         ParamPtr optional_setter) {
    this->SetupDebugAllPreset();
  });
}

TessBaseAPI::~TessBaseAPI() {
  End();
  debug_all.set_on_modify_handler(0);
}

/**
 * Returns the version identifier as a static string. Do not delete.
 */
const char *TessBaseAPI::Version() {
  return TESSERACT_VERSION_STR;
}

Tesseract& TessBaseAPI::tesseract() const {
  if (tesseract_ == nullptr) {
    tesseract_ = new Tesseract();
  }
  return *tesseract_;
}

/**
 * Set the name of the input file. Needed only for training and
 * loading a UNLV zone file.
 */
void TessBaseAPI::SetInputName(const char *name) {
  Tesseract &tess = tesseract();
  tess.input_file_path_ = name;
}

/** Set the name of the visible image files. Needed only for PDF output. */
void TessBaseAPI::SetVisibleImageFilename(const char* name) {
  Tesseract &tess = tesseract();
  tess.visible_image_file_path_ = name;
}

/**
* Return a memory capacity cost estimate for the given image dimensions and
* some heuristics re tesseract behaviour, e.g. input images will be normalized/greyscaled,
* then thresholded, all of which will be kept in memory while the session runs.
*
* Also uses the Tesseract Variable `allowed_image_memory_capacity` to indicate
* whether the estimated cost is oversized --> `cost.is_too_large()`
*
* For user convenience, static functions are provided:
* the static functions MAY be used by userland code *before* the high cost of
* instantiating a Tesseract instance is incurred.
*/
ImageCostEstimate TessBaseAPI::EstimateImageMemoryCost(int image_width, int image_height, float allowance) {
  // The heuristics used:
  // 
  // we reckon with leptonica Pix storage at 4 bytes per pixel,
  // tesseract storing (worst case) 3 different images: original, greyscale, binary thresholded,
  // we DO NOT reckon with the extra image that may serve as background for PDF outputs, etc.
  // we DO NOT reckon with the memory cost for the OCR match tree, etc.
  // However, we attempt a VERY ROUGH estimate by calculating a 20% overdraft for internal operations'
  // storage costs.
  float cost = 4 * 3 * 1.20f;
  cost *= image_width;
  cost *= image_height;

  if (allowed_image_memory_capacity > 0.0) {
    // any rediculous input values will be replaced by the Tesseract configuration value:
    if (allowance > allowed_image_memory_capacity || allowance <= 0.0)
      allowance = allowed_image_memory_capacity;
  }

  return ImageCostEstimate(cost, allowance);
}

ImageCostEstimate TessBaseAPI::EstimateImageMemoryCost(const Pix* pix, float allowance) {
  auto w = pixGetWidth(pix);
  auto h = pixGetHeight(pix);
  return EstimateImageMemoryCost(w, h, allowance);
}

/**
* Ditto, but this API may be invoked after SetInputImage() or equivalent has been called
* and reports the cost estimate for the current instance/image.
*/
ImageCostEstimate TessBaseAPI::EstimateImageMemoryCost() const {
  return tesseract().EstimateImageMemoryCost();
}

/**
* Helper, which may be invoked after SetInputImage() or equivalent has been called:
* reports the cost estimate for the current instance/image via `tprintDebug()` and returns
* `true` when the cost is expected to be too high.
*
* You can use this as a fast pre-flight check. Many major tesseract APIs perform
* this same check as part of their startup routine.
*/
bool TessBaseAPI::CheckAndReportIfImageTooLarge(const Pix* pix) const {
  return tesseract().CheckAndReportIfImageTooLarge(pix);
}

/** Set the name of the output files. Needed only for debugging. */
void TessBaseAPI::SetOutputName(const char *name) {
  tesseract().output_base_filename.set_value(name);
}

const std::string &TessBaseAPI::GetOutputName() {
  return tesseract().output_base_filename;
}

bool TessBaseAPI::SetVariable(const char *name, const char *value) {
  return ParamUtils::SetParam(name, value, tesseract().params_collective());
}

bool TessBaseAPI::SetVariable(const char *name, int value) {
  return ParamUtils::SetParam(name, value, tesseract().params_collective());
}

bool TessBaseAPI::GetIntVariable(const char *name, int *value) const {
  IntParam *p = ParamUtils::FindParam<IntParam>(name, tesseract().params_collective());
  if (!p) {
    return false;
  }
  *value = p->value();
  return true;
}

bool TessBaseAPI::GetBoolVariable(const char *name, bool *value) const {
  BoolParam *p = ParamUtils::FindParam<BoolParam>(name, tesseract().params_collective());
  if (!p) {
    return false;
  }
  *value = p->value();
  return true;
}

const char *TessBaseAPI::GetStringVariable(const char *name) const {
  StringParam *p = ParamUtils::FindParam<StringParam>(name, tesseract().params_collective());
  if (!p) {
    return nullptr;
  }
  return p->c_str();
}

bool TessBaseAPI::GetDoubleVariable(const char *name, double *value) const {
  DoubleParam *p = ParamUtils::FindParam<DoubleParam>(name, tesseract().params_collective());
  if (!p) {
    return false;
  }
  *value = p->value();
  return true;
}

/** Get value of named variable as a string, if it exists. */
bool TessBaseAPI::GetVariableAsString(const char *name, std::string *val) const {
  Param *p = ParamUtils::FindParam(name, tesseract().params_collective());
  if (p != nullptr) {
    if (val != nullptr) {
      *val = p->raw_value_str();
    }
    return true;
  }
  return false;
}

#if !DISABLED_LEGACY_ENGINE

/** Print Tesseract fonts table to the given file. */
void TessBaseAPI::PrintFontsTable(FILE *fp) const {
  if (!fp)
    fp = stdout;
  bool print_info = (fp == stdout || fp == stderr);
  Tesseract& tess = tesseract();
  const int fontinfo_size = tess.get_fontinfo_table().size();
  for (int font_index = 1; font_index < fontinfo_size; ++font_index) {
    FontInfo font = tess.get_fontinfo_table().at(font_index);
    if (print_info) {
      tprintInfo(
          "ID={}: {} is_italic={} is_bold={} is_fixed_pitch={} is_serif={} is_fraktur={}\n",
          font_index, font.name,
          font.is_italic(),
          font.is_bold(),
          font.is_fixed_pitch(),
          font.is_serif(),
          font.is_fraktur());
    } else {
      std::string msg = fmt::format(
          "ID={}: {} is_italic={} is_bold={} is_fixed_pitch={} is_serif={} is_fraktur={}\n",
          font_index, font.name,
          font.is_italic(),
          font.is_bold(),
          font.is_fixed_pitch(),
          font.is_serif(),
          font.is_fraktur());
      fputs(msg.c_str(), fp);
    }
  }
}

#endif

/** 
 * Print Tesseract parameters to the given file with descriptions of each option. 
 * Cannot be used as Tesseract configuration file due to descriptions 
 * (use DumpVariables instead to create config files).
 */
void TessBaseAPI::PrintVariables(FILE *fp) const {
  ParamUtils::PrintParams(fp, tesseract().params_collective(), true);
}

void TessBaseAPI::SaveParameters() {
  // Save current config variables before switching modes.
  FILE *fp = fopen(kOldVarsFile, "wb");
  PrintVariables(fp);
  fclose(fp);
}

void TessBaseAPI::RestoreParameters() {
  ReadConfigFile(kOldVarsFile);
}

/** 
 * Print Tesseract parameters to the given file without descriptions. 
 * Can be used as Tesseract configuration file.
*/
void TessBaseAPI::DumpVariables(FILE *fp) const {
  ParamUtils::PrintParams(fp, tesseract().params_collective(), false);
}

// Report parameters' usage statistics, i.e. report which params have been
// set, modified and read/checked until now during this run-time's lifetime.
//
// Use this method for run-time 'discovery' about which tesseract parameters
// are actually *used* during your particular usage of the library, ergo
// answering the question:
// "Which of all those parameters are actually *relevant* to my use case today?"
void TessBaseAPI::ReportParamsUsageStatistics() const {
  tesseract::ParamsVectorSet &vec = tesseract().params_collective();
  std::string fpath = tesseract::vars_report_file;
  ReportFile f(fpath);
  ParamUtils::ReportParamsUsageStatistics(f(), vec, nullptr);
}

/**
 * The datapath must be the name of the data directory or
 * some other file in which the data directory resides (for instance argv[0].)
 * The language is (usually) an ISO 639-3 string or nullptr will default to eng.
 * If numeric_mode is true, then only digits and Roman numerals will
 * be returned.
 * 
 * @return: 0 on success and -1 on initialization failure.
 */
int TessBaseAPI::Init(const char* datapath,
                      ParamsVectorSet& vars)
{
  std::vector<std::string> nil;
  //ParamsVectorSet vars;
  FileReader nada;
  Tesseract &tess = tesseract();
  //tess.tessedit_ocr_engine_mode = oem;
  //tess.languages_to_try = language;
  if (tess.datadir_base_path.is_set() && !strempty(datapath) && tess.datadir_base_path.value() != datapath) {
    // direct parameter overrides previous parameter set-up
    tess.datadir_base_path = datapath;
  }
  return Init_Internal(datapath, vars, nil, nada, nullptr, 0);
}

int TessBaseAPI::Init(const char *datapath,
         ParamsVectorSet &vars,
                      const std::vector<std::string> &configs)
{

}

int TessBaseAPI::Init(ParamsVectorSet& vars)
{

}

int TessBaseAPI::Init(ParamsVectorSet& vars,
                      const std::vector<std::string>& configs)
{

}

int TessBaseAPI::Init(const char* datapath,
         ParamsVectorSet& vars,
                      FileReader reader)
{

}

int TessBaseAPI::Init(const char* datapath,
         ParamsVectorSet& vars,
         const std::vector<std::string>& configs,
                      FileReader reader)
{

}

int TessBaseAPI::Init(const char* datapath,
         const std::vector<std::string>& vars_vec,
                      const std::vector<std::string>& vars_values)
{

}

int TessBaseAPI::Init(const char* datapath,
         const std::vector<std::string>& vars_vec,
         const std::vector<std::string>& vars_values,
                      const std::vector<std::string>& configs)
{

}

int TessBaseAPI::Init(const char* datapath, const char* language, OcrEngineMode oem)
{

}

int TessBaseAPI::Init(const char* datapath, const char* language, OcrEngineMode oem,
                      const std::vector<std::string>& configs)
{

}

int TessBaseAPI::Init(const char* datapath, const char* language)
{

}

int TessBaseAPI::Init(const char* datapath, const char* language,
                      const std::vector<std::string>& configs)
{

}

int TessBaseAPI::Init(const char *language, OcrEngineMode oem)
{

}

int TessBaseAPI::Init(const char *language, OcrEngineMode oem,
         const std::vector<std::string> &configs)
{

}

int TessBaseAPI::Init(const char* language)
{

}

int TessBaseAPI::Init(const char* language,
                      const std::vector<std::string>& configs)
{

}

// Reads the traineddata via a FileReader from path `datapath`.
int TessBaseAPI::Init(const char* datapath,
         const std::vector<std::string>& vars_vec,
         const std::vector<std::string>& vars_values,
                      FileReader reader)
{

}

int TessBaseAPI::Init(const char* datapath,
         const std::vector<std::string>& vars_vec,
         const std::vector<std::string>& vars_values,
         const std::vector<std::string>& configs,
                      FileReader reader)
{

}

// In-memory version reads the traineddata directly from the given
// data[data_size] array.
int TessBaseAPI::InitFromMemory(const char *data, size_t data_size,
                   const std::vector<std::string>& vars_vec,
                   const std::vector<std::string>& vars_values)
{

}

int TessBaseAPI::InitFromMemory(const char *data, size_t data_size,
                   const std::vector<std::string>& vars_vec,
                   const std::vector<std::string>& vars_values,
                   const std::vector<std::string>& configs)
{

}

int TessBaseAPI::Init_Internal(const char *path,
                               ParamsVectorSet &vars,
                               const std::vector<std::string> &configs,
                               FileReader reader,
                               const char *data, size_t data_size) {
  Tesseract &tess = tesseract();
#if 0
  if (tess.languages_to_try.empty()) {
    tess.languages_to_try = "";
  }
#endif
  if (data == nullptr) {
    data = "";
    data_size = 0; // as a precaution to prevent invalid user-set value to
  }
  std::string datapath;
  if (!strempty(path)) {
    datapath = path;
  } else if (!tess.datadir_base_path.empty()) {
    datapath = tess.datadir_base_path;
  } else {
    datapath = tess.languages_to_try;
  }

  // TODO: re-evaluate this next (old) code chunk which decides when to reset the tesseract instance.

  std::string buggered_languge = "XYZ";

  // If the datapath, OcrEngineMode or the language have changed - start again.
  // Note that the language_ field stores the last requested language that was
  // initialized successfully, while tesseract().lang stores the language
  // actually used. They differ only if the requested language was nullptr, in
  // which case tesseract().lang is set to the Tesseract default ("eng").
  if (
      (datapath_.empty() || language_.empty() || datapath_ != datapath ||
       last_oem_requested_ != oem() || (language_ != buggered_languge && tesseract_->lang_ != buggered_languge))) {
    // TODO: code a proper RESET operation instead of ditching and re-instatiating, which will nuke our `tess` reference.
    assert(0);
    delete tesseract_;
    tesseract_ = nullptr;
  }
  bool reset_classifier = true;
  if (tesseract_ == nullptr) {
    reset_classifier = false;
    tesseract_ = new Tesseract();
    if (reader != nullptr) {
      reader_ = reader;
    }
    TessdataManager mgr(reader_);
    if (data_size != 0) {
      mgr.LoadMemBuffer(buggered_languge.c_str(), data, data_size);
    }
    if (tess.init_tesseract(datapath, output_file_, vars, &mgr) != 0) {
      return -1;
    }
  }

  // Update datapath and language requested for the last valid initialization.
  datapath_ = std::move(datapath);
  if (datapath_.empty() && !tess.datadir_.empty()) {
    datapath_ = tess.datadir_;
  }

  language_ = buggered_languge;
  last_oem_requested_ = oem();

#if !DISABLED_LEGACY_ENGINE
  // For same language and datapath, just reset the adaptive classifier.
  if (reset_classifier) {
    tess.ResetAdaptiveClassifier();
  }
#endif // !DISABLED_LEGACY_ENGINE

  return 0;
}

/**
 * Returns the languages string used in the last valid initialization.
 * If the last initialization specified "deu+hin" then that will be
 * returned. If hin loaded eng automatically as well, then that will
 * not be included in this list. To find the languages actually
 * loaded use GetLoadedLanguagesAsVector.
 * The returned string should NOT be deleted.
 */
const char *TessBaseAPI::GetInitLanguagesAsString() const {
  return language_.c_str();
}

/**
 * Returns the loaded languages in the vector of std::string.
 * Includes all languages loaded by the last Init, including those loaded
 * as dependencies of other loaded languages.
 */
void TessBaseAPI::GetLoadedLanguagesAsVector(std::vector<std::string> *langs) const {
  langs->clear();
  Tesseract &tess = tesseract();
  langs->push_back(tess.lang_);
  int num_subs = tess.num_sub_langs();
  for (int i = 0; i < num_subs; ++i) {
    langs->push_back(tess.get_sub_lang(i)->lang_);
  }
}

/**
 * Returns the available languages in the sorted vector of std::string.
 */
void TessBaseAPI::GetAvailableLanguagesAsVector(std::vector<std::string> *langs) const {
  langs->clear();
  Tesseract &tess = tesseract();
  addAvailableLanguages(tess.datadir_, "", langs);
  std::sort(langs->begin(), langs->end());
}

/**
 * Init only for page layout analysis. Use only for calls to SetImage and
 * AnalysePage. Calls that attempt recognition will generate an error.
 */
void TessBaseAPI::InitForAnalysePage() {
  tesseract().InitAdaptiveClassifier(nullptr);
}

/**
 * Read a "config" file containing a set of parameter name, value pairs.
 * Searches the standard places: tessdata/configs, tessdata/tessconfigs
 * and also accepts a relative or absolute path name.
 */
void TessBaseAPI::ReadConfigFile(const char *filename) {
  tesseract().read_config_file(filename);
}

/**
 * Set the current page segmentation mode. Defaults to PSM_AUTO.
 * The mode is stored as an IntParam so it can also be modified by
 * ReadConfigFile or SetVariable("tessedit_pageseg_mode", mode as string).
 */
void TessBaseAPI::SetPageSegMode(PageSegMode mode) {
  tesseract().tessedit_pageseg_mode.set_value(mode);
}

/** Return the current page segmentation mode. */
PageSegMode TessBaseAPI::GetPageSegMode() const {
  return static_cast<PageSegMode>(tesseract().tessedit_pageseg_mode.value());
}

/**
 * Recognize a rectangle from an image and return the result as a string.
 * May be called many times for a single Init.
 * Currently has no error checking.
 * Greyscale of 8 and color of 24 or 32 bits per pixel may be given.
 * Palette color images will not work properly and must be converted to
 * 24 bit.
 * Binary images of 1 bit per pixel may also be given but they must be
 * byte packed with the MSB of the first byte being the first pixel, and a
 * one pixel is WHITE. For binary images set bytes_per_pixel=0.
 * The recognized text is returned as a char* which is coded
 * as UTF8 and must be freed with the delete [] operator.
 */
char *TessBaseAPI::TesseractRect(const unsigned char *imagedata, int bytes_per_pixel,
                                 int bytes_per_line, int left, int top, int width, int height) {
  if (tesseract_ == nullptr || width < kMinRectSize || height < kMinRectSize) {
    return nullptr; // Nothing worth doing.
  }

  // Since this original api didn't give the exact size of the image,
  // we have to invent a reasonable value.
  int bits_per_pixel = bytes_per_pixel == 0 ? 1 : bytes_per_pixel * 8;
  SetImage(imagedata, bytes_per_line * 8 / bits_per_pixel, height + top, bytes_per_pixel,
           bytes_per_line);
  SetRectangle(left, top, width, height);

  return GetUTF8Text();
}

#if !DISABLED_LEGACY_ENGINE
/**
 * Call between pages or documents etc to free up memory and forget
 * adaptive data.
 */
void TessBaseAPI::ClearAdaptiveClassifier() {
  Tesseract& tess = tesseract();
  tess.ResetAdaptiveClassifier();
  tess.ResetDocumentDictionary();
}
#endif // !DISABLED_LEGACY_ENGINE

/**
 * Provide an image for Tesseract to recognize. Format is as
 * TesseractRect above. Copies the image buffer and converts to Pix.
 * SetImage clears all recognition results, and sets the rectangle to the
 * full image, so it may be followed immediately by a GetUTF8Text, and it
 * will automatically perform recognition.
 */
void TessBaseAPI::SetImage(const unsigned char *imagedata, int width, int height,
                           int bytes_per_pixel, int bytes_per_line, int exif, 
                           const float angle, bool upscale) {
  if (InternalResetImage()) {
    thresholder_->SetImage(imagedata, width, height, bytes_per_pixel, bytes_per_line, exif, angle, upscale);
    SetInputImage(thresholder_->GetPixRect());
  }
}

void TessBaseAPI::SetSourceResolution(int ppi) {
  if (thresholder_) {
    thresholder_->SetSourceYResolution(ppi);
  } else {
    tprintError("Please call SetImage before SetSourceResolution.\n");
  }
}

/**
 * Provide an image for Tesseract to recognize. As with SetImage above,
 * Tesseract takes its own copy of the image, so it need not persist until
 * after Recognize.
 * Pix vs raw, which to use?
 * Use Pix where possible. Tesseract uses Pix as its internal representation
 * and it is therefore more efficient to provide a Pix directly.
 */
void TessBaseAPI::SetImage(Pix *pix, int exif, const float angle, bool upscale) {
  if (InternalResetImage()) {
    if (pixGetSpp(pix) == 4) {
      // remove alpha channel from image; the background color is assumed to be PURE WHITE.
      Pix *p1 = pixRemoveAlpha(pix);
      pixSetSpp(p1, 3);
      (void)pixCopy(pix, p1);
      pixDestroy(&p1);
    }
    thresholder_->SetImage(pix, exif, angle, upscale);
    SetInputImage(thresholder_->GetPixRect());
  }
}

int TessBaseAPI::SetImageFile(int exif, const float angle, bool upscale) {
    const char *filename1 = "/input";
    Pix *pix = pixRead(filename1);
    if (pix == nullptr) {
      tprintError("Image file {} cannot be read!\n", filename1);
      return 1;
    }
    if (pixGetSpp(pix) == 4 && pixGetInputFormat(pix) == IFF_PNG) {
      // remove alpha channel from png
      Pix *p1 = pixRemoveAlpha(pix);
      pixSetSpp(p1, 3);
      (void)pixCopy(pix, p1);
      pixDestroy(&p1);
    }
    thresholder_->SetImage(pix, exif, angle, upscale);
    SetInputImage(thresholder_->GetPixRect());
    pixDestroy(&pix);
    return 0;
}

/**
 * Restrict recognition to a sub-rectangle of the image. Call after SetImage.
 * Each SetRectangle clears the recognition results so multiple rectangles
 * can be recognized with the same image.
 */
void TessBaseAPI::SetRectangle(int left, int top, int width, int height) {
  if (thresholder_ == nullptr) {
    return;
  }
  // TODO: this ClearResults prematurely nukes the page image and pushes for the diagnostics log to be written to output file,
  // while this SetRectangle() very well may be meant to OCR a *second* rectangle in the existing page image, which will fail
  // today as the page image will be lost, thanks to ClearResults.
  //
  // Hm, maybe have two Clear methods: ClearPageResults + ClearPageSource, so we can differentiate? And only push the diagnostics log
  // as late as possible, i.e. when the SourceImage is being discarded then in ClearPageSource().
  ClearResults();
  thresholder_->SetRectangle(left, top, width, height);
}

/**
 * ONLY available after SetImage if you have Leptonica installed.
 * Get a copy of the internal thresholded image from Tesseract.
 */
Pix *TessBaseAPI::GetThresholdedImage() {
  if (tesseract_ == nullptr || thresholder_ == nullptr) {
    return nullptr;
  }

  Tesseract& tess = tesseract();
  if (tess.pix_binary() == nullptr) {
  if (verbose_process) {
      tprintInfo("PROCESS: source image is not a binary image, hence we apply a thresholding algo/subprocess to obtain a binarized image.\n");
  }

    Image pix = Image();
    if (!Threshold(&pix.pix_)) {
      return nullptr;
    }
    tess.set_pix_binary(pix);

    if (tess.tessedit_dump_pageseg_images) {
      tess.AddPixDebugPage(tess.pix_binary(), "Thresholded Image (because it wasn't thresholded yet)");
    }
  }

  const char *debug_output_path = tess.debug_output_path.c_str();

  //Pix *p1 = pixRotate(tess.pix_binary(), 0.15, L_ROTATE_SHEAR, L_BRING_IN_WHITE, 0, 0);
  // if (scribe_save_binary_rotated_image) {
  //   Pix *p1 = tess.pix_binary();
  //   pixWrite("/binary_image.png", p1, IFF_PNG);
  // }
  if (tess.scribe_save_grey_rotated_image) {
    Pix *p1 = tess.pix_grey();
    tess.AddPixDebugPage(p1, "greyscale image");
  }
  if (tess.scribe_save_binary_rotated_image) {
    Pix *p1 = tess.pix_binary();
    tess.AddPixDebugPage(p1, "binary (black & white) image");
  }
  if (tess.scribe_save_original_rotated_image) {
    Pix *p1 = tess.pix_original();
    tess.AddPixDebugPage(p1, "original image");
  }

  return tess.pix_binary().clone();
}

/**
 * Function added by Tesseract.js.
 * Saves a .png image of the type specified by `type` to "/image.png"
 * ONLY available after SetImage if you have Leptonica installed.
 */
void TessBaseAPI::WriteImage(const int type) {
  if (tesseract_ == nullptr || thresholder_ == nullptr) {
    return;
  }

  Tesseract& tess = tesseract();
  if (type == 0) {
    if (tess.pix_original() == nullptr) {
      return;
    }
    Pix *p1 = tess.pix_original();
    pixWrite("/image.png", p1, IFF_PNG);

  } else if (type == 1) {
    if (tess.pix_grey() == nullptr && !Threshold(static_cast<Pix **>(tess.pix_binary()))) {
      return;
    }
    // When the user uploads a black and white image, there will be no pix_grey.
    // Therefore, we return pix_binary instead in this case. 
    if (tess.pix_grey() == nullptr) {
      Pix *p1 = tess.pix_binary();
      pixWrite("/image.png", p1, IFF_PNG);
    } else {
      Pix *p1 = tess.pix_grey();
      pixWrite("/image.png", p1, IFF_PNG);
    }
  } else if (type == 2) {
    if (tess.pix_binary() == nullptr && !Threshold(static_cast<Pix**>(tess.pix_binary()))) {
      return;
    }
    Pix *p1 = tess.pix_binary();
    pixWrite("/image.png", p1, IFF_PNG);
  }

  return;
}

/**
 * Get the result of page layout analysis as a leptonica-style
 * Boxa, Pixa pair, in reading order.
 * Can be called before or after Recognize.
 */
Boxa *TessBaseAPI::GetRegions(Pixa **pixa) {
  return GetComponentImages(RIL_BLOCK, false, pixa, nullptr);
}

/**
 * Get the textlines as a leptonica-style Boxa, Pixa pair, in reading order.
 * Can be called before or after Recognize.
 * If blockids is not nullptr, the block-id of each line is also returned as an
 * array of one element per line. delete [] after use.
 * If paraids is not nullptr, the paragraph-id of each line within its block is
 * also returned as an array of one element per line. delete [] after use.
 */
Boxa *TessBaseAPI::GetTextlines(const bool raw_image, const int raw_padding, Pixa **pixa,
                                int **blockids, int **paraids) {
  return GetComponentImages(RIL_TEXTLINE, true, raw_image, raw_padding, pixa, blockids, paraids);
}

/**
 * Get textlines and strips of image regions as a leptonica-style Boxa, Pixa
 * pair, in reading order. Enables downstream handling of non-rectangular
 * regions.
 * Can be called before or after Recognize.
 * If blockids is not nullptr, the block-id of each line is also returned as an
 * array of one element per line. delete [] after use.
 */
Boxa *TessBaseAPI::GetStrips(Pixa **pixa, int **blockids) {
  return GetComponentImages(RIL_TEXTLINE, false, pixa, blockids);
}

/**
 * Get the words as a leptonica-style
 * Boxa, Pixa pair, in reading order.
 * Can be called before or after Recognize.
 */
Boxa *TessBaseAPI::GetWords(Pixa **pixa) {
  return GetComponentImages(RIL_WORD, true, pixa, nullptr);
}

/**
 * Gets the individual connected (text) components (created
 * after pages segmentation step, but before recognition)
 * as a leptonica-style Boxa, Pixa pair, in reading order.
 * Can be called before or after Recognize.
 */
Boxa *TessBaseAPI::GetConnectedComponents(Pixa **pixa) {
  return GetComponentImages(RIL_SYMBOL, true, pixa, nullptr);
}

/**
 * Get the given level kind of components (block, textline, word etc.) as a
 * leptonica-style Boxa, Pixa pair, in reading order.
 * Can be called before or after Recognize.
 * If blockids is not nullptr, the block-id of each component is also returned
 * as an array of one element per component. delete [] after use.
 * If text_only is true, then only text components are returned.
 */
Boxa *TessBaseAPI::GetComponentImages(PageIteratorLevel level, bool text_only, bool raw_image,
                                      const int raw_padding, Pixa **pixa, int **blockids,
                                      int **paraids) {
  /*non-const*/ std::unique_ptr</*non-const*/ PageIterator> page_it(GetIterator());
  if (page_it == nullptr) {
    page_it.reset(AnalyseLayout());
  }
  if (page_it == nullptr) {
    return nullptr; // Failed.
  }

  // Count the components to get a size for the arrays.
  int component_count = 0;
  int left, top, right, bottom;

  if (raw_image) {
    // Get bounding box in original raw image with padding.
    do {
      if (page_it->BoundingBox(level, raw_padding, &left, &top, &right, &bottom) &&
          (!text_only || PTIsTextType(page_it->BlockType()))) {
        ++component_count;
      }
    } while (page_it->Next(level));
  } else {
    // Get bounding box from binarized imaged. Note that this could be
    // differently scaled from the original image.
    do {
      if (page_it->BoundingBoxInternal(level, &left, &top, &right, &bottom) &&
          (!text_only || PTIsTextType(page_it->BlockType()))) {
        ++component_count;
      }
    } while (page_it->Next(level));
  }

  Boxa *boxa = boxaCreate(component_count);
  if (pixa != nullptr) {
    *pixa = pixaCreate(component_count);
  }
  if (blockids != nullptr) {
    *blockids = new int[component_count];
  }
  if (paraids != nullptr) {
    *paraids = new int[component_count];
  }

  int blockid = 0;
  int paraid = 0;
  int component_index = 0;
  page_it->Begin();
  do {
    bool got_bounding_box;
    if (raw_image) {
      got_bounding_box = page_it->BoundingBox(level, raw_padding, &left, &top, &right, &bottom);
    } else {
      got_bounding_box = page_it->BoundingBoxInternal(level, &left, &top, &right, &bottom);
    }
    if (got_bounding_box && (!text_only || PTIsTextType(page_it->BlockType()))) {
      Box *lbox = boxCreate(left, top, right - left, bottom - top);
      boxaAddBox(boxa, lbox, L_INSERT);
      if (pixa != nullptr) {
        Pix *pix = nullptr;
        if (raw_image) {
          pix = page_it->GetImage(level, raw_padding, GetInputImage(), &left, &top);
        } else {
          pix = page_it->GetBinaryImage(level);
        }
        pixaAddPix(*pixa, pix, L_INSERT);
        pixaAddBox(*pixa, lbox, L_CLONE);
      }
      if (paraids != nullptr) {
        (*paraids)[component_index] = paraid;
        if (page_it->IsAtFinalElement(RIL_PARA, level)) {
          ++paraid;
        }
      }
      if (blockids != nullptr) {
        (*blockids)[component_index] = blockid;
        if (page_it->IsAtFinalElement(RIL_BLOCK, level)) {
          ++blockid;
          paraid = 0;
        }
      }
      ++component_index;
    }
  } while (page_it->Next(level));
  return boxa;
}

/**
 * Stores lstmf based on in-memory data for one line with pix and text
 * This function is (atm) not used in the current processing,
 * but can be used via CAPI e.g. tesserocr
 */
bool TessBaseAPI::WriteLSTMFLineData(const char *name, const char *path,
                                     Pix *pix, const char *truth_text,
                                     bool vertical) {
  // Check if path exists
  std::ifstream test(path);
  if (!test) {
    tprintError("The path {} doesn't exist.\n", path);
    return false;
  }
  // Check if truth_text exists
  if ((truth_text != NULL) && (truth_text[0] == '\0') ||
      (truth_text[0] == '\n')) {
    tprintError("Ground truth text is empty or starts with newline.\n");
    return false;
  }
  // Check if pix exists
  if (!pix) {
    tprintError("No image provided.\n");
    return false;
  }
  // Variables for ImageData for just one line
  std::vector<TBOX> boxes;
  std::vector<std::string> line_texts;
  std::string current_char, last_char, textline_str;
  unsigned text_index = 0;
  std::string truth_text_str = std::string(truth_text);
  TBOX bounding_box = TBOX(0, 0, pixGetWidth(pix), pixGetHeight(pix));
  // Take only the first line from the truth_text, replace tabs with whitespaces
  // and reduce multiple whitespaces to just one
  while (text_index < truth_text_str.size() &&
         truth_text_str[text_index] != '\n') {
    current_char = truth_text_str[text_index];
    if (current_char == "\t") {
      current_char = " ";
    }
    if (last_char != " " || current_char != " ") {
      textline_str.append(current_char);
      last_char = current_char;
    }
    text_index++;
  }
  if (textline_str.empty() || textline_str != " ") {
    tprintError("There is no first line information.\n");
    return false;
  } else {
    boxes.push_back(bounding_box);
    line_texts.push_back(textline_str);
  }

  std::vector<int> page_numbers(boxes.size(), 1);

  // Init ImageData
  auto *image_data = new ImageData(vertical, pix);
  image_data->set_page_number(1);
  image_data->AddBoxes(boxes, line_texts, page_numbers);

  // Write it to a lstmf-file
  std::filesystem::path filename = path;
  filename /= std::string(name) + std::string(".lstmf");
  DocumentData doc_data(filename.string());
  doc_data.AddPageToDocument(image_data);
  if (!doc_data.SaveDocument(filename.string().c_str(), nullptr)) {
    tprintError("Failed to write training data to {}!\n", filename.string());
    return false;
  }
  return true;
}

int TessBaseAPI::GetThresholdedImageScaleFactor() const {
  if (thresholder_ == nullptr) {
    return 0;
  }
  return thresholder_->GetScaleFactor();
}

/**
 * Runs page layout analysis in the mode set by SetPageSegMode.
 * May optionally be called prior to Recognize to get access to just
 * the page layout results. Returns an iterator to the results.
 * If merge_similar_words is true, words are combined where suitable for use
 * with a line recognizer. Use if you want to use AnalyseLayout to find the
 * textlines, and then want to process textline fragments with an external
 * line recognizer.
 * Returns nullptr on error or an empty page.
 * The returned iterator must be deleted after use.
 * WARNING! This class points to data held within the TessBaseAPI class, and
 * therefore can only be used while the TessBaseAPI class still exists and
 * has not been subjected to a call of Init, SetImage, Recognize, Clear, End
 * DetectOS, or anything else that changes the internal PAGE_RES.
 */
PageIterator *TessBaseAPI::AnalyseLayout(bool merge_similar_words) {
  if (FindLines() == 0) {
    Tesseract& tess = tesseract();
    AutoPopDebugSectionLevel section_handle(&tess, tess.PushSubordinatePixDebugSection("Analyse Layout"));

    if (block_list_->empty()) {
      return nullptr; // The page was empty.
    }
    page_res_ = new PAGE_RES(merge_similar_words, block_list_, nullptr);
    DetectParagraphs(false);
    return new PageIterator(page_res_, &tess, thresholder_->GetScaleFactor(),
                            thresholder_->GetScaledYResolution(), rect_left_, rect_top_,
                            rect_width_, rect_height_);
  }
  return nullptr;
}

/**
 * Recognize the tesseract global image and return the result as Tesseract
 * internal structures.
 */
int TessBaseAPI::Recognize(ETEXT_DESC *monitor) {
  if (tesseract_ == nullptr) {
    return -1;
  }

  Tesseract& tess = tesseract();

  if (FindLines() != 0) {
    return -1;
  }

  AutoPopDebugSectionLevel section_handle(&tess, tess.PushSubordinatePixDebugSection("Recognize (OCR)"));

  delete page_res_;
  if (block_list_->empty()) {
    page_res_ = new PAGE_RES(false, block_list_, &tess.prev_word_best_choice_);
    return 0; // Empty page.
  }

  tess.SetBlackAndWhitelist();
  recognition_done_ = true;
#if !DISABLED_LEGACY_ENGINE
  if (tess.tessedit_resegment_from_line_boxes) {
    if (verbose_process)
      tprintInfo("PROCESS: Re-segment from line boxes.\n");
    page_res_ = tess.ApplyBoxes(tess.input_file_path_.c_str(), true, block_list_);
  } else if (tess.tessedit_resegment_from_boxes) {
    if (verbose_process)
      tprintInfo("PROCESS: Re-segment from page boxes.\n");
    page_res_ = tess.ApplyBoxes(tess.input_file_path_.c_str(), false, block_list_);
  } else
#endif // !DISABLED_LEGACY_ENGINE
  {
    if (verbose_process)
      tprintInfo("PROCESS: Re-segment from LSTM / previous word best choice.\n");
    page_res_ = new PAGE_RES(tess.AnyLSTMLang(), block_list_, &tess.prev_word_best_choice_);
  }

  if (page_res_ == nullptr) {
    return -1;
  }

  if (tess.tessedit_train_line_recognizer) {
    AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("Train Line Recognizer: Correct Classify Words"));
    if (!tess.TrainLineRecognizer(tess.input_file_path_.c_str(), output_file_, block_list_)) {
      return -1;
    }
    tess.CorrectClassifyWords(page_res_);
    return 0;
  }
#if !DISABLED_LEGACY_ENGINE
  if (tess.tessedit_make_boxes_from_boxes) {
    AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("Make Boxes From Boxes: Correct Classify Words"));
    tess.CorrectClassifyWords(page_res_);
    return 0;
  }
#endif // !DISABLED_LEGACY_ENGINE

  int result = 0;
  if (tess.SupportsInteractiveScrollView()) {
#if !GRAPHICS_DISABLED
    AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("PGEditor: Interactive Session"));
    tess.pgeditor_main(rect_width_, rect_height_, page_res_);

    // The page_res is invalid after an interactive session, so cleanup
    // in a way that lets us continue to the next page without crashing.
    delete page_res_;
    page_res_ = nullptr;
    return -1;
#else
    ASSERT0(!"Should never get here!");
#endif
#if !DISABLED_LEGACY_ENGINE
  } else if (tess.tessedit_train_from_boxes) {
    AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("Train From Boxes"));
    std::string fontname;
    ExtractFontName(output_file_.c_str(), &fontname);
    tess.ApplyBoxTraining(fontname, page_res_);
  } else if (tess.tessedit_ambigs_training) {
    AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("Train Ambigs"));
    FILE *training_output_file = tess.init_recog_training(tess.input_file_path_.c_str());
    // OCR the page segmented into words by tesseract.
    tess.recog_training_segmented(tess.input_file_path_.c_str(), page_res_, monitor,
                                         training_output_file);
    fclose(training_output_file);
#endif // !DISABLED_LEGACY_ENGINE
  } else {
    AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("The Main Recognition Phase"));

#if !GRAPHICS_DISABLED
    if (scrollview_support) {
      tess.pgeditor_main(rect_width_, rect_height_, page_res_);
    }
#endif

    // Now run the main recognition.
    if (!tess.paragraph_text_based) {
      AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("Detect Paragraphs (Before Recognition)"));
      DetectParagraphs(false);
#if !GRAPHICS_DISABLED
      if (scrollview_support) {
        tess.pgeditor_main(rect_width_, rect_height_, page_res_);
      }
#endif
    }

    AutoPopDebugSectionLevel subsection_handle2(&tess, tess.PushSubordinatePixDebugSection("Recognize All Words"));
    if (tess.recog_all_words(page_res_, monitor, nullptr, nullptr, 0)) {
#if !GRAPHICS_DISABLED
      if (scrollview_support) {
        tess.pgeditor_main(rect_width_, rect_height_, page_res_);
      }
#endif
      subsection_handle2.pop();
      if (tess.paragraph_text_based) {
        AutoPopDebugSectionLevel subsection_handle(&tess, tess.PushSubordinatePixDebugSection("Detect Paragraphs (After Recognition)"));
        DetectParagraphs(true);
#if !GRAPHICS_DISABLED
        if (scrollview_support) {
          tess.pgeditor_main(rect_width_, rect_height_, page_res_);
        }
#endif
      }
    } else {
      result = -1;
    }
  }
  return result;
}

// Takes ownership of the input pix.
void TessBaseAPI::SetInputImage(Pix *pix) {
  tesseract().set_pix_original(pix);
}

void TessBaseAPI::SetVisibleImage(Pix *pix) {
  if (pix_visible_image_)
    pixDestroy(&pix_visible_image_);
  pix_visible_image_ = nullptr;
  if (pix) {
    pix_visible_image_ = pixCopy(NULL, pix);
    // tesseract().set_pix_visible_image(pix);
  }
}

Pix *TessBaseAPI::GetInputImage() {
  return tesseract().pix_original();
}

static const char* NormalizationModeName(int mode) {
  switch(mode) {
    case 0:
      return "No normalization";
    case 1:
      return "Thresholding + Recognition";
    case 2:
      return "Thresholding";
    case 3:
      return "Recognition";
    default:
      ASSERT0(!"Unknown Normalization Mode");
      return "Unknown Normalization Mode";
  }
}

// Grayscale normalization (preprocessing)
bool TessBaseAPI::NormalizeImage(int mode) {
  Tesseract& tess = tesseract();
  AutoPopDebugSectionLevel section_handle(&tess, tess.PushSubordinatePixDebugSection("Normalize Image"));

  if (!GetInputImage()) {
    tprintError("Please use SetImage before applying the image pre-processing steps.\n");
    return false;
  }

  Image pix = thresholder_->GetPixNormRectGrey();
  if (tess.debug_image_normalization) {
    tess.AddPixDebugPage(pix, fmt::format("Grayscale normalization based on nlbin(Thomas Breuel) mode = {} ({})", mode, NormalizationModeName(mode)));
  }
  if (mode == 1) {
    SetInputImage(pix);
    thresholder_->SetImage(GetInputImage());
    if (tess.debug_image_normalization) {
      tess.AddPixDebugPage(thresholder_->GetPixRect(), "Grayscale normalization, as obtained from the thresholder & set up as input image");
    }
  } else if (mode == 2) {
    thresholder_->SetImage(pix);
    if (tess.debug_image_normalization) {
      tess.AddPixDebugPage(thresholder_->GetPixRect(), "Grayscale normalization, as obtained from the thresholder");
    }
  } else if (mode == 3) {
    SetInputImage(pix);
    if (tess.debug_image_normalization) {
      tess.AddPixDebugPage(GetInputImage(), "Grayscale normalization, now set up as input image");
    }
  } else {
    return false;
  }
  return true;
}

Pix* TessBaseAPI::GetVisibleImage() {
  return pix_visible_image_;
}

const char *TessBaseAPI::GetInputName() {
  if (tesseract_ != nullptr && !tesseract_->input_file_path_.empty()) {
    return tesseract_->input_file_path_.c_str();
  }
  return nullptr;
}

const char * TessBaseAPI::GetVisibleImageFilename() {
  if (tesseract_ != nullptr && !tesseract_->visible_image_file_path_.empty()) {
    return tesseract_->visible_image_file_path_.c_str();
  }
  return nullptr;
}

const char *TessBaseAPI::GetDatapath() {
  return tesseract().datadir_.c_str();
}

int TessBaseAPI::GetSourceYResolution() {
  if (thresholder_ == nullptr)
    return -1;
  return thresholder_->GetSourceYResolution();
}

// If `flist` exists, get data from there. Otherwise get data from `buf`.
// Seems convoluted, but is the easiest way I know of to meet multiple
// goals. Support streaming from stdin, and also work on platforms
// lacking fmemopen.
// 
// TODO: check different logic for flist/buf and simplify.
//
// If `tessedit_page_number` is non-negative, will only process that
// single page. Works for multi-page tiff file as well as or filelist.
bool TessBaseAPI::ProcessPagesFileList(FILE *flist, std::string *buf, const char *retry_config,
                                       int timeout_millisec, TessResultRenderer *renderer) {
  if (!flist && !buf) {
    return false;
  }
  Tesseract& tess = tesseract();
  int page_number = (tess.tessedit_page_number >= 0) ? tess.tessedit_page_number : 0;
  char pagename[MAX_PATH];

  std::vector<std::string> lines;
  if (!flist) {
    std::string line;
    for (const auto ch : *buf) {
      if (ch == '\n') {
        lines.push_back(line);
        line.clear();
      } else {
        line.push_back(ch);
      }
    }
    if (!line.empty()) {
      // Add last line without terminating LF.
      lines.push_back(line);
    }
    if (lines.empty()) {
      return false;
    }
  }

  // Begin producing output
  if (renderer && !renderer->BeginDocument(document_title.c_str())) {
    return false;
  }

  // Loop over all pages - or just the requested one
  for (int i = 0; ; i++) {
    if (flist) {
      if (fgets(pagename, sizeof(pagename), flist) == nullptr) {
        break;
      }
    } else {
      // Skip to the requested page number.
      if (i < page_number)
        continue;
      else if (page_number >= lines.size()) {
        break;
      }
      snprintf(pagename, sizeof(pagename), "%s", lines[i].c_str());
    }
    chomp_string(pagename);
    Pix *pix = pixRead(pagename);
    if (pix == nullptr) {
      tprintError("Image file {} cannot be read!\n", pagename);
      return false;
    }
    tprintInfo("Processing page #{} : {}\n", page_number + 1, pagename);
    tess.applybox_page.set_value(page_number, PARAM_VALUE_IS_SET_BY_CORE_RUN);
    bool r = ProcessPage(pix, pagename, retry_config, timeout_millisec, renderer);

    if (two_pass) {
      Boxa *default_boxes = GetComponentImages(tesseract::RIL_BLOCK, true, nullptr, nullptr);

      // pixWrite("/tmp/out.png", pix, IFF_PNG);
      // Pix *newpix = pixPaintBoxa(pix, default_boxes, 0);
      Pix *newpix = pixSetBlackOrWhiteBoxa(pix, default_boxes, L_SET_BLACK);
      // pixWrite("/tmp/out_boxes.png", newpix, IFF_PNG);

      SetPageSegMode(PSM_SINGLE_BLOCK);
      // Set thresholding method to 0 for second pass regardless
      tess.thresholding_method = (int)ThresholdMethod::Otsu;
      // SetPageSegMode(PSM_SPARSE_TEXT);

      SetImage(newpix);

      r = r && !Recognize(NULL);
      renderer->AddImage(this);

      boxaDestroy(&default_boxes);
      pixDestroy(&newpix);
    }

    pixDestroy(&pix);
    if (!r) {
      return false;
    }
    if (tess.tessedit_page_number >= 0) {
      break;
    }
    ++page_number;
  }

  // Finish producing output
  if (renderer && !renderer->EndDocument()) {
    return false;
  }
  return true;
}

// If `tessedit_page_number` is non-negative, will only process that
// single page in the multi-page tiff file.
bool TessBaseAPI::ProcessPagesMultipageTiff(const l_uint8 *data, size_t size, const char *filename,
                                            const char *retry_config, int timeout_millisec,
                                            TessResultRenderer *renderer) {
  Pix *pix = nullptr;
  Tesseract& tess = tesseract();
  int page_number = (tess.tessedit_page_number >= 0) ? tess.tessedit_page_number : 0;
  size_t offset = 0;
  for (int pgn = 1; ; ++pgn) {
    // pix = (data) ? pixReadMemTiff(data, size, page_number) : pixReadTiff(filename, page_number);
    pix = (data) ? pixReadMemFromMultipageTiff(data, size, &offset)
                 : pixReadFromMultipageTiff(filename, &offset);
    if (pix == nullptr) {
      break;
    }
  if (tess.tessedit_page_number > 0 && pgn != tess.tessedit_page_number) {
    continue;
  }
  
    tprintInfo("Processing page #{} of multipage TIFF {}\n", pgn, filename ? filename : "(from internal storage)");
    tess.applybox_page.set_value(pgn, PARAM_VALUE_IS_SET_BY_CORE_RUN);
    bool r = ProcessPage(pix, filename, retry_config, timeout_millisec, renderer);
    pixDestroy(&pix);
    if (!r) {
      return false;
    }
    if (tess.tessedit_page_number >= 0) {
      break;
    }
    if (!offset) {
      break;
    }
  }
  return true;
}

// Master ProcessPages calls ProcessPagesInternal and then does any post-
// processing required due to being in a training mode.
bool TessBaseAPI::ProcessPages(const char *filename, const char *retry_config, int timeout_millisec,
                               TessResultRenderer *renderer) {
  Tesseract& tess = tesseract();
  AutoPopDebugSectionLevel section_handle(&tess, tess.PushSubordinatePixDebugSection("Process pages"));
  
  bool result = ProcessPagesInternal(filename, retry_config, timeout_millisec, renderer);
#if !DISABLED_LEGACY_ENGINE
  if (result) {
    if (tess.tessedit_train_from_boxes && !tess.WriteTRFile(output_file_.c_str())) {
      tprintError("Write of TR file failed: {}\n", output_file_.c_str());
      return false;
    }
  }
#endif // !DISABLED_LEGACY_ENGINE
  return result;
}

#ifdef HAVE_LIBCURL
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size = size * nmemb;
  auto *buf = reinterpret_cast<std::string *>(userp);
  buf->append(reinterpret_cast<const char *>(contents), size);
  return size;
}
#endif

// In the ideal scenario, Tesseract will start working on data as soon
// as it can. For example, if you stream a filelist through stdin, we
// should start the OCR process as soon as the first filename is
// available. This is particularly useful when hooking Tesseract up to
// slow hardware such as a book scanning machine.
//
// Unfortunately there are trade-offs. You can't seek on stdin. That
// makes automatic detection of datatype (TIFF? filelist? PNG?)
// impractical.  So we support a command line flag to explicitly
// identify the scenario that really matters: filelists on
// stdin. We'll still do our best if the user likes pipes.
bool TessBaseAPI::ProcessPagesInternal(const char *filename, const char *retry_config,
                                       int timeout_millisec, TessResultRenderer *renderer) {
  bool stdInput = !strcmp(filename, "stdin") || !strcmp(filename, "/dev/stdin") || !strcmp(filename, "-");
  if (stdInput) {
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
      tprintError("Cannot set STDIN to binary: {}", strerror(errno));
#endif // WIN32
  }

  if (stream_filelist) {
    return ProcessPagesFileList(stdin, nullptr, retry_config, timeout_millisec, renderer);
  }

  // At this point we are officially in auto-detection territory.
  // That means any data in stdin must be buffered, to make it
  // seekable.
  std::string buf;
  const l_uint8 *data = nullptr;
  if (stdInput) {
    buf.assign((std::istreambuf_iterator<char>(std::cin)), (std::istreambuf_iterator<char>()));
    data = reinterpret_cast<const l_uint8 *>(buf.data());
  } else if (strstr(filename, "://") != nullptr) {
    // Get image or image list by URL.
#ifdef HAVE_LIBCURL
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
      fprintf(stderr, "Error, curl_easy_init failed\n");
      return false;
    } else {
      CURLcode curlcode;
      auto error = [curl, &curlcode](const char *function) {
        tprintError("{} failed with error {}\n", function, curl_easy_strerror(curlcode));
        curl_easy_cleanup(curl);
        return false;
      };
      curlcode = curl_easy_setopt(curl, CURLOPT_URL, filename);
      if (curlcode != CURLE_OK) {
        return error("curl_easy_setopt");
      }
      curlcode = curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
      if (curlcode != CURLE_OK) {
        return error("curl_easy_setopt");
      }
      // Follow HTTP, HTTPS, FTP and FTPS redirects.
      curlcode = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
      if (curlcode != CURLE_OK) {
        return error("curl_easy_setopt");
      }
      // Allow no more than 8 redirections to prevent endless loops.
      curlcode = curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 8);
      if (curlcode != CURLE_OK) {
        return error("curl_easy_setopt");
      }
      int timeout = curl_timeout;
      if (timeout > 0) {
        curlcode = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        if (curlcode != CURLE_OK) {
          return error("curl_easy_setopt");
        }
        curlcode = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        if (curlcode != CURLE_OK) {
          return error("curl_easy_setopt");
        }
      }
      std::string cookiefile = curl_cookiefile;
      if (!cookiefile.empty()) {
        curlcode = curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookiefile.c_str());
        if (curlcode != CURLE_OK) {
          return error("curl_easy_setopt");
        }
      }
      curlcode = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
      if (curlcode != CURLE_OK) {
        return error("curl_easy_setopt");
      }
      curlcode = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
      if (curlcode != CURLE_OK) {
        return error("curl_easy_setopt");
      }
      curlcode = curl_easy_setopt(curl, CURLOPT_USERAGENT, "Tesseract OCR");
      if (curlcode != CURLE_OK) {
        return error("curl_easy_setopt");
      }
      curlcode = curl_easy_perform(curl);
      if (curlcode != CURLE_OK) {
        return error("curl_easy_perform");
      }
      curl_easy_cleanup(curl);
      data = reinterpret_cast<const l_uint8 *>(buf.data());
    }
#else
    fprintf(stderr, "Error, this tesseract has no URL support\n");
    return false;
#endif
  } else {
    // Check whether the input file can be read.
    if (FILE *file = fopen(filename, "rb")) {
      fclose(file);
    } else {
      tprintError("cannot read input file {}: {}\n", filename, strerror(errno));
      return false;
    }
  }

  // Here is our autodetection
  int format;
  int r = (data != nullptr) ? findFileFormatBuffer(data, &format) : findFileFormat(filename, &format);

  // Maybe we have a filelist
  if (r != 0 || format == IFF_UNKNOWN) {
    std::string s;
    if (data != nullptr) {
      s = buf.c_str();
    } else {
      std::ifstream t(filename);
      std::string u((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
      s = u.c_str();
    }
    return ProcessPagesFileList(nullptr, &s, retry_config, timeout_millisec, renderer);
  }

  // Maybe we have a TIFF which is potentially multipage
  bool tiff = (format == IFF_TIFF || format == IFF_TIFF_PACKBITS || format == IFF_TIFF_RLE ||
               format == IFF_TIFF_G3 || format == IFF_TIFF_G4 || format == IFF_TIFF_LZW ||
#if LIBLEPT_MAJOR_VERSION > 1 || LIBLEPT_MINOR_VERSION > 76
               format == IFF_TIFF_JPEG ||
#endif
               format == IFF_TIFF_ZIP);

  // Fail early if we can, before producing any output
  Pix *pix = nullptr;
  if (!tiff) {
    pix = (data != nullptr) ? pixReadMem(data, buf.size()) : pixRead(filename);
    if (pix == nullptr) {
      return false;
    }
  }

  // Begin the output
  if (renderer && !renderer->BeginDocument(document_title.c_str())) {
    pixDestroy(&pix);
    return false;
  }

  // Produce output
  if (tiff) {
    r = ProcessPagesMultipageTiff(data, buf.size(), filename, retry_config, timeout_millisec, renderer);
  }
  else {
    tesseract().applybox_page.set_value(-1, PARAM_VALUE_IS_SET_BY_CORE_RUN);
    r = ProcessPage(pix, filename, retry_config, timeout_millisec, renderer);
  }

  // Clean up memory as needed
  pixDestroy(&pix);

  // End the output
  if (!r || (renderer && !renderer->EndDocument())) {
    return false;
  }
  return true;
}

bool TessBaseAPI::ProcessPage(Pix *pix, const char *filename,
                              const char *retry_config, int timeout_millisec,
                              TessResultRenderer *renderer) {
  Tesseract& tess = tesseract();
  AutoPopDebugSectionLevel page_level_handle(&tess, tess.PushSubordinatePixDebugSection(fmt::format("Process a single page: page #{}", static_cast<int>(tess.tessedit_page_number))));
  //page_level_handle.SetAsRootLevelForParamUsageReporting();

  SetInputName(filename);

  SetImage(pix);

  // Before we start to do *real* work, do a preliminary sanity check re expected memory pressure.
  // The check MAY recur in some (semi)public APIs that MAY be called later, but this is the big one
  // and it's a simple check at negligible cost, saving us some headaches when we start feeding large
  // material to the Tesseract animal.
  //
  // TODO: rescale overlarge input images? Or is that left to userland code? (as it'll be pretty fringe anyway)
  {
    auto cost = TessBaseAPI::EstimateImageMemoryCost(pix);
    std::string cost_report = cost;
    tprintInfo("Estimated memory pressure: {} for input image size {} x {} px\n", cost_report, pixGetWidth(pix), pixGetHeight(pix));

    if (CheckAndReportIfImageTooLarge(pix)) {
      return false; // fail early
    }
  }

  // Image preprocessing on image
  
  // pixTRCMap(PIX   *pixs, PIX   *pixm, NUMA  *na)  --> can create and use our own dynamic range mapping with this one!
  // 
  // pixAutoPhotoinvert()
  //
  //     if (edgecrop > 0.0) {
  //  box = boxCreate(0.5f * edgecrop * w, 0.5f * edgecrop * h,
  //                   (1.0f - edgecrop) * w, (1.0f - edgecrop) * h);
  //   pix2 = pixClipRectangle(pix1, box, NULL);
  //   boxDestroy(&box);
  // }
  //   else {
  //   pix2 = pixClone(pix1);
  // }
  //
  // pixCleanBackgroundToWhite()
  //
  //   pixalpha = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);  /* save */
  //   if ((nag = numaGammaTRC(gamma, minval, maxval)) == NULL)
  //     return (PIX *)ERROR_PTR("nag not made", __func__, pixd);
  //   pixTRCMap(pixd, NULL, nag);
  //   pixSetRGBComponent(pixd, pixalpha, L_ALPHA_CHANNEL); /* restore */
  //   pixSetSpp(pixd, 4);
  //   numaDestroy(&nag);
  //   pixDestroy(&pixalpha);
  //
  // l_float32  avefg, avebg;
  //   l_float32 numfg, numbg;
  //   NUMA *na = pixGetGrayHistogram(pixt, 1);
  //   l_float32 mean, median, mode, variance;
  //   numaGetHistogramStats(na, 0.0, 1.0, &mean, &median, &mode, &variance);
  //
  // PIX * pixGetRGBComponent ( PIX *pixs, l_int32 comp );
  //
  // pixGetRankValue()
  // numaHistogramGetValFromRank(na, rank, &val);
  //
  // numaGetMin(), numaGetMax()
  //
  // pixThresholdByConnComp()
  //
  //  numaGetNonzeroRange()
  //
  // pixMaxDynamicRange

  


  

  // Grayscale normalization
  int graynorm_mode = tess.preprocess_graynorm_mode;
  {
    Image input_img = GetInputImage();

  if ((graynorm_mode > 0 || tess.showcase_threshold_methods) && NormalizeImage(graynorm_mode) && tess.tessedit_write_images) {
    // Write normalized image 
    Pix *p1;
    if (graynorm_mode == 2) {
      p1 = thresholder_->GetPixRect();
    } else {
      p1 = GetInputImage();
    }
    tess.AddPixDebugPage(p1, fmt::format("(normalized) image to process @ graynorm_mode = {}", graynorm_mode));
  }

    // rewind the normalize operation as it was only showcased, but not intended for use by the remainder of the process:
    if (tess.showcase_threshold_methods && (graynorm_mode <= 0)) {
      SetInputImage(input_img);
      SetImage(pix);
    }
  }

  // Recognition
  
  bool failed = false;

  if (tess.tessedit_pageseg_mode == PSM_AUTO_ONLY) {
    // Disabled character recognition
    if (! std::unique_ptr<const PageIterator>(AnalyseLayout())) {
      failed = true;
    }
  } else if (tess.tessedit_pageseg_mode == PSM_OSD_ONLY) {
    failed = (FindLines() != 0);
  } else if (timeout_millisec > 0) {
    // Running with a timeout.
    ETEXT_DESC monitor;
    monitor.cancel = nullptr;
    monitor.cancel_this = nullptr;
    monitor.set_deadline_msecs(timeout_millisec);

    // Now run the main recognition.
    failed = (Recognize(&monitor) < 0);
  } else {
    // Normal layout and character recognition with no timeout.
    failed = (Recognize(nullptr) < 0);
  }

  if (tess.tessedit_write_images) {
    Pix *page_pix = GetThresholdedImage();
    tess.AddPixDebugPage(page_pix, fmt::format("processed page #{} : text recog done", static_cast<int>(tess.tessedit_page_number)));
  }

  if (failed && retry_config != nullptr && retry_config[0] != '\0') {
    // Save current config variables before switching modes.
    FILE *fp = fopen(kOldVarsFile, "wb");
    if (fp == nullptr) {
      tprintError("Failed to open file \"{}\"\n", kOldVarsFile);
    } else {
      DumpVariables(fp);
      fclose(fp);
    }
    // Switch to alternate mode for retry.
    ReadConfigFile(retry_config);
    SetImage(pix);
    
    // Apply image preprocessing
    NormalizeImage(graynorm_mode);

    //if (normalize_grayscale) thresholder_->SetImage(thresholder_->GetPixNormRectGrey());
    Recognize(nullptr);
    // Restore saved config variables.
    ReadConfigFile(kOldVarsFile);
  }

  if (renderer && !failed) {
    failed = !renderer->AddImage(this);
  }
  //pixDestroy(&pixs);
  return !failed;
}

/**
 * Get a left-to-right iterator to the results of LayoutAnalysis and/or
 * Recognize. The returned iterator must be deleted after use.
 */
LTRResultIterator *TessBaseAPI::GetLTRIterator() {
  if (tesseract_ == nullptr || page_res_ == nullptr) {
    return nullptr;
  }
  return new LTRResultIterator(page_res_, tesseract_, thresholder_->GetScaleFactor(),
                               thresholder_->GetScaledYResolution(), rect_left_, rect_top_,
                               rect_width_, rect_height_);
}

/**
 * Get a reading-order iterator to the results of LayoutAnalysis and/or
 * Recognize. The returned iterator must be deleted after use.
 * WARNING! This class points to data held within the TessBaseAPI class, and
 * therefore can only be used while the TessBaseAPI class still exists and
 * has not been subjected to a call of Init, SetImage, Recognize, Clear, End
 * DetectOS, or anything else that changes the internal PAGE_RES.
 */
ResultIterator *TessBaseAPI::GetIterator() {
  if (tesseract_ == nullptr || page_res_ == nullptr) {
    return nullptr;
  }
  return ResultIterator::StartOfParagraph(LTRResultIterator(
      page_res_, tesseract_, thresholder_->GetScaleFactor(), thresholder_->GetScaledYResolution(),
      rect_left_, rect_top_, rect_width_, rect_height_));
}

/**
 * Get a mutable iterator to the results of LayoutAnalysis and/or Recognize.
 * The returned iterator must be deleted after use.
 * WARNING! This class points to data held within the TessBaseAPI class, and
 * therefore can only be used while the TessBaseAPI class still exists and
 * has not been subjected to a call of Init, SetImage, Recognize, Clear, End
 * DetectOS, or anything else that changes the internal PAGE_RES.
 */
MutableIterator *TessBaseAPI::GetMutableIterator() {
  if (tesseract_ == nullptr || page_res_ == nullptr) {
    return nullptr;
  }
  return new MutableIterator(page_res_, tesseract_, thresholder_->GetScaleFactor(),
                             thresholder_->GetScaledYResolution(), rect_left_, rect_top_,
                             rect_width_, rect_height_);
}

/** Make a text string from the internal data structures. */
char *TessBaseAPI::GetUTF8Text() {
  if (tesseract_ == nullptr || (!recognition_done_ && Recognize(nullptr) < 0)) {
    return nullptr;
  }
  std::string text("");
  const std::unique_ptr</*non-const*/ ResultIterator> it(GetIterator());
  do {
    if (it->Empty(RIL_PARA)) {
      continue;
    }
    auto block_type = it->BlockType();
    switch (block_type) {
      case PT_FLOWING_IMAGE:
      case PT_HEADING_IMAGE:
      case PT_PULLOUT_IMAGE:
      case PT_HORZ_LINE:
      case PT_VERT_LINE:
        // Ignore images and lines for text output.
        continue;
      case PT_NOISE:
        tprintError("TODO: Please report image which triggers the noise case.\n");
        ASSERT_HOST(false);
    break;
      default:
        break;
    }

    const std::unique_ptr<const char[]> para_text(it->GetUTF8Text(RIL_PARA));
    text += para_text.get();
  } while (it->Next(RIL_PARA));
  return copy_string(text);
}

size_t TessBaseAPI::GetNumberOfTables() const
{
  return constUniqueInstance<std::vector<TessTable>>().size();
}

std::tuple<int,int,int,int> TessBaseAPI::GetTableBoundingBox(unsigned i)
{
  const auto &t = constUniqueInstance<std::vector<TessTable>>();

  if (i >= t.size()) {
    return std::tuple<int, int, int, int>(0, 0, 0, 0);
  }

  const int height = tesseract().ImageHeight();

  return std::make_tuple<int,int,int,int>(
    t[i].box.left(), height - t[i].box.top(),
    t[i].box.right(), height - t[i].box.bottom());
}

std::vector<std::tuple<int,int,int,int>> TessBaseAPI::GetTableRows(unsigned i)
{
  const auto &t = constUniqueInstance<std::vector<TessTable>>();

  if (i >= t.size()) {
    return std::vector<std::tuple<int, int, int, int>>();
  }

  std::vector<std::tuple<int,int,int,int>> rows(t[i].rows.size());
  const int height = tesseract().ImageHeight();

  for (unsigned j = 0; j < t[i].rows.size(); ++j) {
    rows[j] =
        std::make_tuple<int, int, int, int>(t[i].rows[j].left(), height - t[i].rows[j].top(),
                                            t[i].rows[j].right(), height - t[i].rows[j].bottom());
  }

  return rows;
}

std::vector<std::tuple<int,int,int,int>> TessBaseAPI::GetTableCols(unsigned i)
{
  const auto &t = constUniqueInstance<std::vector<TessTable>>();

  if (i >= t.size()) {
    return std::vector<std::tuple<int, int, int, int>>();
  }

  std::vector<std::tuple<int,int,int,int>> cols(t[i].cols.size());
  const int height = tesseract().ImageHeight();

  for (unsigned j = 0; j < t[i].cols.size(); ++j) {
    cols[j] =
        std::make_tuple<int, int, int, int>(t[i].cols[j].left(), height - t[i].cols[j].top(),
                                            t[i].cols[j].right(), height - t[i].cols[j].bottom());
  }

  return cols;
}

static void AddBoxToTSV(const PageIterator *it, PageIteratorLevel level, std::string &text) {
  int left, top, right, bottom;
  it->BoundingBox(level, &left, &top, &right, &bottom);
  text += "\t" + std::to_string(left);
  text += "\t" + std::to_string(top);
  text += "\t" + std::to_string(right - left);
  text += "\t" + std::to_string(bottom - top);
}

/**
 * Make a TSV-formatted string from the internal data structures.
 * Allows additional column with detected language.
 * page_number is 0-based but will appear in the output as 1-based.
 *
 * Returned string must be freed with the delete [] operator.
 */
char *TessBaseAPI::GetTSVText(int page_number, bool lang_info) {
  if (tesseract_ == nullptr || (page_res_ == nullptr && Recognize(nullptr) < 0)) {
    return nullptr;
  }

  int page_id = page_number + 1; // we use 1-based page numbers.

  int page_num = page_id;
  int block_num = 0;
  int par_num = 0;
  int line_num = 0;
  int word_num = 0;
  int symbol_num = 0;
  std::string lang;

  std::string tsv_str;
  tsv_str += "1\t" + std::to_string(page_num); // level 1 - page
  tsv_str += "\t" + std::to_string(block_num);
  tsv_str += "\t" + std::to_string(par_num);
  tsv_str += "\t" + std::to_string(line_num);
  tsv_str += "\t" + std::to_string(word_num);
  tsv_str += "\t" + std::to_string(symbol_num);
  tsv_str += "\t" + std::to_string(rect_left_);
  tsv_str += "\t" + std::to_string(rect_top_);
  tsv_str += "\t" + std::to_string(rect_width_);
  tsv_str += "\t" + std::to_string(rect_height_);
  tsv_str += "\t-1";
  if (lang_info) {
    tsv_str += "\t" + lang;
  }
  tsv_str += "\t\n";

  const std::unique_ptr</*non-const*/ ResultIterator> res_it(GetIterator());
  while (!res_it->Empty(RIL_BLOCK)) {
    if (res_it->Empty(RIL_WORD)) {
      res_it->Next(RIL_WORD);
      continue;
    }

    // Add rows for any new block/paragraph/textline.
    if (res_it->IsAtBeginningOf(RIL_BLOCK)) {
      block_num++;
      par_num = 0;
      line_num = 0;
      word_num = 0;
      symbol_num = 0;
      tsv_str += "2\t" + std::to_string(page_num); // level 2 - block
      tsv_str += "\t" + std::to_string(block_num);
      tsv_str += "\t" + std::to_string(par_num);
      tsv_str += "\t" + std::to_string(line_num);
      tsv_str += "\t" + std::to_string(word_num);
      tsv_str += "\t" + std::to_string(symbol_num);
      AddBoxToTSV(res_it.get(), RIL_BLOCK, tsv_str);
      tsv_str += "\t-1";
      if (lang_info) {
        tsv_str += "\t";
      }
      tsv_str += "\t\n"; // end of row for block
    }
    if (res_it->IsAtBeginningOf(RIL_PARA)) {
      if (lang_info) {
        lang = res_it->WordRecognitionLanguage();
      }
      par_num++;
      line_num = 0;
      word_num = 0;
      symbol_num = 0;
      tsv_str += "3\t" + std::to_string(page_num); // level 3 - paragraph
      tsv_str += "\t" + std::to_string(block_num);
      tsv_str += "\t" + std::to_string(par_num);
      tsv_str += "\t" + std::to_string(line_num);
      tsv_str += "\t" + std::to_string(word_num);
      tsv_str += "\t" + std::to_string(symbol_num);
      AddBoxToTSV(res_it.get(), RIL_PARA, tsv_str);
      tsv_str += "\t-1";
      if (lang_info) {
        tsv_str += "\t" + lang;
      }
      tsv_str += "\t\n"; // end of row for para
    }
    if (res_it->IsAtBeginningOf(RIL_TEXTLINE)) {
      line_num++;
      word_num = 0;
      symbol_num = 0;
      tsv_str += "4\t" + std::to_string(page_num); // level 4 - line
      tsv_str += "\t" + std::to_string(block_num);
      tsv_str += "\t" + std::to_string(par_num);
      tsv_str += "\t" + std::to_string(line_num);
      tsv_str += "\t" + std::to_string(word_num);
      tsv_str += "\t" + std::to_string(symbol_num);
      AddBoxToTSV(res_it.get(), RIL_TEXTLINE, tsv_str);
      tsv_str += "\t-1";
      if (lang_info) {
        tsv_str += "\t";
      }
      tsv_str += "\t\n"; // end of row for line
    }

    // Now, process the word...
    int left, top, right, bottom;
    res_it->BoundingBox(RIL_WORD, &left, &top, &right, &bottom);
    word_num++;
    symbol_num = 0;
    tsv_str += "5\t" + std::to_string(page_num); // level 5 - word
    tsv_str += "\t" + std::to_string(block_num);
    tsv_str += "\t" + std::to_string(par_num);
    tsv_str += "\t" + std::to_string(line_num);
    tsv_str += "\t" + std::to_string(word_num);
    tsv_str += "\t" + std::to_string(symbol_num);
    tsv_str += "\t" + std::to_string(left);
    tsv_str += "\t" + std::to_string(top);
    tsv_str += "\t" + std::to_string(right - left);
    tsv_str += "\t" + std::to_string(bottom - top);
    tsv_str += "\t" + std::to_string(res_it->Confidence(RIL_WORD));

    if (lang_info) {
      const char *word_lang = res_it->WordRecognitionLanguage();
      tsv_str += "\t";
      if (word_lang) {
        tsv_str += word_lang;
      }
    }

    tsv_str += "\t";

    std::string tsv_symbol_lines;

    do {
      tsv_str += std::unique_ptr<const char[]>(res_it->GetUTF8Text(RIL_SYMBOL)).get();

      res_it->BoundingBox(RIL_SYMBOL, &left, &top, &right, &bottom);
      symbol_num++;
      tsv_symbol_lines += "6\t" + std::to_string(page_num); // level 6 - symbol
      tsv_symbol_lines += "\t" + std::to_string(block_num);
      tsv_symbol_lines += "\t" + std::to_string(par_num);
      tsv_symbol_lines += "\t" + std::to_string(line_num);
      tsv_symbol_lines += "\t" + std::to_string(word_num);
      tsv_symbol_lines += "\t" + std::to_string(symbol_num);
      tsv_symbol_lines += "\t" + std::to_string(left);
      tsv_symbol_lines += "\t" + std::to_string(top);
      tsv_symbol_lines += "\t" + std::to_string(right - left);
      tsv_symbol_lines += "\t" + std::to_string(bottom - top);
      tsv_symbol_lines += "\t" + std::to_string(res_it->Confidence(RIL_SYMBOL));
      tsv_symbol_lines += "\t";
      tsv_symbol_lines += std::unique_ptr<const char[]>(res_it->GetUTF8Text(RIL_SYMBOL)).get();
      tsv_symbol_lines += "\n";

      res_it->Next(RIL_SYMBOL);
    } while (!res_it->Empty(RIL_BLOCK) && !res_it->IsAtBeginningOf(RIL_WORD));
    tsv_str += "\n"; // end of row

    tsv_str += tsv_symbol_lines; // add the individual symbol rows right after the word row they are consioered to a part of.
  }

  return copy_string(tsv_str);
}

/** The 5 numbers output for each box (the usual 4 and a page number.) */
const int kNumbersPerBlob = 5;
/**
 * The number of bytes taken by each number. Since we use int16_t for ICOORD,
 * assume only 5 digits max.
 */
const int kBytesPerNumber = 5;
/**
 * Multiplier for max expected textlength assumes (kBytesPerNumber + space)
 * * kNumbersPerBlob plus the newline. Add to this the
 * original UTF8 characters, and one kMaxBytesPerLine for safety.
 */
const int kBytesPerBoxFileLine = (kBytesPerNumber + 1) * kNumbersPerBlob + 1;
/** Max bytes in the decimal representation of int64_t. */
const int kBytesPer64BitNumber = 20;
/**
 * A maximal single box could occupy kNumbersPerBlob numbers at
 * kBytesPer64BitNumber digits (if someone sneaks in a 64 bit value) and a
 * space plus the newline and the maximum length of a UNICHAR.
 * Test against this on each iteration for safety.
 */
const int kMaxBytesPerLine = kNumbersPerBlob * (kBytesPer64BitNumber + 1) + 1 + UNICHAR_LEN;

/**
 * The recognized text is returned as a char* which is coded
 * as a UTF8 box file.
 * page_number is a 0-base page index that will appear in the box file.
 * Returned string must be freed with the delete [] operator.
 */
char *TessBaseAPI::GetBoxText(int page_number) {
  if (tesseract_ == nullptr || (!recognition_done_ && Recognize(nullptr) < 0)) {
    return nullptr;
  }
  int blob_count;
  int utf8_length = TextLength(&blob_count);
  int total_length = blob_count * kBytesPerBoxFileLine + utf8_length + kMaxBytesPerLine;
  char *result = new char[total_length];
  result[0] = '\0';
  int output_length = 0;
  LTRResultIterator *it = GetLTRIterator();
  do {
    int left, top, right, bottom;
    if (it->BoundingBox(RIL_SYMBOL, &left, &top, &right, &bottom)) {
      const std::unique_ptr</*non-const*/ char[]> text(it->GetUTF8Text(RIL_SYMBOL));
      // Tesseract uses space for recognition failure. Fix to a reject
      // character, kTesseractReject so we don't create illegal box files.
      for (int i = 0; text[i] != '\0'; ++i) {
        if (text[i] == ' ') {
          text[i] = kTesseractReject;
        }
      }
      snprintf(result + output_length, total_length - output_length, "%s %d %d %d %d %d\n",
               text.get(), left, image_height_ - bottom, right, image_height_ - top, page_number);
      output_length += strlen(result + output_length);
      // Just in case...
      if (output_length + kMaxBytesPerLine > total_length) {
        break;
      }
    }
  } while (it->Next(RIL_SYMBOL));
  delete it;
  return result;
}

/**
 * Conversion table for non-latin characters.
 * Maps characters out of the latin set into the latin set.
 * TODO(rays) incorporate this translation into unicharset.
 */
const int kUniChs[] = {0x20ac, 0x201c, 0x201d, 0x2018, 0x2019, 0x2022, 0x2014, 0};
/** Latin chars corresponding to the unicode chars above. */
const int kLatinChs[] = {0x00a2, 0x0022, 0x0022, 0x0027, 0x0027, 0x00b7, 0x002d, 0};

/**
 * The recognized text is returned as a char* which is coded
 * as UNLV format Latin-1 with specific reject and suspect codes.
 * Returned string must be freed with the delete [] operator.
 */
char *TessBaseAPI::GetUNLVText() {
  if (tesseract_ == nullptr || (!recognition_done_ && Recognize(nullptr) < 0)) {
    return nullptr;
  }
  bool tilde_crunch_written = false;
  bool last_char_was_newline = true;
  bool last_char_was_tilde = false;

  int total_length = TextLength(nullptr);
  PAGE_RES_IT page_res_it(page_res_);
  char *result = new char[total_length];
  char *ptr = result;
  for (page_res_it.restart_page(); page_res_it.word() != nullptr; page_res_it.forward()) {
    WERD_RES *word = page_res_it.word();
    // Process the current word.
    if (word->unlv_crunch_mode != CR_NONE) {
      if (word->unlv_crunch_mode != CR_DELETE &&
          (!tilde_crunch_written ||
           (word->unlv_crunch_mode == CR_KEEP_SPACE && word->word->space() > 0 &&
            !word->word->flag(W_FUZZY_NON) && !word->word->flag(W_FUZZY_SP)))) {
        if (!word->word->flag(W_BOL) && word->word->space() > 0 && !word->word->flag(W_FUZZY_NON) &&
            !word->word->flag(W_FUZZY_SP)) {
          /* Write a space to separate from preceding good text */
          *ptr++ = ' ';
          last_char_was_tilde = false;
        }
        if (!last_char_was_tilde) {
          // Write a reject char.
          last_char_was_tilde = true;
          *ptr++ = kUNLVReject;
          tilde_crunch_written = true;
          last_char_was_newline = false;
        }
      }
    } else {
      // NORMAL PROCESSING of non tilde crunched words.
      tilde_crunch_written = false;
      tesseract().set_unlv_suspects(word);
      const char *wordstr = word->best_choice->unichar_string().c_str();
      const auto &lengths = word->best_choice->unichar_lengths();
      int length = lengths.length();
      int i = 0;
      int offset = 0;

      if (last_char_was_tilde && word->word->space() == 0 && wordstr[offset] == ' ') {
        // Prevent adjacent tilde across words - we know that adjacent tildes
        // within words have been removed.
        // Skip the first character.
        offset = lengths[i++];
      }
      if (i < length && wordstr[offset] != 0) {
        if (!last_char_was_newline) {
          *ptr++ = ' ';
        } else {
          last_char_was_newline = false;
        }
        for (; i < length; offset += lengths[i++]) {
          if (wordstr[offset] == ' ' || wordstr[offset] == kTesseractReject) {
            *ptr++ = kUNLVReject;
            last_char_was_tilde = true;
          } else {
            if (word->reject_map[i].rejected()) {
              *ptr++ = kUNLVSuspect;
            }
            UNICHAR ch(wordstr + offset, lengths[i]);
            int uni_ch = ch.first_uni();
            for (int j = 0; kUniChs[j] != 0; ++j) {
              if (kUniChs[j] == uni_ch) {
                uni_ch = kLatinChs[j];
                break;
              }
            }
            if (uni_ch <= 0xff) {
              *ptr++ = static_cast<char>(uni_ch);
              last_char_was_tilde = false;
            } else {
              *ptr++ = kUNLVReject;
              last_char_was_tilde = true;
            }
          }
        }
      }
    }
    if (word->word->flag(W_EOL) && !last_char_was_newline) {
      /* Add a new line output */
      *ptr++ = '\n';
      tilde_crunch_written = false;
      last_char_was_newline = true;
      last_char_was_tilde = false;
    }
  }
  *ptr++ = '\n';
  *ptr = '\0';
  return result;
}

#if !DISABLED_LEGACY_ENGINE

/**
 * Detect the orientation of the input image and apparent script (alphabet).
 * orient_deg is the detected clockwise rotation of the input image in degrees
 * (0, 90, 180, 270)
 * orient_conf is the confidence (15.0 is reasonably confident)
 * script_name is an ASCII string, the name of the script, e.g. "Latin"
 * script_conf is confidence level in the script
 * Returns true on success and writes values to each parameter as an output
 */
bool TessBaseAPI::DetectOrientationScript(int *orient_deg, float *orient_conf,
                                          const char **script_name, float *script_conf) {
  OSResults osr;

  bool osd = DetectOS(&osr);
  if (!osd) {
    return false;
  }

  int orient_id = osr.best_result.orientation_id;
  int script_id = osr.get_best_script(orient_id);
  if (orient_conf) {
    *orient_conf = osr.best_result.oconfidence;
  }
  if (orient_deg) {
    *orient_deg = orient_id * 90; // convert quadrant to degrees
  }

  if (script_name) {
    const char *script = osr.unicharset->get_script_from_script_id(script_id);

    *script_name = script;
  }

  if (script_conf) {
    *script_conf = osr.best_result.sconfidence;
  }

  return true;
}

/**
 * The recognized text is returned as a char* which is coded
 * as UTF8 and must be freed with the delete [] operator.
 * page_number is a 0-based page index that will appear in the osd file.
 */
char *TessBaseAPI::GetOsdText(int page_number) {
  int orient_deg;
  float orient_conf;
  const char *script_name;
  float script_conf;

  if (!DetectOrientationScript(&orient_deg, &orient_conf, &script_name, &script_conf)) {
    return nullptr;
  }

  // clockwise rotation needed to make the page upright
  int rotate = OrientationIdToValue(orient_deg / 90);

  std::stringstream stream;
  // Use "C" locale (needed for float values orient_conf and script_conf).
  stream.imbue(std::locale::classic());
  // Use fixed notation with 2 digits after the decimal point for float values.
  stream.precision(2);
  stream << std::fixed << "Page number: " << page_number << "\n"
         << "Orientation in degrees: " << orient_deg << "\n"
         << "Rotate: " << rotate << "\n"
         << "Orientation confidence: " << orient_conf << "\n"
         << "Script: " << script_name << "\n"
         << "Script confidence: " << script_conf << "\n";
  return copy_string(stream.str());
}

#endif // !DISABLED_LEGACY_ENGINE

/** Returns the average word confidence for Tesseract page result. */
int TessBaseAPI::MeanTextConf() {
  int *conf = AllWordConfidences();
  if (!conf) {
    return 0;
  }
  int sum = 0;
  int *pt = conf;
  while (*pt >= 0) {
    sum += *pt++;
  }
  if (pt != conf) {
    sum /= pt - conf;
  }
  delete[] conf;
  return sum;
}

/** Returns an array of all word confidences, terminated by -1. */
int *TessBaseAPI::AllWordConfidences() {
  if (tesseract_ == nullptr || (!recognition_done_ && Recognize(nullptr) < 0)) {
    return nullptr;
  }
  int n_word = 0;
  PAGE_RES_IT res_it(page_res_);
  for (res_it.restart_page(); res_it.word() != nullptr; res_it.forward()) {
    n_word++;
  }

  int *conf = new int[n_word + 1];
  n_word = 0;
  for (res_it.restart_page(); res_it.word() != nullptr; res_it.forward()) {
    WERD_RES *word = res_it.word();
    WERD_CHOICE *choice = word->best_choice;
    int w_conf = static_cast<int>(100 + 5 * choice->certainty());
    // This is the eq for converting Tesseract confidence to 1..100
    if (w_conf < 0) {
      w_conf = 0;
    }
    if (w_conf > 100) {
      w_conf = 100;
    }
    conf[n_word++] = w_conf;
  }
  conf[n_word] = -1;
  return conf;
}

#if !DISABLED_LEGACY_ENGINE
/**
 * Applies the given word to the adaptive classifier if possible.
 * The word must be SPACE-DELIMITED UTF-8 - l i k e t h i s , so it can
 * tell the boundaries of the graphemes.
 * Assumes that SetImage/SetRectangle have been used to set the image
 * to the given word. The mode arg should be PSM_SINGLE_WORD or
 * PSM_CIRCLE_WORD, as that will be used to control layout analysis.
 * The currently set PageSegMode is preserved.
 * Returns false if adaption was not possible for some reason.
 */
bool TessBaseAPI::AdaptToWordStr(PageSegMode mode, const char *wordstr) {
  bool success = true;
  Tesseract& tess = tesseract();
  PageSegMode current_psm = GetPageSegMode();
  SetPageSegMode(mode);

  tess.classify_enable_learning = false;

  const std::unique_ptr<const char[]> text(GetUTF8Text());
  if (tess.applybox_debug) {
    tprintDebug("Trying to adapt \"{}\" to \"{}\"\n", text.get(), wordstr);
  }
  if (text != nullptr) {
    PAGE_RES_IT it(page_res_);
    WERD_RES *word_res = it.word();
    if (word_res != nullptr) {
      word_res->word->set_text(wordstr);
      // Check to see if text matches wordstr.
      int w = 0;
      int t;
      for (t = 0; text[t] != '\0'; ++t) {
        if (text[t] == '\n' || text[t] == ' ') {
          continue;
        }
        while (wordstr[w] == ' ') {
          ++w;
        }
        if (text[t] != wordstr[w]) {
          break;
        }
        ++w;
      }
      if (text[t] != '\0' || wordstr[w] != '\0') {
        // No match.
        delete page_res_;
        std::vector<TBOX> boxes;
        page_res_ = tess.SetupApplyBoxes(boxes, block_list_);
        tess.ReSegmentByClassification(page_res_);
        tess.TidyUp(page_res_);
        PAGE_RES_IT pr_it(page_res_);
        if (pr_it.word() == nullptr) {
          success = false;
        } else {
          word_res = pr_it.word();
        }
      } else {
        word_res->BestChoiceToCorrectText();
      }
      if (success) {
        tess.EnableLearning = true;
        tess.LearnWord(nullptr, word_res);
      }
    } else {
      success = false;
    }
  } else {
    success = false;
  }
  SetPageSegMode(current_psm);
  return success;
}
#endif // !DISABLED_LEGACY_ENGINE

/**
 * Free up recognition results and any stored image data, without actually
 * freeing any recognition data that would be time-consuming to reload.
 * Afterwards, you must call SetImage or TesseractRect before doing
 * any Recognize or Get* operation.
 */
void TessBaseAPI::Clear() {
  // TODO? write/flush log output / ReportDebugInfo() ?

  if (thresholder_ != nullptr) {
    thresholder_->Clear();
  }
  ClearResults();
  if (tesseract_ != nullptr) {
    SetInputImage(nullptr);
  }
}

/**
 * Close down tesseract and free up all memory. End() is equivalent to
 * destructing and reconstructing your TessBaseAPI.
 * Once End() has been used, none of the other API functions may be used
 * other than Init and anything declared above it in the class definition.
 */
void TessBaseAPI::End() {
  ReportDebugInfo();

  Clear();
  delete thresholder_;
  thresholder_ = nullptr;
  delete page_res_;
  page_res_ = nullptr;
  delete block_list_;
  block_list_ = nullptr;
  if (paragraph_models_ != nullptr) {
    for (auto model : *paragraph_models_) {
      delete model;
    }
    delete paragraph_models_;
    paragraph_models_ = nullptr;
  }
#if !DISABLED_LEGACY_ENGINE
  if (osd_tesseract_ == tesseract_) {
    osd_tesseract_ = nullptr;
  }
  delete osd_tesseract_;
  osd_tesseract_ = nullptr;
  delete equ_detect_;
  equ_detect_ = nullptr;
#endif // !DISABLED_LEGACY_ENGINE

  delete tesseract_;
  tesseract_ = nullptr;
  pixDestroy(&pix_visible_image_);
  pix_visible_image_ = nullptr;
  visible_image_file_.clear();
  output_file_.clear();
  datapath_.clear();
  language_.clear();
}

// Clear any library-level memory caches.
// There are a variety of expensive-to-load constant data structures (mostly
// language dictionaries) that are cached globally -- surviving the Init()
// and End() of individual TessBaseAPI's.  This function allows the clearing
// of these caches.
void TessBaseAPI::ClearPersistentCache() {
#if 0
  Dict::GlobalDawgCache()->DeleteUnusedDawgs();
#else
  Dict::CleanGlobalDawgCache();
#endif
}

/**
 * Check whether a word is valid according to Tesseract's language model
 * returns 0 if the word is invalid, non-zero if valid
 */
int TessBaseAPI::IsValidWord(const char *word) const {
  return tesseract().getDict().valid_word(word);
}
// Returns true if utf8_character is defined in the UniCharset.
bool TessBaseAPI::IsValidCharacter(const char *utf8_character) const {
  return tesseract().unicharset_.contains_unichar(utf8_character);
}

// TODO(rays) Obsolete this function and replace with a more aptly named
// function that returns image coordinates rather than tesseract coordinates.
bool TessBaseAPI::GetTextDirection(int *out_offset, float *out_slope) {
  const std::unique_ptr<const PageIterator> it(AnalyseLayout());
  if (it == nullptr) {
    return false;
  }
  int x1, x2, y1, y2;
  it->Baseline(RIL_TEXTLINE, &x1, &y1, &x2, &y2);
  // Calculate offset and slope (NOTE: Kind of ugly)
  if (x2 <= x1) {
    x2 = x1 + 1;
  }
  // Convert the point pair to slope/offset of the baseline (in image coords.)
  *out_slope = static_cast<float>(y2 - y1) / (x2 - x1);
  *out_offset = static_cast<int>(y1 - *out_slope * x1);
  // Get the y-coord of the baseline at the left and right edges of the
  // textline's bounding box.
  int left, top, right, bottom;
  if (!it->BoundingBox(RIL_TEXTLINE, &left, &top, &right, &bottom)) {
    return false;
  }
  int left_y = IntCastRounded(*out_slope * left + *out_offset);
  int right_y = IntCastRounded(*out_slope * right + *out_offset);
  // Shift the baseline down so it passes through the nearest bottom-corner
  // of the textline's bounding box. This is the difference between the y
  // at the lowest (max) edge of the box and the actual box bottom.
  *out_offset += bottom - std::max(left_y, right_y);
  // Switch back to bottom-up tesseract coordinates. Requires negation of
  // the slope and height - offset for the offset.
  *out_slope = -*out_slope;
  *out_offset = rect_height_ - *out_offset;

  return true;
}

/** Sets Dict::letter_is_okay_ function to point to the given function. */
void TessBaseAPI::SetDictFunc(DictFunc f) {
  if (tesseract_ != nullptr) {
    tesseract().getDict().letter_is_okay_ = f;
  }
}

/**
 * Sets Dict::probability_in_context_ function to point to the given
 * function.
 *
 * @param f A single function that returns the probability of the current
 * "character" (in general a utf-8 string), given the context of a previous
 * utf-8 string.
 */
void TessBaseAPI::SetProbabilityInContextFunc(ProbabilityInContextFunc f) {
  if (tesseract_ != nullptr) {
    Tesseract& tess = tesseract();
    tess.getDict().probability_in_context_ = f;
    // Set it for the sublangs too.
    int num_subs = tess.num_sub_langs();
    for (int i = 0; i < num_subs; ++i) {
      tess.get_sub_lang(i)->getDict().probability_in_context_ = f;
    }
  }
}

/** Common code for setting the image. */
bool TessBaseAPI::InternalResetImage() {
  if (tesseract_ == nullptr) {
    tprintError("Please call Init before attempting to set an image.\n");
    return false;
  }
  if (thresholder_ != nullptr) {
    thresholder_->Clear();
  }
  if (thresholder_ == nullptr) {
    thresholder_ = new ImageThresholder(tesseract_);
  }
  ClearResults();
  return true;
}

/**
 * Run the thresholder to make the thresholded image, returned in pix,
 * which must not be nullptr. *pix must be initialized to nullptr, or point
 * to an existing pixDestroyable Pix.
 * The usual argument to Threshold is Tesseract::mutable_pix_binary().
 */
bool TessBaseAPI::Threshold(Pix **pix) {
  Tesseract& tess = tesseract();
  ASSERT_HOST(pix != nullptr);
  if (*pix != nullptr) {
    pixDestroy(pix);
  }
  // Zero resolution messes up the algorithms, so make sure it is credible.
  int user_dpi = tess.user_defined_dpi;
  int y_res = thresholder_->GetScaledYResolution();
  if (user_dpi && (user_dpi < kMinCredibleResolution || user_dpi > kMaxCredibleResolution)) {
    tprintWarn("User defined image dpi is outside of expected range "
        "({} - {})!\n",
        kMinCredibleResolution, kMaxCredibleResolution);
  }
  // Always use user defined dpi
  if (user_dpi) {
    thresholder_->SetSourceYResolution(user_dpi);
  } else if (y_res < kMinCredibleResolution || y_res > kMaxCredibleResolution) {
    if (y_res != 0) {
      // Show warning only if a resolution was given.
      tprintWarn("Invalid resolution {} dpi. Using {} instead.\n",
              y_res, kMinCredibleResolution);
    }
    thresholder_->SetSourceYResolution(kMinCredibleResolution);
  }

  auto selected_thresholding_method = static_cast<ThresholdMethod>(tess.thresholding_method.value());
  auto thresholding_method = selected_thresholding_method;

  AutoPopDebugSectionLevel subsec_handle(tesseract_, tess.PushSubordinatePixDebugSection(tess.showcase_threshold_methods ? "Showcase threshold methods..." : fmt::format("Applying the threshold method chosen for this run: {}", selected_thresholding_method)));

  // debug_all/showcase_threshold_methods: assist diagnostics by cycling through all thresholding methods and applying each,
  // saving each result to a separate diagnostic image for later evaluation, before commencing
  // and finally applying the *user-selected* threshold method and continue with the OCR process:
  for (int m = 0; m <= (int)ThresholdMethod::Max; m++)
  {
    bool go = false;

    if (m != (int)ThresholdMethod::Max)
    {
      if (!tess.showcase_threshold_methods) {
      m = (int)ThresholdMethod::Max - 1;    // jump to the last round of the loop: we need only one round through here.
        continue;
    }

      thresholding_method = (ThresholdMethod)m;
    }
    else
    {
      if (tess.showcase_threshold_methods) {
        tess.PushNextPixDebugSection(fmt::format("Applying the threshold method chosen for this run: {}", selected_thresholding_method));
      }

      // on last round, we reset to the selected threshold method
      thresholding_method = selected_thresholding_method;
      go = true;
    }

    {
      auto [ok, pix_grey, pix_binary, pix_thresholds] = thresholder_->Threshold(thresholding_method);

      if (!ok) {
        return false;
      }

      if (go)
        *pix = pix_binary;

      tess.set_pix_thresholds(pix_thresholds);
      tess.set_pix_grey(pix_grey);

      std::string caption = ThresholdMethodName(thresholding_method);

      if (tess.tessedit_dump_pageseg_images || tess.showcase_threshold_methods || show_threshold_images) {
        tess.AddPixDebugPage(tess.pix_grey(), fmt::format("{} : Grey = pre-image", caption));
        tess.AddPixDebugPage(tess.pix_thresholds(), fmt::format("{} : Thresholds", caption));
        tess.AddPixDebugPage(pix_binary, fmt::format("{} : Binary = post-image", caption));

        const char *sequence = "c1.1 + d3.3";
        const int dispsep = 0;
        Image pix_post = pixMorphSequence(pix_binary, sequence, dispsep);
        tess.AddClippedPixDebugPage(pix_post, fmt::format("{} : post-processed: {}", caption, sequence));
        pix_post.destroy();
      }

      if (!go)
        pix_binary.destroy();
    }
  }

  thresholder_->GetImageSizes(&rect_left_, &rect_top_, &rect_width_, &rect_height_, &image_width_,
                              &image_height_);

  // Set the internal resolution that is used for layout parameters from the
  // estimated resolution, rather than the image resolution, which may be
  // fabricated, but we will use the image resolution, if there is one, to
  // report output point sizes.
  int estimated_res = ClipToRange(thresholder_->GetScaledEstimatedResolution(),
                                  kMinCredibleResolution, kMaxCredibleResolution);
  if (estimated_res != thresholder_->GetScaledEstimatedResolution()) {
    tprintWarn("Estimated internal resolution {} out of range! "
        "Corrected to {}.\n",
        thresholder_->GetScaledEstimatedResolution(), estimated_res);
  }
  tess.set_source_resolution(estimated_res);
  return true;
}

/** Find lines from the image making the BLOCK_LIST. */
int TessBaseAPI::FindLines() {
  if (thresholder_ == nullptr || thresholder_->IsEmpty()) {
    tprintError("Please call SetImage before attempting recognition.\n");
    return -1;
  }
  if (recognition_done_) {
    ClearResults();
  }
  if (!block_list_->empty()) {
    return 0;
  }
  Tesseract& tess = tesseract();
#if !DISABLED_LEGACY_ENGINE
  tess.InitAdaptiveClassifier(nullptr);
#endif

  if (tess.pix_binary() == nullptr) {
    if (verbose_process) {
      tprintInfo("PROCESS: source image is not a binary image, hence we apply a thresholding algo/subprocess to obtain a binarized image.\n");
    }

    Image pix = Image();
    if (!Threshold(&pix.pix_)) {
      return -1;
    }
    tess.set_pix_binary(pix);
  }

  if (tess.tessedit_dump_pageseg_images) {
    tess.AddPixDebugPage(tess.pix_binary(), "FindLines :: Thresholded Image -> this image is now set as the page Master Source Image");
  }

  if (verbose_process) {
    tprintInfo("PROCESS: prepare the image for page segmentation, i.e. discovery of all text areas + bounding boxes & image/text orientation and script{} detection.\n",
#if !DISABLED_LEGACY_ENGINE
               (tess.textord_equation_detect ? " + equations" : "")
#else
               ""
#endif
    );
  }

  AutoPopDebugSectionLevel section_handle(tesseract_, tess.PushSubordinatePixDebugSection("Prepare for Page Segmentation"));

  tess.PrepareForPageseg();

#if !DISABLED_LEGACY_ENGINE
  if (tess.textord_equation_detect) {
    if (equ_detect_ == nullptr && !datapath_.empty()) {
      equ_detect_ = new EquationDetect(datapath_.c_str(), nullptr);
    }
    if (equ_detect_ == nullptr) {
      tprintWarn("Could not set equation detector\n");
    } else {
      tess.SetEquationDetect(equ_detect_);
    }
  }
#endif // !DISABLED_LEGACY_ENGINE

#if !DISABLED_LEGACY_ENGINE
  Tesseract *osd_tess = osd_tesseract_;
#else
  Tesseract *osd_tess = nullptr;
#endif
  OSResults osr;
#if !DISABLED_LEGACY_ENGINE
  if (PSM_OSD_ENABLED(tess.tessedit_pageseg_mode) && osd_tess == nullptr) {
    if (strcmp(language_.c_str(), "osd") == 0) {
      osd_tess = tesseract_;
    } else {
      osd_tesseract_ = new Tesseract(tesseract_);
      TessdataManager mgr(reader_);
      std::vector<std::string> nil;
      if (datapath_.empty()) {
        tprintWarn("Auto orientation and script detection requested,"
            " but data path is undefined\n");
        delete osd_tesseract_;
        osd_tesseract_ = nullptr;
      } else if (osd_tesseract_->init_tesseract(datapath_, "osd", OEM_TESSERACT_ONLY, &mgr) == 0) {
        osd_tesseract_->set_source_resolution(thresholder_->GetSourceYResolution());
      } else {
        tprintWarn(
            "Auto orientation and script detection requested,"
            " but osd language failed to load\n");
        delete osd_tesseract_;
        osd_tesseract_ = nullptr;
      }
    }
  }
#endif // !DISABLED_LEGACY_ENGINE

  if (tess.SegmentPage(tess.input_file_path_.c_str(), block_list_, osd_tess, &osr) < 0) {
    return -1;
  }

  // If Devanagari is being recognized, we use different images for page seg
  // and for OCR.
  tess.PrepareForTessOCR(block_list_, &osr);

  return 0;
}

/**
 * Return average gradient of lines on page.
 */
float TessBaseAPI::GetGradient() {
  return tesseract().gradient();
}

/** Delete the pageres and clear the block list ready for a new page. */
void TessBaseAPI::ClearResults() {
  if (tesseract_ != nullptr) {
    tesseract_->Clear();
  }
  if (osd_tesseract_ != tesseract_ && osd_tesseract_ != nullptr) {
    osd_tesseract_->Clear();
  }
  delete page_res_;
  page_res_ = nullptr;
  recognition_done_ = false;
  if (block_list_ == nullptr) {
    block_list_ = new BLOCK_LIST;
  } else {
    block_list_->clear();
  }
  if (paragraph_models_ != nullptr) {
    for (auto model : *paragraph_models_) {
      delete model;
    }
    delete paragraph_models_;
    paragraph_models_ = nullptr;
  }

  uniqueInstance<std::vector<TessTable>>().clear();
}

/**
 * Return the length of the output text string, as UTF8, assuming
 * liberally two spacing marks after each word (as paragraphs end with two
 * newlines), and assuming a single character reject marker for each rejected
 * character.
 * Also return the number of recognized blobs in blob_count.
 */
int TessBaseAPI::TextLength(int *blob_count) const {
  if (tesseract_ == nullptr || page_res_ == nullptr) {
    return 0;
  }

  PAGE_RES_IT page_res_it(page_res_);
  int total_length = 2;
  int total_blobs = 0;
  // Iterate over the data structures to extract the recognition result.
  for (page_res_it.restart_page(); page_res_it.word() != nullptr; page_res_it.forward()) {
    WERD_RES *word = page_res_it.word();
    WERD_CHOICE *choice = word->best_choice;
    if (choice != nullptr) {
      total_blobs += choice->length() + 2;
      total_length += choice->unichar_string().length() + 2;
      for (int i = 0; i < word->reject_map.length(); ++i) {
        if (word->reject_map[i].rejected()) {
          ++total_length;
        }
      }
    }
  }
  if (blob_count != nullptr) {
    *blob_count = total_blobs;
  }
  return total_length;
}

#if !DISABLED_LEGACY_ENGINE
/**
 * Estimates the Orientation And Script of the image.
 * Returns true if the image was processed successfully.
 */
bool TessBaseAPI::DetectOS(OSResults *osr) {
  if (tesseract_ == nullptr) {
    return false;
  }
  ClearResults();
  Tesseract& tess = tesseract();
  if (tess.pix_binary() == nullptr) {
    Image pix = Image();
    if (!Threshold(&pix.pix_)) {
      return false;
    }
    tess.set_pix_binary(pix);

    tess.AddPixDebugPage(tess.pix_binary(), "DetectOS : Thresholded Image");
  }

  return tess.orientation_and_script_detection(tess.input_file_path_.c_str(), osr) > 0;
}
#endif // !DISABLED_LEGACY_ENGINE

void TessBaseAPI::set_min_orientation_margin(double margin) {
  tesseract().min_orientation_margin.set_value(margin);
}

/**
 * Return text orientation of each block as determined in an earlier page layout
 * analysis operation. Orientation is returned as the number of ccw 90-degree
 * rotations (in [0..3]) required to make the text in the block upright
 * (readable). Note that this may not necessary be the block orientation
 * preferred for recognition (such as the case of vertical CJK text).
 *
 * Also returns whether the text in the block is believed to have vertical
 * writing direction (when in an upright page orientation).
 *
 * The returned array is of length equal to the number of text blocks, which may
 * be less than the total number of blocks. The ordering is intended to be
 * consistent with GetTextLines().
 */
void TessBaseAPI::GetBlockTextOrientations(int **block_orientation, bool **vertical_writing) {
  delete[] * block_orientation;
  *block_orientation = nullptr;
  delete[] * vertical_writing;
  *vertical_writing = nullptr;
  BLOCK_IT block_it(block_list_);

  block_it.move_to_first();
  int num_blocks = 0;
  for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
    if (!block_it.data()->pdblk.poly_block()->IsText()) {
      continue;
    }
    ++num_blocks;
  }
  if (!num_blocks) {
    tprintWarn("Found no blocks\n");
    return;
  }
  *block_orientation = new int[num_blocks];
  *vertical_writing = new bool[num_blocks];
  block_it.move_to_first();
  int i = 0;
  for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
    if (!block_it.data()->pdblk.poly_block()->IsText()) {
      continue;
    }
    FCOORD re_rotation = block_it.data()->re_rotation();
    float re_theta = re_rotation.angle();
    FCOORD classify_rotation = block_it.data()->classify_rotation();
    float classify_theta = classify_rotation.angle();
    double rot_theta = -(re_theta - classify_theta) * 2.0 / M_PI;
    if (rot_theta < 0) {
      rot_theta += 4;
    }
    int num_rotations = static_cast<int>(rot_theta + 0.5);
    (*block_orientation)[i] = num_rotations;
    // The classify_rotation is non-zero only if the text has vertical
    // writing direction.
    (*vertical_writing)[i] = (classify_rotation.y() != 0.0f);
    ++i;
  }
}

void TessBaseAPI::DetectParagraphs(bool after_text_recognition) {
  Tesseract& tess = tesseract();
  if (paragraph_models_ == nullptr) {
    paragraph_models_ = new std::vector<ParagraphModel*>;
  }
  MutableIterator *result_it = GetMutableIterator();
  do { // Detect paragraphs for this block
    std::vector<ParagraphModel *> models;
    tess.DetectParagraphs(after_text_recognition, result_it, &models);
    paragraph_models_->insert(paragraph_models_->end(), models.begin(), models.end());
  } while (result_it->Next(RIL_BLOCK));
  delete result_it;
}

/** This method returns the string form of the specified unichar. */
const char *TessBaseAPI::GetUnichar(int unichar_id) const {
  return tesseract().unicharset_.id_to_unichar(unichar_id);
}

/** Return the pointer to the i-th dawg loaded into tesseract_ object. */
const Dawg *TessBaseAPI::GetDawg(int i) const {
  if (tesseract_ == nullptr || i >= NumDawgs()) {
    return nullptr;
  }
  return tesseract().getDict().GetDawg(i);
}

/** Return the number of dawgs loaded into tesseract_ object. */
int TessBaseAPI::NumDawgs() const {
  return tesseract_ == nullptr ? 0 : tesseract().getDict().NumDawgs();
}


void TessBaseAPI::ReportDebugInfo() {
  if (tesseract_ == nullptr) {
    return;
  }
  tesseract().ReportDebugInfo();
}

void TessBaseAPI::SetupDebugAllPreset() {
  Tesseract& tess = tesseract();
  Textord &textord = *tess.mutable_textord();
  
  const ParamSetBySourceType SRC = PARAM_VALUE_IS_SET_BY_PRESET;

  verbose_process.set_value(true, SRC);
  
#if !GRAPHICS_DISABLED
  scrollview_support.set_value(true, SRC);
#endif

  textord_tabfind_show_images.set_value(true, SRC);
  // textord_tabfind_show_vlines.set_value(true, SRC);

#if !GRAPHICS_DISABLED
  textord_tabfind_show_initial_partitions.set_value(true, SRC);
  textord_tabfind_show_reject_blobs.set_value(true, SRC);
  textord_tabfind_show_partitions.set_value(2, SRC);
  textord_tabfind_show_columns.set_value(true, SRC);
  textord_tabfind_show_blocks.set_value(true, SRC);
#endif

  textord.textord_noise_debug.set_value(true, SRC);
  textord_oldbl_debug.set_value(false, SRC); // turned OFF, for 'true' produces very noisy output
  textord.textord_baseline_debug.set_value(true, SRC);
  textord_debug_block.set_value(9, SRC);
  textord_debug_bugs.set_value(9, SRC);
  textord_debug_tabfind.set_value(1 /* 9 */, SRC); // '9' produces very noisy output

  textord_debug_baselines.set_value(true, SRC);
  textord_debug_blob.set_value(true, SRC);
  textord_debug_pitch_metric.set_value(true, SRC);
  textord_debug_fixed_pitch_test.set_value(true, SRC);
  textord_debug_pitch.set_value(true, SRC);
  textord_debug_printable.set_value(true, SRC);
  textord_debug_xheights.set_value(true, SRC);
  textord_debug_xheights.set_value(true, SRC);

  textord_show_initial_words.set_value(true, SRC);
  textord_blocksall_fixed.set_value(true, SRC);
  textord_blocksall_prop.set_value(true, SRC);

  tess.tessedit_create_hocr.set_value(true, SRC);
  tess.tessedit_create_alto.set_value(true, SRC);
  tess.tessedit_create_page_xml.set_value(true, SRC);
  tess.tessedit_create_tsv.set_value(true, SRC);
  tess.tessedit_create_pdf.set_value(true, SRC);
  tess.textonly_pdf.set_value(false, SRC); // turned OFF
  tess.tessedit_write_unlv.set_value(true, SRC);
  tess.tessedit_create_lstmbox.set_value(true, SRC);
  tess.tessedit_create_boxfile.set_value(true, SRC);
  tess.tessedit_create_wordstrbox.set_value(true, SRC);
  tess.tessedit_create_txt.set_value(true, SRC);

  tess.tessedit_dump_choices.set_value(true, SRC);
  tess.tessedit_dump_pageseg_images.set_value(true, SRC);

  tess.tessedit_write_images.set_value(true, SRC);

  tess.tessedit_adaption_debug.set_value(true, SRC);
  tess.tessedit_debug_block_rejection.set_value(true, SRC);
  tess.tessedit_debug_doc_rejection.set_value(true, SRC);
  tess.tessedit_debug_fonts.set_value(true, SRC);
  tess.tessedit_debug_quality_metrics.set_value(true, SRC);

  tess.tessedit_rejection_debug.set_value(true, SRC);
  tess.tessedit_timing_debug.set_value(true, SRC);

  tess.tessedit_bigram_debug.set_value(true, SRC);

  tess.tess_debug_lstm.set_value(debug_all >= 1 ? 1 : 0, SRC); // LSTM debug output is extremely noisy

  tess.debug_noise_removal.set_value(true, SRC);

  tess.classify_debug_level.set_value(debug_all.value(), SRC); // LSTM debug output is extremely noisy
  tess.classify_learning_debug_level.set_value(9, SRC);
  tess.classify_debug_character_fragments.set_value(true, SRC);
  tess.classify_enable_adaptive_debugger.set_value(true, SRC);
  // tess.classify_learn_debug_str.set_value("????????????????", SRC);
  tess.matcher_debug_separate_windows.set_value(true, SRC);
  tess.matcher_debug_flags.set_value(true, SRC);
  tess.matcher_debug_level.set_value(3, SRC);

  tess.multilang_debug_level.set_value(3, SRC);

  tess.paragraph_debug_level.set_value(3, SRC);

  tess.segsearch_debug_level.set_value(3, SRC);

  // TODO: synchronize the settings of all Dict instances during Dict object creation and after any change...

  Dict &dict = tess.getInitialDict();
  dict.stopper_debug_level.set_value(3, SRC);

  tess.superscript_debug.set_value(true, SRC);

  tess.crunch_debug.set_value(true, SRC);

  dict.dawg_debug_level.set_value(1, SRC); // noisy

  tess.debug_fix_space_level.set_value(9, SRC);
  tess.debug_x_ht_level.set_value(3, SRC);
  // tess.debug_file.set_value("xxxxxxxxxxxxxxxxx", SRC);
  // tess.debug_output_path.set_Value("xxxxxxxxxxxxxx", SRC);
  debug_misc.set_value(true, SRC);

  dict.hyphen_debug_level.set_value(3, SRC);

  LanguageModelSettings &langmodel = tess.getLanguageModelSettings();

  langmodel.language_model_debug_level.set_value(0, SRC); /* 7 */

  textord.tosp_debug_level.set_value(3, SRC);

  tess.wordrec_debug_level.set_value(3, SRC);

  dict.word_to_debug.set_value(true, SRC);

  tess.scribe_save_grey_rotated_image.set_value(true, SRC);
  tess.scribe_save_binary_rotated_image.set_value(true, SRC);
  tess.scribe_save_original_rotated_image.set_value(true, SRC);

  tess.hocr_font_info.set_value(true, SRC);
  tess.hocr_char_boxes.set_value(true, SRC);
  tess.hocr_images.set_value(true, SRC);

  tess.thresholding_debug.set_value(true, SRC);

  tess.preprocess_graynorm_mode.set_value(0, SRC); // 0..3

  tess.tessedit_bigram_debug.set_value(true, SRC);

  tess.wordrec_debug_blamer.set_value(true, SRC);

  devanagari_split_debugimage.set_value(true, SRC);
  devanagari_split_debuglevel.set_value(3, SRC);

  gapmap_debug.set_value(true, SRC);

  poly_debug.set_value(false, SRC); // turned OFF: 'othwerwise 'true' produces very noisy output

  edges_debug.set_value(true, SRC);

  tess.ambigs_debug_level.set_value(3, SRC);

  tess.applybox_debug.set_value(true, SRC);

  tess.bidi_debug.set_value(true, SRC);

  tess.chop_debug.set_value(true, SRC);

  tess.debug_baseline_fit.set_value(1, SRC); // 0..3
  tess.debug_baseline_y_coord.set_value(-2000, SRC);

  tess.showcase_threshold_methods.set_value((debug_all > 2), SRC);

  tess.debug_write_unlv.set_value(true, SRC);
  tess.debug_line_finding.set_value(true, SRC);
  tess.debug_image_normalization.set_value(true, SRC);
  tess.debug_do_not_use_scrollview_app.set_value(true, SRC);

  tess.interactive_display_mode.set_value(true, SRC);

  tess.debug_display_page.set_value(true, SRC);
  tess.debug_display_page_blocks.set_value(true, SRC);
  tess.debug_display_page_baselines.set_value(true, SRC);

  tess.ResyncVariablesInternally();
}

void TessBaseAPI::SetupDefaultPreset() {
  Tesseract &tess = tesseract();
  const ParamSetBySourceType SRC = PARAM_VALUE_IS_SET_BY_PRESET;

  // default: TXT + HOCR renderer     ... plus all the rest of 'em   [GHo patch]
  tess.tessedit_create_hocr.set_value(true, SRC);
  tess.tessedit_create_alto.set_value(true, SRC);
  tess.tessedit_create_page_xml.set_value(true, SRC);
  tess.tessedit_create_tsv.set_value(true, SRC);
  tess.tessedit_create_pdf.set_value(true, SRC);
  tess.textonly_pdf.set_value(false, SRC);         // turned OFF
  tess.tessedit_write_unlv.set_value(true, SRC);
  tess.tessedit_create_lstmbox.set_value(true, SRC);
  tess.tessedit_create_boxfile.set_value(true, SRC);
  tess.tessedit_create_wordstrbox.set_value(true, SRC);
  tess.tessedit_create_txt.set_value(true, SRC);

  tess.ResyncVariablesInternally();
}


// sanity check for the imagelist expander below: any CONTROL characters in here signal binary data and thus NOT AN IMAGELIST format.
static inline bool is_sane_imagelist_line(const char *p) {
  while (*p) {
    uint8_t c = *p++;
    if (c < ' ' && c != '\t')
      return false;
  }
  return true;
}

#if defined(_MSC_VER) && !defined(strtok_r)
static inline char *strtok_r(char * s, const char * sep, char ** state) {
  return strtok_s(s, sep, state);
}
#endif

static void destroy_il_buffer(char *buf) {
  free(buf);
}

std::vector<ImagePageFileSpec> TessBaseAPI::ExpandImagelistFilesInSet(const std::vector<std::string>& paths) {
  std::vector<ImagePageFileSpec> rv;
  std::ostringstream errmsg;

  for (auto spec : paths) {
    // each item in the list must exist?
    if (!fs::exists(spec)) {
      errmsg << "Specified file does not exist. Path: " << spec << "\n";
      goto continue_on_error;
      //continue;
    }

    {
      const size_t SAMPLESIZE = 8192;

      // load the first ~8K and see if that chunk contains a decent set of file paths: is so, the heuristic says it's an imagelist, rather than an image file.
      char scratch[SAMPLESIZE + 2];
      ConfigFile f(spec); // not a problem that this one opens the file in "r" (CRLF conversion) mode: we're after text files and the others will quickly be discovered below.
      if (!f) {
        errmsg << "Cannot open/access specified file";
        int ecode = errno;
        if (ecode != E_OK) {
          errmsg << " due to error: " << strerror(errno);
        }
        errmsg << ". Path: " << spec << "\n";
        goto continue_on_error;
        // continue;
      }
      auto l = fread(scratch, 1, SAMPLESIZE, f());
      // when it's an imagelist, it MAY be smaller than our scratch buffer!
      if (l == 0 || ferror(f())) {
        errmsg << "Failed to read a first chunk of the specified file";
        int ecode = errno;
        if (ecode != E_OK) {
          errmsg << " due to error: " << strerror(errno);
        }
        errmsg << ". Tried to read " << SAMPLESIZE << " bytes, received " << l << " bytes.  Path: " << spec << "\n";
        goto continue_on_error;
        // continue;
      }
      // make sure the sampled chunk is terminated before we go and parse it as a imagelist file (which may be damaged at the end as we sampled only the start of it!)
      scratch[l] = 0;
      scratch[l + 1] = 0;

      bool is_imagelist = true;
      std::vector<char *> lines;
      char *state = nullptr;
      char *s = strtok_r(scratch, "\r\n", &state);
      while (s) {
        char *p = s + strspn(s, " \t");

        // sanity check: any CONTROL characters in here signal binary data and thus NOT AN IMAGELIST format.
        if (!is_sane_imagelist_line(p)) {
          is_imagelist = false;
          break;
        }

        // skip comment lines and empty lines:
        if (!strchr("#;", *p)) {
          lines.push_back(s);
        }

        s = strtok_r(nullptr, "\r\n", &state);
      }
      // do we have a potentially sane imagelist? Do we need to truncate the damaged end, if it is?
      if (l == SAMPLESIZE && is_imagelist && lines.size() >= 1) {
        // the last line will be damaged due to our sampling, so we better discard that one:
        (void)lines.pop_back();
      }

      if (is_imagelist) {
        int error_count = 0;
        int sample_count = 0;
        // validate the lines in the sample:
        for (auto spec : lines) {
          // parse and chop into 1..3 file paths: image;mask;overlay
          state = nullptr;
          int count = 0;
          char *s = strtok_r(spec, ";", &state);
          while (s) {
            count++;
            char *p = s + strspn(s, " \t");

            // trim whitespace at the end...
            char *e = p + strlen(p);
            while (e > p) {
              if (isspace(p[-1])) {
                *p-- = 0;
                continue;
              }
              break;
            }

            sample_count++;
            if (!fs::exists(p)) {
              error_count++;
            }

            s = strtok_r(nullptr, ";", &state);
          }
          if (count < 1 || count > 3) {
            error_count++;
          }
        }

        // we tolerate about 1-in-10 file errors here...
        float err_ratio = error_count * 100.0f / sample_count;
        is_imagelist = (err_ratio < 10.0 /* percent */);
      }

      if (is_imagelist) {
        // now that we know the sample is a sensible imagelist, grab the entire thing and parse it entirely...
        const size_t listfilesize = fs::file_size(spec);

        std::unique_ptr<char, void (*)(char *)> buffer((char *)malloc(listfilesize + 2), destroy_il_buffer);
        if (!buffer) {
          // TODO
          continue;
        }

        // rewind file
        fseek(f(), 0, SEEK_SET);
        l = fread(buffer.get(), 1, listfilesize, f());
        if (l != listfilesize || ferror(f())) {
          // TODO
          continue;
        }
        // make sure the sampled chunk is terminated before we go and parse it as a imagelist file (which may be damaged at the end as we sampled only the start of it!)
        char *b = buffer.get();
        b[l] = 0;
        b[l + 1] = 0;

        std::vector<char *> lines;
        char *state = nullptr;
        char *s = strtok_r(buffer.get(), "\r\n", &state);
        while (s) {
          char *p = s + strspn(s, " \t");

          // sanity check: any CONTROL characters in here signal binary data and thus NOT AN IMAGELIST format.
          if (!is_sane_imagelist_line(p)) {
            is_imagelist = false;
            break;
          }

          // skip comment lines and empty lines:
          if (!strchr("#;", *p)) {
            lines.push_back(s);
          }

          s = strtok_r(nullptr, "\r\n", &state);
        }

        // do we have a potentially sane imagelist? Do we need to truncate the damaged end, if it is?
        if (l == SAMPLESIZE && is_imagelist && lines.size() >= 1) {
          // the last line will be damaged due to our sampling, so we better discard that one:
          (void)lines.pop_back();
        }

        int error_count = 0;
        int sample_count = 0;
        // parse & validate the lines:
        for (auto spec : lines) {
          // parse and chop into 1..3 file paths: image;mask;overlay
          state = nullptr;
          std::vector<std::string> fspecs;
          char *s = strtok_r(spec, ";", &state);
          while (s) {
            char *p = s + strspn(s, " \t");

            // trim whitespace at the end...
            char *e = p + strlen(p);
            while (e > p) {
              if (isspace(p[-1])) {
                *p-- = 0;
                continue;
              }
              break;
            }

            sample_count++;
            if (!fs::exists(p)) {
              error_count++;
            }

            fspecs.push_back(p);

            s = strtok_r(nullptr, ";", &state);
          }
          if (fspecs.size() < 1 || fspecs.size() > 3) {
            error_count++;
          } else {
            ImagePageFileSpec sp = {fspecs[0]};
            if (fspecs.size() > 1) {
              sp.segment_mask_image_path = fspecs[1];
            }
            if (fspecs.size() > 2) {
              sp.visible_page_image_path = fspecs[2];
            }
            rv.push_back(sp);
          }
        }
      } else {
        // not an image list: pick this one up as a sole image file spec:
        goto continue_on_error;
      }
    }

    if (false) {
continue_on_error:
      // treat situation as simple as possible: `spec` is not an image list; pick this one up as a sole image file spec:
      ImagePageFileSpec sp = {spec};
      rv.push_back(sp);
    }
  }
  return rv;
}

/** Escape a char string - replace <>&"' with HTML codes. */
std::string HOcrEscape(const char *text) {
  std::string ret;
  const char *ptr;
  for (ptr = text; *ptr; ptr++) {
    switch (*ptr) {
      case '<':
        ret += "&lt;";
        break;
      case '>':
        ret += "&gt;";
        break;
      case '&':
        ret += "&amp;";
        break;
      case '"':
        ret += "&quot;";
        break;
      case '\'':
        ret += "&#39;";
        break;
      default:
        ret += *ptr;
    }
  }
  return ret;
}

std::string mkUniqueOutputFilePath(const char* basepath, int page_number, const char* label, const char* filename_extension)
{
  size_t pos = strcspn(basepath, ":\\/");
  const char* filename = basepath;
  const char* p = basepath + pos;
  while (*p)
  {
    filename = p + 1;
    pos = strcspn(filename, ":\\/");
    p = filename + pos;
  }
  size_t pathlen = filename - basepath;
  if (!*filename)
    filename = "tesseract";

  char ns[40] = { 0 };
  if (page_number != 0)
  {
    snprintf(ns, sizeof(ns), ".p%04d", page_number);
  }

  static int unique_seq_counter = 0;
  unique_seq_counter++;

  char nq[40] = { 0 };
  snprintf(nq, sizeof(nq), ".n%04d", unique_seq_counter);

  std::string f(basepath);
  f = f.substr(0, pathlen);
  f += filename;
  f += nq;
  if (label && *label)
  {
    f += ".";
    f += label;
  }
  if (*ns)
  {
    f += ns;
  }
  f += ".";
  f += filename_extension;

  // sanitize generated filename part:
  char* str = f.data();
  int slen = f.length();
  int dpos = pathlen;
  bool marker = false;
  for (int spos = pathlen; spos < slen; spos++)
  {
    int c = str[spos];
    switch (c)
    {
    case '.':
    case '-':
    case '_':
    case ' ':
      if (!marker)
      {
        marker = true;
        str[dpos++] = c;
      }
      // otherwise skip additional 'marker' characters in the filename
      break;

    default:
      marker = false;
      str[dpos++] = c;
      break;
    }
  }
  // no marker tolerated at end of filename:
  if (marker)
    dpos--;
  // fringe case: filename is *only markers*:
  if (dpos == pathlen)
    str[dpos++] = '_';

  f.resize(dpos);

  return std::move(f);
}

void WritePix(const std::string &file_path, Pix *pic, int file_type)
{
  tprintInfo("Saving image file {}\n", file_path);
#if defined(HAVE_MUPDF)
  fz_mkdir_for_file(fz_get_global_context(), file_path.c_str());
#endif
  if (pixWrite(file_path.c_str(), pic, file_type))
  {
    tprintError("Writing image file {} failed\n", file_path);
  }
}

} // namespace tesseract
