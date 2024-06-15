/**********************************************************************
 * File:        params.h
 * Description: Class definitions of the *_VAR classes for tunable constants.
 * Author:      Ray Smith
 * Author:      Ger Hobbelt
 * 
 * UTF8 detect helper statement: «bloody MSVC»
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

#ifndef TESSERACT_PARAMS_H
#define TESSERACT_PARAMS_H

#include <tesseract/export.h> // for TESS_API
#include <parameters/parameters.h>

namespace tesseract {

class TFile;


// --------------------------------------------------------------------------------------------------

// Utility functions for working with Tesseract parameters.
class TESS_API ParamUtils {
public:
  // Read parameters from the given file pointer.
  // Otherwise identical to ReadParamsFile().
  static bool ReadParamsFromFp(TFile *fp,
                               const ParamsVectorSet &set,
                               SOURCE_REF);

};


// --------------------------------------------------------------------------------------------------


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

} // namespace tesseract

#endif
