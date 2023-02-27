///////////////////////////////////////////////////////////////////////
// File:        tesstypes.h
// Description: Simple data types used by Tesseract code.
// Author:      Stefan Weil
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
///////////////////////////////////////////////////////////////////////

#ifndef TESSERACT_TESSTYPES_H
#define TESSERACT_TESSTYPES_H

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h" // FAST_FLOAT
#endif

#include <cstdint> // for int16_t, int32_t

namespace tesseract {

// Image dimensions (width and height, coordinates).
#if defined(LARGE_IMAGES)
typedef int32_t TDimension;

#define TDIMENSION_MAX   (1 << 24)    // cutline() and other code has multiply-by 256 operations, so we keep this safe / conservative by keeping 8 bits headspace.
#define TDIMENSION_MIN   (-TDIMENSION_MAX)
#else
typedef int16_t TDimension;

#define TDIMENSION_MAX   INT16_MAX
#define TDIMENSION_MAX   (-INT16_MAX)
#endif

// Floating point data type used for LSTM calculations.
#if defined(FAST_FLOAT)
using TFloat = float;
#else
using TFloat = double;
#endif

}

#endif // TESSERACT_TESSTYPES_H
