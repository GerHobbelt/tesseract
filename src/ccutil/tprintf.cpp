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
#include <cstdio>

#ifdef HAVE_MUPDF

#include "mupdf/fitz/config.h"
#include "mupdf/fitz/system.h"
#include "mupdf/fitz/version.h"
#include "mupdf/fitz/context.h"
#include "mupdf/assertions.h"     // for ASSERT

#endif

namespace tesseract {

#ifdef HAVE_MUPDF

// Warning: tprintf() is invoked in tesseract for PARTIAL lines, so we SHOULD gather these fragments
// here before dispatching the gathered lines to the appropriate back-end API!
static void fz_tess_tprintf(int level, fmt::string_view format, fmt::format_args args) {
  static int block_level = T_LOG_DEBUG;
  // sanity check/clipping: there's no log level beyond ERROR severity: ERROR is the highest it can possibly get.
  if (level < T_LOG_ERROR) {
	  level = T_LOG_ERROR;
  }
  // make the entire message line have the most severe log level given for any part of the line:
  if (level < block_level) {
    block_level = level;
  }

  auto msg = fmt::vformat(format, args);

  static std::string msg_buffer;
  msg_buffer += msg;
  if (!msg_buffer.ends_with('\n'))
    return;

  const char *s = msg_buffer.c_str();
  level = block_level;

  if (!strncmp(s, "ERROR: ", 7))
    fz_error(NULL, "%s", s + 7);
  else if (!strncmp(s, "WARNING: ", 9))
    fz_warn(NULL, "%s", s + 9);
  else {
	switch (level) {
	case T_LOG_ERROR:
      fz_error(NULL, "%s", s);
	  break;
	case T_LOG_WARN:
	  fz_warn(NULL, "%s", s);
	  break;
	case T_LOG_INFO:
      fz_info(NULL, "%s", s);
	  break;
	case T_LOG_DEBUG:
	default:
      fz_info(NULL, "%s", s);
	  break;
	}
  }

  msg_buffer.clear();

  // reset next line log level to lowest possible:
  block_level = T_LOG_DEBUG;
}

#endif

#define MAX_MSG_LEN 2048

// when we use tesseract as part of MuPDF (or mixed with it), we use the fz_error/fz_warn/fz_info APIs to
// output any error/info/debug messages and have the callbacks which MAY be registered with those APIs
// handle any writing to logfile, etc., thus *obsoleting/duplicating* the `debug_file` configuration
// option here.
#ifndef HAVE_MUPDF
static STRING_VAR(debug_file, "", "File to send tesseract::tprintf output to");
#endif

static int print_level_offset = 0;

int tprintSetLogLevelElevation(int offset)
{
	print_level_offset = offset;
	return print_level_offset;
}

int tprintAddLogLevelElevation(int offset)
{
	print_level_offset += offset;
	return print_level_offset;
}

const int tprintGetLevelElevation(void)
{
	return print_level_offset;
}

// Trace printf
void vTessPrint(int level, fmt::string_view format, fmt::format_args args) {
#ifdef HAVE_MUPDF
	fz_tess_tprintf(level, format, args);
#else
  const char *debug_file_name = debug_file.c_str();
  static FILE *debugfp = nullptr; // debug file

  ASSERT0(debug_file_name != nullptr);
  if (debug_file_name == nullptr) {
    // This should not happen.
    return;
  }

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
  // Replace /dev/null by nil for Windows.
  if (strcmp(debug_file_name, "/dev/null") == 0) {
    debug_file_name = "";
    debug_file.set_value(debug_file_name);
  }
#endif

  if (debugfp == nullptr && debug_file_name[0] != '\0') {
    debugfp = fopen(debug_file_name, "a+b");
  } else if (debugfp != nullptr && debug_file_name[0] == '\0') {
    fclose(debugfp);
    debugfp = nullptr;
  }

  if (debugfp != nullptr) {
    fmt::vprint(debugfp, format, args);
  } else {
    fmt::vprint(stderr, format, args);
  }
#endif
}

} // namespace tesseract
