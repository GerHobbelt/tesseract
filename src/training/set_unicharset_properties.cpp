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

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h" // HAS_LIBICU
#endif

#include "common/commandlineflags.h"
#include "common/commontraining.h" // CheckSharedLibraryVersion
#include "tprintf.h"
#include "unicharset/unicharset_training_utils.h"

#include "tesseract/capi_training_tools.h"


#if defined(HAS_LIBICU)

using namespace tesseract;

// The directory that is searched for universal script unicharsets.
#if !defined(BUILD_MONOLITHIC)
STRING_PARAM_FLAG(script_dir, "", "Directory name for input script unicharsets/xheights");
#else
DECLARE_STRING_PARAM_FLAG(script_dir);        // already declared in combine_lang_model.cpp
#endif

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_set_unicharset_properties_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  int rv = tesseract::ParseCommandLineFlags("-U file -O file -X file --script_dir path", &argc, &argv);
  if (rv >= 0)
	  return rv;

  // Check validity of input flags.
  if (FLAGS_U.empty() || FLAGS_O.empty()) {
    tprintError("Specify both input and output unicharsets!\n");
    return EXIT_FAILURE;
  }
  if (FLAGS_script_dir.empty()) {
    tprintError("Must specify a script_dir!\n");
    return EXIT_FAILURE;
  }

  tesseract::SetPropertiesForInputFile(FLAGS_script_dir.c_str(), FLAGS_U.c_str(), FLAGS_O.c_str(),
                                       FLAGS_X.c_str());
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
