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

#include <diagnostics/diagnostics.h>

#ifdef HAVE_MUPDF

#include "mupdf/fitz/config.h"
#include "mupdf/fitz/system.h"
#include "mupdf/fitz/version.h"
#include "mupdf/fitz/context.h"
#include "mupdf/assertions.h"     // for ASSERT

#endif

namespace tesseract {

static void assert_that_a_spdlog_sink_and_logger_are_active() {
  std::shared_ptr<spdlog::logger> logger = spdlog::default_logger();
  ASSERT0(!!logger);
  const std::vector<spdlog::sink_ptr> &sinks = logger->sinks();
  ASSERT0(!sinks.empty());
}


// Warning: tprintf() is invoked in tesseract for PARTIAL lines, so we SHOULD gather these fragments
// here before dispatching the gathered lines to the appropriate back-end API!
static void gather_and_log_a_single_tprintf_line(int level, fmt::string_view format, fmt::format_args args) {
  static int block_level = T_LOG_TRACE;

  // elevation means LOWERING the level value as lower is higher severity!
  level -= tprintGetLevelElevation();

  // sanity check/clipping: there's no log level beyond ERROR severity: ERROR is the highest it can possibly get.
  if (level < T_LOG_ERROR) {
    level = T_LOG_ERROR;
  }

  auto msg = fmt::vformat(format, args);

  // when this is a partial message, store it in the buffer until later, when the message is completed.
  static std::string msg_buffer;
  if (!msg.ends_with('\n')) {
    // make the entire message line have the most severe log level given for any part of the line:
    if (level < block_level) {
      block_level = level;
    }
    msg_buffer += msg;
    return;
  }

  // `msg` carries a complete message, or at least the end of it:
  // when there's some old stuff waiting for us: append and pick up the tracked error level.
  if (!msg_buffer.empty()) {
    level = block_level;
    msg = msg_buffer + msg;
    msg_buffer.clear();
  }

  // We've gathered a single, entire, message: now output it line-by-line (if it's multi-line internally).
  const char *s = msg.c_str();

  if (!strncmp(s, "ERROR: ", 7)) {
    s += 7;
    if (level > T_LOG_ERROR)
      level = T_LOG_ERROR;
  } else if (!strncmp(s, "WARNING: ", 9)) {
    s += 9;
    if (level > T_LOG_WARN)
      level = T_LOG_WARN;
  }

  switch (level) {
    case T_LOG_ERROR:
      spdlog::error(s);
      break;
    case T_LOG_WARN:
      spdlog::warn(s);
      break;
    case T_LOG_INFO:
      spdlog::info(s);
      break;
    case T_LOG_DEBUG:
    default:
      spdlog::debug(s);
      break;
  }

  // reset next line log level to lowest possible:
  block_level = T_LOG_DEBUG;
}


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
  assert_that_a_spdlog_sink_and_logger_are_active();

  gather_and_log_a_single_tprintf_line(level, format, args);

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
}

} // namespace tesseract
