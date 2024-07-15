/**********************************************************************
 * File:        memcost_estimate.cpp 
 * Description: Implement the light memory capacity cost info class; actual cost calculation happens elsewhere.
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

#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/baseapi.h> // FileReader
#include <cstdint>             
#include <cstdlib>
#include <cstring>
#include <climits>

#include <tesseract/memcost_estimate.h>


namespace tesseract {

  ImageCostEstimate::ImageCostEstimate(float c, float allowance)
    : cost(c), allowed_image_memory_capacity(allowance) {
  }

  float ImageCostEstimate::get_max_system_allowance() {
    const float max_reasonable_RAM_usage = (sizeof(size_t) <= 4 ? 1.5e9 /* 1.5 GByte) */ : 64e9 /* 64 GByte */);
    return max_reasonable_RAM_usage;
  }

  float ImageCostEstimate::get_max_allowance() const {
    return fmin(get_max_system_allowance(), allowed_image_memory_capacity);
  }

  void ImageCostEstimate::set_max_allowance(float allowance) {
    allowed_image_memory_capacity = allowance;
  }

  bool ImageCostEstimate::is_too_large() const {
    return cost > get_max_allowance();
  }

  std::string ImageCostEstimate::capacity_to_string(float cost) {
    if (cost < 0)
      return "?negative/NaN cost?";

    char buf[30];
    int pwr = (int) log10f(cost);
    pwr++;
    int mil = pwr / 3;
    static const char* range[] = { "", "K", "M", "G", "T", "P" };
    const int max_range = sizeof(range) / sizeof(range[0]) - 1;
    int prec = 3 - (pwr % 3); // % --> {0,1,2}--> {3,2,1} --> {2,2,1}
    if (prec > 2) {
      prec = 0;
      mil--;
    }
    if (mil <= 0) {  // `mil` can now be -1 when cost is tiny (few bytes) or hard negative for cost==NaN
      prec = 0;      // and there cannot be *partial* bytes in a cost report
      mil = 0;
    }
    if (mil > max_range) {
      mil = max_range;
      prec = 0;
    }

    float val = cost / pow(10, mil * 3);
    snprintf(buf, sizeof(buf), "%0.*f %sByte%s", prec, val, range[mil], (mil == 0 ? "s" : ""));
    return buf;
  }

  std::string ImageCostEstimate::to_string() const {
    // warning C5263: calling 'std::move' on a temporary object prevents copy elision
    return ImageCostEstimate::capacity_to_string(cost);
  }

  // implicit conversion
  ImageCostEstimate:: operator std::string() const {
    // warning C5263: calling 'std::move' on a temporary object prevents copy elision
    return to_string();
  }

} // namespace tesseract.



// test code:

#if 0

#include "tesseractclass.h"

using namespace tesseract;

int main() {
  auto cost = Tesseract::EstimateImageMemoryCost(1280,1920, 1.0e30f);
  std::string cost_report = cost;
  printf("Demo: estimated memory pressure: %s\nDisplay test ...\n", cost_report.c_str());

  for (float sz = 0; sz < 1e30; sz += 1 + sz * 0.3) {
    cost.cost = sz;
    cost_report = cost;
    printf("%s\n", cost_report.c_str());
  }
  return 0;
}

