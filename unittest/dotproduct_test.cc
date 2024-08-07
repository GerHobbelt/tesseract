///////////////////////////////////////////////////////////////////////
// File:        dotproduct_test.cc
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

#include <gtest/gtest.h>
#include <gtest/internal/gtest-port.h>
#include <memory>
#include <vector>
#include "include_gunit.h"
#include "dotproduct.h"
#include "matrix.h"
#include "simddetect.h"
#include <tesseract/tprintf.h>

namespace tesseract {
class DotProductTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::locale::global(std::locale(""));
    u[0] = static_cast<TFloat>(1.41421);
    v[0] = static_cast<TFloat>(1.41421);
  }
  void RunTest(TFloat (*f)(const TFloat *u, const TFloat *v, int n));
  static const size_t multiplications = 5000000000ULL;
  static const size_t n = 40;
  //static const size_t n = 1000000;
  TFloat u[n];
  TFloat v[n];
};

void DotProductTest::RunTest(TFloat (*f)(const TFloat *u, const TFloat *v, int n)) {
  TFloat dp = 0;
  for (auto i = multiplications / n; i > 0; i--) {
    dp = f(u, v, n);
  }
  if (std::abs(2.0 - dp) > 0.0001) {
    printf("warning: dp=%f, expected %f\n", dp, 2.0);
  }
}

static TFloat DotProductGeneric(const TFloat *u, const TFloat *v, int n) {
  TFloat total = 0;
#pragma omp simd reduction(+:total)
  for (int k = 0; k < n; ++k) {
    total += u[k] * v[k];
  }
  return total;
}

// Test the C++ implementation without SIMD.
TEST_F(DotProductTest, C) {
  RunTest(DotProductGeneric);
}

TEST_F(DotProductTest, Native) {
  RunTest(DotProductNative);
}

// Tests that the SSE implementation gets the same result as the vanilla.
TEST_F(DotProductTest, SSE) {
#if defined(HAVE_SSE4_1)
  if (!SIMDDetect::IsSSEAvailable()) {
    GTEST_LOG_(INFO) << "No SSE found! Not tested!";
    GTEST_SKIP();
  }
  RunTest(DotProductSSE);
#else
  GTEST_LOG_(INFO) << "SSE unsupported! Not tested!";
  GTEST_SKIP();
#endif
}

// Tests that the AVX implementation gets the same result as the vanilla.
TEST_F(DotProductTest, AVX) {
#if defined(HAVE_AVX2)
  if (!SIMDDetect::IsAVX2Available()) {
    GTEST_LOG_(INFO) << "No AVX2 found! Not tested!";
    GTEST_SKIP();
  }
  RunTest(DotProductAVX);
#else
  GTEST_LOG_(INFO) << "AVX2 unsupported! Not tested!";
  GTEST_SKIP();
#endif
}

// Tests that the AVX1 implementation gets the same result as the vanilla.
TEST_F(DotProductTest, AVX1) {
#if defined(HAVE_AVX2)
  if (!SIMDDetect::IsAVX2Available()) {
    GTEST_LOG_(INFO) << "No AVX2 found! Not tested!";
    GTEST_SKIP();
  }
  RunTest(DotProductAVX1);
#else
  GTEST_LOG_(INFO) << "AVX2 unsupported! Not tested!";
  GTEST_SKIP();
#endif
}

// Tests that the FMA implementation gets the same result as the vanilla.
TEST_F(DotProductTest, FMA) {
#if defined(HAVE_FMA)
  if (!SIMDDetect::IsFMAAvailable()) {
    GTEST_LOG_(INFO) << "No FMA found! Not tested!";
    GTEST_SKIP();
  }
  RunTest(DotProductFMA);
#else
  GTEST_LOG_(INFO) << "FMA unsupported! Not tested!";
  GTEST_SKIP();
#endif
}

#if defined(HAVE_FRAMEWORK_ACCELERATE)
TEST_F(DotProductTest, Accelerate) {
  RunTest(DotProductAccelerate);
}
#endif

#if 0
// Tests that the NEON implementation gets the same result as the vanilla.
TEST_F(DotProductTest, NEON) {
#if defined(HAVE_NEON)
  if (!SIMDDetect::IsNEONAvailable()) {
    GTEST_LOG_(INFO) << "No NEON found! Not tested!";
    GTEST_SKIP();
  }
  RunTest(DotProductNEON);
#else
  GTEST_LOG_(INFO) << "NEON unsupported! Not tested!";
  GTEST_SKIP();
#endif
}
#endif

} // namespace tesseract
