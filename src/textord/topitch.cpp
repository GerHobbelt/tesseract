/**********************************************************************
 * File:        topitch.cpp  (Formerly to_pitch.c)
 * Description: Code to determine fixed pitchness and the pitch if fixed.
 * Author:      Ray Smith
 *
 * (C) Copyright 1993, Hewlett-Packard Ltd.
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

// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.

#include "topitch.h"

#include "blobbox.h"
#include "drawtord.h"
#include "makerow.h"
#include "pithsync.h"
#include "pitsync1.h"
#include "statistc.h"
#include "tovars.h"
#include "wordseg.h"

#include "helpers.h"

#include <memory>

namespace tesseract {

BOOL_VAR(textord_all_prop, false, "All doc is proportial text");
BOOL_VAR(textord_debug_fixed_pitch_test, false, "Debug on fixed pitch test");
BOOL_VAR(textord_debug_pitch, false, "Debug pitch detection");
BOOL_VAR(textord_disable_pitch_test, false, "Turn off dp fixed pitch algorithm");
BOOL_VAR(textord_fast_pitch_test, false, "Do even faster pitch algorithm");
BOOL_VAR(textord_debug_pitch_metric, false, "Write full metric stuff");
BOOL_VAR(textord_show_row_cuts, false, "Draw row-level cuts");
BOOL_VAR(textord_show_page_cuts, false, "Draw page-level cuts");
BOOL_VAR(textord_blockndoc_fixed, false, "Attempt whole doc/block fixed pitch");
DOUBLE_VAR(textord_projection_scale, 0.200, "Ding rate for mid-cuts");
DOUBLE_VAR(textord_balance_factor, 1.0, "Ding rate for unbalanced char cells");

#define BLOCK_STATS_CLUSTERS 10
#define MAX_ALLOWED_PITCH 100 // max pixel pitch.

// qsort function to sort 2 floats.
static int sort_floats(const void *arg1, const void *arg2) {
  float diff = *reinterpret_cast<const float *>(arg1) - *reinterpret_cast<const float *>(arg2);
  if (diff > 0) {
    return 1;
  } else if (diff < 0) {
    return -1;
  } else {
    return 0;
  }
}

/**********************************************************************
 * compute_fixed_pitch
 *
 * Decide whether each row is fixed pitch individually.
 * Correlate definite and uncertain results to obtain an individual
 * result for each row in the TO_ROW class.
 **********************************************************************/

void compute_fixed_pitch(ICOORD page_tr,             // top right
                         TO_BLOCK_LIST *port_blocks, // input list
                         float gradient,             // page skew
                         FCOORD rotation             // for drawing
) {   
  TO_BLOCK_IT block_it;                              // iterator
  TO_BLOCK *block;                                   // current block;
  TO_ROW *row;                                       // current row
  int block_index;                                   // block number
  int row_index;                                     // row number

#if !GRAPHICS_DISABLED
  if (textord_show_initial_words) {
    if (!to_win) {
      create_to_win(page_tr);
    }
  }
#endif

  block_it.set_to_list(port_blocks);
  block_index = 1;
  for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
    block = block_it.data();
    compute_block_pitch(block, rotation, block_index);
    block_index++;
  }

  if (!try_doc_fixed(page_tr, port_blocks, gradient)) {
    block_index = 1;
    for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
      block = block_it.data();
      if (!try_block_fixed(block, block_index)) {
        try_rows_fixed(block, block_index);
      }
      block_index++;
    }
  }

  block_index = 1;
  for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
    block = block_it.data();
    POLY_BLOCK *pb = block->block->pdblk.poly_block();
    if (pb != nullptr && !pb->IsText()) {
      continue; // Non-text doesn't exist!
    }
    // row iterator
    TO_ROW_IT row_it(block->get_rows());
    row_index = 1;
    for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
      row = row_it.data();
      fix_row_pitch(row, block, port_blocks, row_index, block_index);
      row_index++;
    }
    block_index++;
  }
#if !GRAPHICS_DISABLED
  if (textord_show_initial_words) {
    ScrollView::Update();
  }
#endif
}

/**********************************************************************
 * fix_row_pitch
 *
 * Get a pitch_decision for this row by voting among similar rows in the
 * block, then similar rows over all the page, or any other rows at all.
 **********************************************************************/

void fix_row_pitch(TO_ROW *bad_row,        // row to fix
                   TO_BLOCK *bad_block,    // block of bad_row
                   TO_BLOCK_LIST *blocks,  // blocks to scan
                   int32_t row_target,     // number of row
                   int32_t block_target) { // number of block
  int16_t mid_cuts;
  int block_votes;               // votes in block
  int like_votes;                // votes over page
  int other_votes;               // votes of unlike blocks
  int block_index;               // number of block
  int maxwidth;                  // max pitch
  TO_BLOCK_IT block_it = blocks; // block iterator
  TO_BLOCK *block;               // current block
  TO_ROW *row;                   // current row
  float sp_sd;                   // space deviation
  STATS block_stats;             // pitches in block
  STATS like_stats;              // pitches in page

  block_votes = like_votes = other_votes = 0;
  maxwidth = static_cast<int32_t>(ceil(bad_row->xheight * textord_words_maxspace));
  if (bad_row->pitch_decision != PITCH_DEF_FIXED && bad_row->pitch_decision != PITCH_DEF_PROP) {
    block_stats.set_range(0, maxwidth - 1);
    like_stats.set_range(0, maxwidth - 1);
    block_index = 1;
    for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
      block = block_it.data();
      POLY_BLOCK *pb = block->block->pdblk.poly_block();
      if (pb != nullptr && !pb->IsText()) {
        continue; // Non text doesn't exist!
      }
      TO_ROW_IT row_it(block->get_rows());
      for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
        row = row_it.data();
        if ((bad_row->all_caps &&
             row->xheight + row->ascrise <
                 (bad_row->xheight + bad_row->ascrise) * (1 + textord_pitch_rowsimilarity) &&
             row->xheight + row->ascrise >
                 (bad_row->xheight + bad_row->ascrise) * (1 - textord_pitch_rowsimilarity)) ||
            (!bad_row->all_caps &&
             row->xheight < bad_row->xheight * (1 + textord_pitch_rowsimilarity) &&
             row->xheight > bad_row->xheight * (1 - textord_pitch_rowsimilarity))) {
          if (block_index == block_target) {
            if (row->pitch_decision == PITCH_DEF_FIXED) {
              block_votes += textord_words_veto_power;
              block_stats.add(static_cast<int32_t>(row->fixed_pitch), textord_words_veto_power);
            } else if (row->pitch_decision == PITCH_MAYBE_FIXED ||
                       row->pitch_decision == PITCH_CORR_FIXED) {
              block_votes++;
              block_stats.add(static_cast<int32_t>(row->fixed_pitch), 1);
            } else if (row->pitch_decision == PITCH_DEF_PROP) {
              block_votes -= textord_words_veto_power;
            } else if (row->pitch_decision == PITCH_MAYBE_PROP ||
                       row->pitch_decision == PITCH_CORR_PROP) {
              block_votes--;
            }
          } else {
            if (row->pitch_decision == PITCH_DEF_FIXED) {
              like_votes += textord_words_veto_power;
              like_stats.add(static_cast<int32_t>(row->fixed_pitch), textord_words_veto_power);
            } else if (row->pitch_decision == PITCH_MAYBE_FIXED ||
                       row->pitch_decision == PITCH_CORR_FIXED) {
              like_votes++;
              like_stats.add(static_cast<int32_t>(row->fixed_pitch), 1);
            } else if (row->pitch_decision == PITCH_DEF_PROP) {
              like_votes -= textord_words_veto_power;
            } else if (row->pitch_decision == PITCH_MAYBE_PROP ||
                       row->pitch_decision == PITCH_CORR_PROP) {
              like_votes--;
            }
          }
        } else {
          if (row->pitch_decision == PITCH_DEF_FIXED) {
            other_votes += textord_words_veto_power;
          } else if (row->pitch_decision == PITCH_MAYBE_FIXED ||
                     row->pitch_decision == PITCH_CORR_FIXED) {
            other_votes++;
          } else if (row->pitch_decision == PITCH_DEF_PROP) {
            other_votes -= textord_words_veto_power;
          } else if (row->pitch_decision == PITCH_MAYBE_PROP ||
                     row->pitch_decision == PITCH_CORR_PROP) {
            other_votes--;
          }
        }
      }
      block_index++;
    }
    if (block_votes > textord_words_veto_power) {
      bad_row->fixed_pitch = block_stats.ile(0.5);
      bad_row->pitch_decision = PITCH_CORR_FIXED;
    } else if (block_votes <= textord_words_veto_power && like_votes > 0) {
      bad_row->fixed_pitch = like_stats.ile(0.5);
      bad_row->pitch_decision = PITCH_CORR_FIXED;
    } else {
      bad_row->pitch_decision = PITCH_CORR_PROP;
      if (block_votes == 0 && like_votes == 0 && other_votes > 0 &&
          (textord_debug_pitch || textord_debug_fixed_pitch_test || textord_debug_pitch_metric)) {
        tprintWarn("row {} of block {} set prop with no like rows against trend.\n",
            row_target, block_target);
      }
    }
  }
  if (textord_debug_pitch_metric) {
    tprintDebug(":block_votes={}:like_votes={}:other_votes={}", block_votes, like_votes, other_votes);
    tprintDebug("xheight={}:ascrise={}\n", bad_row->xheight, bad_row->ascrise);
  }
  if (bad_row->pitch_decision == PITCH_CORR_FIXED) {
    if (bad_row->fixed_pitch < textord_min_xheight) {
      if (block_votes > 0) {
        bad_row->fixed_pitch = block_stats.ile(0.5);
      } else if (block_votes == 0 && like_votes > 0) {
        bad_row->fixed_pitch = like_stats.ile(0.5);
      } else {
        tprintWarn("Guessing pitch as xheight on row {}, block {}\n", row_target,
                block_target);
        bad_row->fixed_pitch = bad_row->xheight;
      }
    }
    if (bad_row->fixed_pitch < textord_min_xheight) {
      bad_row->fixed_pitch = (float)textord_min_xheight;
    }
    bad_row->kern_size = bad_row->fixed_pitch / 4;
    bad_row->min_space = static_cast<int32_t>(bad_row->fixed_pitch * 0.6);
    bad_row->max_nonspace = static_cast<int32_t>(bad_row->fixed_pitch * 0.4);
    bad_row->space_threshold = (bad_row->min_space + bad_row->max_nonspace) / 2;
    bad_row->space_size = bad_row->fixed_pitch;
    if (bad_row->char_cells.empty() && !bad_row->blob_list()->empty()) {
      tune_row_pitch(bad_row, &bad_row->projection, bad_row->projection_left,
                     bad_row->projection_right,
                     (bad_row->fixed_pitch + bad_row->max_nonspace * 3) / 4, bad_row->fixed_pitch,
                     sp_sd, mid_cuts, &bad_row->char_cells);
    }
  } else if (bad_row->pitch_decision == PITCH_CORR_PROP ||
             bad_row->pitch_decision == PITCH_DEF_PROP) {
    bad_row->fixed_pitch = 0.0f;
    bad_row->char_cells.clear();
  }
}

