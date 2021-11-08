/**********************************************************************
 * File:        tprintf.cpp
 * Description: Trace version of printf - portable between UX and NT
 * Author:      Phil Cheatle
 *
 * (C) Copyright 1995, Hewlett-Packard Ltd.
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

// Include automatically generated configuration file if running autoconf.
#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#include "tprintf.h"

#include "params.h"

#include <climits> // for INT_MAX
#include <cstdarg>
#include <cstdio>

namespace tesseract {

#ifdef HAVE_MUPDF

// for fz_error():
#ifdef __cplusplus
extern "C" {
#endif

#include "mupdf/fitz/config.h"
#include "mupdf/fitz/system.h"
#include "mupdf/fitz/version.h"
#include "mupdf/fitz/context.h"

#ifdef __cplusplus
}
#endif

TESS_API void tprintf(const char* format, ...) {
  va_list args;
  va_start(args, format);

  if (!strncmp(format, "ERROR: ", 7))
	fz_verror(NULL, format + 7, args);
  else if (!strncmp(format, "WARNING: ", 9))
    fz_vwarn(NULL, format + 9, args);
  else
    fz_vinfo(NULL, format, args);
  va_end(args);
}

#else

#define MAX_MSG_LEN 2048

INT_VAR(log_level, INT_MAX, "Logging level");

static STRING_VAR(debug_file, "", "File to send tprintf output to");

// Trace printf
TESS_API void tprintf(const char *format, ...) {
  const char *debug_file_name = debug_file.c_str();
  static FILE *debugfp = nullptr; // debug file

  if (debug_file_name == nullptr) {
    // This should not happen.
    return;
  }

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
  // Replace /dev/null by nul for Windows.
  if (strcmp(debug_file_name, "/dev/null") == 0) {
    debug_file_name = "nul";
    debug_file.set_value(debug_file_name);
  }
#endif

  if (debugfp == nullptr && debug_file_name[0] != '\0') {
    debugfp = fopen(debug_file_name, "wb");
  } else if (debugfp != nullptr && debug_file_name[0] == '\0') {
    fclose(debugfp);
    debugfp = nullptr;
  }

  va_list args;           // variable args
  va_start(args, format); // variable list
  if (debugfp != nullptr) {
    vfprintf(debugfp, format, args);
  } else {
    vfprintf(stderr, format, args);
  }
  va_end(args);
}

#endif

} // namespace tesseract
