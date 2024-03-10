/**********************************************************************
 * File:        tlog.cpp
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

#include "tlog.h"

using namespace tesseract;

INT_PARAM_FLAG(tlog_level, 29, "Minimum logging level. -1 = absolute quiet; 0 = fatal errors only, ..9 = fatal error + their elaboration, 10 = all errors, ..19 = all errors + their elaboration, 20 = all errors and warnings, ..29 = <ditto> + their elaboration, 30..39 = infos, warnings, errors, 40..49 = hints(40)/diag(41)/debug(42)/traces(43+), infos, warnings, errors, all you could possibly get with the debug flags set they are!");