/**********************************************************************
 * compute_block_pitch
 *
 * Decide whether each block is fixed pitch individually.
 **********************************************************************/

void compute_block_pitch(TO_BLOCK *block,     // input list
                         FCOORD rotation,     // for drawing
                         int32_t block_index  // block number
) {
  TBOX block_box;                             // bounding box

  block_box = block->block->pdblk.bounding_box();
  if (textord_debug_fixed_pitch_test) {
    tprintDebug("Block {} at ({},{})->({},{})\n", block_index, block_box.left(), block_box.bottom(),
            block_box.right(), block_box.top());
  }
  block->min_space = static_cast<int32_t>(floor(block->xheight * textord_words_default_minspace));
  block->max_nonspace = static_cast<int32_t>(ceil(block->xheight * textord_words_default_nonspace));
  block->fixed_pitch = 0.0f;
  block->space_size = static_cast<float>(block->min_space);
  block->kern_size = static_cast<float>(block->max_nonspace);
  block->pr_nonsp = block->xheight * words_default_prop_nonspace;
  block->pr_space = block->pr_nonsp * textord_spacesize_ratioprop;
  if (!block->get_rows()->empty()) {
    ASSERT_HOST(block->xheight > 0);
    find_repeated_chars(block);
#if !GRAPHICS_DISABLED
    if (textord_show_initial_words) {
      // overlap_picture_ops(true);
      ScrollView::Update();
    }
#endif
    compute_rows_pitch(block, block_index);
  }
}

/**********************************************************************
 * compute_rows_pitch
 *
 * Decide whether each row is fixed pitch individually.
 **********************************************************************/

bool compute_rows_pitch( // find line stats
    TO_BLOCK *block,     // block to do
    int32_t block_index  // block number
) {
  int32_t maxwidth;   // of spaces
  TO_ROW *row;        // current row
  int32_t row_index;  // row number.
  float lower, upper; // cluster thresholds
  TO_ROW_IT row_it = block->get_rows();

  row_index = 1;
  for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
    row = row_it.data();
    ASSERT_HOST(row->xheight > 0);
    row->compute_vertical_projection();
    maxwidth = static_cast<int32_t>(ceil(row->xheight * textord_words_maxspace));
    if (row_pitch_stats(row, maxwidth) &&
        find_row_pitch(row, maxwidth, textord_dotmatrix_gap + 1, block, block_index, row_index)) {
      if (row->fixed_pitch == 0) {
        lower = row->pr_nonsp;
        upper = row->pr_space;
        row->space_size = upper;
        row->kern_size = lower;
      }
    } else {
      row->fixed_pitch = 0.0f; // insufficient data
      row->pitch_decision = PITCH_DUNNO;
    }
    row_index++;
  }
  return false;
}

/**********************************************************************
 * try_doc_fixed
 *
 * Attempt to call the entire document fixed pitch.
 **********************************************************************/

bool try_doc_fixed(             // determine pitch
    ICOORD page_tr,             // top right
    TO_BLOCK_LIST *port_blocks, // input list
    float gradient              // page skew
) {
  int16_t master_x; // uniform shifts
  int16_t pitch;    // median pitch.
  int x;            // profile coord
  int prop_blocks;  // correct counts
  int fixed_blocks;
  int total_row_count; // total in page
                       // iterator
  TO_BLOCK_IT block_it = port_blocks;
  TO_BLOCK *block;         // current block;
  TO_ROW *row;             // current row
  TDimension projection_left; // edges
  TDimension projection_right;
  TDimension row_left; // edges of row
  TDimension row_right;
  float master_y;     // uniform shifts
  float shift_factor; // page skew correction
  float final_pitch;  // output pitch
  float row_y;        // baseline
  STATS projection;   // entire page
  STATS pitches(0, MAX_ALLOWED_PITCH - 1);
  // for median
  float sp_sd;      // space sd
  int16_t mid_cuts; // no of cheap cuts
  float pitch_sd;   // sync rating

  if (!textord_blockndoc_fixed ||
      block_it.empty() || block_it.data()->get_rows()->empty()) {
    return false;
  }
  shift_factor = gradient / (gradient * gradient + 1);
  // row iterator
  TO_ROW_IT row_it(block_it.data()->get_rows());
  master_x = row_it.data()->projection_left;
  master_y = row_it.data()->baseline.y(master_x);
  projection_left = TDIMENSION_MAX;
  projection_right = TDIMENSION_MIN;
  prop_blocks = 0;
  fixed_blocks = 0;
  total_row_count = 0;

  for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
    block = block_it.data();
    row_it.set_to_list(block->get_rows());
    for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
      row = row_it.data();
      total_row_count++;
      if (row->fixed_pitch > 0) {
        pitches.add(static_cast<int32_t>(row->fixed_pitch), 1);
      }
      // find median
      row_y = row->baseline.y(master_x);
      row_left = static_cast<TDimension>(row->projection_left - shift_factor * (master_y - row_y));
      row_right = static_cast<TDimension>(row->projection_right - shift_factor * (master_y - row_y));
      if (row_left < projection_left) {
        projection_left = row_left;
      }
      if (row_right > projection_right) {
        projection_right = row_right;
      }
    }
  }
  if (pitches.get_total() == 0) {
    return false;
  }
  projection.set_range(projection_left, projection_right - 1);

  for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
    block = block_it.data();
    row_it.set_to_list(block->get_rows());
    for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
      row = row_it.data();
      row_y = row->baseline.y(master_x);
      row_left = static_cast<TDimension>(row->projection_left - shift_factor * (master_y - row_y));
      for (x = row->projection_left; x < row->projection_right; x++, row_left++) {
        projection.add(row_left, row->projection.pile_count(x));
      }
    }
  }

  row_it.set_to_list(block_it.data()->get_rows());
  row = row_it.data();
