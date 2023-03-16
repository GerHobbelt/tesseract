// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TESSERACT_DIAGNOSTICS_IO_H_
#define TESSERACT_DIAGNOSTICS_IO_H_

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h" // DISABLED_LEGACY_ENGINE
#endif

#include <tesseract/publictypes.h> // for OcrEngineMode, PageSegMode, OEM_L...

#include <cstdint>
#include <vector>

namespace tesseract {

  class Tesseract;

  const std::vector<uint32_t>& initDiagPlotColorMap(void);

} // namespace tesseract

#endif // TESSERACT_CCUTIL_FOPENUTF8_H_
