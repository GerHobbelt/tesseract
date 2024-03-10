///////////////////////////////////////////////////////////////////////
// File:        unicharset_extractor.cpp
// Description: Unicode character/ligature set extractor.
// Author:      Thomas Kielbus
//
// (C) Copyright 2006, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////

// Given a list of box files or text files on the command line, this program
// normalizes the text according to command-line options and generates
// a unicharset.

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h" // HAS_LIBICU
#endif

#include <cstdlib>
#include <filesystem>
#include "boxread.h"
#include "common/commandlineflags.h"
#include "common/commontraining.h"     // CheckSharedLibraryVersion
#include "unicharset/lang_model_helpers.h"
#include "unicharset/normstrngs.h"
#include "unicharset.h"
#include "unicharset/unicharset_training_utils.h"

#include "tesseract/capi_training_tools.h"


#if defined(HAS_LIBICU)

using namespace tesseract;

STRING_PARAM_FLAG(output_unicharset, "unicharset", "Output file path");
INT_PARAM_FLAG(norm_mode, 1,
                      "Normalization mode: 1=Combine graphemes, "
                      "2=Split graphemes, 3=Pure unicode");

namespace tesseract {

// Helper normalizes and segments the given strings according to norm_mode, and
// adds the segmented parts to unicharset.
static void AddStringsToUnicharset(const std::vector<std::string> &strings, int norm_mode,
                                   UNICHARSET *unicharset) {
  for (const auto &string : strings) {
    std::vector<std::string> normalized;
    if (NormalizeCleanAndSegmentUTF8(UnicodeNormMode::kNFC, OCRNorm::kNone,
                                     static_cast<GraphemeNormMode>(norm_mode),
                                     /*report_errors*/ true, string.c_str(), &normalized)) {
      for (const std::string &normed : normalized) {
        // normed is a UTF-8 encoded string
        if (normed.empty() || IsUTF8Whitespace(normed.c_str())) {
          continue;
        }
        unicharset->unichar_insert(normed.c_str());
      }
    } else {
      tprintError("Normalization failed for string '{}'\n", string);
    }
  }
}

static int Main(int argc, const char** argv) {
  UNICHARSET unicharset;
  // Load input files
  for (int arg = 1; arg < argc; ++arg) {
    std::filesystem::path filePath = argv[arg];
    std::string file_data = tesseract::ReadFile(argv[arg]);
    if (file_data.empty()) {
      continue;
    }
    std::vector<std::string> texts;
    if (filePath.extension() == ".box") {
      tprintDebug("Extracting unicharset from box file {}\n", argv[arg]);
      bool res = ReadMemBoxes(-1, /*skip_blanks*/ true, &file_data[0],
                   /*continue_on_failure*/ false, /*boxes*/ nullptr, &texts,
                   /*box_texts*/ nullptr, /*pages*/ nullptr);
      if (!res) {
        tprintError("Cannot read box data from '{}'\n", argv[arg]);
        return EXIT_FAILURE;
      }
    } else {
      tprintDebug("Extracting unicharset from plain text file {}\n", argv[arg]);
      texts.clear();
      texts = split(file_data, '\n');
    }
    AddStringsToUnicharset(texts, FLAGS_norm_mode, &unicharset);
  }
  SetupBasicProperties(/*report_errors*/ true, /*decompose*/ false, &unicharset);
  // Write unicharset file.
  if (unicharset.save_to_file(FLAGS_output_unicharset.c_str())) {
    tprintDebug("Wrote unicharset file {}\n", FLAGS_output_unicharset.value());
  } else {
    tprintError("Cannot save unicharset file {}\n", FLAGS_output_unicharset.value());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

} // namespace tesseract

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_unicharset_extractor_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  int rv = tesseract::ParseCommandLineFlags(
    "[--output_unicharset filename] [--norm_mode mode] box_or_text_file [...]\n"
    "\n"
    "Where mode means:\n"
    " 1=combine graphemes (use for Latin and other simple scripts)\n"
    " 2=split graphemes (use for Indic/Khmer/Myanmar)\n"
    " 3=pure unicode (use for Arabic/Hebrew/Thai/Tibetan)\n"
    "Reads box or plain text files to extract the unicharset.\n",
    &argc, &argv);
	if (rv >= 0)
		return rv;
  return tesseract::Main(argc, argv);
}

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_unicharset_extractor_main(int argc, const char** argv)
#endif
{
  fprintf(stderr, "unicharset_extractor tool not supported in this non-ICU / Unicode build.\n");
  return EXIT_FAILURE;
}

#endif