#if !GRAPHICS_DISABLED
  if (textord_show_page_cuts && to_win) {
    projection.plot(to_win, projection_left, row->intercept(), 1.0f, -1.0f, Diagnostics::CORAL);
  }
#endif
  final_pitch = pitches.ile(0.5);
  pitch = static_cast<int16_t>(final_pitch);
  pitch_sd = tune_row_pitch(row, &projection, projection_left, projection_right, pitch * 0.75,
                            final_pitch, sp_sd, mid_cuts, &row->char_cells);

  if (textord_debug_pitch_metric) {
    tprintDebug(
        "try_doc:prop_blocks={}:fixed_blocks={}:pitch={}:final_pitch={}:pitch_sd={}:sp_sd={},trc(rowcount)={}"
        ":sd/trc={}:sd/pitch={}:sd/trc/pitch={}\n",
        prop_blocks, fixed_blocks, pitch, final_pitch, pitch_sd, sp_sd, total_row_count, pitch_sd / total_row_count,
        pitch_sd / pitch, pitch_sd / total_row_count / pitch);
  }

#if !GRAPHICS_DISABLED
  if (textord_show_page_cuts && to_win) {
    float row_shift;              // shift for row
    ICOORDELT_LIST *master_cells; // cells for page
    master_cells = &row->char_cells;
    for (block_it.mark_cycle_pt(); !block_it.cycled_list(); block_it.forward()) {
      block = block_it.data();
      row_it.set_to_list(block->get_rows());
      for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
        row = row_it.data();
        row_y = row->baseline.y(master_x);
        row_shift = shift_factor * (master_y - row_y);
        plot_row_cells(to_win, Diagnostics::GOLDENROD, row, row_shift, master_cells);
      }
    }
  }
#endif
  row->char_cells.clear();
  return false;
}

/**********************************************************************
 * try_block_fixed
 *
 * Try to call the entire block fixed.
 **********************************************************************/

bool try_block_fixed(   // find line stats
    TO_BLOCK *block,    // block to do
    int32_t block_index // block number
) {
  return false;
}

/**********************************************************************
 * try_rows_fixed
 *
 * Decide whether each row is fixed pitch individually.
 **********************************************************************/

bool try_rows_fixed(     // find line stats
    TO_BLOCK *block,     // block to do
    int32_t block_index  // block number
) {
  TO_ROW *row;           // current row
  int32_t def_fixed = 0; // counters
  int32_t def_prop = 0;
  int32_t maybe_fixed = 0;
  int32_t maybe_prop = 0;
  int32_t dunno = 0;
  int32_t corr_fixed = 0;
  int32_t corr_prop = 0;
  float lower, upper; // cluster thresholds
  TO_ROW_IT row_it = block->get_rows();

  for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
    row = row_it.data();
    ASSERT_HOST(row->xheight > 0);
    if (row->fixed_pitch > 0 && fixed_pitch_row(row, block->block, block_index)) {
      if (row->fixed_pitch == 0) {
        lower = row->pr_nonsp;
        upper = row->pr_space;
        row->space_size = upper;
        row->kern_size = lower;
      }
    }
  }
  count_block_votes(block, def_fixed, def_prop, maybe_fixed, maybe_prop, corr_fixed, corr_prop,
                    dunno);
  if (textord_debug_fixed_pitch_test || textord_blocksall_prop || textord_blocksall_fixed) {
    tprintDebug("Initially:");
    print_block_counts(block, block_index);
  }
  if (def_fixed > def_prop * textord_words_veto_power) {
    block->pitch_decision = PITCH_DEF_FIXED;
  } else if (def_prop > def_fixed * textord_words_veto_power) {
    block->pitch_decision = PITCH_DEF_PROP;
  } else if (def_fixed > 0 || def_prop > 0) {
    block->pitch_decision = PITCH_DUNNO;
  } else if (maybe_fixed > maybe_prop * textord_words_veto_power) {
    block->pitch_decision = PITCH_MAYBE_FIXED;
  } else if (maybe_prop > maybe_fixed * textord_words_veto_power) {
    block->pitch_decision = PITCH_MAYBE_PROP;
  } else {
    block->pitch_decision = PITCH_DUNNO;
  }
  return false;
}

/**********************************************************************
 * print_block_counts
 *
 * Count up how many rows have what decision and print the results.
 **********************************************************************/

void print_block_counts( // find line stats
    TO_BLOCK *block,     // block to do
    int32_t block_index  // block number
) {
  int32_t def_fixed = 0; // counters
  int32_t def_prop = 0;
  int32_t maybe_fixed = 0;
  int32_t maybe_prop = 0;
  int32_t dunno = 0;
  int32_t corr_fixed = 0;
  int32_t corr_prop = 0;

  count_block_votes(block, def_fixed, def_prop, maybe_fixed, maybe_prop, corr_fixed, corr_prop,
                    dunno);
  tprintDebug("Block {} has ({},{},{})", block_index, def_fixed, maybe_fixed, corr_fixed);
  if (textord_blocksall_prop && (def_fixed || maybe_fixed || corr_fixed)) {
    tprintDebug(" (Wrongly)");
  }
  tprintDebug(" fixed, ({},{},{})", def_prop, maybe_prop, corr_prop);
  if (textord_blocksall_fixed && (def_prop || maybe_prop || corr_prop)) {
    tprintDebug(" (Wrongly)");
  }
  tprintDebug(" prop, {} dunno\n", dunno);
}

/**********************************************************************
 * count_block_votes
 *
 * Count the number of rows in the block with each kind of pitch_decision.
 **********************************************************************/

void count_block_votes( // find line stats
    TO_BLOCK *block,    // block to do
    int32_t &def_fixed, // add to counts
    int32_t &def_prop, int32_t &maybe_fixed, int32_t &maybe_prop, int32_t &corr_fixed,
    int32_t &corr_prop, int32_t &dunno) {
  TO_ROW *row; // current row
  TO_ROW_IT row_it = block->get_rows();

  for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
    row = row_it.data();
    switch (row->pitch_decision) {
      case PITCH_DUNNO:
        dunno++;
        break;
      case PITCH_DEF_PROP:
        def_prop++;
        break;
      case PITCH_MAYBE_PROP:
        maybe_prop++;
        break;
      case PITCH_DEF_FIXED:
        def_fixed++;
        break;
      case PITCH_MAYBE_FIXED:
        maybe_fixed++;
        break;
      case PITCH_CORR_PROP:
        corr_prop++;
        break;
      case PITCH_CORR_FIXED:
        corr_fixed++;
        break;
    }
  }
}

/**********************************************************************
 * row_pitch_stats
 *
 * Decide whether each row is fixed pitch individually.
 **********************************************************************/

