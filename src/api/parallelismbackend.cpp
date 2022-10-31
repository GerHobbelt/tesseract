// SPDX-License-Identifier: Apache-2.0
// File:        capi.h
// Description: C-API TessBaseAPI
// Author: Povilas Kanapickas
//
// (C) Copyright 2022, Povilas Kanapickas
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <tesseract/parallelismbackend.h>

#ifdef _OPENMP
#  include <omp.h>
#endif

namespace tesseract {

struct ParallelSettings::Data {
  bool multithreading_enabled = true;
  unsigned thread_count = ParallelSettings::ANY_THREAD_COUNT;
};

ParallelSettings::ParallelSettings() : d_{std::make_unique<Data>()} {}

ParallelSettings::~ParallelSettings() = default;

ParallelSettings& ParallelSettings::SetMultiThreadingEnabled(bool enabled) {
  d_->multithreading_enabled = enabled;
  return *this;
}

bool ParallelSettings::IsMultiThreadingEnabled() const {
  return d_->multithreading_enabled;
}

ParallelSettings& ParallelSettings::SetThreadCount(unsigned count) {
  d_->thread_count = count;
  return *this;
}

unsigned ParallelSettings::ThreadCount() const {
  return d_->thread_count;
}

ParallelismBackend::~ParallelismBackend() = default;

ParallelismBackendSingleThread::ParallelismBackendSingleThread() = default;

ParallelismBackendSingleThread::~ParallelismBackendSingleThread() = default;

void ParallelismBackendSingleThread::ParallelForImpl(std::int64_t lower_bound,
                                                     std::int64_t upper_bound,
                                                     const ParallelSettings& settings,
                                                     const ParallelForCallback &callback)
{
  for (std::int64_t i = lower_bound; i < upper_bound; ++i) {
    callback(i, 0);
  }
}

int ParallelismBackendSingleThread::GetMaxThreadCount() const {
  return 1;
}

#ifdef _OPENMP
// Empty struct to allow adding new data to ParallelismBackendOpenMP without breaking ABI
struct ParallelismBackendOpenMP::Data {};

ParallelismBackendOpenMP::ParallelismBackendOpenMP() : d_{std::make_unique<Data>()} {}

ParallelismBackendOpenMP::~ParallelismBackendOpenMP() = default;

void ParallelismBackendOpenMP::ParallelForImpl(std::int64_t lower_bound,
                                               std::int64_t upper_bound,
                                               const ParallelSettings& settings,
                                               const ParallelForCallback &callback)
{
  if (settings.ThreadCount() != ParallelSettings::ANY_THREAD_COUNT) {
    #pragma omp parallel for if(settings.IsMultiThreadingEnabled()) num_threads(settings.ThreadCount())
    for (std::int64_t i = lower_bound; i < upper_bound; ++i) {
      callback(i, omp_get_thread_num());
    }
  } else {
    #pragma omp parallel for if(settings.IsMultiThreadingEnabled())
    for (std::int64_t i = lower_bound; i < upper_bound; ++i) {
      callback(i, omp_get_thread_num());
    }
  }
}

int ParallelismBackendOpenMP::GetMaxThreadCount() const {
  return omp_get_max_threads();
}
#endif // _OPENMP

std::unique_ptr<ParallelismBackend> GetDefaultParallelismBackend()
{
#ifdef _OPENMP
  return std::make_unique<ParallelismBackendOpenMP>();
#else
  return std::make_unique<ParallelismBackendSingleThread>();
#endif
}

} // namespace tesseract
