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

// We've gathered a single, entire, message: now output it line-by-line (if it's multi-line internally).
static void write_gathered_log_message(int level, const std::string &msg) {
  const char *s = msg.c_str();

  if (!strncmp(s, "ERROR: ", 7)) {
    s += 7;
  } else if (!strncmp(s, "WARNING: ", 9)) {
    s += 9;
  }

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

// Warning: tprintf() is invoked in tesseract for PARTIAL lines, so we SHOULD gather these fragments
// here before dispatching the gathered lines to the appropriate back-end API!
//
// This routine does this "message gathering" per loglevel this way: as long as the loglevel remains
// the same we're clearly busy logging the same overarching message.
// The *proper* behvaiour is to end a message with a `\n` LF, but when the loglevel changes this is
// treated as another (*irregular*) end-of-message signal and the gathered message will be logged.
static void gather_and_log_a_single_tprintf_line(int level, fmt::string_view format, fmt::format_args args) {
  static int block_level = INT_MAX;
  static std::string msg_buffer;

  // sanity check/clipping: there's no log level beyond ERROR severity: ERROR is the highest it can possibly get.
  if (level < T_LOG_ERROR) {
    level = T_LOG_ERROR;
  }

  auto msg = fmt::vformat(format, args);

  // check the loglevel remains the same across the message particles: if not, this is a after-the-fact
  // *irregular* message end marker!
  if (level != block_level) {
    if (block_level != INT_MAX) {
      // after-the-fact end-of-message: log/dump the buffered log message!
      if (!msg_buffer.ends_with('\n'))
        msg_buffer += '\n';
      write_gathered_log_message(block_level, msg_buffer);
      msg_buffer.clear();

      // now we've handled the irregular end-of-message for the pre-exisiting buffered message,
      // continue processing the current message (particle).
    }

    block_level = level;
  }

  bool end_signaled = msg.ends_with('\n');

  // when this is a partial message, store it in the buffer until later, when the message is completed.
  if (!end_signaled) {
    msg_buffer += msg;
    return;
  }

  // `msg` carries a complete message, or at least the end of it:
  // when there's some old stuff waiting for us, append to it and proceed to log.
  if (!msg_buffer.empty()) {
    msg = msg_buffer + msg;
    msg_buffer.clear();
  }

  write_gathered_log_message(level, msg);

  // reset next line log level to lowest possible:
  block_level = INT_MAX;
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

// Trace printf
void vTessPrint(int level, fmt::string_view format, fmt::format_args args) {
#ifdef HAVE_MUPDF
  gather_and_log_a_single_tprintf_line(level, format, args);
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
