// Copyright 2017 Google Inc. All Rights Reserved.
// Author: rays@google.com (Ray Smith)
// Purpose: Program to generate a traineddata file that can be used to train an
//          LSTM-based neural network model from a unicharset and an optional
//          set of wordlists. Eliminates the need to run
//          set_unicharset_properties, wordlist2dawg, some non-existent binary
//          to generate the recoder, and finally combine_tessdata.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <tesseract/preparation.h> // compiler config, etc.

#include "common/commandlineflags.h"
#include "common/commontraining.h" // CheckSharedLibraryVersion
#include "unicharset/lang_model_helpers.h"
#include <tesseract/tprintf.h>
#include "unicharset/unicharset_training_utils.h"

#include "tesseract/capi_training_tools.h"


#if defined(HAS_LIBICU)

using namespace tesseract;

STRING_VAR(model_input_unicharset, "",
                         "Filename with unicharset to complete and use in encoding");
STRING_VAR(model_script_dir, "", "Directory name for input script unicharsets");
STRING_VAR(model_words, "", "File listing words to use for the system dictionary");
STRING_VAR(model_puncs, "", "File listing punctuation patterns");
STRING_VAR(model_numbers, "", "File listing number patterns");
STRING_VAR(model_output_dir, "", "Root directory for output files");
STRING_VAR(model_version_str, "", "Version string to add to traineddata file");
STRING_VAR(model_lang, "", "Name of language being processed");
BOOL_VAR(model_lang_is_rtl, false, "True if lang being processed is written right-to-left");
BOOL_VAR(model_pass_through_recoder, false,
                       "If true, the recoder is a simple pass-through of the "
                       "unicharset. Otherwise, potentially a compression of it");

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_combine_lang_model_main(int argc, const char** argv)
#endif
{
  // Sets properties on the input unicharset file, and writes:
  //   rootdir/lang/lang.charset_size=ddd.txt
  //   rootdir/lang/lang.traineddata
  //   rootdir/lang/lang.unicharset
  // If the 3 word lists are provided, the dawgs are also added
  // to the traineddata file.
  // The output unicharset and charset_size files are just for
  // human readability.
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  int rv = tesseract::ParseCommandLineFlags(argv[0], &argc, &argv, true);
  if (rv >= 0)
	  return rv;

  // If these reads fail, we get a warning message and an empty list of words.
  std::vector<std::string> words = split(tesseract::ReadFile(model_words), '\n');
  std::vector<std::string> puncs = split(tesseract::ReadFile(model_puncs), '\n');
  std::vector<std::string> numbers = split(tesseract::ReadFile(model_numbers), '\n');
  // Load the input unicharset
  UNICHARSET unicharset;
  if (!unicharset.load_from_file(model_input_unicharset.c_str(), false)) {
    tprintError("Failed to load unicharset from {}\n", model_input_unicharset.c_str());
    return EXIT_FAILURE;
  }
  tprintDebug("Loaded unicharset of size {} from file {}\n", unicharset.size(),
          model_input_unicharset.c_str());

  // Set unichar properties
  tprintDebug("Setting unichar properties\n");
  tesseract::SetupBasicProperties(/*report_errors*/ true,
                                  /*decompose (NFD)*/ false, &unicharset);
  tprintDebug("Setting script properties\n");
  tesseract::SetScriptProperties(model_script_dir, &unicharset);
  // Combine everything into a traineddata file.
  return tesseract::CombineLangModel(unicharset, model_script_dir,
                                     model_version_str, model_output_dir,
                                     model_lang, model_pass_through_recoder, words, puncs,
                                     numbers, model_lang_is_rtl, /*reader*/ nullptr,
                                     /*writer*/ nullptr);
}

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_combine_lang_model_main(int argc, const char** argv)
#endif
{
  fprintf(stderr,
          "combine_lang_model tool not supported in this non-ICU / Unicode "
          "build.\n");
  return EXIT_FAILURE;
}

#endif
