/**********************************************************************
 * File:        normalis.cpp  (Formerly denorm.c)
 * Description: Code for the DENORM class.
 * Author:      Ray Smith
 *
 * (C) Copyright 1992, Hewlett-Packard Ltd.
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

#include "normalis.h"

#include <leptonica/allheaders.h>
#include "blobs.h"
#include "helpers.h"
#include "matrix.h"
#include "ocrblock.h"
#include "unicharset.h"
#include "werd.h"

#include <cfloat> // for FLT_MAX
#include <cstdlib>

namespace tesseract {

// Tolerance in pixels used for baseline and xheight on non-upper/lower scripts.
const int kSloppyTolerance = 4;
// Final tolerance in pixels added to the computed xheight range.
const float kFinalPixelTolerance = 0.125f;

DENORM::DENORM() {
  Init();
}

DENORM::DENORM(const DENORM &src) {
  if (this != &src) {
    *this = src;
  }
}

DENORM::DENORM(DENORM &&src) noexcept {
  if (this != &src) {
    inverse_ = src.inverse_;
    src.inverse_ = false;
    block_ = src.block_;
    src.block_ = nullptr;
    rotation_ = src.rotation_;
    src.rotation_ = nullptr;
    predecessor_ = src.predecessor_;
    src.predecessor_ = nullptr;
    x_map_ = src.x_map_;
    src.x_map_ = nullptr;
    y_map_ = src.y_map_;
    src.y_map_ = nullptr;
    x_origin_ = src.x_origin_;
    y_origin_ = src.y_origin_;
    x_scale_ = src.x_scale_;
    y_scale_ = src.y_scale_;
    final_xshift_ = src.final_xshift_;
    final_yshift_ = src.final_yshift_;
  }
}

DENORM &DENORM::operator=(DENORM &&src) noexcept {
  if (this != &src) {
    inverse_ = src.inverse_;
    src.inverse_ = false;
    block_ = src.block_;
    src.block_ = nullptr;
    rotation_ = src.rotation_;
    src.rotation_ = nullptr;
    predecessor_ = src.predecessor_;
    src.predecessor_ = nullptr;
    x_map_ = src.x_map_;
    src.x_map_ = nullptr;
    y_map_ = src.y_map_;
    src.y_map_ = nullptr;
    x_origin_ = src.x_origin_;
    y_origin_ = src.y_origin_;
    x_scale_ = src.x_scale_;
    y_scale_ = src.y_scale_;
    final_xshift_ = src.final_xshift_;
    final_yshift_ = src.final_yshift_;
  }
  return *this;
}

DENORM &DENORM::operator=(const DENORM &src) {
  if (this != &src) {
    Clear();
    inverse_ = src.inverse_;
    predecessor_ = src.predecessor_;
    block_ = src.block_;
    if (src.rotation_ == nullptr) {
      rotation_ = nullptr;
    } else {
      rotation_ = new FCOORD(*src.rotation_);
    }
    x_origin_ = src.x_origin_;
    y_origin_ = src.y_origin_;
    x_scale_ = src.x_scale_;
    y_scale_ = src.y_scale_;
    final_xshift_ = src.final_xshift_;
    final_yshift_ = src.final_yshift_;
  }
  return *this;
}

DENORM::~DENORM() {
  Clear();
}

// Initializes the denorm for a transformation. For details see the large
// comment in normalis.h.
// Arguments:
// block: if not nullptr, then this is the first transformation, and
//        block->re_rotation() needs to be used after the Denorm
//        transformation to get back to the image coords.
// rotation: if not nullptr, apply this rotation after translation to the
//           origin and scaling. (Usually a classify rotation.)
// predecessor: if not nullptr, then predecessor has been applied to the
//              input space and needs to be undone to complete the inverse.
// The above pointers are not owned by this DENORM and are assumed to live
// longer than this denorm, except rotation, which is deep copied on input.
//
// x_origin: The x origin which will be mapped to final_xshift in the result.
// y_origin: The y origin which will be mapped to final_yshift in the result.
//           Added to result of row->baseline(x) if not nullptr.
//
// x_scale: scale factor for the x-coordinate.
// y_scale: scale factor for the y-coordinate. Ignored if segs is given.
// Note that these scale factors apply to the same x and y system as the
// x-origin and y-origin apply, ie after any block rotation, but before
// the rotation argument is applied.
//
// final_xshift: The x component of the final translation.
// final_yshift: The y component of the final translation.
void DENORM::SetupNormalization(const BLOCK *block, const FCOORD *rotation,
                                const DENORM *predecessor, float x_origin, float y_origin,
                                float x_scale, float y_scale, float final_xshift,
                                float final_yshift) {
  Clear();
  block_ = block;
  if (rotation == nullptr) {
    rotation_ = nullptr;
  } else {
    rotation_ = new FCOORD(*rotation);
  }
  predecessor_ = predecessor;
  x_origin_ = x_origin;
  y_origin_ = y_origin;
  x_scale_ = x_scale;
  y_scale_ = y_scale;
  final_xshift_ = final_xshift;
  final_yshift_ = final_yshift;
}

// Helper for SetupNonLinear computes an image of shortest run-lengths from
// the x/y edges provided.
// Based on "A nonlinear normalization method for handprinted Kanji character
// recognition -- line density equalization" by Hiromitsu Yamada et al.
// Eg below is an O in a 1-pixel margin-ed bounding box and the corresponding
//  ______________     input x_coords and y_coords.
// |  _________  |     <empty>
// | |    _    | |     1, 6
// | |   | |   | |     1, 3, 4, 6
// | |   | |   | |     1, 3, 4, 6
// | |   | |   | |     1, 3, 4, 6
// | |   |_|   | |     1, 3, 4, 6
// | |_________| |     1, 6
// |_____________|     <empty>
//  E 1 1 1 1 1 E
//  m 7 7 2 7 7 m
//  p     6     p
//  t     7     t
//  y           y
// The output image contains the min of the x and y run-length (distance
// between edges) at each coordinate in the image thus:
//  ______________
// |7 1_1_1_1_1 7|
// |1|5 5 1 5 5|1|
// |1|2 2|1|2 2|1|
// |1|2 2|1|2 2|1|
// |1|2 2|1|2 2|1|
// |1|2 2|1|2 2|1|
// |1|5_5_1_5_5|1|
// |7_1_1_1_1_1_7|
// Note that the input coords are all integer, so all partial pixels are dealt
// with elsewhere. Although it is nice for outlines to be properly connected
// and continuous, there is no requirement that they be as such, so they could
// have been derived from a flaky source, such as greyscale.
// This function works only within the provided box, and it is assumed that the
// input x_coords and y_coords have already been translated to have the bottom-
// left of box as the origin. Although an output, the minruns should have been
// pre-initialized to be the same size as box. Each element will contain the
// minimum of x and y run-length as shown above.
static void ComputeRunlengthImage(const TBOX &box,
                                  const std::vector<std::vector<int>> &x_coords,
                                  const std::vector<std::vector<int>> &y_coords,
                                  GENERIC_2D_ARRAY<int> *minruns) {
  int width = box.width();
  int height = box.height();
  ASSERT_HOST(minruns->dim1() == width);
  ASSERT_HOST(minruns->dim2() == height);
  // Set a 2-d image array to the run lengths at each pixel.
  for (int ix = 0; ix < width; ++ix) {
    int y = 0;
    for (auto y_coord : y_coords[ix]) {
      int y_edge = ClipToRange(y_coord, 0, height);
      int gap = y_edge - y;
      // Every pixel between the last and current edge get set to the gap.
      while (y < y_edge) {
        (*minruns)(ix, y) = gap;
        ++y;
      }
    }
    // Pretend there is a bounding box of edges all around the image.
    int gap = height - y;
    while (y < height) {
      (*minruns)(ix, y) = gap;
      ++y;
    }
  }
  // Now set the image pixels the MIN of the x and y runlengths.
  for (int iy = 0; iy < height; ++iy) {
    int x = 0;
    for (auto x_coord : x_coords[iy]) {
      int x_edge = ClipToRange(x_coord, 0, width);
      int gap = x_edge - x;
      while (x < x_edge) {
        if (gap < (*minruns)(x, iy)) {
          (*minruns)(x, iy) = gap;
        }
        ++x;
      }
    }
    int gap = width - x;
    while (x < width) {
      if (gap < (*minruns)(x, iy)) {
        (*minruns)(x, iy) = gap;
      }
      ++x;
    }
  }
}
// Converts the run-length image (see above to the edge density profiles used
// for scaling, thus:
//  ______________
// |7 1_1_1_1_1 7|  = 5.28
// |1|5 5 1 5 5|1|  = 3.8
// |1|2 2|1|2 2|1|  = 5
// |1|2 2|1|2 2|1|  = 5
// |1|2 2|1|2 2|1|  = 5
// |1|2 2|1|2 2|1|  = 5
// |1|5_5_1_5_5|1|  = 3.8
// |7_1_1_1_1_1_7|  = 5.28
//  6 4 4 8 4 4 6
//  . . . . . . .
//  2 4 4 0 4 4 2
//  8           8
// Each profile is the sum of the reciprocals of the pixels in the image in
// the appropriate row or column, and these are then normalized to sum to 1.
// On output hx, hy contain an extra element, which will eventually be used
// to guarantee that the top/right edge of the box (and anything beyond) always
// gets mapped to the maximum target coordinate.
static void ComputeEdgeDensityProfiles(const TBOX &box, const GENERIC_2D_ARRAY<int> &minruns,
                                       std::vector<float> &hx, std::vector<float> &hy) {
  int width = box.width();
  int height = box.height();
  hx.clear();
  hx.resize(width + 1);
  hy.clear();
  hy.resize(height + 1);
  double total = 0.0;
  for (int iy = 0; iy < height; ++iy) {
    for (int ix = 0; ix < width; ++ix) {
      int run = minruns(ix, iy);
      if (run == 0) {
        run = 1;
      }
      float density = 1.0f / run;
      hx[ix] += density;
      hy[iy] += density;
    }
    total += hy[iy];
  }
  // Normalize each profile to sum to 1.
  if (total > 0.0) {
    for (int ix = 0; ix < width; ++ix) {
      hx[ix] /= total;
    }
    for (int iy = 0; iy < height; ++iy) {
      hy[iy] /= total;
    }
  }
  // There is an extra element in each array, so initialize to 1.
  hx[width] = 1.0f;
  hy[height] = 1.0f;
}

// Sets up the DENORM to execute a non-linear transformation based on
// preserving an even distribution of stroke edges. The transformation
// operates only within the given box.
// x_coords is a collection of the x-coords of vertical edges for each
// y-coord starting at box.bottom().
// y_coords is a collection of the y-coords of horizontal edges for each
// x-coord starting at box.left().
// Eg x_coords[0] is a collection of the x-coords of edges at y=bottom.
// Eg x_coords[1] is a collection of the x-coords of edges at y=bottom + 1.
// The second-level vectors must all be sorted in ascending order.
// See comments on the helper functions above for more details.
void DENORM::SetupNonLinear(const DENORM *predecessor, const TBOX &box, float target_width,
                            float target_height, float final_xshift, float final_yshift,
                            const std::vector<std::vector<int>> &x_coords,
                            const std::vector<std::vector<int>> &y_coords) {
  Clear();
  predecessor_ = predecessor;
  // x_map_ and y_map_ store a mapping from input x and y coordinate to output
  // x and y coordinate, based on scaling to the supplied target_width and
  // target_height.
  x_map_ = new std::vector<float>;
  y_map_ = new std::vector<float>;
  // Set a 2-d image array to the run lengths at each pixel.
  int width = box.width();
  int height = box.height();
  GENERIC_2D_ARRAY<int> minruns(width, height, 0);
  ComputeRunlengthImage(box, x_coords, y_coords, &minruns);
  // Edge density is the sum of the inverses of the run lengths. Compute
  // edge density projection profiles.
  ComputeEdgeDensityProfiles(box, minruns, *x_map_, *y_map_);
  // Convert the edge density profiles to the coordinates by multiplying by
  // the desired size and accumulating.
  (*x_map_)[width] = target_width;
  for (int x = width - 1; x >= 0; --x) {
    (*x_map_)[x] = (*x_map_)[x + 1] - (*x_map_)[x] * target_width;
  }
  (*y_map_)[height] = target_height;
  for (int y = height - 1; y >= 0; --y) {
    (*y_map_)[y] = (*y_map_)[y + 1] - (*y_map_)[y] * target_height;
  }
  x_origin_ = box.left();
  y_origin_ = box.bottom();
  final_xshift_ = final_xshift;
  final_yshift_ = final_yshift;
}

// Transforms the given coords one step forward to normalized space, without
// using any block rotation or predecessor.
void DENORM::LocalNormTransform(const TPOINT &pt, TPOINT *transformed) const {
  FCOORD src_pt(pt.x, pt.y);
  FCOORD float_result;
  LocalNormTransform(src_pt, &float_result);
  transformed->x = IntCastRounded(float_result.x());
  transformed->y = IntCastRounded(float_result.y());
}
void DENORM::LocalNormTransform(const FCOORD &pt, FCOORD *transformed) const {
  FCOORD translated(pt.x() - x_origin_, pt.y() - y_origin_);
  if (x_map_ != nullptr && y_map_ != nullptr) {
    int x = ClipToRange(IntCastRounded(translated.x()), 0, static_cast<int>(x_map_->size() - 1));
    translated.set_x((*x_map_)[x]);
    int y = ClipToRange(IntCastRounded(translated.y()), 0, static_cast<int>(y_map_->size() - 1));
    translated.set_y((*y_map_)[y]);
  } else {
    translated.set_x(translated.x() * x_scale_);
    translated.set_y(translated.y() * y_scale_);
    if (rotation_ != nullptr) {
      translated.rotate(*rotation_);
    }
  }
  transformed->set_x(translated.x() + final_xshift_);
  transformed->set_y(translated.y() + final_yshift_);
}

// Transforms the given coords forward to normalized space using the
// full transformation sequence defined by the block rotation, the
// predecessors, deepest first, and finally this. If first_norm is not nullptr,
// then the first and deepest transformation used is first_norm, ending
// with this, and the block rotation will not be applied.
void DENORM::NormTransform(const DENORM *first_norm, const TPOINT &pt, TPOINT *transformed) const {
  FCOORD src_pt(pt.x, pt.y);
  FCOORD float_result;
  NormTransform(first_norm, src_pt, &float_result);
  transformed->x = IntCastRounded(float_result.x());
  transformed->y = IntCastRounded(float_result.y());
}
void DENORM::NormTransform(const DENORM *first_norm, const FCOORD &pt, FCOORD *transformed) const {
  FCOORD src_pt(pt);
  if (first_norm != this) {
    if (predecessor_ != nullptr) {
      predecessor_->NormTransform(first_norm, pt, &src_pt);
    } else if (block_ != nullptr) {
      FCOORD fwd_rotation(block_->re_rotation().x(), -block_->re_rotation().y());
      src_pt.rotate(fwd_rotation);
    }
  }
  LocalNormTransform(src_pt, transformed);
}

// Transforms the given coords one step back to source space, without
// using to any block rotation or predecessor.
void DENORM::LocalDenormTransform(const TPOINT &pt, TPOINT *original) const {
  FCOORD src_pt(pt.x, pt.y);
  FCOORD float_result;
  LocalDenormTransform(src_pt, &float_result);
  original->x = IntCastRounded(float_result.x());
  original->y = IntCastRounded(float_result.y());
}

void DENORM::LocalDenormTransform(const FCOORD &pt, FCOORD *original) const {
  FCOORD rotated(pt.x() - final_xshift_, pt.y() - final_yshift_);
  if (x_map_ != nullptr && y_map_ != nullptr) {
    auto pos = std::upper_bound(x_map_->begin(), x_map_->end(), rotated.x());
    if (pos > x_map_->begin()) {
      --pos;
    }
    auto x = pos - x_map_->begin();
    original->set_x(x + x_origin_);
    pos = std::upper_bound(y_map_->begin(), y_map_->end(), rotated.y());
    if (pos > y_map_->begin()) {
      --pos;
    }
    auto y = pos - y_map_->begin();
    original->set_y(y + y_origin_);
  } else {
    if (rotation_ != nullptr) {
      FCOORD inverse_rotation(rotation_->x(), -rotation_->y());
      rotated.rotate(inverse_rotation);
    }
    original->set_x(rotated.x() / x_scale_ + x_origin_);
    float y_scale = y_scale_;
    original->set_y(rotated.y() / y_scale + y_origin_);
  }
}

// Transforms the given coords all the way back to source image space using
// the full transformation sequence defined by this and its predecessors
// recursively, shallowest first, and finally any block re_rotation.
// If last_denorm is not nullptr, then the last transformation used will
// be last_denorm, and the block re_rotation will never be executed.
void DENORM::DenormTransform(const DENORM *last_denorm, const TPOINT &pt, TPOINT *original) const {
  FCOORD src_pt(pt.x, pt.y);
  FCOORD float_result;
  DenormTransform(last_denorm, src_pt, &float_result);
  original->x = IntCastRounded(float_result.x());
  original->y = IntCastRounded(float_result.y());
}
void DENORM::DenormTransform(const DENORM *last_denorm, const FCOORD &pt, FCOORD *original) const {
  LocalDenormTransform(pt, original);
  if (last_denorm != this) {
    if (predecessor_ != nullptr) {
      predecessor_->DenormTransform(last_denorm, *original, original);
    } else if (block_ != nullptr) {
      original->rotate(block_->re_rotation());
    }
  }
}

// Normalize a blob using blob transformations. Less accurate, but
// more accurately copies the old way.
void DENORM::LocalNormBlob(TBLOB *blob) const {
  ICOORD translation(-IntCastRounded(x_origin_), -IntCastRounded(y_origin_));
  blob->Move(translation);
  if (y_scale_ != 1.0f) {
    blob->Scale(y_scale_);
  }
  if (rotation_ != nullptr) {
    blob->Rotate(*rotation_);
  }
  translation.set_x(IntCastRounded(final_xshift_));
  translation.set_y(IntCastRounded(final_yshift_));
  blob->Move(translation);
}

// Fills in the x-height range accepted by the given unichar_id, given its
// bounding box in the usual baseline-normalized coordinates, with some
// initial crude x-height estimate (such as word size) and this denoting the
// transformation that was used.
void DENORM::XHeightRange(int unichar_id, const UNICHARSET &unicharset, const TBOX &bbox,
                          float *min_xht, float *max_xht, float *yshift) const {
  // Default return -- accept anything.
  *yshift = 0.0f;
  *min_xht = 0.0f;
  *max_xht = FLT_MAX;

  if (!unicharset.top_bottom_useful()) {
    return;
  }

  // Clip the top and bottom to the limit of normalized feature space.
  int top = ClipToRange<int>(bbox.top(), 0, kBlnCellHeight - 1);
  int bottom = ClipToRange<int>(bbox.bottom(), 0, kBlnCellHeight - 1);
  // A tolerance of yscale corresponds to 1 pixel in the image.
  double tolerance = y_scale();
  // If the script doesn't have upper and lower-case characters, widen the
  // tolerance to allow sloppy baseline/x-height estimates.
  if (!unicharset.script_has_upper_lower()) {
    tolerance = y_scale() * kSloppyTolerance;
  }

  int min_bottom, max_bottom, min_top, max_top;
  unicharset.get_top_bottom(unichar_id, &min_bottom, &max_bottom, &min_top, &max_top);

  // Calculate the scale factor we'll use to get to image y-pixels
  double midx = (bbox.left() + bbox.right()) / 2.0;
  double ydiff = (bbox.top() - bbox.bottom()) + 2.0;
  FCOORD mid_bot(midx, bbox.bottom()), tmid_bot;
  FCOORD mid_high(midx, bbox.bottom() + ydiff), tmid_high;
  DenormTransform(nullptr, mid_bot, &tmid_bot);
  DenormTransform(nullptr, mid_high, &tmid_high);

  // bln_y_measure * yscale = image_y_measure
  double yscale = tmid_high.pt_to_pt_dist(tmid_bot) / ydiff;

  // Calculate y-shift
  int bln_yshift = 0, bottom_shift = 0, top_shift = 0;
  if (bottom < min_bottom - tolerance) {
    bottom_shift = bottom - min_bottom;
  } else if (bottom > max_bottom + tolerance) {
    bottom_shift = bottom - max_bottom;
  }
  if (top < min_top - tolerance) {
    top_shift = top - min_top;
  } else if (top > max_top + tolerance) {
    top_shift = top - max_top;
  }
  if ((top_shift >= 0 && bottom_shift > 0) || (top_shift < 0 && bottom_shift < 0)) {
    bln_yshift = (top_shift + bottom_shift) / 2;
  }
  *yshift = bln_yshift * yscale;

  // To help very high cap/xheight ratio fonts accept the correct x-height,
  // and to allow the large caps in small caps to accept the xheight of the
  // small caps, add kBlnBaselineOffset to chars with a maximum max, and have
  // a top already at a significantly high position.
  if (max_top == kBlnCellHeight - 1 && top > kBlnCellHeight - kBlnBaselineOffset / 2) {
    max_top += kBlnBaselineOffset;
  }
  top -= bln_yshift;
  int height = top - kBlnBaselineOffset;
  double min_height = min_top - kBlnBaselineOffset - tolerance;
  double max_height = max_top - kBlnBaselineOffset + tolerance;

  // We shouldn't try calculations if the characters are very short (for example
  // for punctuation).
  if (min_height > kBlnXHeight / 8 && height > 0) {
    float result = height * kBlnXHeight * yscale / min_height;
    *max_xht = result + kFinalPixelTolerance;
    result = height * kBlnXHeight * yscale / max_height;
    *min_xht = result - kFinalPixelTolerance;
  }
}

// Prints the content of the DENORM for debug purposes.
void DENORM::Print() const {
  if (inverse_) {
    tprintDebug("Inverse\n");
  }
  if (block_ && block_->re_rotation().x() != 1.0f) {
    tprintDebug("Block rotation {}, {}\n", block_->re_rotation().x(), block_->re_rotation().y());
  }
  tprintDebug("Input Origin = ({}, {})\n", x_origin_, y_origin_);
  if (x_map_ != nullptr && y_map_ != nullptr) {
    tprintDebug("x map:\n");
    for (auto x : *x_map_) {
      tprintDebug("{} ", x);
    }
    tprintDebug("\ny map:\n");
    for (auto y : *y_map_) {
      tprintDebug("{} ", y);
    }
    tprintDebug("\n");
  } else {
    tprintDebug("Scale = ({}, {})\n", x_scale_, y_scale_);
    if (rotation_ != nullptr) {
      tprintDebug("Rotation = ({}, {})\n", rotation_->x(), rotation_->y());
    }
  }
  tprintDebug("Final Origin = ({}, {})\n", final_xshift_, final_xshift_);
  if (predecessor_ != nullptr) {
    tprintDebug("Predecessor:\n");
    predecessor_->Print();
  }
}

// ============== Private Code ======================

// Free allocated memory and clear pointers.
void DENORM::Clear() {
  delete x_map_;
  x_map_ = nullptr;
  delete y_map_;
  y_map_ = nullptr;
  delete rotation_;
  rotation_ = nullptr;
  Init();
}

// Setup default values.
void DENORM::Init() {
  inverse_ = false;
  block_ = nullptr;
  rotation_ = nullptr;
  predecessor_ = nullptr;
  x_map_ = nullptr;
  y_map_ = nullptr;
  x_origin_ = 0.0f;
  y_origin_ = 0.0f;
  x_scale_ = 1.0f;
  y_scale_ = 1.0f;
  final_xshift_ = 0.0f;
  final_yshift_ = static_cast<float>(kBlnBaselineOffset);
}

} // namespace tesseract
