/**********************************************************************
 * File:        memcost_estimate.h
 * Description: Declare the light memory capacity cost info class
 * Author:      Ger Hobbelt
 *
 * (C) Copyright 1990, Hewlett-Packard Ltd.
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 *
 **********************************************************************/

#ifndef T_MEMCOST_ESTIMATE_H
#define T_MEMCOST_ESTIMATE_H

#include <string>

namespace tesseract {

  // Image memory capacity cost estimate report. Cost is measured in BYTES. Cost is reported
  // (`to_string()`) in GBYTES.
  //
  // Uses `allowed_image_memory_capacity` plus some compile-time heuristics to indicate
  // whether the estimated cost is oversized --> `cost.is_too_large()`
  struct ImageCostEstimate {
    float cost;

  protected:
    float allowed_image_memory_capacity;

  public:
    ImageCostEstimate()
      : ImageCostEstimate(0.0f, 1e30f) {
    }

    ImageCostEstimate(float c, float allowance = 1e30f);

    static float get_max_system_allowance();

    float get_max_allowance() const;

    void set_max_allowance(float allowance);

    bool is_too_large() const;

    std::string to_string() const;

    // implicit conversion
    operator std::string() const;

    static std::string capacity_to_string(float cost);
  };

} // namespace tesseract.

#endif
