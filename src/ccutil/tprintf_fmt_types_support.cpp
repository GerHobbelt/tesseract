/**********************************************************************
 * File: tprintf_fmt_support.h
 * Description: Provides custom types' formatters for use with the fmt library.
 * Author: Ger Hobbelt
 *
 * (C) Copyright 2023, Ger Hobbelt / Tesseract
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

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#include <tesseract/fmt-support.h>

#include "blobbox.h"
#include "network.h"
#include "ratngs.h"
#include "static_shape.h"
#include "paragraphs_internal.h"
#include "dawg.h"

#include <algorithm>
#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

namespace fmt {

using namespace tesseract;

// template <>
// struct formatter<PITCH_TYPE> : formatter<string_view> {
//   // parse is inherited from formatter<string_view>.
//
//   template <typename FormatContext>
//   auto format(const PITCH_TYPE &c, FormatContext &ctx) const {
//     ...

auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown_pitch";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<PolyBlockType>::format(PolyBlockType c,
                                           format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PolyBlockType:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown_blocktype";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<Orientation>::format(Orientation c,
                                         format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum Orientation:
  switch (c) {
    case Orientation::ORIENTATION_PAGE_UP:
      name = "page_up";
      break;
    case Orientation::ORIENTATION_PAGE_RIGHT:
      name = "page_right";
      break;
    case Orientation::ORIENTATION_PAGE_DOWN:
      name = "page_down";
      break;
    case Orientation::ORIENTATION_PAGE_LEFT:
      name = "page_left";
      break;
    default:
      name = "unknown_orientation";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<WritingDirection>::format(WritingDirection c,
                                              format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum WritingDirection:
  switch (c) {
    case WritingDirection::WRITING_DIRECTION_LEFT_TO_RIGHT:
      name = "left_to_right";
      break;
    case WritingDirection::WRITING_DIRECTION_RIGHT_TO_LEFT:
      name = "right_to_left";
      break;
    case WritingDirection::WRITING_DIRECTION_TOP_TO_BOTTOM:
      name = "top_to_bottom";
      break;
    default:
      name = "unknown_direction";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<TextlineOrder>::format(TextlineOrder c,
                                           format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum TextlineOrder:
  switch (c) {
    case TextlineOrder::TEXTLINE_ORDER_LEFT_TO_RIGHT:
      name = "order_left_to_right";
      break;
    case TextlineOrder::TEXTLINE_ORDER_RIGHT_TO_LEFT:
      name = "order_right_to_left";
      break;
    case TextlineOrder::TEXTLINE_ORDER_TOP_TO_BOTTOM:
      name = "order_top_to_bottom";
      break;
    default:
      name = "order_unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<PageSegMode>::format(PageSegMode c,
                                         format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PageSegMode:
  switch (c) {
    case PageSegMode::PSM_OSD_ONLY:
      name = "Orientation_and_Script_Detection_only";
      break;
      case PageSegMode::PSM_AUTO_OSD:
      name = "Automatic_page_segmentation_with_OSD";
      break;
      case PageSegMode::PSM_AUTO_ONLY:
      name = "Automatic_page_segmentation_sans_OSD_sans_OCR";
      break;
      case PageSegMode::PSM_AUTO:
      name = "Fully_automatic_page_segmentation_sans_OSD";
      break;
      case PageSegMode::PSM_SINGLE_COLUMN:
      name = "Assume_a_single_column_of_text_of_variable_sizes";
      break;
      case PageSegMode::PSM_SINGLE_BLOCK_VERT_TEXT:
      name = "Assume_a_single_uniform_block_of_vertically_aligned_text";
      break;
      case PageSegMode::PSM_SINGLE_BLOCK:
      name = "Assume_a_single_uniform_block_of_text";
      break;
      case PageSegMode::PSM_SINGLE_LINE:
      name = "Treat_as_a_single_text_line";
      break;
      case PageSegMode::PSM_SINGLE_WORD:
      name = "Treat_as_a_single_word";
      break;
      case PageSegMode::PSM_CIRCLE_WORD:
      name = "Treat_as_a_single_word_in_a_circle";
      break;
      case PageSegMode::PSM_SINGLE_CHAR:
      name = "Treat_as_a_single_character";
      break;
      case PageSegMode::PSM_SPARSE_TEXT:
      name = "Find_as_much_text_as_possible_in_no_particular_order";
      break;
      case PageSegMode::PSM_SPARSE_TEXT_OSD:
      name = "Sparse_text_with_Orientation_and_Script_Detection";
      break;
      case PageSegMode::PSM_RAW_LINE:
      name = "Treat_as_a_single_text_line_bypassing_all_tesseract_hacks";
      break;
      default:
      name = "unknown_page_seg_mode";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<TabType>::format(TabType c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum TabType:
  switch (c) {
    case TabType::TT_NONE:
      name = "not_a_tab";
      break;
    case TabType::TT_DELETED:
      name = "deleted_not_a_tab_after_analysis";
      break;
    case TabType::TT_MAYBE_RAGGED:
      name = "maybe_ragged";
      break;
    case TabType::TT_MAYBE_ALIGNED:
      name = "maybe_aligned";
      break;
    case TabType::TT_CONFIRMED:
      name = "aligned_with_neighbours";
      break;
    case TabType::TT_VLINE:
      name = "vertical_line";
      break;
    default:
      name = "unknown_tab";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<BlobRegionType>::format(BlobRegionType c,
                                            format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum BlobRegionType:
  switch (c) {
    case BlobRegionType::BRT_NOISE:
      name = "neither_text_nor_image";
      break;
    case BlobRegionType::BRT_HLINE:
      name = "horizontal_separator_line";
      break;
    case BlobRegionType::BRT_VLINE:
      name = "vertical_separator_line";
      break;
    case BlobRegionType::BRT_RECTIMAGE:
      name = "rectangular_image";
      break;
    case BlobRegionType::BRT_POLYIMAGE:
      name = "nonrectangular_image";
      break;
    case BlobRegionType::BRT_UNKNOWN:
      name = "not_determined_yet";
      break;
    case BlobRegionType::BRT_VERT_TEXT:
      name = "vertical_aligned_text";
      break;
    case BlobRegionType::BRT_TEXT:
      name = "convincing_text";
      break;
    default:
      name = "unknown_blob_region";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<BlobNeighbourDir>::format(BlobNeighbourDir c,
                                              format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum BlobNeighbourDir:
  switch (c) {
    case BlobNeighbourDir::BND_LEFT:
      name = "left";
      break;
    case BlobNeighbourDir::BND_BELOW:
      name = "below";
      break;
    case BlobNeighbourDir::BND_RIGHT:
      name = "right";
      break;
    case BlobNeighbourDir::BND_ABOVE:
      name = "above";
      break;
    default:
      name = "unknown_neighbour_dir";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<BlobSpecialTextType>::format(BlobSpecialTextType c,
                                                 format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum BlobSpecialTextType:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<BlobTextFlowType>::format(BlobTextFlowType c,
                                              format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum BlobTextFlowType:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown_textflow";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<NetworkType>::format(NetworkType c,
                                         format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum NetworkType:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown_networktype";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<LineType>::format(LineType c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
      case LineType::LT_START:
        name = "start";
        break;
      case LineType::LT_BODY:
        name = "body";
        break;
      case LineType::LT_UNKNOWN:
        name = "no_clue/unknown";
        break;
      case LineType::LT_MULTIPLE:
        name = "multiple";
        break;
    default:
      name = "unknown_linetype";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<BlobChoiceClassifier>::format(BlobChoiceClassifier c,
                                                  format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
      case BlobChoiceClassifier::BCC_STATIC_CLASSIFIER:
        name = "static";
        break;
      case BlobChoiceClassifier::BCC_ADAPTED_CLASSIFIER:
        name = "adapted";
        break;
      case BlobChoiceClassifier::BCC_SPECKLE_CLASSIFIER:
        name = "speckle";
        break;
      case BlobChoiceClassifier::BCC_AMBIG:
        name = "ambiguous";
        break;
      case BlobChoiceClassifier::BCC_FAKE:
        name = "fake";
        break;
    default:
      name = "unknown_blobchoice";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<PermuterType>::format(PermuterType c,
                                          format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
      case PermuterType::NO_PERM:
        name = "none";
        break;
      case PermuterType::PUNC_PERM:
        name = "punctuation";
        break;
      case PermuterType::TOP_CHOICE_PERM:
        name = "top_choice";
        break;
      case PermuterType::LOWER_CASE_PERM:
        name = "lower_case";
        break;
      case PermuterType::UPPER_CASE_PERM:
        name = "upper_case";
        break;
      case PermuterType::NGRAM_PERM:
        name = "ngram";
        break;
      case PermuterType::NUMBER_PERM:
        name = "number";
        break;
      case PermuterType::USER_PATTERN_PERM:
        name = "user_pattern";
        break;
      case PermuterType::SYSTEM_DAWG_PERM:
        name = "system_dawg";
        break;
      case PermuterType::DOC_DAWG_PERM:
        name = "doc_dawg";
        break;
      case PermuterType::USER_DAWG_PERM:
        name = "user_dawg";
        break;
      case PermuterType::FREQ_DAWG_PERM:
        name = "freq_dawg";
        break;
      case PermuterType::COMPOUND_PERM:
        name = "compound";
        break;
    default:
      name = "unknown_permuter";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<DawgType>::format(DawgType c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
      case DawgType::DAWG_TYPE_PUNCTUATION:
        name = "punctuation";
        break;
      case DawgType::DAWG_TYPE_WORD:
        name = "word";
        break;
      case DawgType::DAWG_TYPE_NUMBER:
        name = "number";
        break;
      case DawgType::DAWG_TYPE_PATTERN:
        name = "pattern";
        break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<LossType>::format(LossType c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
      case LossType::LT_NONE:
        name = "none/undefined";
        break;
      case LossType::LT_CTC:
        name = "CTC";
        break;
      case LossType::LT_SOFTMAX:
        name = "softMax";
        break;
      case LossType::LT_LOGISTIC:
        name = "logistic";
        break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

#if 0

auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<PITCH_TYPE>::format(PITCH_TYPE c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case PITCH_TYPE::PITCH_DUNNO:
      name = "insufficient_data";
      break;
    case PITCH_TYPE::PITCH_DEF_FIXED:
      name = "definitely_fixed";
      break;
    case PITCH_TYPE::PITCH_MAYBE_FIXED:
      name = "maybe_fixed";
      break;
    case PITCH_TYPE::PITCH_DEF_PROP:
      name = "definitely_proportional";
      break;
    case PITCH_TYPE::PITCH_MAYBE_PROP:
      name = "maybe_proportional";
      break;
    case PITCH_TYPE::PITCH_CORR_FIXED:
      name = "corrected_fixed";
      break;
    case PITCH_TYPE::PITCH_CORR_PROP:
      name = "corrected_proportional";
      break;
    default:
      name = "unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

#endif

} // namespace fmt
