// SPDX-License-Identifier: Apache-2.0
// File:        fmt-support.h
// Description: Support code for FMT library usage within and without tesseract.
// Author:      Ger Hobbelt
//
// (C) Copyright 2023
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TESSERACT_FMT_SUPPORT_H_
#define TESSERACT_FMT_SUPPORT_H_

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h" // DISABLED_LEGACY_ENGINE
#endif

#include <fmt/base.h>
#include <fmt/format.h>


#define DECL_FMT_FORMAT_TESSENUMTYPE(Type)                                                   \
                                                                                             \
} /* close current namespace tesseract */                                                    \
                                                                                             \
namespace fmt {                                                                              \
                                                                                             \
  template <>                                                                                \
  struct formatter<tesseract::Type> : formatter<std::string_view> {                          \
    /* parse is inherited from formatter<string_view>. */                                    \
                                                                                             \
    auto format(tesseract::Type c, format_context &ctx) const -> decltype(ctx.out());        \
  };                                                                                         \
                                                                                             \
}                                                                                            \
                                                                                             \
namespace tesseract {                                                                        \
  /* re-open namepsace tesseract */


#endif // TESSERACT_API_BASEAPI_H_
