// SPDX-License-Identifier: Apache-2.0
// File:        autosupressor.h
// Description: xxxxxxxx.
// Author:      Ger Hobbelt
//
// (C) Copyright 2023
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TESSERACT_AUTOSUPRESSOR_H_
#define TESSERACT_AUTOSUPRESSOR_H_

#include "export.h" // for TESS_API

#include <limits.h>

namespace tesseract {

  class AutoSupressDatum;
  class TESS_API TessBaseAPI;
  class TESS_API Tesseract;
  
  typedef void AutoSupressDatumEventHandler(AutoSupressDatum *datum, TessBaseAPI *api_ref, Tesseract *ocr_ref);

  class AutoSupressDatum {
  public:
    AutoSupressDatum(TessBaseAPI *api_ref, Tesseract *ocr_ref = nullptr, AutoSupressDatumEventHandler *event_handler = nullptr) :
        api_ref_(api_ref), 
        ocr_ref_(ocr_ref), 
        event_handler_(event_handler), 
        marker_(0) {}

    virtual ~AutoSupressDatum() {
		clear();
    }

    AutoSupressDatum(const AutoSupressDatum &o) = delete;
    AutoSupressDatum(AutoSupressDatum &&o) = delete;

    AutoSupressDatum &operator=(const AutoSupressDatum &other) = delete;
    AutoSupressDatum &operator=(AutoSupressDatum &&other) = delete;

    void clear() {
        if (marker_ > 0) {
            marker_ = 0;
            fire();
        }
    }

    operator bool() const {
        return marker_ == 0;
    }
    operator int() const {
        return marker_;
    }

    void increment() {
        marker_++;
    }

    void decrement() {
        --marker_;
        if (marker_ == 0) {
          fire();
        }
    }

    void fire() {
        if (event_handler_) {
          event_handler_(this, api_ref_, ocr_ref_);
        }
        // make sure we only fire *once*:
        marker_ = INT_MIN / 2;
    }

  protected:
    TessBaseAPI *api_ref_;
    Tesseract *ocr_ref_;
    AutoSupressDatumEventHandler *event_handler_;
    int marker_;
  };

  // Counter which auto-increments at construction time and auto-decrements
  // at destructor time and can serve (for example) as auto-supressor for
  // certain functionality.
  // 
  // In our case, it's used to auto-supress debug_pixa based logging+images HTML
  // output that is invoked too early in the overall OCR process.
  class AutoSupressMarker {
  public:
    AutoSupressMarker(AutoSupressDatum& supressor_mark_counter)
        : marker_(supressor_mark_counter) {
      marker_.increment();
      stepped_ = true;
    }

    void stepdown() {
        if (stepped_) {
            marker_.decrement();
            stepped_ = false;
        }
    }

    ~AutoSupressMarker() {
      if (stepped_) {
        marker_.decrement();
        stepped_ = false;
      }
    }

  protected:
    AutoSupressDatum& marker_;
    bool stepped_;
  };

} // namespace tesseract

#endif // TESSERACT_CCMAIN_OSDETECT_H_
