///////////////////////////////////////////////////////////////////////
// File:        linefind.h
// Description: Class to find vertical lines in an image and create
//              a corresponding list of empty blobs.
// Author:      Ray Smith
//
// (C) Copyright 2008, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////

#ifndef TESSERACT_TEXTORD_LINEFIND_H_
#define TESSERACT_TEXTORD_LINEFIND_H_

namespace tesseract {

class TabVector_LIST;

class Tesseract;

/**
 * The LineFinder class is a simple wrapper class that mainly
 * exposes the FindVerticalLines function.
 */
class LineFinder {
public:
  LineFinder(Tesseract* tess);
  ~LineFinder() = default;

  /**
   * Finds vertical and horizontal line objects in the given pix and removes
   * them.
   *
   * Uses the given resolution to determine size thresholds instead of any
   * that may be present in the pix.
   *
   * The output vertical_x and vertical_y contain a sum of the output vectors,
   * thereby giving the mean vertical direction.
   *
   * If pix_music_mask != nullptr, and music is detected, a mask of the staves
   * and anything that is connected (bars, notes etc.) will be returned in
   * pix_music_mask, the mask subtracted from pix, and the lines will not
   * appear in v_lines or h_lines.
   *
   * The output vectors are owned by the list and Frozen (cannot refit) by
   * having no boxes, as there is no need to refit or merge separator lines.
   *
   * The detected lines are removed from the pix.
   */
  void FindAndRemoveLines(int resolution, Image pix, int *vertical_x,
                                 int *vertical_y, Image *pix_music_mask, TabVector_LIST *v_lines,
                                 TabVector_LIST *h_lines);

protected:

  // Most of the heavy lifting of line finding. Given src_pix and its separate
  // resolution, returns image masks:
  // pix_vline           candidate vertical lines.
  // pix_non_vline       pixels that didn't look like vertical lines.
  // pix_hline           candidate horizontal lines.
  // pix_non_hline       pixels that didn't look like horizontal lines.
  // pix_intersections   pixels where vertical and horizontal lines meet.
  // pix_music_mask      candidate music staves.
  // This function promises to initialize all the output (2nd level) pointers,
  // but any of the returns that are empty will be nullptr on output.
  // None of the input (1st level) pointers may be nullptr except
  // pix_music_mask, which will disable music detection, and pixa_display, which
  // is for debug.
  void GetLineMasks(int resolution, Image src_pix, Image* pix_vline, Image* pix_non_vline,
                           Image* pix_hline, Image* pix_non_hline, Image* pix_intersections,
                           Image* pix_music_mask);

  // Finds vertical lines in the given list of BLOBNBOXes. bleft and tright
  // are the bounds of the image on which the input line_bblobs were found.
  // The input line_bblobs list is const really.
  // The output vertical_x and vertical_y are the total of all the vectors.
  // The output list of TabVector makes no reference to the input BLOBNBOXes.
  void FindLineVectors(const ICOORD& bleft, const ICOORD& tright,
                              BLOBNBOX_LIST* line_bblobs, int* vertical_x, int* vertical_y,
                              TabVector_LIST* vectors);

  // Finds vertical line objects in pix_vline and removes them from src_pix.
  // Uses the given resolution to determine size thresholds instead of any
  // that may be present in the pix.
  // The output vertical_x and vertical_y contain a sum of the output vectors,
  // thereby giving the mean vertical direction.
  // The output vectors are owned by the list and Frozen (cannot refit) by
  // having no boxes, as there is no need to refit or merge separator lines.
  // If no good lines are found, pix_vline is destroyed.
  // None of the input pointers may be nullptr, and if *pix_vline is nullptr then
  // the function does nothing.
  void FindAndRemoveVLines(Image pix_intersections, int* vertical_x,
                                  int* vertical_y, Image* pix_vline, Image pix_non_vline,
                                  Image src_pix, TabVector_LIST* vectors);

  // Finds horizontal line objects in pix_hline and removes them from src_pix.
  // Uses the given resolution to determine size thresholds instead of any
  // that may be present in the pix.
  // The output vertical_x and vertical_y contain a sum of the output vectors,
  // thereby giving the mean vertical direction.
  // The output vectors are owned by the list and Frozen (cannot refit) by
  // having no boxes, as there is no need to refit or merge separator lines.
  // If no good lines are found, pix_hline is destroyed.
  // None of the input pointers may be nullptr, and if *pix_hline is nullptr then
  // the function does nothing.
  void FindAndRemoveHLines(Image pix_intersections, int vertical_x,
                                  int vertical_y, Image* pix_hline, Image pix_non_hline,
                                  Image src_pix, TabVector_LIST* vectors);

private:
  Tesseract* tesseract_; // reference to the active instance
};

} // namespace tesseract.

#endif // TESSERACT_TEXTORD_LINEFIND_H_
