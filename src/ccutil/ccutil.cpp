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
#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#include <tesseract/debugheap.h>

#include "ccutil.h"
#include "winutils.h"
#include "pathutils.h"
#include "helpers.h"

#if defined(_WIN32)
#  include <io.h> // for _access
#endif

#include <cstdlib>
#include <cstring> // for std::strrchr
#include <filesystem>


namespace tesseract {

CCUtil::CCUtil()
    : params_("tesseract")
    , params_collective_({&params_, &GlobalParams()})
      , INT_INIT_MEMBER(ambigs_debug_level, 0, "Debug level for unichar ambiguities", params())
      , BOOL_MEMBER(use_ambigs_for_adaption, false, "Use ambigs for deciding whether to adapt to a character", params())
      , BOOL_MEMBER(debug_datadir_discovery, false, "Show which paths tesseract will inspect while looking for its designated data directory, which contains the traineddata, configs, etc.", params())
      , STRING_MEMBER(datadir_base_path, nullptr, "The designated tesseract data directory, which contains the traineddata, configs, etc.; setting this variable is one way to help tesseract locate the desired data path. (C++ API, the location of the current tesseract binary/application, the environment variable TESSDATA_PREFIX and current working directory are the other ways)", params())
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
static bool has_traineddata_files(const std::string &datadir) {
  if (!fs::exists(datadir))
    return false;
  if (!fs::is_directory(datadir) || fs::is_symlink(datadir))
    return false;

  // for (const fs::directory_entry &dir_entry : fs::recursive_directory_iterator(datadir)) {
  for (const fs::directory_entry &dir_entry : fs::directory_iterator(datadir)) {
    tprintDebug("testing for traineddata file: inspecting {}\n", dir_entry.path().string());

    // Don't use string.ends_with() as we wish to support traineddata archive bundles as well. (future music)
    if (/* dir_entry.is_regular_file() && */ dir_entry.file_size() > 0 && dir_entry.path().filename().string().find(".traineddata") != std::string::npos)
      return true;
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
static bool is_viable_datapath(std::string& datadir) {
  if (datadir.empty())
    return false;
  if (!fs::exists(datadir))
    return false;
  if (!fs::is_directory(datadir) || fs::is_symlink(datadir))
    return false;

  std::string subdir = datadir;
  if (!subdir.ends_with('/')) {
    (void)subdir.pop_back();
  }
  std::string subdir2 = subdir + "/tessdata";
  if (is_viable_datapath(subdir2)) {
    datadir = subdir2;
    return true;
  }
  if (has_traineddata_files(subdir)) {
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

static bool determine_datadir(std::string &datadir, const std::string &argv0, const std::string &primary, bool debug_datadir_discovery) {
  datadir.clear();

  std::vector<std::string> attempts;

  const char *tessdata_prefix = getenv("TESSDATA_PREFIX");

  if (!primary.empty()) {
    /* Use specified primary directory override. */
    attempts.push_back(primary);
  }

  if (!argv0.empty()) {
    /* Use tessdata prefix from the command line. */
    attempts.push_back(argv0 + "/tessdata/");
    attempts.push_back(argv0);
  }

  if (tessdata_prefix && *tessdata_prefix) {
    /* Use tessdata prefix from the environment. */
    std::string testdir = tessdata_prefix;
    attempts.push_back(testdir + "/tessdata/");
    attempts.push_back(testdir);
  }

#if defined(_WIN32)
  if (datadir.empty() || _access(datadir.c_str(), 0) != 0) {
    /* Look for tessdata in directory of executable. */
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
      wchar_t *separator = std::wcsrchr(path, '\\');
      if (separator != nullptr) {
        *separator = '\0';
        std::string subdir = winutils::Utf16ToUtf8(path);
        attempts.push_back(subdir + "/tessdata/");
        attempts.push_back(subdir);
      }
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
    attempts.push_back(testdir + "/tessdata/");
    attempts.push_back(testdir);
  }
#endif /* TESSDATA_PREFIX */

  // last resort: check in current working directory
  {
    attempts.push_back("./tessdata/");
  }

  std::vector<std::filesystem::path> canonical_attempts;

  if (debug_datadir_discovery) {
    report_datadir_attempt(attempts, canonical_attempts);
  }

  // now run through the attempts in order and see which one is the first viable one.
  for (const std::string &entry : attempts) {
    auto canon = std::filesystem::weakly_canonical(entry);
    canonical_attempts.push_back(canon);

    std::string testdir = canon.string();
    if (is_viable_datapath(testdir)) {
      unixify_path(testdir);
      // check for missing directory separator
      if (!testdir.ends_with('/')) {
        testdir += '/';
      }
      datadir = testdir;
      return true;
    }
  }

  report_datadir_attempt(attempts, canonical_attempts, "failed to locate the mandatory tesseract data directory.");
  return false;
}

/**
 * @brief CCUtil::main_setup - set location of tessdata and name of image
 *
 * @param argv0 - paths to the directory with language files and config files.
 * An actual value of argv0 is used if not nullptr, otherwise TESSDATA_PREFIX is
 * used if not nullptr, next try to use compiled in -DTESSDATA_PREFIX. If
 * previous is not successful - use current directory.
 * @param basename - name of image
 */
void CCUtil::main_setup(const std::string &argv0, const std::string &output_image_basename) {
  if (output_image_basename == "-" /* stdout */)
    imagebasename = "tesseract-stdio-session";
  else
    imagebasename = output_image_basename; /**< name of output/debug image(s) */

  if (!determine_datadir(datadir, argv0, this->datadir_base_path.value(), this->debug_datadir_discovery)) {
    assert(datadir.empty());
    // TODO
    assert(0);
  }

  // check for missing directory separator
  assert(datadir.ends_with('/'));
}

} // namespace tesseract
