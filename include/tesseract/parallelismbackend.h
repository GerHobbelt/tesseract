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

#ifndef TESSERACT_PARALLELISMBACKEND_H_
#define TESSERACT_PARALLELISMBACKEND_H_

#include "export.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace tesseract {

class TESS_API ParallelSettings {
public:
  static constexpr unsigned ANY_THREAD_COUNT = 0;

  ParallelSettings();
  ~ParallelSettings();
  ParallelSettings& SetMultiThreadingEnabled(bool enabled);
  bool IsMultiThreadingEnabled() const;
  ParallelSettings& SetThreadCount(unsigned count);
  unsigned ThreadCount() const;

private:
  // Even though the class could be POD, the data is hidden behind a pointer to allow
  // adding new options without breaking ABI.
  struct Data;
  std::unique_ptr<Data> d_;
};

/**
 * Implements parallelism primitives. This allows the user to configure tesseract to use user's
 * own thread pool for example.
 */
class TESS_API ParallelismBackend {
public:
  // Arguments: current index, and thread number in team executing the parallel construct
  using ParallelForCallback = std::function<void (std::int64_t, int)>;

  virtual ~ParallelismBackend();

  template<class F>
  void ParallelFor(std::int64_t lower_bound, std::int64_t upper_bound, F&& callback) {
    ParallelForImpl(lower_bound, upper_bound, ParallelSettings(),
                    [&](std::int64_t i, int thread_id) { callback(i); });
  }

  template<class F>
  void ParallelFor(std::int64_t lower_bound, std::int64_t upper_bound,
                   const ParallelSettings& settings, F&& callback) {
    ParallelForImpl(lower_bound, upper_bound, settings,
                    [&](std::int64_t i, int thread_id) { callback(i); });
  }

  template<class F>
  void ParallelForWithThreadId(std::int64_t lower_bound, std::int64_t upper_bound,
                               const ParallelSettings& settings, F&& callback) {
    ParallelForImpl(lower_bound, upper_bound, settings, callback);
  }

  virtual int GetMaxThreadCount() const = 0;

protected:
  virtual void ParallelForImpl(std::int64_t lower_bound, std::int64_t upper_bound,
                               const ParallelSettings& settings,
                               const ParallelForCallback& callback) = 0;
};

/**
 * Implements no parallelism.
 */
class TESS_API ParallelismBackendSingleThread : public ParallelismBackend {
public:
  ParallelismBackendSingleThread();
  ~ParallelismBackendSingleThread() override;

  void ParallelForImpl(std::int64_t lower_bound, std::int64_t upper_bound,
                       const ParallelSettings& settings,
                       const ParallelForCallback& callback) override;

  int GetMaxThreadCount() const override;
};

#ifdef _OPENMP
/**
 * Implements parallelism in terms of primitives provided by OpenMP.
 */
class TESS_API ParallelismBackendOpenMP : public ParallelismBackend {
public:
  ParallelismBackendOpenMP();
  ~ParallelismBackendOpenMP() override;

  void ParallelForImpl(std::int64_t lower_bound, std::int64_t upper_bound,
                       const ParallelSettings& settings,
                       const ParallelForCallback& callback) override;

  int GetMaxThreadCount() const override;
private:
  struct Data;
  std::unique_ptr<Data> d_;
};
#endif // _OPENMP

std::unique_ptr<ParallelismBackend> GetDefaultParallelismBackend();

} // namespace tesseract

#endif
