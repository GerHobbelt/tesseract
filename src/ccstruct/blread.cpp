/**********************************************************************
 * File:        blread.cpp  (Formerly pdread.c)
 * Description: Friend function of BLOCK to read the uscan pd file.
 * Author:      Ray Smith
 *
 * (C) Copyright 1991, Hewlett-Packard Ltd.
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

#include "blread.h"

#include "ocrblock.h"  // for BLOCK_IT, BLOCK, BLOCK_LIST (ptr only)
#include "scanutils.h" // for tfscanf

#include <cstdio> // for fclose, fopen, FILE

namespace tesseract {

#define UNLV_EXT ".uzn" // unlv zone file

/**********************************************************************
 * read_unlv_file
 *
 * Read a whole unlv zone file to make a list of blocks.
 **********************************************************************/

bool read_unlv_file(   // print list of sides
    std::string &name, // basename of file
    int32_t xsize,     // image size
    int32_t ysize,     // image size
    BLOCK_LIST *blocks // output list
) {
  FILE *pdfp;   // file pointer
  BLOCK *block; // current block
  int x;        // current top-down coords
  int y;
  int width; // of current block
  int height;
  BLOCK_IT block_it = blocks; // block iterator

  if (!name.ends_with(UNLV_EXT)) {
    name += UNLV_EXT; // add extension
  }
  if ((pdfp = fopen(name.c_str(), "rb")) == nullptr) {
    tprintError("Cannot read UZN file {}.\n", name);
    return false; // didn't read one
  } else {
    while (tfscanf(pdfp, "%d %d %d %d %*s", &x, &y, &width, &height) >= 4) {
      // make rect block
      block = new BLOCK(name.c_str(), true, 0, 0, static_cast<TDimension>(x),
                        static_cast<TDimension>(ysize - y - height), static_cast<TDimension>(x + width),
                        static_cast<TDimension>(ysize - y));
      // on end of list
      block_it.add_to_end(block);
    }
    fclose(pdfp);
  }
  tprintDebug("UZN file {} loaded.\n", name);
  return true;
}

/**********************************************************************
 * write_unlv_file
 *
 * Write a whole unlv zone file from a list of blocks.
 **********************************************************************/

bool write_unlv_file(   // print list of sides
    std::string name, // basename of file
    int32_t xsize,     // image size
    int32_t ysize,     // image size
    const BLOCK_LIST* blocks // block list
) {
  FILE* pdfp;   // file pointer
  int x;        // current top-down coords
  int y;
  int width; // of current block
  int height;
  BLOCK_IT block_it = (BLOCK_LIST *)blocks; // block iterator

  if (!name.ends_with(UNLV_EXT)) {
    name += UNLV_EXT; // add extension
  }
  if ((pdfp = fopen(name.c_str(), "wb")) == nullptr) {
    tprintError("Cannot create UZN file {}.\n", name);
    return false; // didn't write one
  }
  else {
    if (!block_it.empty())
    {
      auto len = block_it.length();
      auto cursor = block_it.move_to_first();
      BLOCK* el = static_cast<BLOCK*>(cursor);
      for (; el && len > 0; len--) {
        auto box = el->pdblk.bounding_box();
        auto name = el->name();
        auto rr = el->re_rotation();
        auto cr = el->classify_rotation();
        auto skew = el->skew();
        auto prop = el->prop();
        auto kern = el->kern();
        auto spac = el->space();
        auto fc = el->font(); // not assigned
        auto coh = el->cell_over_xheight();
        auto sh = el->x_height();
        auto &pdblk = el->pdblk;
        auto pagebounds = pdblk.bounding_box();

        x = box.left();
        width = box.width();
        y = box.bottom();
        height = box.height();

        std::string s = fmt::format("{} {} {} {}\n", x, y, width, height);
        auto len = s.length();
        int l = fwrite(s.c_str(), 1, len, pdfp);
        if (l != len) { 
          tprintError("Write error while producing UZN file {}.\n", name);
          fclose(pdfp);
          return false; // didn't write one
        }

        el = block_it.forward();
      }
      fclose(pdfp);
    }
  }
  tprintDebug("UZN file {} saved.\n", name);
  return true;
}


void FullPageBlock(int width, int height, BLOCK_LIST *blocks) {
  BLOCK_IT block_it(blocks);
  auto *block = new BLOCK("", true, 0, 0, 0, 0, width, height);
  block_it.add_to_end(block);
}

} // namespace tesseract
