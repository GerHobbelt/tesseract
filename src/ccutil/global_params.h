/**********************************************************************
 * File:        global_params.h
 * Author:      Ray Smith
 * 
 * (C) Copyright 1991, Hewlett-Packard Ltd.
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

#ifndef TESS_GLOBAL_PARAMS_H
#define TESS_GLOBAL_PARAMS_H

#include <tesseract/params.h>
#include <utility>            // for std::forward

namespace tesseract {

extern BOOL_VAR_H(stream_filelist);
extern STRING_VAR_H(document_title);
#ifdef HAVE_LIBCURL
extern INT_VAR_H(curl_timeout);
extern STRING_VAR_H(curl_cookiefile);
#endif
extern INT_VAR_H(debug_all);
extern BOOL_VAR_H(debug_misc);
extern BOOL_VAR_H(verbose_process);
#if !GRAPHICS_DISABLED
extern BOOL_VAR_H(scrollview_support);
#endif
extern STRING_VAR_H(vars_report_file);
extern BOOL_VAR_H(report_all_variables);
extern DOUBLE_VAR_H(allowed_image_memory_capacity);
extern BOOL_VAR_H(two_pass);

// Disable some log messages by setting log_level > 0.
extern TESS_API INT_VAR_H(log_level);

// Get file for debug output.
TESS_API FILE *get_debugfp();

} // namespace tesseract

#endif
