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

#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/fmt-support.h>

#include "blobbox.h"
#include "network.h"
#include "ratngs.h"
#include "static_shape.h"
#include "paragraphs_internal.h"
#include "dawg.h"
#include "thresholder.h"
#include "unicharset.h"

#include <algorithm>
#include <string>

#include <fmt/base.h>
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
    case PolyBlockType::PT_UNKNOWN:
      name = "PT_UNKNOWN";
      break;
    case PolyBlockType::PT_FLOWING_TEXT:
      name = "PT_FLOWING_TEXT";
      break;
    case PolyBlockType::PT_HEADING_TEXT:
      name = "PT_HEADING_TEXT";
      break;
    case PolyBlockType::PT_PULLOUT_TEXT:
      name = "PT_PULLOUT_TEXT";
      break;
    case PolyBlockType::PT_EQUATION:
      name = "PT_EQUATION";
      break;
    case PolyBlockType::PT_INLINE_EQUATION:
      name = "PT_INLINE_EQUATION";
      break;
    case PolyBlockType::PT_TABLE:
      name = "PT_TABLE";
      break;
    case PolyBlockType::PT_VERTICAL_TEXT:
      name = "PT_VERTICAL_TEXT";
      break;
    case PolyBlockType::PT_CAPTION_TEXT:
      name = "PT_CAPTION_TEXT";
      break;
    case PolyBlockType::PT_FLOWING_IMAGE:
      name = "PT_FLOWING_IMAGE";
      break;
    case PolyBlockType::PT_HEADING_IMAGE:
      name = "PT_HEADING_IMAGE";
      break;
    case PolyBlockType::PT_PULLOUT_IMAGE:
      name = "PT_PULLOUT_IMAGE";
      break;
    case PolyBlockType::PT_HORZ_LINE:
      name = "PT_HORZ_LINE";
      break;
    case PolyBlockType::PT_VERT_LINE:
      name = "PT_VERT_LINE";
      break;
    case PolyBlockType::PT_NOISE:
      name = "PT_NOISE";
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
    case BlobSpecialTextType::BSTT_NONE:
      name = "BSTT_NONE";
      break;
    case BlobSpecialTextType::BSTT_ITALIC:
      name = "BSTT_ITALIC";
      break;
    case BlobSpecialTextType::BSTT_DIGIT:
      name = "BSTT_DIGIT";
      break;
    case BlobSpecialTextType::BSTT_MATH:
      name = "BSTT_MATH";
      break;
    case BlobSpecialTextType::BSTT_UNCLEAR:
      name = "BSTT_UNCLEAR";
      break;
    case BlobSpecialTextType::BSTT_SKIP:
      name = "BSTT_SKIP";
      break;
    default:
      name = "unknown_special_text_type";
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
    case BlobTextFlowType::BTFT_NONE:
      name = "BTFT_NONE";
      break;
    case BlobTextFlowType::BTFT_NONTEXT:
      name = "BTFT_NONTEXT";
      break;
    case BlobTextFlowType::BTFT_NEIGHBOURS:
      name = "BTFT_NEIGHBOURS";
      break;
    case BlobTextFlowType::BTFT_CHAIN:
      name = "BTFT_CHAIN";
      break;
    case BlobTextFlowType::BTFT_STRONG_CHAIN:
      name = "BTFT_STRONG_CHAIN";
      break;
    case BlobTextFlowType::BTFT_TEXT_ON_IMAGE:
      name = "BTFT_TEXT_ON_IMAGE";
      break;
    case BlobTextFlowType::BTFT_LEADER:
      name = "BTFT_LEADER";
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
    case NetworkType::NT_NONE:
      name = "NT_NONE";
      break;
    case NetworkType::NT_INPUT:
      name = "NT_INPUT";
      break;
    case NetworkType::NT_CONVOLVE:
      name = "NT_CONVOLVE";
      break;
    case NetworkType::NT_MAXPOOL:
      name = "NT_MAXPOOL";
      break;
    case NetworkType::NT_PARALLEL:
      name = "NT_PARALLEL";
      break;
    case NetworkType::NT_REPLICATED:
      name = "NT_REPLICATED";
      break;
    case NetworkType::NT_PAR_RL_LSTM:
      name = "NT_PAR_RL_LSTM";
      break;
    case NetworkType::NT_PAR_UD_LSTM:
      name = "NT_PAR_UD_LSTM";
      break;
    case NetworkType::NT_PAR_2D_LSTM:
      name = "NT_PAR_2D_LSTM";
      break;
    case NetworkType::NT_SERIES:
      name = "NT_SERIES";
      break;
    case NetworkType::NT_RECONFIG:
      name = "NT_RECONFIG";
      break;
    case NetworkType::NT_XREVERSED:
      name = "NT_XREVERSED";
      break;
    case NetworkType::NT_YREVERSED:
      name = "NT_YREVERSED";
      break;
    case NetworkType::NT_XYTRANSPOSE:
      name = "NT_XYTRANSPOSE";
      break;
    case NetworkType::NT_LSTM:
      name = "LSTM";
      break;
    case NetworkType::NT_LSTM_SUMMARY:
      name = "LSTM_which_only_keeps_last_output";
      break;
    case NetworkType::NT_LOGISTIC:
      name = "logistic_nonlinearity";
      break;
    case NetworkType::NT_POSCLIP:
      name = "rect_linear_version_of_logistic";
      break;
    case NetworkType::NT_SYMCLIP:
      name = "rect_linear_version_of_tanh";
      break;
    case NetworkType::NT_TANH:
      name = "with_tanh_nonlinearity";
      break;
    case NetworkType::NT_RELU:
      name = "with_rectifier_nonlinearity";
      break;
    case NetworkType::NT_LINEAR:
      name = "fully_connected_with_no_nonlinearity";
      break;
    case NetworkType::NT_SOFTMAX:
      name = "SoftMax_with_CTC";
      break;
    case NetworkType::NT_SOFTMAX_NO_CTC:
      name = "SoftMax_no_CTC";
      break;
    case NetworkType::NT_LSTM_SOFTMAX:
      name = "1D_LSTM_with_softmax";
      break;
    case NetworkType::NT_LSTM_SOFTMAX_ENCODED:
      name = "1D_LSTM_with_binary_encoded_softmax";
      break;
    case NetworkType::NT_TENSORFLOW:
      name = "NT_TENSORFLOW";
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
        name = "Punctuation";
        break;
      case DawgType::DAWG_TYPE_WORD:
        name = "Word";
        break;
      case DawgType::DAWG_TYPE_NUMBER:
        name = "Number";
        break;
      case DawgType::DAWG_TYPE_PATTERN:
        name = "Pattern";
        break;
    default:
      name = "Unknown";
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
        name = "None/Undefined";
        break;
      case LossType::LT_CTC:
        name = "CTC";
        break;
      case LossType::LT_SOFTMAX:
        name = "SoftMax";
        break;
      case LossType::LT_LOGISTIC:
        name = "Logistic";
        break;
    default:
      name = "Unknown";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}


