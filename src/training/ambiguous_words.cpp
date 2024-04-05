///////////////////////////////////////////////////////////////////////
// File:        ambiguous_words.cpp
// Description: A program that takes a text file with a list of words as
//              input (one per line) and outputs a file with the words
//              that were found in the dictionary followed by the words
//              that are ambiguous to them.
// Author:      Rika Antonova
//
// (C) Copyright 2011, Google Inc.
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
//

#include "common/commontraining.h" // CheckSharedLibraryVersion
#include "dict.h"
#include "tesseractclass.h"

#include <tesseract/baseapi.h>
#include "helpers.h"
#include "tprintf.h"

#include "tesseract/capi_training_tools.h"


#if !DISABLED_LEGACY_ENGINE

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_ambiguous_words_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  // Parse input arguments.
  for (int err_round = 0;; err_round++) {
    int rv = tesseract::ParseCommandLineFlags("[-l lang] tessdata_dir wordlist_file output_ambiguous_wordlist_file", &argc, &argv);
    if (rv > 0)
      return rv;
    if (rv == 0)
      return err_round;

    if (argc < 3) {
      tesseract::tprintError("Not enough parameters specified on commandline.\n");
      argc = 1;
      continue;
    }
    int argv_offset = 0;
    std::string lang;
    if (0 == strcmp(argv[1], "-l")) {
      lang = argv[2];
      argv_offset = 2;
    }
    else {
      lang = "eng";
    }
    if (argv_offset + 3 >= argc) {
      tesseract::tprintError("Not enough parameters specified on commandline.\n");
      argc = 1;
      continue;
    }
    const char* tessdata_dir = argv[++argv_offset];
    const char* input_file_str = argv[++argv_offset];
    const char* output_file_str = argv[++argv_offset];
    if (argv_offset + 1 != argc) {
      tesseract::tprintError("Incorrect number of parameters specified on commandline.\n");
      argc = 1;
      continue;
    }

    // Initialize Tesseract.
    tesseract::TessBaseAPI api;
    std::vector<std::string> vars_vec;
    std::vector<std::string> vars_values;
    std::vector<std::string> configs;		// = nil
    vars_vec.emplace_back("output_ambig_words_file");
    vars_values.emplace_back(output_file_str);
    api.InitFull(tessdata_dir, lang.c_str(), tesseract::OEM_TESSERACT_ONLY, configs, vars_vec,
             vars_values, false);
    tesseract::Dict& dict = api.tesseract()->getDict();
    FILE* input_file = fopen(input_file_str, "rb");
    if (input_file == nullptr) {
      tesseract::tprintError("Failed to open input wordlist file {}\n", input_file_str);
      return EXIT_FAILURE;
    }
    char str[CHARS_PER_LINE];

    // Read word list and call Dict::NoDangerousAmbig() for each word
    // to record ambiguities in the output file.
    while (fgets(str, CHARS_PER_LINE, input_file) != nullptr) {
      tesseract::chomp_string(str); // remove newline
      tesseract::WERD_CHOICE word(str, dict.getUnicharset());
      dict.NoDangerousAmbig(&word, nullptr, false, nullptr);
    }
    // Clean up.
    fclose(input_file);
    return EXIT_SUCCESS;
  }
}

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_ambiguous_words_main(int argc, const char** argv)
#endif
{
	tesseract::tprintError("the {} tool is not supported in this build.\n", fz_basename(argv[0]));
  return EXIT_FAILURE;
}

#endif
