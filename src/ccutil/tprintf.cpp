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
#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/tprintf.h>

#include <tesseract/params.h>

#include <climits> // for INT_MAX
#include <cstdio>
#include <map>    // for std::map
#include <string>
#include <algorithm>
#include <type_traits>

#include <cpp/result.hpp> // alternative for C++23 std::expected<>

#include <spdlog/spdlog.h>

#include "tlog.h"
#include "porting.h"


namespace tesseract {

#if 0


static int block_level = T_LOG_DEBUG;
static unsigned int pending_grouping_count = 0;
static std::string msg_buffer;

static void do_transmit_logline() {
  const char *s = msg_buffer.c_str();

  // can't use fz_error et al here (we CAN, but there are consequences:)
  // as we MAY send some seriously large messages through here, thanks to 
  // the new tprintXXX() line grouping feature. And we DO NOT want those 
  // messages getting reported as yadayadayada(...truncated...) which
  // is what will happen when you switch this section back over to the 
  // old code.
  // 
  // Instead, we use the fz_write_info_line(ctx, buf) APIs, which should
  // be fine with pumping a few extra kilobytes of logging through here, 
  // pronto! Besides, there's nothing left to 'printf format' for us in
  // here anyway, so we're golden with that new fz_ API.
#if 0
  if (!strncmp(s, "ERROR: ", 7))
    fz_error(NULL, "%s", s + 7);
  else if (!strncmp(s, "WARNING: ", 9))
    fz_warn(NULL, "%s", s + 9);
  else {
    switch (block_level) {
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
#else
  if (!strncmp(s, "ERROR: ", 7))
    fz_write_error_line(NULL, s + 7);
  else if (!strncmp(s, "WARNING: ", 9))
    fz_write_warn_line(NULL, s + 9);
  else {
    switch (block_level) {
      case T_LOG_ERROR:
        fz_write_error_line(NULL, s);
        break;
      case T_LOG_WARN:
        fz_write_warn_line(NULL, s);
        break;
      case T_LOG_INFO:
        fz_write_info_line(NULL, s);
        break;
      case T_LOG_DEBUG:
      default:
        fz_write_info_line(NULL, s);
        break;
    }
  }
#  endif
  msg_buffer.clear();
}

// push grouping signal
TPrintGroupLinesTillEndOfScope::TPrintGroupLinesTillEndOfScope() {
  pending_grouping_count++;
}
// pop pending grouping signal
TPrintGroupLinesTillEndOfScope::~TPrintGroupLinesTillEndOfScope() {
  // once we get here, a spurious higher level log message may have broken up
  // our gathering, so we'd better cope with that scenario...
  if (pending_grouping_count) {
    pending_grouping_count--;
    if (!pending_grouping_count) {
      // dropping out of the group clutch is another reason to push what we've
      // gathered *right now*: send the message before continuing as otherwise
      // the gatherer will combine this with the next incoming if we're not real
      // lucky... and we don't want that kind of visual, don't we? So make lucky
      // by transmitting on the spot:
      if (!msg_buffer.empty()) {
        if (!msg_buffer.ends_with('\n'))
          msg_buffer += '\n';
        do_transmit_logline();
      }
    }
  }
}

// Warning: tprintf() is invoked in tesseract for PARTIAL lines, so we SHOULD gather these fragments
// here before dispatching the gathered lines to the appropriate back-end API!
static void fz_tess_tprintf(int level, fmt::string_view format, fmt::format_args args) {
  // sanity check/clipping: there's no log level beyond ERROR severity: ERROR is the highest it can possibly get.
  if (level < T_LOG_ERROR) {
	  level = T_LOG_ERROR;
  }
  // make the entire message line have the most severe log level given for any part of the line,
  // but break up any gathering when a more important line zips through here before we hit that
  // terminating '\n' newline, that otherwise means it's the end.
  if (level < block_level) {
    if (!msg_buffer.empty()) {
      if (!msg_buffer.ends_with('\n'))
        msg_buffer += '\n';
      // send the lower prio message before continuing with our intermittant
      // higher prio current message:
      do_transmit_logline();
    }
    block_level = level;
  }

  auto msg = fmt::vformat(format, args);

  if (pending_grouping_count) {
    if (msg_buffer.ends_with('\n')) {
      // Fixup: every 'clustered' error/warning message MUST, individually, be prefixed with ERROR/WARNING
      // for rapid unambiguous identification by the human final receiver.
      switch (level) {
        case T_LOG_ERROR:
          if (!msg.starts_with("ERROR: ")) {
            msg_buffer += "ERROR: ";
          }
          break;

        case T_LOG_WARN:
          if (!msg.starts_with("WARNING: ")) {
            msg_buffer += "WARNING: ";
          }
          break;

        default:
          break;
      }
    }
  }
  msg_buffer += msg;
  // Can't/Won't do message clustering at the *error* level: those must get out there ASAP!
  if ((level > T_LOG_ERROR && pending_grouping_count) || !msg_buffer.ends_with('\n')) {
    return;
  }

  do_transmit_logline();

  // reset next line log level to lowest possible:
  block_level = T_LOG_DEBUG;
}


#endif




// using loglevel_return_type = std::expected<int, bool>;
using loglevel_return_type = cpp::result<int, bool>;

static loglevel_return_type ParseLogLevel(const char *loglevel) {
  // Allow the log levels which are used by log4cxx.
  const std::string loglevel_string = loglevel;
  static const std::map<const std::string, int> loglevels{
      {"ALL", INT_MIN},
      {"TRACE", 5000},
      {"DEBUG", 10000},
      {"INFO", 20000},
      {"WARN", 30000},
      {"ERROR", 40000},
      {"FATAL", 50000},
      {"OFF", INT_MAX},
  };
  try {
    std::transform(loglevel_string.begin(), loglevel_string.end(), loglevel_string.begin(), ::toupper);
    int loglevel = loglevels.at(loglevel_string);
    return loglevel;
  } catch (const std::out_of_range &) {
    // TODO: Allow numeric argument?
    tprintError("Unsupported --loglevel {}\n", loglevel);
    return cpp::fail(false);
  }
}

#ifdef HAVE_MUPDF

// We've gathered a single, entire, message: now output it line-by-line (if it's multi-line internally).
static void write_gathered_log_message(int level, const std::string &msg) {
  const char *s = msg.c_str();

  if (!strncasecmp(s, "ERROR: ", 7)) {
    s += 7;
    if (level > T_LOG_ERROR)
      level = T_LOG_ERROR;
  } else if (!strncasecmp(s, "WARNING: ", 9)) {
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
    case T_LOG_TRACE:
      spdlog::trace(s);
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
  } else if (level > T_LOG_TRACE) {
    level = T_LOG_TRACE;
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

// when we use tesseract as part of MuPDF (or mixed with it), we use the fz_error/fz_warn/fz_info APIs to
// output any error/info/debug messages and have the callbacks which MAY be registered with those APIs
// handle any writing to logfile, etc., thus *obsoleting/duplicating* the `debug_file` configuration
// option here.
#if 0
static STRING_VAR(debug_file, "", "File to send tesseract::tprintf output to");
#endif

// Trace printf
void vTessPrint(int level, fmt::string_view format, fmt::format_args args) {
  gather_and_log_a_single_tprintf_line(level, format, args);
#if 0
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
