// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This program reads a unicharset file, puts the result in a UNICHARSET
// object, fills it with properties about the unichars it contains and writes
// the result back to a file.

#include <tesseract/preparation.h> // compiler config, etc.

#include "common/commandlineflags.h"
#include "common/commontraining.h" // CheckSharedLibraryVersion
#include <tesseract/tprintf.h>
#include "unicharset/unicharset_training_utils.h"

#include "tesseract/capi_training_tools.h"


#if defined(HAS_LIBICU)

using namespace tesseract;

// The directory that is searched for universal script unicharsets.
STRING_VAR(unicharsets_script_dir, "", "Directory name for input script unicharsets/xheights");

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_set_unicharset_properties_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  tesseract::ParseCommandLineFlags(argv[0], &argc, &argv, true);

  // Check validity of input flags.
  if (trainer_input_unicharset_file.empty() || trainer_output_unicharset_file.empty()) {
    tprintError("Specify both input and output unicharsets!\n");
    return EXIT_FAILURE;
  }
  if (unicharsets_script_dir.empty()) {
    tprintError("Must specify a script_dir!\n");
    return EXIT_FAILURE;
  }

  tesseract::SetPropertiesForInputFile(unicharsets_script_dir.c_str(), trainer_input_unicharset_file.c_str(), trainer_output_unicharset_file.c_str(),
                                       trainer_xheights.c_str());
  return EXIT_SUCCESS;
}

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_set_unicharset_properties_main(int argc, const char** argv)
#endif
{
  fprintf(stderr, "set_unicharset_properties tool not supported in this non-ICU / Unicode build.\n");
  return EXIT_FAILURE;
}

#endif
