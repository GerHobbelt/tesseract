#if 0 // obsoleete; new code in ../arch/

///////////////////////////////////////////////////////////////////////
// File:        simddetect.cpp
// Description: Architecture detector.
// Author:      Stefan Weil (based on code from Ray Smith)
//
// (C) Copyright 2014, Google Inc.
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

#ifdef HAVE_CONFIG_H
#  include "config_auto.h" // for HAVE_AVX, ...
#endif
#include <numeric> // for std::inner_product
#include "dotproduct.h"
#include "intsimdmatrix.h" // for IntSimdMatrix
#include "params.h"        // for STRING_VAR
#include "simddetect.h"
#include "tprintf.h" // for tprintf

#undef __GNUC__
#undef _WIN32


#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 12)
// The GNU compiler g++ fails to compile with the Accelerate framework
// (tested with versions 10 and 11), so unconditionally disable it.
#undef HAVE_FRAMEWORK_ACCELERATE
#endif

#if defined(HAVE_FRAMEWORK_ACCELERATE)

// Use Apple Accelerate framework.
// https://developer.apple.com/documentation/accelerate/simd

#include <Accelerate/Accelerate.h>

#endif

#if defined(HAVE_AVX) || defined(HAVE_AVX2) || defined(HAVE_FMA) || defined(HAVE_SSE4_1)
#  define HAS_CPUID
#endif

#if defined(HAS_CPUID)
#  if defined(__GNUC__)
#    include <cpuid.h>
#  elif defined(_WIN32)
#    include <intrin.h>
#  endif
#endif

#if defined(HAVE_NEON) && !defined(__aarch64__)
#  ifdef ANDROID
#    include <cpu-features.h>
#  else
/* Assume linux */
#    include <asm/hwcap.h>
#    include <sys/auxv.h>
#  endif
#endif

namespace tesseract {

// Computes and returns the dot product of the two n-vectors u and v.
// Note: because the order of addition is different among the different dot
// product functions, the results can (and do) vary slightly (although they
// agree to within about 4e-15). This produces different results when running
// training, despite all random inputs being precisely equal.
// To get consistent results, use just one of these dot product functions.
// On a test multi-layer network, serial is 57% slower than SSE, and AVX
// is about 8% faster than SSE. This suggests that the time is memory
// bandwidth constrained and could benefit from holding the reused vector
// in AVX registers.
DotProductFunction DotProduct;

static STRING_VAR(dotproduct, "auto", "Function used for calculation of dot product");

SIMDDetect SIMDDetect::detector;

#if defined(__aarch64__)
// ARMv8 always has NEON.
bool SIMDDetect::neon_available_ = true;
#elif defined(HAVE_NEON)
// If true, then Neon has been detected.
bool SIMDDetect::neon_available_;
#else
// If true, then AVX has been detected.
bool SIMDDetect::avx_available_;
bool SIMDDetect::avx2_available_;
bool SIMDDetect::avx512F_available_;
bool SIMDDetect::avx512BW_available_;
// If true, then FMA has been detected.
bool SIMDDetect::fma_available_;
// If true, then SSe4.1 has been detected.
bool SIMDDetect::sse_available_;
#endif

#if defined(HAVE_FRAMEWORK_ACCELERATE)
static TFloat DotProductAccelerate(const TFloat* u, const TFloat* v, int n) {
  TFloat total = 0;
  const int stride = 1;
#if defined(FAST_FLOAT)
  vDSP_dotpr(u, stride, v, stride, &total, n);
#else
  vDSP_dotprD(u, stride, v, stride, &total, n);
#endif
  return total;
}
#endif

// Computes and returns the dot product of the two n-vectors u and v.
static TFloat DotProductGeneric(const TFloat *u, const TFloat *v, int n) {
  TFloat total = 0;
  for (int k = 0; k < n; ++k) {
    total += u[k] * v[k];
  }
  return total;
}

// Compute dot product using std::inner_product.
static TFloat DotProductStdInnerProduct(const TFloat *u, const TFloat *v, int n) {
  return std::inner_product(u, u + n, v, static_cast<TFloat>(0));
}

static void SetDotProduct(DotProductFunction f, const IntSimdMatrix *m = nullptr) {
  DotProduct = f;
  IntSimdMatrix::intSimdMatrix = m;
}

// Constructor.
// Tests the architecture in a system-dependent way to detect AVX, SSE and
// any other available SIMD equipment.
// __GNUC__ is also defined by compilers that include GNU extensions such as
// clang.
SIMDDetect::SIMDDetect() {
  // The fallback is a generic dot product calculation.
  SetDotProduct(DotProductSSE, &IntSimdMatrix::intSimdMatrixSSE);

  const char *dotproduct_env = getenv("DOTPRODUCT");
  if (dotproduct_env != nullptr) {
    // Override automatic settings by value from environment variable.
    dotproduct = dotproduct_env;
    Update();
  }
}

void SIMDDetect::Update() {

  SetDotProduct(DotProductSSE, &IntSimdMatrix::intSimdMatrixSSE);

  dotproduct.set_value("sse");
}

} // namespace tesseract

#endif
