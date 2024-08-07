///////////////////////////////////////////////////////////////////////
// File:        merge_unicharsets.cpp
// Description: Simple tool to merge two or more unicharsets.
// Author:      Ray Smith
//
// (C) Copyright 2015, Google Inc.
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

#include <tesseract/preparation.h> // compiler config, etc.

#include "common/commontraining.h" // CheckSharedLibraryVersion
#include "unicharset.h"
#include <tesseract/tprintf.h>

using namespace tesseract;

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_merge_unicharsets_main(int argc, const char** argv)
#endif
{
  const char* appname = fz_basename(argv[0]);
  CheckSharedLibraryVersion();

  if (argc > 1 && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))) {
    tprintInfo("{}\n", TessBaseAPI::Version());
    return EXIT_SUCCESS;
  } else if (argc < 4) {
    // Print usage
    tprintInfo(
        "Usage: {} -v | --version |\n"
        "       {} unicharset-in-1 ... unicharset-in-n unicharset-out\n",
        appname, appname);
    return EXIT_FAILURE;
  }

  UNICHARSET input_unicharset, result_unicharset;
  for (int arg = 1; arg < argc - 1; ++arg) {
    // Load the input unicharset
    if (input_unicharset.load_from_file(argv[arg])) {
      tprintDebug("Loaded unicharset of size {} from file {}\n", input_unicharset.size(), argv[arg]);
      result_unicharset.AppendOtherUnicharset(input_unicharset);
    } else {
      tprintError("Failed to load unicharset from file {}!!\n", argv[arg]);
      return EXIT_FAILURE;
    }
  }

  // Save the combined unicharset.
  if (result_unicharset.save_to_file(argv[argc - 1])) {
    tprintDebug("Wrote unicharset file {}\n", argv[argc - 1]);
  } else {
    tprintError("Cannot save unicharset file {}\n", argv[argc - 1]);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
