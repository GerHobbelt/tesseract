///////////////////////////////////////////////////////////////////////
// File:        imagefind.h
// Description: Class to find image and drawing regions in an image
//              and create a corresponding list of empty blobs.
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

#ifndef TESSERACT_TEXTORD_IMAGEFIND_H_
#define TESSERACT_TEXTORD_IMAGEFIND_H_

#include "debugpixa.h"
#include <tesseract/params.h>

#include <cstdint>

namespace tesseract {

class ColPartition;
class ColPartitionGrid;
class ColPartition_LIST;
class TabFind;
class TBOX;
class FCOORD;
class TO_BLOCK;
class ColPartitionGridSearch;
class TESS_API Tesseract;

// globals
extern INT_VAR_H(textord_tabfind_show_images);

// The ImageFind class is a simple wrapper class that
// exposes the FindImages function and some useful helper functions.
class ImageFind {
public:
  ImageFind(Tesseract* tess);
  ~ImageFind() = default;

  // Finds image regions within the BINARY source pix (page image) and returns
  // the image regions as a mask image.
  // The returned pix may be nullptr, meaning no images found.
  // If not nullptr, it must be PixDestroyed by the caller.
  // If textord_tabfind_show_images, debug images are appended to pixa_debug.
  Image FindImages(Image pix);

  // Given an input pix, and a bounding rectangle, the sides of the rectangle
  // are shrunk inwards until they bound any black pixels found within the
  // original rectangle. Returns false if the rectangle contains no black
  // pixels at all.
  bool BoundsWithinRect(Image pix, int *x_start, int *y_start, int *x_end, int *y_end);

  // Given a point in 3-D (RGB) space, returns the squared Euclidean distance
  // of the point from the given line, defined by a pair of points in the 3-D
  // (RGB) space, line1 and line2.
  double ColorDistanceFromLine(const uint8_t *line1, const uint8_t *line2,
                                      const uint8_t *point);

  // Returns true if there are no black pixels in between the boxes.
  // The im_box must represent the bounding box of the pix in tesseract
  // coordinates, which may be negative, due to rotations to make the textlines
  // horizontal. The boxes are rotated by rotation, which should undo such
  // rotations, before mapping them onto the pix.
  bool BlankImageInBetween(const TBOX &box1, const TBOX &box2, const TBOX &im_box,
                                  const FCOORD &rotation, Image pix);

  // Returns the number of pixels in box in the pix.
  // The im_box must represent the bounding box of the pix in tesseract
  // coordinates, which may be negative, due to rotations to make the textlines
  // horizontal. The boxes are rotated by rotation, which should undo such
  // rotations, before mapping them onto the pix.
  int CountPixelsInRotatedBox(TBOX box, const TBOX &im_box, const FCOORD &rotation,
                                     Image pix);

  // Locates all the image partitions in the part_grid, that were found by a
  // previous call to FindImagePartitions, marks them in the image_mask,
  // removes them from the grid, and deletes them. This makes it possible to
  // call FindImagePartitions again to produce less broken-up and less
  // overlapping image partitions.
  // rerotation specifies how to rotate the partition coords to match
  // the image_mask, since this function is used after orientation correction.
  void TransferImagePartsToImageMask(const FCOORD &rerotation, ColPartitionGrid *part_grid,
                                            Image image_mask);

  // Runs a CC analysis on the image_pix mask image, and creates
  // image partitions from them, cutting out strong text, and merging with
  // nearby image regions such that they don't interfere with text.
  // Rotation and rerotation specify how to rotate image coords to match
  // the blob and partition coords and back again.
  // The input/output part_grid owns all the created partitions, and
  // the partitions own all the fake blobs that belong in the partitions.
  // Since the other blobs in the other partitions will be owned by the block,
  // ColPartitionGrid::ReTypeBlobs must be called afterwards to fix this
  // situation and collect the image blobs.
  void FindImagePartitions(Image image_pix, const FCOORD &rotation, const FCOORD &rerotation,
                                  TO_BLOCK *block, TabFind *tab_grid, 
                                  ColPartitionGrid *part_grid, ColPartition_LIST *big_parts);

protected:
  // Generates a Boxa, Pixa pair from the input binary (image mask) pix,
  // analogous to pixConnComp, except that connected components which are nearly
  // rectangular are replaced with solid rectangles.
  // The returned boxa, pixa may be nullptr, meaning no images found.
  // If not nullptr, they must be destroyed by the caller.
  // Resolution of pix should match the source image (Tesseract::pix_binary_)
  // so the output coordinate systems match.
  void ConnCompAndRectangularize(Image pix, Boxa** boxa, Pixa** pixa);

