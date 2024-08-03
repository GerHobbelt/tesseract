// Copyright 2008 Google Inc. All Rights Reserved.
// Author: scharron@google.com (Samuel Charron)
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.
#include <tesseract/tprintf.h>

#include "ccutil.h"
#include "winutils.h"
#include "pathutils.h"
#include "helpers.h"

#if defined(_WIN32)
#  include <io.h> // for _access
#endif

#include <cstdlib>
#include <cstring>    // for std::strrchrA
#include <filesystem> // for std::filesystem


namespace tesseract {

CCUtil::CCUtil()
    : params_()
    , INT_MEMBER(ambigs_debug_level, 0, "Debug level for the unichar ambiguities", params())
    , INT_MEMBER(universal_ambigs_debug_level, 0, "Debug level for loading the universal unichar ambiguities", params())
    , BOOL_MEMBER(use_ambigs_for_adaption, false, "Use ambigs for deciding whether to adapt to a character", params())
    , BOOL_MEMBER(debug_datadir_discovery, false, "Show which paths tesseract will inspect while looking for its designated data directory, which contains the traineddata, configs, etc.", params())
    , STRING_MEMBER(datadir_base_path, "", "The designated tesseract data directory, which contains the traineddata, configs, etc.; setting this variable is one way to help tesseract locate the desired data path. (C++ API, the location of the current tesseract binary/application, the environment variable TESSDATA_PREFIX and current working directory are the other ways) A null/empty path spec means ignore-look-elsewhere for hints to the actual data directory, i.e. go down the afore-mentioned list to find the data path.", params())
{}

// Destructor.
// It is defined here, so the compiler can create a single vtable
// instead of weak vtables in every compilation unit.
CCUtil::~CCUtil() = default;

/**
 * Return `true` when the given directory contains at least one `*traineddata*` file
 * or is itself named 'tessdata*', i.e. has 'tessdata' as its name or at least
 * as its prefix.
 */
static bool has_traineddata_files(const std::string &datadir, const std::vector<std::string> &languages_to_load) {
  if (!fs::exists(datadir))
    return false;
  if (!fs::is_directory(datadir) || fs::is_symlink(datadir))
    return false;

  // the first language we hit is makes this directory 'viable':
  for (const std::string &lang : languages_to_load) {
    auto fname = datadir + "/" + lang + ".traineddata";
    std::error_code ec;     // ensure the file_size() check doesn't throw.
    tprintDebug("testing for traineddata file: inspecting {}\n", fname);
    if (fs::exists(fname) && fs::file_size(fname, ec) > 10240) {
      return true;
    }
  }

  // for (const fs::directory_entry &dir_entry : fs::recursive_directory_iterator(datadir)) {
  for (const fs::directory_entry &dir_entry : fs::directory_iterator(datadir)) {
    tprintDebug("testing for traineddata file: inspecting {}\n", dir_entry.path().string());

    // Don't use string.ends_with() as we wish to support traineddata archive bundles as well. (future music)
    auto fname = dir_entry.path().filename().string();
    std::error_code ec; // ensure the file_size() check doesn't throw.
    if (/* dir_entry.is_regular_file() && */ fname.find(".traineddata") != std::string::npos && dir_entry.file_size(ec) > 10240) {
      if (languages_to_load.empty()) {
        return true;
      } else {
        // the first language we hit is makes this directory 'viable':
        for (const std::string &lang : languages_to_load) {
          if (fname.starts_with(lang + ".")) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

/**
 * Return `true` when the path is a viable /tessdata/ directory tree.
 *
 * The path is deemed viable when it contains at least one `*traineddata*` file
 * or is itself named 'tessdata*', i.e. has 'tessdata' as its name or at least
 * as its prefix.
 *
 * When the path is found viable, it MAY have been modified to point at the precise
 * location in the filesystem.
 */
static bool is_viable_datapath(std::string &datadir, const std::vector<std::string> &languages_to_load) {
  if (datadir.empty())
    return false;
  if (!fs::exists(datadir))
    return false;
  if (!fs::is_directory(datadir) || fs::is_symlink(datadir))
    return false;

  std::string subdir = datadir;
  if (subdir.ends_with('/')) {
    (void)subdir.pop_back();
  }
  std::string subdir2 = subdir + "/tessdata";
  if (is_viable_datapath(subdir2, languages_to_load)) {
    datadir = subdir2;
    return true;
  }
  if (has_traineddata_files(subdir, languages_to_load)) {
    datadir = subdir;
    return true;
  }
  return false;
}

static void report_datadir_attempt(std::vector<std::string>& attempts, std::vector<std::filesystem::path>& canonical_attempts, const char* error_msg = nullptr) {
  std::ostringstream msg;
  if (error_msg) {
    msg << error_msg;
    msg << "\n  tesseract was looking for (and in) these directories, in order:\n";
  } else {
    msg << "Determining the tesseract data directory. tesseract is going to look for (and in) these directories, in order:\n";
  }

  for (int i = 0, l = attempts.size(), c_l = canonical_attempts.size(); i < l; i++) {
    std::string testdir = attempts[i];
    unixify_path(testdir);
    if (i < c_l) {
      auto canon = canonical_attempts[i];
      std::string canon_testdir = canon.string();
      unixify_path(canon_testdir);

      msg << fmt::format("  {}    --> {}\n", testdir, canon_testdir);
    } else {
      auto canon = std::filesystem::weakly_canonical(testdir);
      std::string canon_testdir = canon.string();
      unixify_path(canon_testdir);

      msg << fmt::format("  {}    --> {}\n", testdir, canon_testdir);
    }
  }

  const std::string &s = msg.str();
  if (!error_msg) {
    tprintDebug("{}", s);
  } else {
    tprintError("ERROR: {}", s);
  }
}

static bool determine_datadir(std::string &datadir, const std::string &argv0, const std::string &primary, const std::vector<std::string> &languages_to_load, bool debug_datadir_discovery) {
  datadir.clear();

  std::vector<std::string> attempts;

  const char *tessdata_prefix = getenv("TESSDATA_PREFIX");

  // Ignore TESSDATA_PREFIX if there is no matching filesystem entry.
  if (tessdata_prefix != nullptr && !std::filesystem::exists(tessdata_prefix)) {
    tprintWarn("Environment variable TESSDATA_PREFIX's value '{}' is not a directory that exists in your filesystem; tesseract will ignore it.\n", tessdata_prefix);
    tessdata_prefix = nullptr;
  }

  if (!primary.empty()) {
    /* Use specified primary directory override. */
    attempts.push_back(primary);
  }

  if (!argv0.empty()) {
    /* Use tessdata prefix from the command line. */
    attempts.push_back(argv0);
    std::filesystem::path p(argv0);
    attempts.push_back(p.parent_path().string());    // = basedir(argv0)
  }

  if (tessdata_prefix && *tessdata_prefix) {
    /* Use tessdata prefix from the environment. */
    std::string testdir = tessdata_prefix;
    attempts.push_back(testdir);
  }

#if defined(_WIN32)
  if (datadir.empty() || _access(datadir.c_str(), 0) != 0) {
    /* Look for tessdata in directory of executable. */
    wchar_t pth[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, pth, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
      std::filesystem::path p(pth);
      attempts.push_back(p.parent_path().string());  // = basedir(executable)
    }
  }
#endif /* _WIN32 */

#if defined(TESSDATA_PREFIX)
  {
    // Use tessdata prefix which was compiled in.
    std::string testdir = TESSDATA_PREFIX "/tessdata/";
    // Note that some software (for example conda) patches TESSDATA_PREFIX
    // in the binary, so it might be shorter. Recalculate its length.
    testdir.resize(std::strlen(datadir.c_str()));
    attempts.push_back(testdir);
  }
#endif /* TESSDATA_PREFIX */

  // last resort: check in current working directory
  attempts.push_back(std::filesystem::current_path().string() + "/tessdata/");

  std::vector<std::filesystem::path> canonical_attempts;

  if (debug_datadir_discovery) {
    report_datadir_attempt(attempts, canonical_attempts);
  }

  decltype(languages_to_load) nil{};
  const auto *setptr = &languages_to_load;
  for (int state = 1; state >= 0; state--) {
    // now run through the attempts in order and see which one is the first viable one.
    for (const std::string &entry : attempts) {
      auto canon = std::filesystem::weakly_canonical(entry);
      canonical_attempts.push_back(canon);

      std::string testdir = canon.string();
      if (is_viable_datapath(testdir, *setptr)) {
        unixify_path(testdir);
        // check for missing directory separator
        if (!testdir.ends_with('/')) {
          testdir += '/';
        }
        datadir = testdir;
        return true;
      }
    }

    // when we have specified a list of preferred languages and haven't found a viable datadir yet, then we check again and pick the first *generically* viable datadir instead:
    setptr = &nil;
  }

  report_datadir_attempt(attempts, canonical_attempts, "failed to locate the mandatory tesseract data directory containing the traineddata language model files.");
  return false;
}

int CCUtil::main_setup(const std::string &argv0, const std::string &output_image_basename, const std::vector<std::string> &languages_to_load) {
  if (imagebasename_.empty()) {
    if (output_image_basename == "-" /* stdout */)
      imagebasename_ = "tesseract-stdio-session";
    else
      imagebasename_ = output_image_basename; /**< name of output/debug image(s) */
  }

  if (!determine_datadir(datadir_, argv0, this->datadir_base_path.value(), languages_to_load, this->debug_datadir_discovery)) {
    ASSERT_HOST(datadir_.empty());
    return -1;
  }

  // check for missing directory separator
  ASSERT_HOST(datadir_.ends_with('/'));
  return 0;
}

} // namespace tesseract
