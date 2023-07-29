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

auto fmt::formatter<TabType>::format(TabType c, format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum TabType:
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

auto fmt::formatter<BlobRegionType>::format(BlobRegionType c,
                                            format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum BlobRegionType:
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

auto fmt::formatter<BlobNeighbourDir>::format(BlobNeighbourDir c,
                                              format_context &ctx) const
    -> decltype(ctx.out()) {
  const char *name;
  // enum BlobNeighbourDir:
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
      name = "unknown";
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

#endif

} // namespace fmt
