///////////////////////////////////////////////////////////////////////
// File:        intsindmatrixsse.cpp
// Description: SSE implementation of 8-bit int SIMD matrix multiply.
// Author:      Ray Smith
//
// (C) Copyright 2017, Google Inc.
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

#include <tesseract/preparation.h> // compiler config, etc.

#include "intsimdmatrix.h"

// General Notice:
// 
// This is not about whether the compiler is optimizing **the rest of your code using FMA instructions**.
// This code should be compiled *anyway*, because tesseract will pick the best variant (this one or another one)
// **at run-time** on the actual hardware it will be running on.
// Hence to safely compile tesseract for multiple architectures, one should set the compiler code generation
// options as low as possible. Meanwhile these important functions are made available, independent of that compiler
// "optimization setting", by using the appropriate intrinsics. Then, at run-time, a CPU check is performed
// which will help tesseract decide which actual code chunk to execute. **Irrespective of the original compiler
// flags setting -- that one only determines the lowest capability hardware this compiled product can actually
// run on.
// See also the SIMDDetect::SIMDDetect() code.
//
#if defined(__SSE4_1__) || defined(__AVX__) || defined(_M_IX86) || defined(_M_X64)

#  include <emmintrin.h>
#  include <smmintrin.h>
#  include <cstdint>

namespace tesseract {

// Computes and returns the dot product of the n-vectors u and v.
// Uses Intel SSE intrinsics to access the SIMD instruction set.
static int32_t IntDotProductSSE(const int8_t *u, const int8_t *v, int n) {
  int max_offset = n - 8;
  int offset = 0;
  // Accumulate a set of 4 32-bit sums in sum, by loading 8 pairs of 8-bit
  // values, extending to 16 bit, multiplying to make 32 bit results.
  int32_t result = 0;
  if (offset <= max_offset) {
    offset = 8;
    __m128i packed1 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(u));
    __m128i packed2 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(v));
    __m128i sum = _mm_cvtepi8_epi16(packed1);
    packed2 = _mm_cvtepi8_epi16(packed2);
    // The magic _mm_add_epi16 is perfect here. It multiplies 8 pairs of 16 bit
    // ints to make 32 bit results, which are then horizontally added in pairs
    // to make 4 32 bit results that still fit in a 128 bit register.
    sum = _mm_madd_epi16(sum, packed2);
    while (offset <= max_offset) {
      packed1 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(u + offset));
      packed2 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(v + offset));
      offset += 8;
      packed1 = _mm_cvtepi8_epi16(packed1);
      packed2 = _mm_cvtepi8_epi16(packed2);
      packed1 = _mm_madd_epi16(packed1, packed2);
      sum = _mm_add_epi32(sum, packed1);
    }
    // Sum the 4 packed 32 bit sums and extract the low result.
    sum = _mm_hadd_epi32(sum, sum);
    sum = _mm_hadd_epi32(sum, sum);
    result = _mm_cvtsi128_si32(sum);
  }
  while (offset < n) {
    result += u[offset] * v[offset];
    ++offset;
  }
  return result;
}

// Computes part of matrix.vector v = Wu. Computes 1 result.
template <class TFloat>
static void PartialMatrixDotVector1(const int8_t *wi, const TFloat *scales, const int8_t *u,
                                    int num_in, TFloat *v) {
  TFloat total = IntDotProductSSE(u, wi, num_in);
  // Add in the bias and correct for integer values.
  *v = (total + wi[num_in] * TFloat(INT8_MAX)) * *scales;
}

template <class TFloat>
static void matrixDotVector(int dim1, int dim2, const int8_t *wi, const TFloat *scales,
                            const int8_t *u, TFloat *v) {
  const int num_out = dim1;
  const int num_in = dim2 - 1;
  int output = 0;

  for (; output < num_out; output++) {
    PartialMatrixDotVector1(wi, scales, u, num_in, v);
    wi += dim2;
    scales++;
    v++;
  }
}

static const IntSimdMatrix simdMatrix = {
    matrixDotVector,
    // Number of 32 bit outputs held in each register.
    1,
    // Maximum number of registers that we will use to hold outputs.
    1,
    // Number of 8 bit inputs in the inputs register.
    1,
    // Number of inputs in each weight group.
    1
};

const IntSimdMatrix *IntSimdMatrix::intSimdMatrixSSE = &simdMatrix;

} // namespace tesseract.

#else

namespace tesseract {

	const IntSimdMatrix* IntSimdMatrix::intSimdMatrixSSE = nullptr;

} // namespace tesseract.

#endif
