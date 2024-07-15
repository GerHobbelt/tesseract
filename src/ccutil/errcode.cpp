/**********************************************************************
 * File:        errcode.cpp  (Formerly error.c)
 * Description: Generic error handler function
 * Author:      Ray Smith
 *
 * (C) Copyright 1989, Hewlett-Packard Ltd.
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

#include <tesseract/preparation.h>    // compiler config, etc.

#include "errcode.h"
#include <tesseract/tprintf.h>


namespace tesseract {

#pragma optimize("", off)

constexpr ERRCODE BADERRACTION("Illegal error action");
#define MAX_MSG 1024

[[noreturn]] static void abort_application();

static void error_action(TessErrorLogCode action) {
  switch (action) {
    case DBG:
    case TESSLOG:
      return; // report only
    case TESSEXIT:
    case ABORT:
      abort_application();
    default:
      BADERRACTION.abort("error");
  }
}

[[noreturn]] static void abort_application() {
#if !defined(NDEBUG)
  // Create a deliberate abnormal exit as the stack trace is more useful
  // that way. This is done only in debug builds, because the
  // error message "segmentation fault" confuses most normal users.
#  if defined(__GNUC__)
  __builtin_trap();
#  else
  * reinterpret_cast<int*>(0) = 0;
#  endif
#endif
  ::abort();
}

/**********************************************************************
 * error
 *
 * Print an error message and continue, exit or abort according to action.
 * Makes use of error messages and numbers in a common place.
 *
 **********************************************************************/
void ERRCODE::error(const char *caller, TessErrorLogCode action) const {
  if (caller != nullptr) {
    // name of caller
    tprintError("{}:{}\n", caller, message);
  } else {
    tprintError("{}\n", message);
  }
  error_action(action);
}

void ERRCODE::verror(const char *caller, TessErrorLogCode action, fmt::string_view format, fmt::format_args args) const {
  if (caller != nullptr) {
    // name of caller
    tprintError("{}\n", fmt::format("{}:{}:{}", caller, message, fmt::vformat(format, args)).c_str());
  } else {
    tprintError("{}\n", fmt::format("{}:{}", message, fmt::vformat(format, args)).c_str());
  }
  error_action(action);
}

[[noreturn]] void ERRCODE::abort(const char* caller) const {
  if (caller != nullptr) {
    // name of caller
    tprintError("{}:{}\n", caller, message);
  }
  else {
    tprintError("{}\n", message);
  }
  abort_application();
}

[[noreturn]] void ERRCODE::vabort(const char* caller, fmt::string_view format, fmt::format_args args) const {
  if (caller != nullptr) {
    // name of caller
    tprintError("{}\n", fmt::format("{}:{}:{}", caller, message, fmt::vformat(format, args)).c_str());
  }
  else {
    tprintError("{}\n", fmt::format("{}:{}", message, fmt::vformat(format, args)).c_str());
  }
  abort_application();
}

} // namespace tesseract
