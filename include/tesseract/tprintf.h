/**********************************************************************
 * File:        tprintf.h
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

#ifndef TESSERACT_CCUTIL_TPRINTF_H
#define TESSERACT_CCUTIL_TPRINTF_H

#include <fmt/format.h>       // for fmt
#include <tesseract/export.h> // for TESS_API

namespace tesseract {

// Note: You can disable some log messages by setting FLAGS_tlog_level > 0.

enum LogLevel : int {
	//T_LOG_CRITICAL,
	T_LOG_ERROR,
	T_LOG_WARN,
	T_LOG_INFO,
	T_LOG_DEBUG,
	T_LOG_TRACE,
};

// Helper function for tprintf.
extern TESS_API void vTessPrint(int level, fmt::string_view format, fmt::format_args args);

// Main logging functions.

template <typename S, typename... Args>
void tprintError(const S *format, Args &&...args) {
  vTessPrint(T_LOG_ERROR, format, fmt::make_format_args(args...));
}

template <typename S, typename... Args>
void tprintWarn(const S *format, Args &&...args) {
	vTessPrint(T_LOG_WARN, format, fmt::make_format_args(args...));
}

template <typename S, typename... Args>
void tprintInfo(const S *format, Args &&...args) {
	vTessPrint(T_LOG_INFO, format, fmt::make_format_args(args...));
}

template <typename S, typename... Args>
void tprintDebug(const S *format, Args &&...args) {
	vTessPrint(T_LOG_DEBUG, format, fmt::make_format_args(args...));
}

template <typename S, typename... Args>
void tprintTrace(const S *format, Args &&...args) {
  vTessPrint(T_LOG_TRACE, format, fmt::make_format_args(args...));
}

/////////////////////////////////////////////////////////////////////////////////

// Signal the tprintf line gatherer that the next lines printed, even when terminated
// by a '\n' newline, are to be kept together as a single pack, a single message.
// 
// Any such grouping is ended by the class instance going out of scope (and its destructor
// being invoked to produce the desired 'side effect') or the grouping is broken up
// when a different log level message zips through: errors break up warnings/info/debug info, etc.
//
// Anyway, this class only lives for its side effects in tprint log channel:
class TPrintGroupLinesTillEndOfScope {
public:
  TPrintGroupLinesTillEndOfScope();   // push grouping signal
  ~TPrintGroupLinesTillEndOfScope();  // pop pending grouping signal
};

} // namespace tesseract

#endif // define TESSERACT_CCUTIL_TPRINTF_H