bool row_pitch_stats( // find line stats
    TO_ROW *row,      // current row
    int32_t maxwidth  // of spaces
) {
  BLOBNBOX *blob;        // current blob
  int gap_index;         // current gap
  int32_t prev_x;        // end of prev blob
  int32_t cluster_count; // no of clusters
  int32_t prev_count;    // of clusters
  int32_t smooth_factor; // for smoothing stats
  TBOX blob_box;         // bounding box
  float lower, upper;    // cluster thresholds
                         // gap sizes
  float gaps[BLOCK_STATS_CLUSTERS];
  // blobs
  BLOBNBOX_IT blob_it = row->blob_list();
  STATS gap_stats(0, maxwidth - 1);
  STATS cluster_stats[BLOCK_STATS_CLUSTERS + 1];
  // clusters

  smooth_factor = static_cast<int32_t>(row->xheight * textord_wordstats_smooth_factor + 1.5);
  if (!blob_it.empty()) {
    prev_x = blob_it.data()->bounding_box().right();
    blob_it.forward();
    while (!blob_it.at_first()) {
      blob = blob_it.data();
      if (!blob->joined_to_prev()) {
        blob_box = blob->bounding_box();
        if (blob_box.left() - prev_x < maxwidth) {
          gap_stats.add(blob_box.left() - prev_x, 1);
        }
        prev_x = blob_box.right();
      }
      blob_it.forward();
    }
  }
  if (gap_stats.get_total() == 0) {
    return false;
  }
  cluster_count = 0;
  lower = row->xheight * words_initial_lower;
  upper = row->xheight * words_initial_upper;
  gap_stats.smooth(smooth_factor);
  do {
    prev_count = cluster_count;
    cluster_count = gap_stats.cluster(lower, upper, textord_spacesize_ratioprop,
                                      BLOCK_STATS_CLUSTERS, cluster_stats);
  } while (cluster_count > prev_count && cluster_count < BLOCK_STATS_CLUSTERS);
  if (cluster_count < 1) {
    return false;
  }
  for (gap_index = 0; gap_index < cluster_count; gap_index++) {
    gaps[gap_index] = cluster_stats[gap_index + 1].ile(0.5);
  }
  // get medians
  if (textord_debug_pitch) {
    tprintDebug("cluster_count={}:", cluster_count);
    for (gap_index = 0; gap_index < cluster_count; gap_index++) {
      tprintDebug(" {}({})", gaps[gap_index], cluster_stats[gap_index + 1].get_total());
    }
    tprintDebug("\n");
  }
  qsort(gaps, cluster_count, sizeof(float), sort_floats);

  // Try to find proportional non-space and space for row.
  lower = row->xheight * words_default_prop_nonspace;
  upper = row->xheight * textord_words_min_minspace;
  for (gap_index = 0; gap_index < cluster_count && gaps[gap_index] < lower; gap_index++) {
    ;
  }
  if (gap_index == 0) {
    if (textord_debug_pitch) {
      tprintDebug("No clusters below nonspace threshold!!\n");
    }
    if (cluster_count > 1) {
      row->pr_nonsp = gaps[0];
      row->pr_space = gaps[1];
    } else {
      row->pr_nonsp = lower;
      row->pr_space = gaps[0];
    }
  } else {
    row->pr_nonsp = gaps[gap_index - 1];
    while (gap_index < cluster_count && gaps[gap_index] < upper) {
      gap_index++;
    }
    if (gap_index == cluster_count) {
      if (textord_debug_pitch) {
        tprintDebug("No clusters above nonspace threshold!!\n");
      }
      row->pr_space = lower * textord_spacesize_ratioprop;
    } else {
      row->pr_space = gaps[gap_index];
    }
  }

  // Now try to find the fixed pitch space and non-space.
  upper = row->xheight * words_default_fixed_space;
  for (gap_index = 0; gap_index < cluster_count && gaps[gap_index] < upper; gap_index++) {
    ;
  }
  if (gap_index == 0) {
    if (textord_debug_pitch) {
      tprintDebug("No clusters below space threshold!!\n");
    }
    row->fp_nonsp = upper;
    row->fp_space = gaps[0];
  } else {
    row->fp_nonsp = gaps[gap_index - 1];
    if (gap_index == cluster_count) {
      if (textord_debug_pitch) {
        tprintDebug("No clusters above space threshold!!\n");
      }
      row->fp_space = row->xheight;
    } else {
      row->fp_space = gaps[gap_index];
    }
  }
  if (textord_debug_pitch) {
    tprintDebug(
        "Initial estimates: pr_nonsp={}, pr_space={}, fp_nonsp={}, "
        "fp_space={}\n",
        row->pr_nonsp, row->pr_space, row->fp_nonsp, row->fp_space);
  }
  return true; // computed some stats
}

/**********************************************************************
 * find_row_pitch
 *
 * Check to see if this row could be fixed pitch using the given spacings.
 * Blobs with gaps smaller than the lower threshold are assumed to be one.
 * The larger threshold is the word gap threshold.
 **********************************************************************/

bool find_row_pitch(     // find lines
    TO_ROW *row,         // row to do
    int32_t maxwidth,    // max permitted space
    int32_t dm_gap,      // ignorable gaps
    TO_BLOCK *block,     // block of row
    int32_t block_index, // block_number
    int32_t row_index    // number of row
) {
  bool used_dm_model; // looks like dot matrix
  float min_space;    // estimate threshold
  float non_space;    // gap size
  float gap_iqr;      // interquartile range
  float pitch_iqr;
  float dm_gap_iqr; // interquartile range
  float dm_pitch_iqr;
  float dm_pitch;      // pitch with dm on
  float pitch;         // revised estimate
  float initial_pitch; // guess at pitch
  STATS gap_stats(0, maxwidth - 1);
  // centre-centre
  STATS pitch_stats(0, maxwidth - 1);

  row->fixed_pitch = 0.0f;
  initial_pitch = row->fp_space;
  if (initial_pitch > row->xheight * (1 + words_default_fixed_limit)) {
    initial_pitch = row->xheight; // keep pitch decent
  }
  non_space = row->fp_nonsp;
  if (non_space > initial_pitch) {
    non_space = initial_pitch;
  }
  min_space = (initial_pitch + non_space) / 2;

  if (!count_pitch_stats(row, &gap_stats, &pitch_stats, initial_pitch, min_space, true, false,
                         dm_gap)) {
    dm_gap_iqr = 0.0001f;
    dm_pitch_iqr = maxwidth * 2.0f;
    dm_pitch = initial_pitch;
  } else {
    dm_gap_iqr = gap_stats.ile(0.75) - gap_stats.ile(0.25);
    dm_pitch_iqr = pitch_stats.ile(0.75) - pitch_stats.ile(0.25);
    dm_pitch = pitch_stats.ile(0.5);
  }
  gap_stats.clear();
  pitch_stats.clear();
  if (!count_pitch_stats(row, &gap_stats, &pitch_stats, initial_pitch, min_space, true, false, 0)) {
    gap_iqr = 0.0001f;
    pitch_iqr = maxwidth * 3.0f;
  } else {
    gap_iqr = gap_stats.ile(0.75) - gap_stats.ile(0.25);
    pitch_iqr = pitch_stats.ile(0.75) - pitch_stats.ile(0.25);
    if (textord_debug_pitch) {
      tprintDebug(
          "First fp iteration:initial_pitch={}, gap_iqr={}, pitch_iqr={}, "
          "pitch={}\n",
          initial_pitch, gap_iqr, pitch_iqr, pitch_stats.ile(0.5));
    }
    initial_pitch = pitch_stats.ile(0.5);
    if (min_space > initial_pitch && count_pitch_stats(row, &gap_stats, &pitch_stats, initial_pitch,
                                                       initial_pitch, true, false, 0)) {
      min_space = initial_pitch;
      gap_iqr = gap_stats.ile(0.75) - gap_stats.ile(0.25);
      pitch_iqr = pitch_stats.ile(0.75) - pitch_stats.ile(0.25);
      if (textord_debug_pitch) {
        tprintDebug(
            "Revised fp iteration:initial_pitch={}, gap_iqr={}, pitch_iqr={}, "
            "pitch={}\n",
            initial_pitch, gap_iqr, pitch_iqr, pitch_stats.ile(0.5));
      }
      initial_pitch = pitch_stats.ile(0.5);
    }
  }
  if (textord_debug_pitch_metric) {
    tprintDebug("Blk={}:Row={}:{}:p_iqr={}:g_iqr={}:dm_p_iqr={}:dm_g_iqr={}:{}:", block_index,
            row_index, "X", pitch_iqr, gap_iqr, dm_pitch_iqr, dm_gap_iqr,
            pitch_iqr > maxwidth && dm_pitch_iqr > maxwidth
                ? "D"
                : (pitch_iqr * dm_gap_iqr <= dm_pitch_iqr * gap_iqr ? "S" : "M"));
  }
  if (pitch_iqr > maxwidth && dm_pitch_iqr > maxwidth) {
    row->pitch_decision = PITCH_DUNNO;
    if (textord_debug_pitch_metric) {
      tprintDebug("\n");
    }
    return false; // insufficient data
  }
  if (pitch_iqr * dm_gap_iqr <= dm_pitch_iqr * gap_iqr) {
    if (textord_debug_pitch) {
      tprintDebug(
          "Choosing non dm version:pitch_iqr={}, gap_iqr={}, dm_pitch_iqr={}, "
          "dm_gap_iqr={}\n",
          pitch_iqr, gap_iqr, dm_pitch_iqr, dm_gap_iqr);
    }
    gap_iqr = gap_stats.ile(0.75) - gap_stats.ile(0.25);
    pitch_iqr = pitch_stats.ile(0.75) - pitch_stats.ile(0.25);
    pitch = pitch_stats.ile(0.5);
    used_dm_model = false;
  } else {
    if (textord_debug_pitch) {
      tprintDebug(
          "Choosing dm version:pitch_iqr={}, gap_iqr={}, dm_pitch_iqr={}, "
          "dm_gap_iqr={}\n",
          pitch_iqr, gap_iqr, dm_pitch_iqr, dm_gap_iqr);
    }
    gap_iqr = dm_gap_iqr;
    pitch_iqr = dm_pitch_iqr;
    pitch = dm_pitch;
    used_dm_model = true;
  }
  if (textord_debug_pitch_metric) {
    tprintDebug("rev_p_iqr={}:rev_g_iqr={}:pitch={}:", pitch_iqr, gap_iqr, pitch);
    tprintDebug("p_iqr/g={}:p_iqr/x={}:iqr_res={}:", pitch_iqr / gap_iqr, pitch_iqr / block->xheight,
            pitch_iqr < gap_iqr * textord_fpiqr_ratio &&
                    pitch_iqr < block->xheight * textord_max_pitch_iqr &&
                    pitch < block->xheight * textord_words_default_maxspace
                ? "F"
                : "P");
  }
  if (pitch_iqr < gap_iqr * textord_fpiqr_ratio &&
      pitch_iqr < block->xheight * textord_max_pitch_iqr &&
      pitch < block->xheight * textord_words_default_maxspace) {
    row->pitch_decision = PITCH_MAYBE_FIXED;
  } else {
    row->pitch_decision = PITCH_MAYBE_PROP;
  }
  row->fixed_pitch = pitch;
  row->kern_size = gap_stats.ile(0.5);
  row->min_space = static_cast<int32_t>(row->fixed_pitch + non_space) / 2;
  if (row->min_space > row->fixed_pitch) {
    row->min_space = static_cast<int32_t>(row->fixed_pitch);
  }
  row->max_nonspace = row->min_space;
  row->space_size = row->fixed_pitch;
  row->space_threshold = (row->max_nonspace + row->min_space) / 2;
  row->used_dm_model = used_dm_model;
  return true;
}

