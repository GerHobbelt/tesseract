
#pragma once

#if 01
#include <ghc/filesystem.hpp>
#else
#include <filesystem>
#endif

namespace tesseract {
namespace fs {
#if defined(GHC_FS_API)
  using namespace ::ghc::filesystem;
#else
  using namespace ::std::filesystem;
#endif

  static inline bool exists(const char *filename) {
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
    return _access(filename, 0) == 0;
#else
    return access(filename, 0) == 0;
#endif
  }

  static inline bool exists(const std::string &filename) {
    return exists(filename.c_str());
  }

  } // namespace fs
} // namespace tesseract
