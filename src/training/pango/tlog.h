/**********************************************************************
 * File:        tlog.h
 * Description: Variant of printf with logging level controllable by a
 *              commandline flag.
 * Author:      Ranjith Unnikrishnan
 * Created:     Wed Nov 20 2013
 *
 * (C) Copyright 2013, Google Inc.
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
#ifndef TESSERACT_TRAINING_TLOG_H_
#define TESSERACT_TRAINING_TLOG_H_

#include "export.h"
#include <tesseract/export.h>
#include <fmt/format.h>       // for fmt

#include "../common/commandlineflags.h"
#include "errcode.h"
#include "tprintf.h"

TESS_PANGO_TRAINING_API
DECLARE_INT_PARAM_FLAG(tlog_level);

namespace tesseract {

  // Variant guarded by the numeric logging level parameter FLAGS_tlog_level
  // (default 0).  Code using ParseCommandLineFlags() can control its value using
  // the --tlog_level commandline argument. Otherwise it must be specified in a
  // config file like other params.
  template <typename S, typename... Args>
  void tlog(int level, const S* format, Args &&...args) {
    if (FLAGS_tlog_level >= level) {
      //tprintf(format, ...);
      vTessPrint(format, fmt::make_format_args(args...));
    }
  }

  static inline bool
    TLOG_IS_ON(int level) {
    return (FLAGS_tlog_level >= level);
  }

}

#endif // TESSERACT_TRAINING_TLOG_H_
