///////////////////////////////////////////////////////////////////////
// File:        ccutil.h
// Description: ccutil class.
// Author:      Samuel Charron
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

#ifndef TESSERACT_CCUTIL_CCUTIL_H_
#define TESSERACT_CCUTIL_CCUTIL_H_

#include <tesseract/preparation.h> // compiler config, etc.

#if !(defined(WIN32) || defined(_WIN32) || defined(_WIN64))
#  include <pthread.h>
#  include <semaphore.h>
#endif

#if !DISABLED_LEGACY_ENGINE
#  include "ambigs.h"
#endif
#include "errcode.h"
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#  include "host.h" // windows.h for HANDLE, ...
#endif
#include <tesseract/params.h>
#include "unicharset.h"

namespace tesseract {

class TESS_API CCUtil {
public:
  CCUtil();
  virtual ~CCUtil();

public:
  /**
   * @brief CCUtil::main_setup - set location of tessdata and template name of output images
   *
   * @param argv0 - paths to the directory with language files and config files.
   * An actual value of argv0 is used if not nullptr, otherwise TESSDATA_PREFIX is
   * used if not nullptr, next try to use compiled in -DTESSDATA_PREFIX. If
   * previous is not successful - use current directory.
   * 
   * @param basename - template name of output images
   *
   * @param language_to_load - (optional) language identifier of the language model we wish to use
   *
   * Return 0 on success, non-zero on error. (The error will already have been reported via tprintError().)
   */
  [[nodiscard]] int main_setup(const std::string &argv0,                 /// program name
                               const std::string &output_image_basename, /// name of output/debug image(s)
                               const std::string &language_to_load)
  {
    std::vector<std::string> lang_vec;
    if (!language_to_load.empty())
      lang_vec.push_back(language_to_load);
    return main_setup(argv0, output_image_basename, lang_vec);
  }
  [[nodiscard]] int main_setup(const std::string &argv0,                 /// program name
                               const std::string &output_image_basename, /// name of output/debug image(s)
                               const std::vector<std::string> &languages_to_load);
  ParamsVectors *params() {
    return &params_;
  }

  std::string input_file_path_; // name of currently processed input file
  std::string datadir_;       // dir for data files
  std::string imagebasename_; // name of image
  std::string lang_;
  std::string language_data_path_prefix_;
  UNICHARSET unicharset_;
#if !DISABLED_LEGACY_ENGINE
  UnicharAmbigs unichar_ambigs_;
#endif
  std::string imagefile_; // image file name
  std::string directory_; // main directory

private:
  ParamsVectors params_;

public:
  // Member parameters.
  // These have to be declared and initialized after params_ member, since
  // params_ should be initialized before parameters are added to it.
  INT_VAR_H(ambigs_debug_level);
  INT_VAR_H(universal_ambigs_debug_level);
  BOOL_VAR_H(use_ambigs_for_adaption);
  BOOL_VAR_H(debug_datadir_discovery);
  STRING_VAR_H(datadir_base_path);
};

} // namespace tesseract

#endif // TESSERACT_CCUTIL_CCUTIL_H_
