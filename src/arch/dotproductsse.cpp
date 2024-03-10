///////////////////////////////////////////////////////////////////////
// File:        dotproductsse.cpp
// Description: Architecture-specific dot-product function.
// Author:      Ray Smith
//
// (C) Copyright 2015, Google Inc.
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

#include "dotproduct.h"

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

// ---------------------------- FAST FLOAT section ------------------------

// Computes and returns the dot product of the n-vectors u and v.
// Uses Intel SSE intrinsics to access the SIMD instruction set.
float DotProductSSE(const float *u, const float *v, int n) {
  int max_offset = n - 4;
  int offset = 0;
  // Accumulate a set of 4 sums in sum, by loading pairs of 4 values from u and
  // v, and multiplying them together in parallel.
  __m128 sum = _mm_setzero_ps();
  if (offset <= max_offset) {
    offset = 4;
    // Aligned load is reputedly faster but requires 16 byte aligned input.
    if ((reinterpret_cast<uintptr_t>(u) & 15) == 0 &&
        (reinterpret_cast<uintptr_t>(v) & 15) == 0) {
      // Use aligned load.
      sum = _mm_load_ps(u);
      __m128 floats2 = _mm_load_ps(v);
      // Multiply.
      sum = _mm_mul_ps(sum, floats2);
      while (offset <= max_offset) {
        __m128 floats1 = _mm_load_ps(u + offset);
        floats2 = _mm_load_ps(v + offset);
        floats1 = _mm_mul_ps(floats1, floats2);
        sum = _mm_add_ps(sum, floats1);
        offset += 4;
      }
    } else {
      // Use unaligned load.
      sum = _mm_loadu_ps(u);
      __m128 floats2 = _mm_loadu_ps(v);
      // Multiply.
      sum = _mm_mul_ps(sum, floats2);
      while (offset <= max_offset) {
        __m128 floats1 = _mm_loadu_ps(u + offset);
        floats2 = _mm_loadu_ps(v + offset);
        floats1 = _mm_mul_ps(floats1, floats2);
        sum = _mm_add_ps(sum, floats1);
        offset += 4;
      }
    }
  }
  // Add the 4 sums in sum horizontally.
#if 0
  alignas(32) float tmp[4];
  _mm_store_ps(tmp, sum);
  float result = tmp[0] + tmp[1] + tmp[2] + tmp[3];
#else
  __m128 zero = _mm_setzero_ps();
  // https://www.felixcloutier.com/x86/haddps
  sum = _mm_hadd_ps(sum, zero);
  sum = _mm_hadd_ps(sum, zero);
  // Extract the low result.
  float result = _mm_cvtss_f32(sum);
#endif
  // Add on any left-over products.
  while (offset < n) {
    result += u[offset] * v[offset];
    ++offset;
  }
  return result;
}

// ---------------------------- HIGH-PRECISION DOUBLE section ------------------------

double DotProductSSE(const double *u, const double *v, int n) {
  int max_offset = n - 2;
  int offset = 0;
  // Accumulate a set of 2 sums in sum, by loading pairs of 2 values from u and
  // v, and multiplying them together in parallel.
  __m128d sum = _mm_setzero_pd();
  if (offset <= max_offset) {
    offset = 2;
    // Aligned load is reputedly faster but requires 16 byte aligned input.
    if ((reinterpret_cast<uintptr_t>(u) & 15) == 0 &&
        (reinterpret_cast<uintptr_t>(v) & 15) == 0) {
      // Use aligned load.
      sum = _mm_load_pd(u);
      __m128d floats2 = _mm_load_pd(v);
      // Multiply.
      sum = _mm_mul_pd(sum, floats2);
      while (offset <= max_offset) {
        __m128d floats1 = _mm_load_pd(u + offset);
        floats2 = _mm_load_pd(v + offset);
        offset += 2;
        floats1 = _mm_mul_pd(floats1, floats2);
        sum = _mm_add_pd(sum, floats1);
      }
    } else {
      // Use unaligned load.
      sum = _mm_loadu_pd(u);
      __m128d floats2 = _mm_loadu_pd(v);
      // Multiply.
      sum = _mm_mul_pd(sum, floats2);
      while (offset <= max_offset) {
        __m128d floats1 = _mm_loadu_pd(u + offset);
        floats2 = _mm_loadu_pd(v + offset);
        offset += 2;
        floats1 = _mm_mul_pd(floats1, floats2);
        sum = _mm_add_pd(sum, floats1);
      }
    }
  }
  // Add the 2 sums in sum horizontally.
  // https://www.felixcloutier.com/x86/haddpd
  sum = _mm_hadd_pd(sum, sum);
  // Extract the low result.
  double result = _mm_cvtsd_f64(sum);
  // Add on any left-over products.
  while (offset < n) {
    result += u[offset] * v[offset];
    ++offset;
  }
  return result;
}

// ---------------------------- END section ------------------------

} // namespace tesseract.

#else

namespace tesseract {

	// Computes and returns the dot product of the n-vectors u and v.
	// Uses Intel FMA intrinsics to access the SIMD instruction set.
	float DotProductSSE(const float* u, const float* v, int n) {
		return DotProductNative(u, v, n);
	}
	double DotProductSSE(const double* u, const double* v, int n) {
		return DotProductNative(u, v, n);
	}

}

#endif
