/**********************************************************************
 * File:        drawtord.cpp  (Formerly drawto.c)
 * Description: Draw things to do with textord.
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

#include "drawtord.h"

#include "pithsync.h"
#include "topitch.h"

namespace tesseract {

#define TO_WIN_XPOS 0 // default window pos
#define TO_WIN_YPOS 0
#define TO_WIN_NAME "Textord"
// title of window

BOOL_VAR(textord_show_fixed_cuts, false, "Draw fixed pitch cell boundaries");

#if !GRAPHICS_DISABLED

ScrollViewReference to_win;

/**********************************************************************
 * create_to_win
 *
 * Create the to window used to show the fit.
 **********************************************************************/

ScrollViewReference &create_to_win(ICOORD page_tr) {
  if (to_win) {
    return to_win;
  }
  to_win = ScrollViewManager::MakeScrollView(TESSERACT_NULLPTR, TO_WIN_NAME, TO_WIN_XPOS, TO_WIN_YPOS, page_tr.x() + 1, page_tr.y() + 1,
                          page_tr.x(), page_tr.y(), true);
  to_win->RegisterGlobalRefToMe(&to_win);
  return to_win;
}

void close_to_win() {
  // to_win is leaked, but this enables the user to view the contents.
  if (to_win) {
    to_win->UpdateWindow();
    //to_win->ExitHelper();
    to_win = nullptr;
  }
}

/**********************************************************************
 * plot_box_list
 *
 * Draw a list of blobs.
 **********************************************************************/

void plot_box_list(               // make gradients win
    ScrollViewReference &win,              // window to draw in
    BLOBNBOX_LIST *list,          // blob list
    Diagnostics::Color body_colour // colour to draw
) {
  BLOBNBOX_IT it = list; // iterator

  win->Pen(body_colour);
  win->Brush(Diagnostics::NONE);
  for (it.mark_cycle_pt(); !it.cycled_list(); it.forward()) {
    it.data()->bounding_box().plot(win);
  }
}

/**********************************************************************
 * plot_box_list
 *
 * Draw a list of blobs.
 **********************************************************************/

void plot_box_list(               // make gradients win
    Image& pix,                   // image to draw in
    BLOBNBOX_LIST* list,          // blob list
    std::vector<uint32_t>& cmap, int& cmap_offset, bool noise    // colour to draw
) {
  BLOBNBOX_IT it = list; // iterator

  //pix->Pen(body_colour);
  //pix->Brush(Diagnostics::NONE);
  for (it.mark_cycle_pt(); !it.cycled_list(); it.forward()) {
    it.data()->bounding_box().plot(pix, cmap, cmap_offset, noise);
  }
}

/**********************************************************************
 * plot_to_row
 *
 * Draw the blobs of a row in a given colour and draw the line fit.
 **********************************************************************/

void plot_to_row(             // draw a row
    TO_ROW *row,              // row to draw
    Diagnostics::Color colour, // colour to draw in
    FCOORD rotation           // rotation for line
) {
  FCOORD plot_pt; // point to plot
                  // blobs
  BLOBNBOX_IT it = row->blob_list();
  float left, right; // end of row

  if (!to_win)
    return;

  if (it.empty()) {
    tprintError("No blobs in row at {}\n", row->parallel_c());
    return;
  }
  left = it.data()->bounding_box().left();
  it.move_to_last();
  right = it.data()->bounding_box().right();
  plot_blob_list(to_win, row->blob_list(), colour, Diagnostics::BROWN);
  to_win->Pen(colour);
  plot_pt = FCOORD(left, row->line_m() * left + row->line_c());
  plot_pt.rotate(rotation);
  to_win->SetCursor(plot_pt.x(), plot_pt.y());
  plot_pt = FCOORD(right, row->line_m() * right + row->line_c());
  plot_pt.rotate(rotation);
  to_win->DrawTo(plot_pt.x(), plot_pt.y());
}

