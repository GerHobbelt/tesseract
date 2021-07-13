///////////////////////////////////////////////////////////////////////
// File:        tfloat_benchmark.cc
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

#include <memory>
#include <vector>
#include <numeric> // for std::inner_product
#include "include_gunit.h"
#include "matrix.h"
#include "simddetect.h"
#include "tprintf.h"
#include "intsimdmatrix.h" // for IntSimdMatrix
#include "simddetect.h" // for DotProduct
#include "tfloat.h"
#include "intsimdmatrix.h"

// Tests various implementations of costly Matrix calls, using
// AVX/FMA/SSE/NEON/OPENMP/native in both TFloat=float and TFloat=double
// incantations.


namespace tesseract {

	static bool approx_eq(double a, double b) {
		auto diff = a - b;
		if (diff == 0.0)
			return true;
		// take the log of both, as we know all incoming values will be positive,
		// so we can check the precision easily, i.e. at which significant digit
		// did the difference occur? ::
		a = log(a);
		b = log(b);
		diff = a - b;
		return (diff >= -1e-4 && diff <= 1e-4);
	}



	class MatrixChecker {
	public:
		void SetUp() {
			std::locale::global(std::locale(""));
		}

		// Makes a random weights matrix of the given size.
		GENERIC_2D_ARRAY<int8_t> InitRandom(int no, int ni) {
			GENERIC_2D_ARRAY<int8_t> a(no, ni, 0);
			for (int i = 0; i < no; ++i) {
				for (int j = 0; j < ni; ++j) {
					a(i, j) = static_cast<int8_t>(random_.SignedRand(INT8_MAX));
				}
			}
			return a;
		}

		// Makes a random input vector of the given size, with rounding up.
		std::vector<int8_t> RandomVector(int size, const IntSimdMatrix& matrix) {
			int rounded_size = matrix.RoundInputs(size);
			std::vector<int8_t> v(rounded_size, 0);
			for (int i = 0; i < size; ++i) {
				v[i] = static_cast<int8_t>(random_.SignedRand(INT8_MAX));
			}
			return v;
		}

		// Makes a random scales vector of the given size.
		std::vector<TFloat> RandomScales(int size) {
			std::vector<TFloat> v(size);
			for (int i = 0; i < size; ++i) {
				v[i] = (1.0 + random_.SignedRand(1.0)) / INT8_MAX;
			}
			return v;
		}

		// Tests a range of sizes and compares the results against the generic version.
		void ExpectEqualResults(const IntSimdMatrix& matrix) {
			// reset random generator as well, so we can be assured we'll get the same semi-random data for this test!
			random_.set_seed("tesseract performance testing");
			const int DIM_MAX = 200;

			TFloat total = 0.0;
			for (int num_out = 1; num_out < DIM_MAX; ++num_out) {
				for (int num_in = 1; num_in < DIM_MAX; ++num_in) {
					GENERIC_2D_ARRAY<int8_t> w = InitRandom(num_out, num_in + 1);
					std::vector<int8_t> u = RandomVector(num_in, matrix);
					std::vector<TFloat> scales = RandomScales(num_out);
					int ro = num_out;
					if (IntSimdMatrix::intSimdMatrix) {
						ro = IntSimdMatrix::intSimdMatrix->RoundOutputs(ro);
					}
					std::vector<TFloat> base_result(num_out);
					IntSimdMatrix::MatrixDotVector(w, scales, u.data(), base_result.data());
					std::vector<TFloat> test_result(ro);
					std::vector<int8_t> shaped_wi;
					int32_t rounded_num_out;
					matrix.Init(w, shaped_wi, rounded_num_out);
					scales.resize(rounded_num_out);
					if (matrix.matrixDotVectorFunction) {
						matrix.matrixDotVectorFunction(w.dim1(), w.dim2(), &shaped_wi[0], &scales[0], &u[0],
							&test_result[0]);
					}
					else {
						IntSimdMatrix::MatrixDotVector(w, scales, u.data(), test_result.data());
					}
					for (int i = 0; i < num_out; ++i) {
						EXPECT_FLOAT_EQ(base_result[i], test_result[i]) << "i=" << i;
						total += base_result[i];
					}
				}
			}
			// Compare sum of all results with expected value.
			const double sollwert = 675095.938;

			if (!approx_eq(total, sollwert)) {
				fprintf(stderr, "FAIL: matrix: %lf\n", total);
			}
		}

	protected:
		TRand random_;
	};



	void run_tfloat_matrix_benchmark(void)
	{
		MatrixChecker cls;
		cls.SetUp();
		static const IntSimdMatrix matrix = { nullptr, 1, 1, 1, 1 };
		cls.ExpectEqualResults(matrix);

		// check: is random test repeatable?
		cls.ExpectEqualResults(matrix);
		cls.ExpectEqualResults(matrix);
		cls.ExpectEqualResults(matrix);

		// now run the real buggers...

		if (IntSimdMatrix::intSimdMatrixSSE != nullptr) {
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixSSE);

			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixSSE);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixSSE);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixSSE);
		}
		if (IntSimdMatrix::intSimdMatrixAVX2 != nullptr) {
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixAVX2);

			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixAVX2);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixAVX2);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixAVX2);
		}
		if (IntSimdMatrix::intSimdMatrixNEON != nullptr) {
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixNEON);

			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixNEON);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixNEON);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrixNEON);
		}
		if (IntSimdMatrix::intSimdMatrix != nullptr) {
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);

			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);
		}
	}

} // namespace tesseract