/**********************************************************************
 * fixed_pitch_row
 *
 * Check to see if this row could be fixed pitch using the given spacings.
 * Blobs with gaps smaller than the lower threshold are assumed to be one.
 * The larger threshold is the word gap threshold.
 **********************************************************************/

bool fixed_pitch_row(TO_ROW *row, // row to do
                     BLOCK *block,
                     int32_t block_index // block_number
) {
  const char *res_string; // pitch result
  int16_t mid_cuts;       // no of cheap cuts
  float non_space;        // gap size
  float pitch_sd;         // error on pitch
  float sp_sd = 0.0f;     // space sd

  non_space = row->fp_nonsp;
  if (non_space > row->fixed_pitch) {
    non_space = row->fixed_pitch;
  }
  POLY_BLOCK *pb = block != nullptr ? block->pdblk.poly_block() : nullptr;
  if (textord_all_prop || (pb != nullptr && !pb->IsText())) {
    // Set the decision to definitely proportional.
    pitch_sd = textord_words_def_prop * row->fixed_pitch;
    row->pitch_decision = PITCH_DEF_PROP;
  } else {
    pitch_sd = tune_row_pitch(row, &row->projection, row->projection_left, row->projection_right,
                              (row->fixed_pitch + non_space * 3) / 4, row->fixed_pitch, sp_sd,
                              mid_cuts, &row->char_cells);
    if (pitch_sd < textord_words_pitchsd_threshold * row->fixed_pitch &&
        ((pitsync_linear_version & 3) < 3 ||
         ((pitsync_linear_version & 3) >= 3 &&
          (row->used_dm_model || sp_sd > 20 || (pitch_sd == 0 && sp_sd > 10))))) {
      if (pitch_sd < textord_words_def_fixed * row->fixed_pitch && !row->all_caps &&
          ((pitsync_linear_version & 3) < 3 || sp_sd > 20)) {
        row->pitch_decision = PITCH_DEF_FIXED;
      } else {
        row->pitch_decision = PITCH_MAYBE_FIXED;
      }
    } else if ((pitsync_linear_version & 3) < 3 || sp_sd > 20 || mid_cuts > 0 ||
               pitch_sd >= textord_words_pitchsd_threshold * row->fixed_pitch) {
      if (pitch_sd < textord_words_def_prop * row->fixed_pitch) {
        row->pitch_decision = PITCH_MAYBE_PROP;
      } else {
        row->pitch_decision = PITCH_DEF_PROP;
      }
    } else {
      row->pitch_decision = PITCH_DUNNO;
    }
  }

  if (textord_debug_pitch_metric) {
    res_string = "??";
    switch (row->pitch_decision) {
      case PITCH_DEF_PROP:
        res_string = "DP";
        break;
      case PITCH_MAYBE_PROP:
        res_string = "MP";
        break;
      case PITCH_DEF_FIXED:
        res_string = "DF";
        break;
      case PITCH_MAYBE_FIXED:
        res_string = "MF";
        break;
      default:
        ;
    }
    tprintDebug(":sd/p={}:occ={}:init_res={}\n", pitch_sd / row->fixed_pitch, sp_sd, res_string);
  }
  return true;
}

/**********************************************************************
 * count_pitch_stats
 *
 * Count up the gap and pitch stats on the block to see if it is fixed pitch.
 * Blobs with gaps smaller than the lower threshold are assumed to be one.
 * The larger threshold is the word gap threshold.
 * The return value indicates whether there were any decent values to use.
 **********************************************************************/

bool count_pitch_stats(  // find lines
    TO_ROW *row,         // row to do
    STATS *gap_stats,    // blob gaps
    STATS *pitch_stats,  // centre-centre stats
    float initial_pitch, // guess at pitch
    float min_space,     // estimate space size
    bool ignore_outsize, // discard big objects
    bool split_outsize,  // split big objects
    int32_t dm_gap       // ignorable gaps
) {
  bool prev_valid; // not word broken
  BLOBNBOX *blob;  // current blob
                   // blobs
  BLOBNBOX_IT blob_it = row->blob_list();
  int32_t prev_right;  // end of prev blob
  int32_t prev_centre; // centre of previous blob
  int32_t x_centre;    // centre of this blob
  int32_t blob_width;  // width of blob
  int32_t width_units; // no of widths in blob
  float width;         // blob width
  TBOX blob_box;       // bounding box
  TBOX joined_box;     // of super blob

  gap_stats->clear();
  pitch_stats->clear();
  if (blob_it.empty()) {
    return false;
  }
  prev_valid = false;
  prev_centre = 0;
  prev_right = 0; // stop compiler warning
  joined_box = blob_it.data()->bounding_box();
  do {
    blob_it.forward();
    blob = blob_it.data();
    if (!blob->joined_to_prev()) {
      blob_box = blob->bounding_box();
      if ((blob_box.left() - joined_box.right() < dm_gap && !blob_it.at_first()) ||
          blob->cblob() == nullptr) {
        joined_box += blob_box; // merge blobs
      } else {
        blob_width = joined_box.width();
        if (split_outsize) {
          width_units =
              static_cast<int32_t>(floor(static_cast<float>(blob_width) / initial_pitch + 0.5));
          if (width_units < 1) {
            width_units = 1;
          }
          width_units--;
        } else if (ignore_outsize) {
          width = static_cast<float>(blob_width) / initial_pitch;
          width_units =
              width < 1 + words_default_fixed_limit && width > 1 - words_default_fixed_limit ? 0
                                                                                             : -1;
        } else {
          width_units = 0; // everything in
        }
        x_centre = static_cast<int32_t>(joined_box.left() +
                                        (blob_width - width_units * initial_pitch) / 2);
        if (prev_valid && width_units >= 0) {
          //                                              if (width_units>0)
          //                                              {
          //                                                      tprintDebug("wu={},
          //                                                      width={},
          //                                                      xc={}, adding
          //                                                      {}\n",
          //                                                              width_units,blob_width,x_centre,x_centre-prev_centre);
          //                                              }
          gap_stats->add(joined_box.left() - prev_right, 1);
          pitch_stats->add(x_centre - prev_centre, 1);
        }
        prev_centre = static_cast<int32_t>(x_centre + width_units * initial_pitch);
        prev_right = joined_box.right();
        prev_valid = blob_box.left() - joined_box.right() < min_space;
        prev_valid = prev_valid && width_units >= 0;
        joined_box = blob_box;
      }
    }
  } while (!blob_it.at_first());
  return gap_stats->get_total() >= 3;
}