/**********************************************************************
 * plot_parallel_row
 *
 * Draw the blobs of a row in a given colour and draw the line fit.
 **********************************************************************/

void plot_parallel_row(       // draw a row
    TO_ROW *row,              // row to draw
    float gradient,           // gradients of lines
    int32_t left,             // edge of block
    Diagnostics::Color colour, // colour to draw in
    FCOORD rotation           // rotation for line
) {
  FCOORD plot_pt; // point to plot
                  // blobs
  BLOBNBOX_IT it = row->blob_list();
  auto fleft = static_cast<float>(left); // floating version
  float right;                           // end of row

  if (!to_win)
    return;

  //      left=it.data()->bounding_box().left();
  it.move_to_last();
  right = it.data()->bounding_box().right();
  plot_blob_list(to_win, row->blob_list(), colour, Diagnostics::BROWN);
  to_win->Pen(colour);
  plot_pt = FCOORD(fleft, gradient * left + row->max_y());
  plot_pt.rotate(rotation);
  to_win->SetCursor(plot_pt.x(), plot_pt.y());
  plot_pt = FCOORD(fleft, gradient * left + row->min_y());
  plot_pt.rotate(rotation);
  to_win->DrawTo(plot_pt.x(), plot_pt.y());
  plot_pt = FCOORD(fleft, gradient * left + row->parallel_c());
  plot_pt.rotate(rotation);
  to_win->SetCursor(plot_pt.x(), plot_pt.y());
  plot_pt = FCOORD(right, gradient * right + row->parallel_c());
  plot_pt.rotate(rotation);
  to_win->DrawTo(plot_pt.x(), plot_pt.y());
}

/**********************************************************************
 * draw_occupation
 *
 * Draw the row occupation with points above the threshold in white
 * and points below the threshold in black.
 **********************************************************************/

void draw_occupation(                    // draw projection
    int32_t xleft,                       // edge of block
    int32_t ybottom,                     // bottom of block
    int32_t min_y,                       // coordinate limits
    int32_t max_y, int32_t occupation[], // projection counts
    int32_t thresholds[]                 // for drop out
) {
  int32_t line_index;                     // pixel coord
  Diagnostics::Color colour;               // of histogram
  auto fleft = static_cast<float>(xleft); // float version

  if (!to_win)
    return;

  colour = Diagnostics::WHITE;
  to_win->Pen(colour);
  to_win->SetCursor(fleft, static_cast<float>(ybottom));
  for (line_index = min_y; line_index <= max_y; line_index++) {
    if (occupation[line_index - min_y] < thresholds[line_index - min_y]) {
      if (colour != Diagnostics::BLUE) {
        colour = Diagnostics::BLUE;
        to_win->Pen(colour);
      }
    } else {
      if (colour != Diagnostics::WHITE) {
        colour = Diagnostics::WHITE;
        to_win->Pen(colour);
      }
    }
    to_win->DrawTo(fleft + occupation[line_index - min_y] / 10.0, static_cast<float>(line_index));
  }
  colour = Diagnostics::STEEL_BLUE;
  to_win->Pen(colour);
  to_win->SetCursor(fleft, static_cast<float>(ybottom));
  for (line_index = min_y; line_index <= max_y; line_index++) {
    to_win->DrawTo(fleft + thresholds[line_index - min_y] / 10.0, static_cast<float>(line_index));
  }
}

/**********************************************************************
 * draw_meanlines
 *
 * Draw the meanlines of the given block in the given colour.
 **********************************************************************/

