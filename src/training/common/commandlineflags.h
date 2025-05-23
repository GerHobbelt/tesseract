/**********************************************************************
 * File:        commandlineflags.h
 * Description: Header file for commandline flag parsing.
 * Author:      Ranjith Unnikrishnan
 *
 * (C) Copyright 2013, Google Inc.
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
#ifndef TESSERACT_TRAINING_COMMANDLINEFLAGS_H_
#define TESSERACT_TRAINING_COMMANDLINEFLAGS_H_

#include "export.h"
#include <tesseract/export.h>
#include <tesseract/version.h>
#include <tesseract/params.h>
#include <tesseract/capi_training_tools.h>

#include <cstdlib>
#include <functional>

#if 0
#define INT_PARAM_FLAG(name, val, comment) INT_VAR(FLAGS_##name, val, comment)
#define DECLARE_INT_PARAM_FLAG(name) extern INT_VAR_H(FLAGS_##name)
#define DOUBLE_PARAM_FLAG(name, val, comment) DOUBLE_VAR(FLAGS_##name, val, comment)
#define DECLARE_DOUBLE_PARAM_FLAG(name) extern DOUBLE_VAR_H(FLAGS_##name)
#define BOOL_PARAM_FLAG(name, val, comment) BOOL_VAR(FLAGS_##name, val, comment)
#define DECLARE_BOOL_PARAM_FLAG(name) extern BOOL_VAR_H(FLAGS_##name)
#define STRING_PARAM_FLAG(name, val, comment) STRING_VAR(FLAGS_##name, val, comment)
#define DECLARE_STRING_PARAM_FLAG(name) extern STRING_VAR_H(FLAGS_##name)
#endif

namespace tesseract {

// Flags from commontraining.cpp
// Command line arguments for font_properties, xheights and unicharset.

TESS_COMMON_TRAINING_API
extern INT_VAR_H(trainer_debug_level);
TESS_COMMON_TRAINING_API
extern INT_VAR_H(trainer_load_images);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_configfile);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_directory);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_font_properties);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_xheights);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_input_unicharset_file);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_output_unicharset_file);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_output_trainer);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_test_ch);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_fonts_dir);
TESS_COMMON_TRAINING_API
extern STRING_VAR_H(trainer_fontconfig_tmpdir);
TESS_COMMON_TRAINING_API
extern DOUBLE_VAR_H(clusterconfig_min_samples_fraction);
TESS_COMMON_TRAINING_API
extern DOUBLE_VAR_H(clusterconfig_max_illegal);
TESS_COMMON_TRAINING_API
extern DOUBLE_VAR_H(clusterconfig_independence);
TESS_COMMON_TRAINING_API
extern DOUBLE_VAR_H(clusterconfig_confidence);


// Parse commandline flags and values. Prints the usage string and exits on
// input of --help or --version.
//
// If remove_flags is true, the argv pointer is advanced so that (*argv)[1]
// points to the first non-flag argument, (*argv)[0] points to the same string
// as before, and argc is decremented to reflect the new shorter length of argv.
// eg. If the input *argv is
// { "program", "--foo=4", "--bar=true", "file1", "file2" } with *argc = 5, the
// output *argv is { "program", "file1", "file2" } with *argc = 3
//
// Returns either exit code >= 0 (help command found and executed: 0, error in argv set: 1)
// or -1 to signal the argv[] set has been parsed into the application parameters and
// execution should continue.
TESS_COMMON_TRAINING_API
int ParseCommandLineFlags(const char* extra_usage, std::function<void(const char* exename)> extra_usage_f, int* argc, const char*** argv, const bool remove_flags = true, std::function<void()> print_version_f = nullptr);

static inline int ParseCommandLineFlags(const char* extra_usage, int* argc, const char*** argv, const bool remove_flags = true, std::function<void()> print_version_f = nullptr) {
  return ParseCommandLineFlags(extra_usage, nullptr, argc, argv, remove_flags, print_version_f);
}

TESS_COMMON_TRAINING_API
bool SetConsoleModeToUTF8(void);

} // namespace tesseract

#endif // TESSERACT_TRAINING_COMMANDLINEFLAGS_H_