/**********************************************************************
 * tune_row_pitch
 *
 * Use a dp algorithm to fit the character cells and return the sd of
 * the cell size over the row.
 **********************************************************************/

float tune_row_pitch(           // find fp cells
    TO_ROW *row,                // row to do
    STATS *projection,          // vertical projection
    TDimension projection_left,    // edge of projection
    TDimension projection_right,   // edge of projection
    float space_size,           // size of blank
    float &initial_pitch,       // guess at pitch
    float &best_sp_sd,          // space sd
    int16_t &best_mid_cuts,     // no of cheap cuts
    ICOORDELT_LIST *best_cells  // row cells
) {
  int pitch_delta;           // offset pitch
  int16_t mid_cuts;          // cheap cuts
  float pitch_sd;            // current sd
  float best_sd;             // best result
  float best_pitch;          // pitch for best result
  float initial_sd;          // starting error
  float sp_sd;               // space sd
  ICOORDELT_LIST test_cells; // row cells
  ICOORDELT_IT best_it;      // start of best list

  if (textord_fast_pitch_test) {
    return tune_row_pitch2(row, projection, projection_left, projection_right, space_size,
                           initial_pitch, best_sp_sd,
                           // space sd
                           best_mid_cuts, best_cells);
  }
  if (textord_disable_pitch_test) {
    best_sp_sd = initial_pitch;
    return initial_pitch;
  }
  initial_sd = compute_pitch_sd(row, projection, projection_left, projection_right, space_size,
                                initial_pitch, best_sp_sd, best_mid_cuts, best_cells);
  best_sd = initial_sd;
  best_pitch = initial_pitch;
  if (textord_debug_pitch) {
    tprintDebug("tune_row_pitch:start pitch={}, sd={}\n", best_pitch, best_sd);
  }
  for (pitch_delta = 1; pitch_delta <= textord_pitch_range; pitch_delta++) {
    pitch_sd =
        compute_pitch_sd(row, projection, projection_left, projection_right, space_size,
                         initial_pitch + pitch_delta, sp_sd, mid_cuts, &test_cells);
    if (textord_debug_pitch) {
      tprintDebug("testing pitch at {}, sd={}\n", initial_pitch + pitch_delta, pitch_sd);
    }
    if (pitch_sd < best_sd) {
      best_sd = pitch_sd;
      best_mid_cuts = mid_cuts;
      best_sp_sd = sp_sd;
      best_pitch = initial_pitch + pitch_delta;
      best_cells->clear();
      best_it.set_to_list(best_cells);
      best_it.add_list_after(&test_cells);
    } else {
      test_cells.clear();
    }
    if (pitch_sd > initial_sd) {
      break; // getting worse
    }
  }
  for (pitch_delta = 1; pitch_delta <= textord_pitch_range; pitch_delta++) {
    pitch_sd =
        compute_pitch_sd(row, projection, projection_left, projection_right, space_size,
                         initial_pitch - pitch_delta, sp_sd, mid_cuts, &test_cells);
    if (textord_debug_pitch) {
      tprintDebug("testing pitch at {}, sd={}\n", initial_pitch - pitch_delta, pitch_sd);
    }
    if (pitch_sd < best_sd) {
      best_sd = pitch_sd;
      best_mid_cuts = mid_cuts;
      best_sp_sd = sp_sd;
      best_pitch = initial_pitch - pitch_delta;
      best_cells->clear();
      best_it.set_to_list(best_cells);
      best_it.add_list_after(&test_cells);
    } else {
      test_cells.clear();
    }
    if (pitch_sd > initial_sd) {
      break;
    }
  }
  initial_pitch = best_pitch;

  if (textord_debug_pitch_metric) {
    print_pitch_sd(row, projection, projection_left, projection_right, space_size, best_pitch);
  }

  return best_sd;
}

/**********************************************************************
 * tune_row_pitch
 *
 * Use a dp algorithm to fit the character cells and return the sd of
 * the cell size over the row.
 **********************************************************************/

float tune_row_pitch2(          // find fp cells
    TO_ROW *row,                // row to do
    STATS *projection,          // vertical projection
    TDimension projection_left,    // edge of projection
    TDimension projection_right,   // edge of projection
    float space_size,           // size of blank
    float &initial_pitch,       // guess at pitch
    float &best_sp_sd,          // space sd
    int16_t &best_mid_cuts,     // no of cheap cuts
    ICOORDELT_LIST *best_cells  // row cells
) {
  int pitch_delta;    // offset pitch
  int16_t pixel;      // pixel coord
  int16_t best_pixel; // pixel coord
  int16_t best_delta; // best pitch
  int16_t best_pitch; // best pitch
  int16_t start;      // of good range
  int16_t end;        // of good range
  int32_t best_count; // lowest sum
  float best_sd;      // best result

  best_sp_sd = initial_pitch;

  best_pitch = static_cast<int>(initial_pitch);
  if (textord_disable_pitch_test || best_pitch <= textord_pitch_range) {
    return initial_pitch;
  }
  std::unique_ptr<STATS[]> sum_proj(new STATS[textord_pitch_range * 2 + 1]); // summed projection

  for (pitch_delta = -textord_pitch_range; pitch_delta <= textord_pitch_range; pitch_delta++) {
    sum_proj[textord_pitch_range + pitch_delta].set_range(0, best_pitch + pitch_delta);
  }
  for (pixel = projection_left; pixel <= projection_right; pixel++) {
    for (pitch_delta = -textord_pitch_range; pitch_delta <= textord_pitch_range; pitch_delta++) {
      sum_proj[textord_pitch_range + pitch_delta].add(
          (pixel - projection_left) % (best_pitch + pitch_delta), projection->pile_count(pixel));
    }
  }
  best_count = sum_proj[textord_pitch_range].pile_count(0);
  best_delta = 0;
  best_pixel = 0;
  for (pitch_delta = -textord_pitch_range; pitch_delta <= textord_pitch_range; pitch_delta++) {
    for (pixel = 0; pixel < best_pitch + pitch_delta; pixel++) {
      if (sum_proj[textord_pitch_range + pitch_delta].pile_count(pixel) < best_count) {
        best_count = sum_proj[textord_pitch_range + pitch_delta].pile_count(pixel);
        best_delta = pitch_delta;
        best_pixel = pixel;
      }
    }
  }
  if (textord_debug_pitch) {
    tprintDebug("tune_row_pitch:start pitch={}, best_delta={}, count={}\n", initial_pitch, best_delta,
            best_count);
  }
  best_pitch += best_delta;
  initial_pitch = best_pitch;
  best_count++;
  best_count += best_count;
  for (start = best_pixel - 2;
       start > best_pixel - best_pitch &&
       sum_proj[textord_pitch_range + best_delta].pile_count(start % best_pitch) <= best_count;
       start--) {
    ;
  }
  for (end = best_pixel + 2;
       end < best_pixel + best_pitch &&
       sum_proj[textord_pitch_range + best_delta].pile_count(end % best_pitch) <= best_count;
       end++) {
    ;
  }

  best_sd = compute_pitch_sd(row, projection, projection_left, projection_right, space_size,
                             initial_pitch, best_sp_sd, best_mid_cuts, best_cells, 
                             start, end);
  if (textord_debug_pitch) {
    tprintDebug("tune_row_pitch:output pitch={}, best_sd={}\n", initial_pitch, best_sd);
  }

  if (textord_debug_pitch_metric) {
    print_pitch_sd(row, projection, projection_left, projection_right, space_size, initial_pitch);
  }

  return best_sd;
}

/**********************************************************************
 * compute_pitch_sd
 *
 * Use a dp algorithm to fit the character cells and return the sd of
 * the cell size over the row.
 **********************************************************************/