void draw_meanlines(          // draw a block
    TO_BLOCK *block,          // block to draw
    float gradient,           // gradients of lines
    int32_t left,             // edge of block
    Diagnostics::Color colour, // colour to draw in
    FCOORD rotation           // rotation for line
) {
  FCOORD plot_pt; // point to plot
                  // rows
  TO_ROW_IT row_it = block->get_rows();
  TO_ROW *row;         // current row
  BLOBNBOX_IT blob_it; // blobs
  float right;         // end of row

  if (!to_win)
    return;

  to_win->Pen(colour);
  for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
    row = row_it.data();
    blob_it.set_to_list(row->blob_list());
    blob_it.move_to_last();
    right = blob_it.data()->bounding_box().right();
    plot_pt = FCOORD(static_cast<float>(left), gradient * left + row->parallel_c() + row->xheight);
    plot_pt.rotate(rotation);
    to_win->SetCursor(plot_pt.x(), plot_pt.y());
    plot_pt = FCOORD(right, gradient * right + row->parallel_c() + row->xheight);
    plot_pt.rotate(rotation);
    to_win->DrawTo(plot_pt.x(), plot_pt.y());
  }
}

/**********************************************************************
 * plot_word_decisions
 *
 * Plot a row with words in different colours and fuzzy spaces
 * highlighted.
 **********************************************************************/

void plot_word_decisions(          // draw words
    ScrollViewReference &win,      // window to draw in
    TDimension pitch,              // of block
    TO_ROW *row                    // row to draw
) {
  Diagnostics::Color colour = Diagnostics::MAGENTA; // current colour
  TDimension prev_x;                              // end of prev blob
  int16_t blob_count;                             // blobs in word
  BLOBNBOX *blob;                                 // current blob
  TBOX blob_box;                                  // bounding box
                                                  // iterator
  BLOBNBOX_IT blob_it = row->blob_list();
  BLOBNBOX_IT start_it = blob_it; // word start

  prev_x = TDIMENSION_MIN;
  blob_count = 0;
  for (blob_it.mark_cycle_pt(); !blob_it.cycled_list(); blob_it.forward()) {
    blob = blob_it.data();
    blob_box = blob->bounding_box();
    if (!blob->joined_to_prev() && blob_box.left() - prev_x > row->max_nonspace) {
      if ((blob_box.left() - prev_x >= row->min_space ||
           blob_box.left() - prev_x > row->space_threshold) &&
          blob_count > 0) {
        if (pitch > 0 && textord_show_fixed_cuts) {
          plot_fp_cells(win, colour, &start_it, pitch, blob_count, &row->projection,
                        row->projection_left, row->projection_right,
                        row->xheight * textord_projection_scale);
        }
        blob_count = 0;
        start_it = blob_it;
      }
      if (colour == Diagnostics::MAGENTA) {
        colour = Diagnostics::RED;
      } else {
        colour = static_cast<Diagnostics::Color>(colour + 1);
      }
      if (blob_box.left() - prev_x < row->min_space) {
        Diagnostics::Color rect_colour; // fuzzy colour
        if (blob_box.left() - prev_x > row->space_threshold) {
          rect_colour = Diagnostics::GOLDENROD;
        } else {
          rect_colour = Diagnostics::CORAL;
        }
        // fill_color_index(win, rect_colour);
        win->Brush(rect_colour);
        win->Rectangle(prev_x, blob_box.bottom(), blob_box.left(), blob_box.top());
      }
    }
    if (!blob->joined_to_prev()) {
      prev_x = blob_box.right();
    }
    if (blob->cblob() != nullptr) {
      blob->cblob()->plot(win, colour, colour);
    }
    if (!blob->joined_to_prev() && blob->cblob() != nullptr) {
      blob_count++;
    }
  }
  if (pitch > 0 && textord_show_fixed_cuts && blob_count > 0) {
    plot_fp_cells(win, colour, &start_it, pitch, blob_count, &row->projection, row->projection_left,
                  row->projection_right, row->xheight * textord_projection_scale);
  }
}

/**********************************************************************
 * plot_fp_cells
 *
 * Make a list of fixed pitch cuts and draw them.
 **********************************************************************/

