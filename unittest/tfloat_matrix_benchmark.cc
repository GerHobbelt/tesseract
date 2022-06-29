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
#include "tesstypes.h"
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
		// did the difference occur?
		//
		// However, when one or both are negative, we need to act slightly different
		// as we are primarily interested in their relative *distance*.
		// The log() of their distance will tell us the first different significant
		// digit in there, so instead of comparing log(a) and log(b) we can
		// compare log(abs(a)) and log(abs(distance(a, b))), i.e. log(abs(a - b))!
		auto aa = log(fabs(a) + 1e-25);  // makes sure the value going into the log() is never zero
		auto ld = log(fabs(a - b) + 1e-25);  // makes sure the value going into the log() is never zero
		// now we're close enough, when the diff is ~6 digits below the most significant digit of a (= power of a).
		diff = aa - ld;
		return (diff >= 13);
	}



	const int DIM_MAX = 400;    // maximum 1-dimensional size of the matrices tested. --> biggest matrix will be DIMxDIM cells.



	// Note: as the initial benchmark runs indicated that the InitRandom() call plus setup was eating over 50% of the total
	// CPU time, most of it spent in the random generator, we 'fix' that by collecting a big ream of semi-random data
	// ONCE and then cycling through it from then on.
	// As long as we collect enough random samples up front, we still would be using good quality random data in every run.
	//
	// Total amount needed: one round takes DIM*(DIM+1) samples. We've got DIM*DIM rounds, but we don't mind re-using
	// random samples, just as long as there's a reasonable guarantee that the matrixes will not be just scaled up copies.
	// To accomplish that, we make sure the number of samples in the cache is mutual prime to DIM*(DIM+1): adding a prime (51)
	// other than 1 does not exactly *guarantee* this under all mathematical conditions, but it's good enough for a
	// table-top benchmark.
	// When you don't agree, tweak this to your heart's content. <wink/>
	//
	static int8_t random_data_cache[DIM_MAX * (DIM_MAX + 1) + 51];
	static TFloat frandom_data_cache[DIM_MAX + 43]; // same story for the scales: DIM per round, mutual prime cache size
	static int random_data_cache_index = -1;
	static int frandom_data_cache_index = -1;

	static void InitRandomCache(void) {
		TRand random_;
		random_.set_seed("tesseract performance testing");
		for (int i = 0; i < countof(random_data_cache); i++) {
			random_data_cache[i] = static_cast<int8_t>(random_.SignedRand(INT8_MAX));
		}
		for (int i = 0; i < countof(frandom_data_cache); i++) {
			frandom_data_cache[i] = (1.0 + random_.SignedRand(1.0)) / INT8_MAX;
		}
	}

	// cycle through the random pool while we fetch one random value on each call.
	static auto get_random_sample() {
		random_data_cache_index++;
		if (random_data_cache_index >= countof(random_data_cache)) {
			random_data_cache_index = 0;
		}
		return random_data_cache[random_data_cache_index];
	}
	static auto get_frandom_sample() {
		frandom_data_cache_index++;
		if (frandom_data_cache_index >= countof(frandom_data_cache)) {
			frandom_data_cache_index = 0;
		}
		return frandom_data_cache[frandom_data_cache_index];
	}
	static auto reset_random_pool_index() {
		random_data_cache_index = -1;
		frandom_data_cache_index = -1;
	}


	class MatrixChecker {
	public:
		void SetUp() {
			std::locale::global(std::locale(""));
			InitRandomCache();
		}

		// Makes a random weights matrix of the given size.
		GENERIC_2D_ARRAY<int8_t> InitRandom(int no, int ni) {
			GENERIC_2D_ARRAY<int8_t> a(no, ni, 0);
			for (int i = 0; i < no; ++i) {
				for (int j = 0; j < ni; ++j) {
					a(i, j) = get_random_sample();
				}
			}
			return a;
		}

		// Makes a random input vector of the given size, with rounding up.
		std::vector<int8_t> RandomVector(int size, const IntSimdMatrix& matrix) {
			int rounded_size = matrix.RoundInputs(size);
			std::vector<int8_t> v(rounded_size, 0);
			for (int i = 0; i < size; ++i) {
				v[i] = get_random_sample();
			}
			return v;
		}

		// Makes a random scales vector of the given size.
		std::vector<TFloat> RandomScales(int size) {
			std::vector<TFloat> v(size);
			for (int i = 0; i < size; ++i) {
				v[i] = get_frandom_sample();
			}
			return v;
		}

		// Tests a range of sizes and compares the results against the generic version.
		void ExpectEqualResults(const IntSimdMatrix& matrix) {
			// reset random generator as well, so we can be assured we'll get the same semi-random data for this test!
			//random_.set_seed("tesseract performance testing");
			reset_random_pool_index();
			//const int DIM_MAX = 200;

			TFloat total = 0.0;
			for (int num_out = DIM_MAX * 126 / 128; num_out < DIM_MAX; ++num_out) {
				for (int num_in = DIM_MAX * 126 / 128; num_in < DIM_MAX; ++num_in) {
					GENERIC_2D_ARRAY<int8_t> w = InitRandom(num_out, num_in + 1);
					std::vector<int8_t> u = RandomVector(num_in, matrix);
					std::vector<TFloat> scales = RandomScales(num_out);
					for (int iter = 0; iter < 300; ++iter) {
						// slowly mutate the shaped matrix so it's not a regurgitation of same ol' same ol' while we run the calculations:
						int8_t x = abs(get_random_sample());
						x = x % w.dim1();
						w(x, 0) = get_random_sample();

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
			}
			// Compare sum of all results with expected value.
			const double sollwert = -3115826.00;

			if (!approx_eq(total, sollwert)) {
				fprintf(stderr, "FAIL: matrix: %lf\n", total);
			}
		}
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
#if 0
		if (IntSimdMatrix::intSimdMatrix != nullptr) {
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);

			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);
			cls.ExpectEqualResults(*IntSimdMatrix::intSimdMatrix);
		}
#endif
	}

} // namespace tesseract