float compute_pitch_sd(        // find fp cells
    TO_ROW *row,               // row to do
    STATS *projection,         // vertical projection
    TDimension projection_left,   // edge
    TDimension projection_right,  // edge
    float space_size,          // size of blank
    float initial_pitch,       // guess at pitch
    float &sp_sd,              // space sd
    int16_t &mid_cuts,         // no of free cuts
    ICOORDELT_LIST *row_cells, // list of chop pts
    int16_t start,             // start of good range
    int16_t end                // end of good range
) {
  int16_t occupation; // no of cells in word.
                      // blobs
  BLOBNBOX_IT blob_it = row->blob_list();
  BLOBNBOX_IT start_it;  // start of word
  BLOBNBOX_IT plot_it;   // for plotting
  int16_t blob_count;    // no of blobs
  TBOX blob_box;         // bounding box
  TBOX prev_box;         // of super blob
  int32_t prev_right;    // of word sync
  int scale_factor;      // on scores for big words
  int32_t sp_count;      // spaces
  FPSEGPT_LIST seg_list; // char cells
  FPSEGPT_IT seg_it;     // iterator
  TDimension segpos;     // position of segment
  TDimension cellpos;    // previous cell boundary
                         // iterator
  ICOORDELT_IT cell_it = row_cells;
  ICOORDELT *cell;     // new cell
  double sqsum;        // sum of squares
  double spsum;        // of spaces
  double sp_var;       // space error
  double word_sync;    // result for word
  int32_t total_count; // total blobs

  if ((pitsync_linear_version & 3) > 1) {
    word_sync = compute_pitch_sd2(row, projection, projection_left, projection_right, initial_pitch,
                                  occupation, mid_cuts, row_cells, start, end);
    sp_sd = occupation;
    return word_sync;
  }
  mid_cuts = 0;
  cellpos = 0;
  total_count = 0;
  sqsum = 0;
  sp_count = 0;
  spsum = 0;
  prev_right = -1;
  if (blob_it.empty()) {
    return space_size * 10;
  }
#if !GRAPHICS_DISABLED
  if (to_win) {
    blob_box = blob_it.data()->bounding_box();
    projection->plot(to_win, projection_left, row->intercept(), 1.0f, -1.0f, Diagnostics::CORAL);
  }
#endif
  start_it = blob_it;
  blob_count = 0;
  blob_box = box_next(&blob_it); // first blob
  blob_it.mark_cycle_pt();
  do {
    for (; blob_count > 0; blob_count--) {
      box_next(&start_it);
    }
    do {
      prev_box = blob_box;
      blob_count++;
      blob_box = box_next(&blob_it);
    } while (!blob_it.cycled_list() && blob_box.left() - prev_box.right() < space_size);
    plot_it = start_it;
    if (pitsync_linear_version & 3) {
      word_sync = check_pitch_sync2(&start_it, blob_count, static_cast<int16_t>(initial_pitch), 2,
                                    projection, projection_left, projection_right,
                                    row->xheight * textord_projection_scale, occupation, &seg_list,
                                    start, end);
    } else {
      word_sync = check_pitch_sync(&start_it, blob_count, static_cast<int16_t>(initial_pitch), 2,
                                   projection, &seg_list);
    }
    if (textord_debug_pitch) {
      tprintDebug("Word ending at ({},{}), len={}, sync rating={}, positions: ", prev_box.right(), prev_box.top(),
              seg_list.length() - 1, word_sync);
      seg_it.set_to_list(&seg_list);
      for (seg_it.mark_cycle_pt(); !seg_it.cycled_list(); seg_it.forward()) {
        if (seg_it.data()->faked) {
          tprintDebug("(F)");
        }
        tprintDebug("x={}, ", seg_it.data()->position());
        //                              tprintDebug("C={}, s={}, sq={}\n",
        //                                      seg_it.data()->cost_function(),
        //                                      seg_it.data()->sum(),
        //                                      seg_it.data()->squares());
      }
      tprintDebug("\n");
    }
#if !GRAPHICS_DISABLED
    if (textord_show_fixed_cuts && blob_count > 0 && to_win) {
      plot_fp_cells2(to_win, Diagnostics::GOLDENROD, row, &seg_list);
    }
#endif
    seg_it.set_to_list(&seg_list);
    if (prev_right >= 0) {
      sp_var = seg_it.data()->position() - prev_right;
      sp_var -= floor(sp_var / initial_pitch + 0.5) * initial_pitch;
      sp_var *= sp_var;
      spsum += sp_var;
      sp_count++;
    }
    for (seg_it.mark_cycle_pt(); !seg_it.cycled_list(); seg_it.forward()) {
      segpos = seg_it.data()->position();
      if (cell_it.empty() || segpos > cellpos + initial_pitch / 2) {
        // big gap
        while (!cell_it.empty() && segpos > cellpos + initial_pitch * 3 / 2) {
          cell = new ICOORDELT(cellpos + static_cast<TDimension>(initial_pitch), 0);
          cell_it.add_after_then_move(cell);
          cellpos += static_cast<TDimension>(initial_pitch);
        }
        // make new one
        cell = new ICOORDELT(segpos, 0);
        cell_it.add_after_then_move(cell);
        cellpos = segpos;
      } else if (segpos > cellpos - initial_pitch / 2) {
        cell = cell_it.data();
        // average positions
        cell->set_x((cellpos + segpos) / 2);
        cellpos = cell->x();
      }
    }
    seg_it.move_to_last();
    prev_right = seg_it.data()->position();
    if (textord_pitch_scalebigwords) {
      scale_factor = (seg_list.length() - 2) / 2;
      if (scale_factor < 1) {
        scale_factor = 1;
      }
    } else {
      scale_factor = 1;
    }
    sqsum += word_sync * scale_factor;
    total_count += (seg_list.length() - 1) * scale_factor;
    seg_list.clear();
  } while (!blob_it.cycled_list());
  sp_sd = sp_count > 0 ? sqrt(spsum / sp_count) : 0;
  return total_count > 0 ? sqrt(sqsum / total_count) : space_size * 10;
}

/**********************************************************************
 * compute_pitch_sd2
 *
 * Use a dp algorithm to fit the character cells and return the sd of
 * the cell size over the row.
 **********************************************************************/

float compute_pitch_sd2(       // find fp cells
    TO_ROW *row,               // row to do
    STATS *projection,         // vertical projection
    TDimension projection_left,   // edge
    TDimension projection_right,  // edge
    float initial_pitch,       // guess at pitch
    int16_t &occupation,       // no of occupied cells
    int16_t &mid_cuts,         // no of free cuts
    ICOORDELT_LIST *row_cells, // list of chop pts
    int16_t start,             // start of good range
    int16_t end                // end of good range
) {
  // blobs
  BLOBNBOX_IT blob_it = row->blob_list();
  BLOBNBOX_IT plot_it;
  int16_t blob_count;    // no of blobs
  TBOX blob_box;         // bounding box
  FPSEGPT_LIST seg_list; // char cells
  FPSEGPT_IT seg_it;     // iterator
  TDimension segpos;     // position of segment
                         // iterator
  ICOORDELT_IT cell_it = row_cells;
  ICOORDELT *cell;  // new cell
  double word_sync; // result for word

  mid_cuts = 0;
  if (blob_it.empty()) {
    occupation = 0;
    return initial_pitch * 10;
  }
#if !GRAPHICS_DISABLED
  if (to_win) {
    projection->plot(to_win, projection_left, row->intercept(), 1.0f, -1.0f, Diagnostics::CORAL);
  }
#endif
  blob_count = 0;
  blob_it.mark_cycle_pt();
  do {
    // first blob
    blob_box = box_next(&blob_it);
    blob_count++;
  } while (!blob_it.cycled_list());
  plot_it = blob_it;
  word_sync = check_pitch_sync2(
      &blob_it, blob_count, static_cast<TDimension>(initial_pitch), 2, projection, projection_left,
      projection_right, row->xheight * textord_projection_scale, occupation, &seg_list, start, end);
  if (textord_debug_pitch) {
    tprintDebug("Row ending at ({},{}), len={}, sync rating={}, ", blob_box.right(), blob_box.top(),
            seg_list.length() - 1, word_sync);
    seg_it.set_to_list(&seg_list);
    for (seg_it.mark_cycle_pt(); !seg_it.cycled_list(); seg_it.forward()) {
      if (seg_it.data()->faked) {
        tprintDebug("(F)");
      }
      tprintDebug("{}, ", seg_it.data()->position());
      tprintDebug("Cost={}, sum={}, squared={}\n",
			seg_it.data()->cost_function(),
			seg_it.data()->sum(),
			seg_it.data()->squares());
    }
    tprintDebug("\n");
  }
#if !GRAPHICS_DISABLED
  if (textord_show_fixed_cuts && blob_count > 0 && to_win) {
    plot_fp_cells2(to_win, Diagnostics::GOLDENROD, row, &seg_list);
  }
#endif
  seg_it.set_to_list(&seg_list);
  for (seg_it.mark_cycle_pt(); !seg_it.cycled_list(); seg_it.forward()) {
    segpos = seg_it.data()->position();
    // make new one
    cell = new ICOORDELT(segpos, 0);
    cell_it.add_after_then_move(cell);
    if (seg_it.at_last()) {
      mid_cuts = seg_it.data()->cheap_cuts();
    }
  }
  seg_list.clear();
  return occupation > 0 ? sqrt(word_sync / occupation) : initial_pitch * 10;
}