void plot_fp_cells(           // draw words
    ScrollViewReference &win,          // window to draw in
    Diagnostics::Color colour, // colour of lines
    BLOBNBOX_IT *blob_it,     // blobs
    int16_t pitch,            // of block
    int16_t blob_count,       // no of real blobs
    STATS *projection,        // vertical
    int16_t projection_left,  // edges //scale factor
    int16_t projection_right, float projection_scale) {
  int16_t occupation;    // occupied cells
  TBOX word_box;         // bounding box
  FPSEGPT_LIST seg_list; // list of cuts
  FPSEGPT_IT seg_it;
  FPSEGPT *segpt; // current point

  if (pitsync_linear_version) {
    check_pitch_sync2(blob_it, blob_count, pitch, 2, projection, projection_left, projection_right,
                      projection_scale, occupation, &seg_list, 0, 0);
  } else {
    check_pitch_sync(blob_it, blob_count, pitch, 2, projection, &seg_list);
  }
  word_box = blob_it->data()->bounding_box();
  for (; blob_count > 0; blob_count--) {
    word_box += box_next(blob_it);
  }
  seg_it.set_to_list(&seg_list);
  for (seg_it.mark_cycle_pt(); !seg_it.cycled_list(); seg_it.forward()) {
    segpt = seg_it.data();
    if (segpt->faked) {
      colour = Diagnostics::WHITE;
      win->Pen(colour);
    } else {
      win->Pen(colour);
    }
    win->Line(segpt->position(), word_box.bottom(), segpt->position(), word_box.top());
  }
}

/**********************************************************************
 * plot_fp_cells2
 *
 * Make a list of fixed pitch cuts and draw them.
 **********************************************************************/

void plot_fp_cells2(          // draw words
    ScrollViewReference &win,          // window to draw in
    Diagnostics::Color colour, // colour of lines
    TO_ROW *row,              // for location
    FPSEGPT_LIST *seg_list    // segments to plot
) {
  TBOX word_box; // bounding box
  FPSEGPT_IT seg_it = seg_list;
  // blobs in row
  BLOBNBOX_IT blob_it = row->blob_list();
  FPSEGPT *segpt; // current point

  word_box = blob_it.data()->bounding_box();
  for (blob_it.mark_cycle_pt(); !blob_it.cycled_list();) {
    word_box += box_next(&blob_it);
  }
  for (seg_it.mark_cycle_pt(); !seg_it.cycled_list(); seg_it.forward()) {
    segpt = seg_it.data();
    if (segpt->faked) {
      colour = Diagnostics::WHITE;
      win->Pen(colour);
    } else {
      win->Pen(colour);
    }
    win->Line(segpt->position(), word_box.bottom(), segpt->position(), word_box.top());
  }
}

/**********************************************************************
 * plot_row_cells
 *
 * Make a list of fixed pitch cuts and draw them.
 **********************************************************************/

void plot_row_cells(          // draw words
    ScrollViewReference &win,          // window to draw in
    Diagnostics::Color colour, // colour of lines
    TO_ROW *row,              // for location
    float xshift,             // amount of shift
    ICOORDELT_LIST *cells     // cells to draw
) {
  TBOX word_box; // bounding box
  ICOORDELT_IT cell_it = cells;
  // blobs in row
  BLOBNBOX_IT blob_it = row->blob_list();
  ICOORDELT *cell; // current cell

  word_box = blob_it.data()->bounding_box();
  for (blob_it.mark_cycle_pt(); !blob_it.cycled_list();) {
    word_box += box_next(&blob_it);
  }
  win->Pen(colour);
  for (cell_it.mark_cycle_pt(); !cell_it.cycled_list(); cell_it.forward()) {
    cell = cell_it.data();
    win->Line(cell->x() + xshift, word_box.bottom(), cell->x() + xshift, word_box.top());
  }
}

#endif // !GRAPHICS_DISABLED

} // namespace tesseract
