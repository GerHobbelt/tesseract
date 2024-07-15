///////////////////////////////////////////////////////////////////////
// File:        dotproductavx512.cpp
// Description: Architecture-specific dot-product function.
// Author:      Stefan Weil
//
// (C) Copyright 2022
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
#if defined(__AVX__) || defined(_M_IX86) || defined(_M_X64)

#  include <immintrin.h>
#  include <cstdint>
#  include "dotproduct.h"

namespace tesseract {

// Computes and returns the dot product of the n-vectors u and v.
// Uses Intel AVX intrinsics to access the SIMD instruction set.
#  if defined(FAST_FLOAT)

float DotProductAVX512F(const float *u, const float *v, int n) {
  const unsigned quot = n / 16;
  const unsigned rem = n % 16;
  __m512 t0 = _mm512_setzero_ps();
  for (unsigned k = 0; k < quot; k++) {
    __m512 f0 = _mm512_loadu_ps(u);
    __m512 f1 = _mm512_loadu_ps(v);
    t0 = _mm512_fmadd_ps(f0, f1, t0);
    u += 16;
    v += 16;
  }
  float result = _mm512_reduce_add_ps(t0);
  for (unsigned k = 0; k < rem; k++) {
    result += *u++ * *v++;
  }
  return result;
}

#  else

double DotProductAVX512F(const double *u, const double *v, int n) {
  const unsigned quot = n / 8;
  const unsigned rem = n % 8;
  __m512d t0 = _mm512_setzero_pd();
  for (unsigned k = 0; k < quot; k++) {
    t0 = _mm512_fmadd_pd(_mm512_loadu_pd(u), _mm512_loadu_pd(v), t0);
    u += 8;
    v += 8;
  }
  double result = _mm512_reduce_add_pd(t0);
  for (unsigned k = 0; k < rem; k++) {
    result += *u++ * *v++;
  }
  return result;
}

#  endif

} // namespace tesseract.

#endif