auto fmt::formatter<ThresholdMethod>::format(ThresholdMethod c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum PITCH_TYPE:
  switch (c) {
    case ThresholdMethod::Otsu:
      name = "Otsu";
      break;
    case ThresholdMethod::LeptonicaOtsu:
      name = "Leptonica_Otsu";
      break;
    case ThresholdMethod::Sauvola:
      name = "Sauvola";
      break;
    case ThresholdMethod::OtsuOnNormalizedBackground:
      name = "Otsu_On_Normalized_Background";
      break;
    case ThresholdMethod::MaskingAndOtsuOnNormalizedBackground:
      name = "Masking_And_Otsu_On_Normalized_Background";
      break;
    case ThresholdMethod::Nlbin:
      name = "Nlbin";
      break;
    case ThresholdMethod::Max:
      name = "MaxThreshold";
      break;
    default:
      name = "unknown_threshold_method";
      break;
  }
  auto id = fmt::format("{}({})", name, static_cast<int>(c));

  return formatter<string_view>::format(id, ctx);
}

auto fmt::formatter<UNICHARSET::Direction>::format(UNICHARSET::Direction c, format_context &ctx) const
-> decltype(ctx.out()) {
	const char *name;
	// enum UNICHARSET::Direction:
	switch (c) {
	case UNICHARSET::Direction::U_LEFT_TO_RIGHT:
		name = "U_LEFT_TO_RIGHT";
		break;
	case UNICHARSET::Direction::U_RIGHT_TO_LEFT:
		name = "U_RIGHT_TO_LEFT";
		break;
	case UNICHARSET::Direction::U_EUROPEAN_NUMBER:
		name = "U_EUROPEAN_NUMBER";
		break;
	case UNICHARSET::Direction::U_EUROPEAN_NUMBER_SEPARATOR:
		name = "U_EUROPEAN_NUMBER_SEPARATOR";
		break;
	case UNICHARSET::Direction::U_EUROPEAN_NUMBER_TERMINATOR:
		name = "U_EUROPEAN_NUMBER_TERMINATOR";
		break;
	case UNICHARSET::Direction::U_ARABIC_NUMBER:
		name = "U_ARABIC_NUMBER";
		break;
	case UNICHARSET::Direction::U_COMMON_NUMBER_SEPARATOR:
		name = "U_COMMON_NUMBER_SEPARATOR";
		break;
	case UNICHARSET::Direction::U_BLOCK_SEPARATOR:
		name = "U_BLOCK_SEPARATOR";
		break;
	case UNICHARSET::Direction::U_SEGMENT_SEPARATOR:
		name = "U_SEGMENT_SEPARATOR";
		break;
	case UNICHARSET::Direction::U_WHITE_SPACE_NEUTRAL:
		name = "U_WHITE_SPACE_NEUTRAL";
		break;
	case UNICHARSET::Direction::U_OTHER_NEUTRAL:
		name = "U_OTHER_NEUTRAL";
		break;
	case UNICHARSET::Direction::U_LEFT_TO_RIGHT_EMBEDDING:
		name = "U_LEFT_TO_RIGHT_EMBEDDING";
		break;
	case UNICHARSET::Direction::U_LEFT_TO_RIGHT_OVERRIDE:
		name = "U_LEFT_TO_RIGHT_OVERRIDE";
		break;
	case UNICHARSET::Direction::U_RIGHT_TO_LEFT_ARABIC:
		name = "U_RIGHT_TO_LEFT_ARABIC";
		break;
	case UNICHARSET::Direction::U_RIGHT_TO_LEFT_EMBEDDING:
		name = "U_RIGHT_TO_LEFT_EMBEDDING";
		break;
	case UNICHARSET::Direction::U_RIGHT_TO_LEFT_OVERRIDE:
		name = "U_RIGHT_TO_LEFT_OVERRIDE";
		break;
	case UNICHARSET::Direction::U_POP_DIRECTIONAL_FORMAT:
		name = "U_POP_DIRECTIONAL_FORMAT";
		break;
	case UNICHARSET::Direction::U_DIR_NON_SPACING_MARK:
		name = "U_DIR_NON_SPACING_MARK";
		break;
	case UNICHARSET::Direction::U_BOUNDARY_NEUTRAL:
		name = "U_BOUNDARY_NEUTRAL";
		break;
	case UNICHARSET::Direction::U_FIRST_STRONG_ISOLATE:
		name = "U_FIRST_STRONG_ISOLATE";
		break;
	case UNICHARSET::Direction::U_LEFT_TO_RIGHT_ISOLATE:
		name = "U_LEFT_TO_RIGHT_ISOLATE";
		break;
	case UNICHARSET::Direction::U_RIGHT_TO_LEFT_ISOLATE:
		name = "U_RIGHT_TO_LEFT_ISOLATE";
		break;
	case UNICHARSET::Direction::U_POP_DIRECTIONAL_ISOLATE:
		name = "U_POP_DIRECTIONAL_ISOLATE";
		break;
#ifndef U_HIDE_DEPRECATED_API
	case UNICHARSET::Direction::U_CHAR_DIRECTION_COUNT:
		name = "U_CHAR_DIRECTION_COUNT";
		break;
#endif // U_HIDE_DEPRECATED_API
	default:
		name = "unknown_threshold_method";
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

#endif

} // namespace fmt
