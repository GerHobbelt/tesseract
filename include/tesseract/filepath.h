///////////////////////////////////////////////////////////////////////
// File:        filepath.h
// Description: Support for easily tracking a file path in several styles: original (as specified by user/application, the canonical path and a beautified display variant)
// Author:      Ger Hobbelt
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
///////////////////////////////////////////////////////////////////////

#ifndef TESSERACT_FILEPATH_H
#define TESSERACT_FILEPATH_H

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h" // FAST_FLOAT
#endif

#include <string>
#include <filesystem>

namespace tesseract {

/**
 * Stores a (user-specified) path, while producing and caching various forms on demand:
 *
 * - user-specified original path string
 * - (weakly) canonicalized filesystem path
 * - 'beautified' path for display/reporting, where overly large path specs are not appreciated.
 */
class FilePath {
public:
  FilePath();
  FilePath(const char *path);
  FilePath(const std::string &path);
  FilePath(const std::filesystem::path &path);

  ~FilePath();

  const char *original() const;
  const char *unixified();      // all native directory separators replaced by UNIX-y '/'

  const char *normalized();

  const char *display(int max_dir_count = 4, bool reduce_middle_instead_of_start_part = false);

protected:
  // cache flags:
  //bool has_orig_path{false};

  bool has_canonicalized_slot{false};

  bool has_unixified_the_slot{false};
  bool unixified_is_different{false};

  bool beautify_from_middle{false};      // caches `reduce_middle_instead_of_start_part`
  uint8_t beautify_slot_span{0};         // caches `max_dir_count`

  // storage:
  // 
  // (We like to keep our class SMALL in memory footprint, which is why we don't bulk it out with std::string instances (~40 bytes each), but use allocated `char *` instead.)
  char *orig_path{nullptr};
  char *canonicalized{nullptr};
  char *beautified_path{nullptr};
};

} // namespace

#endif // TESSERACT_FILEPATH_H
