// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TESSERACT_CCUTIL_FOPENUTF8_H_
#define TESSERACT_CCUTIL_FOPENUTF8_H_

#include <cstdio>

namespace tesseract {

/**
 * fopen that accepts path and mode in UTF-8.
 * 
 * When `mode` is a write mode, any not-yet-exisiting directories
 * in the `path` will be created on the spot, hence this function
 * acts very similar to UNIX's `mkdir -p dirname $FPATH; open $FPATH`.
 */
FILE* fopenUtf8(const char* path, const char* mode);

} // namespace tesseract

#endif // TESSERACT_CCUTIL_FOPENUTF8_H_