  // The box given by slice contains some black pixels, but not necessarily
  // over the whole box. Shrink the x bounds of slice, but not the y bounds
  // until there is at least one black pixel in the outermost columns.
  // rotation, rerotation, pix and im_box are defined in the large comment above.
  void AttemptToShrinkBox(const FCOORD& rotation, const FCOORD& rerotation, const TBOX& im_box,
                                 Image pix, TBOX* slice);

  // The meat of cutting a polygonal image around text.
  // This function covers the general case of cutting a box out of a box
  // as shown:
  // Input                               Output
  // ------------------------------      ------------------------------
  // |   Single input partition   |      | 1 Cut up output partitions |
  // |                            |      ------------------------------
  // |         ----------         |      ---------           ----------
  // |         |  box   |         |      |   2   |   box     |    3   |
  // |         |        |         |      |       |  is cut   |        |
  // |         ----------         |      ---------   out     ----------
  // |                            |      ------------------------------
  // |                            |      |   4                        |
  // ------------------------------      ------------------------------
  // In the context that this function is used, at most 3 of the above output
  // boxes will be created, as the overlapping box is never contained by the
  // input.
  // The above cutting operation is executed for each element of part_list that
  // is overlapped by the input box. Each modified ColPartition is replaced
  // in place in the list by the output of the cutting operation in the order
  // shown above, so iff no holes are ever created, the output will be in
  // top-to-bottom order, but in extreme cases, hole creation is possible.
  // In such cases, the output order may cause strange block polygons.
  // rotation, rerotation, pix and im_box are defined in the large comment above.
  void CutChunkFromParts(const TBOX& box, const TBOX& im_box, const FCOORD& rotation,
                                const FCOORD& rerotation, Image pix, ColPartition_LIST* part_list);


  // Starts with the bounding box of the image component and cuts it up
  // so that it doesn't intersect text where possible.
  // Strong fully contained horizontal text is marked as text on image,
  // and does not cause a division of the image.
  // For more detail see the large comment above on cutting polygonal images
  // from a rectangle.
  // rotation, rerotation, pix and im_box are defined in the large comment above.
  void DivideImageIntoParts(const TBOX& im_box, const FCOORD& rotation,
                                   const FCOORD& rerotation, Image pix,
                                   ColPartitionGridSearch* rectsearch, ColPartition_LIST* part_list);

  // The meat of joining fragmented images and consuming ColPartitions of
  // uncertain type.
  // *part_ptr is an input/output BRT_RECTIMAGE ColPartition that is to be
  // expanded to consume overlapping and nearby ColPartitions of uncertain type
  // and other BRT_RECTIMAGE partitions, but NOT to be expanded beyond
  // max_image_box. *part_ptr is NOT in the part_grid.
  // rectsearch is already constructed on the part_grid, and is used for
  // searching for overlapping and nearby ColPartitions.
  // ExpandImageIntoParts is called iteratively until it returns false. Each
  // time it absorbs the nearest non-contained candidate, and everything that
  // is fully contained within part_ptr's bounding box.
  // TODO(rays) what if it just eats everything inside max_image_box in one go?
  bool ExpandImageIntoParts(const TBOX& max_image_box, ColPartitionGridSearch* rectsearch,
                                   ColPartitionGrid* part_grid, ColPartition** part_ptr);

private:
  Tesseract* tesseract_;   // reference to the driving tesseract instance
};

} // namespace tesseract.

#endif // TESSERACT_TEXTORD_LINEFIND_H_
