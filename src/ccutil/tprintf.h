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

#include "params.h" // for BOOL_VAR_H
#include <tesseract/export.h> // for TESS_API

namespace tesseract {

// Note: You can disable some log messages by setting FLAGS_tlog_level > 0.

// Main logging function.
#if defined(__GNUC__) && defined(__attribute__)
__attribute__((format(printf, 1, 2)))
#endif
extern TESS_API void tprintf( // Trace printf
    TS_FORMAT_STRING(const char *format), ...) TS_PRINTFLIKE(1, 2);  // Message

} // namespace tesseract

#undef __attribute__

#endif // define TESSERACT_CCUTIL_TPRINTF_H
