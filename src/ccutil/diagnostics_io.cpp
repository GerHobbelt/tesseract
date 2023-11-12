// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "diagnostics_io.h"

#include <leptonica/allheaders.h> // for pixGetHeight, pixGetPixel

#include <algorithm> // for max, min
#include <cmath>
#include <cstdint>   // for INT32_MAX, INT16_MAX

namespace tesseract {

  static inline int cmapInterpolate(int factor, int c0, int c1) {
    int c = c0 * factor + c1 * (256 - factor);
    return c >> 8;
  }

  static std::vector<uint32_t> cmap;
  static bool cmap_is_init = false;

  static void initDiagPlotColorMapColorRange(std::vector<uint32_t>& cmap, int start_index, int h0, int s0, int v0, int h1, int s1, int v1) {
	for (int i = 0; i < 64; i++) {
      int h = cmapInterpolate(i, h0, h1);
      int s = cmapInterpolate(i, s0, s1);
      int v = cmapInterpolate(i, v0, v1);

      // leptonica uses a HSV unit sets of (240, 255, 255) instead of (360, 100%, 100%) so we must convert to that too:
      h = (h * 240) / 360;
      s = (s * 255) / 100;
      v = (v * 255) / 100;

      int r, g, b;
      convertHSVToRGB(h, s, v, &r, &g, &b);
      uint32_t color;
      composeRGBPixel(r, g, b, &color);
      cmap.push_back(color);
    }
  }

  const std::vector<uint32_t>& initDiagPlotColorMap(void) {
    if (cmap_is_init)
	  return cmap;

	cmap.reserve(256);

    // hsv(204, 100%, 71%) - hsv(262, 100%, 71%)
    // --> noise_blobs
    initDiagPlotColorMapColorRange(cmap, 0, 204, 100, 71, 262, 100, 71);

    // hsv(143, 100%, 64%) - hsv(115, 77%, 71%)
    // --> small_blobs
    initDiagPlotColorMapColorRange(cmap, 64, 143, 100, 64, 115, 77, 71);

    // hsv(297, 100%, 81%) - hsv(321, 100%, 82%)
    // --> large_blobs
    initDiagPlotColorMapColorRange(cmap, 2 * 64, 297, 100, 81, 321, 100, 82);

    // hsv(61, 100%, 76%) - hsv(26, 100%, 94%)
    // --> blobs
    initDiagPlotColorMapColorRange(cmap, 3 * 64, 61, 100, 76, 26, 100, 94);

    return cmap;
  }

} // namespace tesseract