/**********************************************************************
 * print_pitch_sd
 *
 * Use a dp algorithm to fit the character cells and return the sd of
 * the cell size over the row.
 **********************************************************************/

void print_pitch_sd(         // find fp cells
    TO_ROW *row,             // row to do
    STATS *projection,       // vertical projection
    TDimension projection_left, // edges //size of blank
    TDimension projection_right, 
	float space_size,
    float initial_pitch // guess at pitch
) {
  const char *res2;   // pitch result
  int16_t occupation; // used cells
  float sp_sd;        // space sd
                      // blobs
  BLOBNBOX_IT blob_it = row->blob_list();
  BLOBNBOX_IT start_it;     // start of word
  BLOBNBOX_IT row_start;    // start of row
  int16_t blob_count;       // no of blobs
  int16_t total_blob_count; // total blobs in line
  TBOX blob_box;            // bounding box
  TBOX prev_box;            // of super blob
  int32_t prev_right;       // of word sync
  int scale_factor;         // on scores for big words
  int32_t sp_count;         // spaces
  FPSEGPT_LIST seg_list;    // char cells
  FPSEGPT_IT seg_it;        // iterator
  double sqsum;             // sum of squares
  double spsum;             // of spaces
  double sp_var;            // space error
  double word_sync;         // result for word
  double total_count;       // total cuts

  if (blob_it.empty()) {
    return;
  }
  row_start = blob_it;
  total_blob_count = 0;

  total_count = 0;
  sqsum = 0;
  sp_count = 0;
  spsum = 0;
  prev_right = -1;
  blob_it = row_start;
  start_it = blob_it;
  blob_count = 0;
  blob_box = box_next(&blob_it); // first blob
  blob_it.mark_cycle_pt();
  do {
    for (; blob_count > 0; blob_count--) {
      box_next(&start_it);
    }
    do {
      prev_box = blob_box;
      blob_count++;
      blob_box = box_next(&blob_it);
    } while (!blob_it.cycled_list() && blob_box.left() - prev_box.right() < space_size);
    word_sync = check_pitch_sync2(
        &start_it, blob_count, static_cast<TDimension>(initial_pitch), 2, projection, projection_left,
        projection_right, row->xheight * textord_projection_scale, occupation, &seg_list, 0, 0);
    total_blob_count += blob_count;
    seg_it.set_to_list(&seg_list);
    if (prev_right >= 0) {
      sp_var = seg_it.data()->position() - prev_right;
      sp_var -= floor(sp_var / initial_pitch + 0.5) * initial_pitch;
      sp_var *= sp_var;
      spsum += sp_var;
      sp_count++;
    }
    seg_it.move_to_last();
    prev_right = seg_it.data()->position();
    if (textord_pitch_scalebigwords) {
      scale_factor = (seg_list.length() - 2) / 2;
      if (scale_factor < 1) {
        scale_factor = 1;
      }
    } else {
      scale_factor = 1;
    }
    sqsum += word_sync * scale_factor;
    total_count += (seg_list.length() - 1) * scale_factor;
    seg_list.clear();
  } while (!blob_it.cycled_list());
  sp_sd = sp_count > 0 ? sqrt(spsum / sp_count) : 0;
  word_sync = total_count > 0 ? sqrt(sqsum / total_count) : space_size * 10;
  tprintDebug("new_sd={}:sd/p={}:new_sp_sd={}:res={}:", word_sync, word_sync / initial_pitch, sp_sd,
          word_sync < textord_words_pitchsd_threshold * initial_pitch ? "F" : "P");

  start_it = row_start;
  blob_it = row_start;
  word_sync =
      check_pitch_sync2(&blob_it, total_blob_count, static_cast<TDimension>(initial_pitch), 2,
                        projection, projection_left, projection_right,
                        row->xheight * textord_projection_scale, occupation, &seg_list, 0, 0);
  if (occupation > 1) {
    word_sync /= occupation;
  }
  word_sync = sqrt(word_sync);

#if !GRAPHICS_DISABLED
  if (textord_show_row_cuts && to_win) {
    plot_fp_cells2(to_win, Diagnostics::CORAL, row, &seg_list);
  }
#endif
  seg_list.clear();
  if (word_sync < textord_words_pitchsd_threshold * initial_pitch) {
    if (word_sync < textord_words_def_fixed * initial_pitch && !row->all_caps) {
      res2 = "DF";
    } else {
      res2 = "MF";
    }
  } else {
    res2 = word_sync < textord_words_def_prop * initial_pitch ? "MP" : "DP";
  }
  tprintDebug(
      "row_sd={}:sd/p={}:res={}:N={}:res2={},init pitch={}, row_pitch={}, "
      "all_caps={}\n",
      word_sync, word_sync / initial_pitch,
      word_sync < textord_words_pitchsd_threshold * initial_pitch ? "F" : "P", occupation, res2,
      initial_pitch, row->fixed_pitch, row->all_caps);
}

/**********************************************************************
 * find_repeated_chars
 *
 * Extract marked leader blobs and put them
 * into words in advance of fixed pitch checking and word generation.
 **********************************************************************/
void find_repeated_chars(TO_BLOCK *block    // Block to search.
) {
  POLY_BLOCK *pb = block->block->pdblk.poly_block();
  if (pb != nullptr && !pb->IsText()) {
    return; // Don't find repeated chars in non-text blocks.
  }

  TO_ROW *row;
  BLOBNBOX_IT box_it;
  BLOBNBOX_IT search_it; // forward search
  WERD *word;            // new word
  TBOX word_box;         // for plotting
  int blobcount, repeated_set;

  TO_ROW_IT row_it = block->get_rows();
  if (row_it.empty()) {
    return; // empty block
  }
  for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
    row = row_it.data();
    box_it.set_to_list(row->blob_list());
    if (box_it.empty()) {
      continue; // no blobs in this row
    }
    if (!row->rep_chars_marked()) {
      mark_repeated_chars(row);
    }
    if (row->num_repeated_sets() == 0) {
      continue; // nothing to do for this row
    }
    // new words
    WERD_IT word_it(&row->rep_words);
    do {
      if (box_it.data()->repeated_set() != 0 && !box_it.data()->joined_to_prev()) {
        blobcount = 1;
        repeated_set = box_it.data()->repeated_set();
        search_it = box_it;
        search_it.forward();
        while (!search_it.at_first() && search_it.data()->repeated_set() == repeated_set) {
          blobcount++;
          search_it.forward();
        }
        // After the call to make_real_word() all the blobs from this
        // repeated set will be removed from the blob list. box_it will be
        // set to point to the blob after the end of the extracted sequence.
        word = make_real_word(&box_it, blobcount, box_it.at_first(), 1);
        if (!box_it.empty() && box_it.data()->joined_to_prev()) {
          tprintDebug("Bad box joined to prev at ");
          box_it.data()->bounding_box().print();
          tprintDebug("After repeated word: ");
          word->bounding_box().print();
        }
        ASSERT_HOST(box_it.empty() || !box_it.data()->joined_to_prev());
        word->set_flag(W_REP_CHAR, true);
        word->set_flag(W_DONT_CHOP, true);
        word_it.add_after_then_move(word);
      } else {
        box_it.forward();
      }
    } while (!box_it.at_first());
  }
}

/**********************************************************************
 * plot_fp_word
 *
 * Plot a block of words as if fixed pitch.
 **********************************************************************/

#if !GRAPHICS_DISABLED
void plot_fp_word(   // draw block of words
    TO_BLOCK *block, // block to draw
    float pitch,     // pitch to draw with
    float nonspace   // for space threshold
) {
  TO_ROW *row; // current row
  TO_ROW_IT row_it = block->get_rows();

  for (row_it.mark_cycle_pt(); !row_it.cycled_list(); row_it.forward()) {
    row = row_it.data();
    row->min_space = static_cast<int32_t>((pitch + nonspace) / 2);
    row->max_nonspace = row->min_space;
    row->space_threshold = row->min_space;
    plot_word_decisions(to_win, static_cast<TDimension>(pitch), row);
  }
}
#endif

} // namespace tesseract
