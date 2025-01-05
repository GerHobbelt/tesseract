/**********************************************************************
 * File:        errcode.h  (Formerly error.h)
 * Description: Header file for generic error handler class
 * Author:      Ray Smith
 *
 * (C) Copyright 1990, Hewlett-Packard Ltd.
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

#ifndef ERRCODE_H
#define ERRCODE_H

#include <fmt/printf.h> 
#include <fmt/base.h> 
#include <fmt/format.h>       // for fmt
#include <tesseract/export.h> // for TESS_API

namespace tesseract {

/*Control parameters for error()*/
enum TessErrorLogCode {
  DBG = -1,     /*log without alert */
  TESSLOG = 0,  /*alert user */
  TESSEXIT = 1, /*exit after error */
  ABORT = 2     /*abort after error */
};

#if 0
/* Explicit Error Abort codes */
#define NO_ABORT_CODE 0
#define LIST_ABORT 1
#define MEMORY_ABORT 2
#define FILE_ABORT 3
#endif

class TESS_API ERRCODE { // error handler class
  const char *message;   // error message

public:
  void verror(const char *caller, TessErrorLogCode action, fmt::string_view format, fmt::format_args args) const;

  template <typename S, typename... Args>
  void error(                  // error print function
      const char *caller,      // function location
      TessErrorLogCode action, // action to take
      const S *format,
      Args&&... args
  ) const {
    verror(caller, action, format, fmt::make_format_args(args...));
  }

  void error(const char *caller, TessErrorLogCode action) const;
  
  [[noreturn]] void vabort(const char* caller, fmt::string_view format, fmt::format_args args) const;

  template <typename S, typename... Args>
  [[noreturn]] void abort(     // print function for fatal errors
      const char* caller,      // function location
      const S* format,
      Args&&... args
  ) const {
    vabort(caller, format, fmt::make_format_args(args...));
  }

  [[noreturn]] void abort(const char* caller) const;
  
  constexpr ERRCODE(const char *string) : message(string) {} // initialize with string
};

constexpr ERRCODE ASSERT_FAILED("Assert failed");

#if defined(_MSC_VER) && !defined(__FUNCTION__)
#define __FUNCTION__ __func__
#endif

#define DO_NOTHING static_cast<void>(0)

#define ASSERT_HOST(x) \
  (x) ? DO_NOTHING : ASSERT_FAILED.abort(#x, "in file {}, line {} @ {}()", __FILE__, __LINE__, __FUNCTION__)

#define ASSERT_HOST_MSG(x, ...)                                                \
  if (!(x)) {                                                                  \
    tprintError(__VA_ARGS__);                                                  \
    ASSERT_FAILED.abort(#x, "in file {}, line {} @ {}()", __FILE__, __LINE__, __FUNCTION__); \
  }

} // namespace tesseract

#endif