/*
Expected: 3 significant digits in display. Tested ! OK ! for *extended* expected working range, up to Petabytes.

Display test output:

0 Bytes
1 Bytes
2 Bytes
4 Bytes
6 Bytes
9 Bytes
13 Bytes
18 Bytes
24 Bytes
32 Bytes
43 Bytes
56 Bytes
74 Bytes
98 Bytes
128 Bytes
167 Bytes
218 Bytes
285 Bytes
372 Bytes
484 Bytes
630 Bytes
820 Bytes
1.07 KByte
1.39 KByte
1.81 KByte
2.35 KByte
3.05 KByte
3.97 KByte
5.16 KByte
6.71 KByte
8.73 KByte
11.3 KByte
14.8 KByte
19.2 KByte
24.9 KByte
32.4 KByte
42.2 KByte
54.8 KByte
71.2 KByte
92.6 KByte
120 KByte
157 KByte
203 KByte
265 KByte
344 KByte
447 KByte
581 KByte
755 KByte
982 KByte
1.28 MByte
1.66 MByte
2.16 MByte
2.80 MByte
3.65 MByte
4.74 MByte
6.16 MByte
8.01 MByte
10.4 MByte
13.5 MByte
17.6 MByte
22.9 MByte
29.7 MByte
38.7 MByte
50.3 MByte
65.4 MByte
85.0 MByte
110 MByte
144 MByte
187 MByte
243 MByte
315 MByte
410 MByte
533 MByte
693 MByte
901 MByte
1.17 GByte
1.52 GByte
1.98 GByte
2.57 GByte
3.35 GByte
4.35 GByte
5.65 GByte
7.35 GByte
9.55 GByte
12.4 GByte
16.1 GByte
21.0 GByte
27.3 GByte
35.5 GByte
46.1 GByte
59.9 GByte
77.9 GByte
101 GByte
132 GByte
171 GByte
223 GByte
289 GByte
376 GByte
489 GByte
636 GByte
826 GByte
1.07 TByte
1.40 TByte
1.82 TByte
2.36 TByte
3.07 TByte
3.99 TByte
5.19 TByte
6.74 TByte
8.76 TByte
11.4 TByte
14.8 TByte
19.3 TByte
25.0 TByte
32.5 TByte
42.3 TByte
55.0 TByte
71.5 TByte
92.9 TByte
121 TByte
157 TByte
204 TByte
265 TByte
345 TByte
449 TByte
583 TByte
758 TByte
986 TByte
1.28 PByte
1.67 PByte
2.17 PByte
2.81 PByte
3.66 PByte
4.76 PByte
6.18 PByte
8.04 PByte
10.5 PByte
13.6 PByte
17.7 PByte
23.0 PByte
29.9 PByte
38.8 PByte
50.4 PByte
65.6 PByte
85.3 PByte
111 PByte
144 PByte
187 PByte
243 PByte
317 PByte
412 PByte
535 PByte
695 PByte
904 PByte
1175 PByte
1528 PByte
1986 PByte
2582 PByte
3357 PByte
4364 PByte
5673 PByte
7375 PByte
9587 PByte
12464 PByte
16203 PByte
21064 PByte
27383 PByte
35597 PByte
46277 PByte
60160 PByte
78208 PByte
101670 PByte
132171 PByte
171822 PByte
223369 PByte
290379 PByte
377493 PByte
490741 PByte
637963 PByte
829352 PByte
1078158 PByte
1401605 PByte
1822086 PByte
2368712 PByte
3079326 PByte
4003124 PByte
5204061 PByte
6765279 PByte
8794863 PByte
11433322 PByte
14863319 PByte
19322314 PByte
25119008 PByte
32654710 PByte
42451124 PByte
55186460 PByte
71742400 PByte
93265120 PByte
121244648 PByte
157618048 PByte
204903456 PByte
266374496 PByte
346286848 PByte
450172896 PByte
585224768 PByte
760792128 PByte
989029824 PByte
1285738752 PByte
1671460352 PByte
2172898560 PByte
2824768000 PByte
3672198656 PByte
4773857792 PByte
6206015488 PByte
8067820032 PByte
10488166400 PByte
13634615296 PByte
17724999680 PByte
23042500608 PByte
29955252224 PByte
38941827072 PByte
50624376832 PByte
65811693568 PByte
85555200000 PByte
111221760000 PByte
144588275712 PByte
187964751872 PByte
244354187264 PByte
317660463104 PByte
412958588928 PByte
536846139392 PByte
697900007424 PByte
907270029312 PByte
1179451129856 PByte
1533286416384 PByte
1993272328192 PByte
2591253856256 PByte
3368630091776 PByte
4379219329024 PByte
5692984918016 PByte
7400880865280 PByte
9621145124864 PByte
12507488976896 PByte
16259735355392 PByte
21137656381440 PByte
27478955393024 PByte
35722639704064 PByte
46439434551296 PByte
60371268272128 PByte
78482650431488 PByte
102027443044352 PByte
132635678474240 PByte
172426394599424 PByte
224154309623808 PByte
291400579022848 PByte
378820746018816 PByte
492466989957120 PByte
640207086944256 PByte
832269166051328 PByte

*/

#endif
