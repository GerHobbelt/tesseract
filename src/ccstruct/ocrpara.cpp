/////////////////////////////////////////////////////////////////////
// File:        ocrpara.cpp
// Description: OCR Paragraph Output Type
// Author:      David Eger
//
// (C) Copyright 2010, Google Inc.
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

#include <tesseract/preparation.h> // compiler config, etc.

#include "ocrpara.h"

#include "host.h" // For NearlyEqual()

#include <cstdio>

namespace tesseract {

using tesseract::JUSTIFICATION_CENTER;
using tesseract::JUSTIFICATION_LEFT;
using tesseract::JUSTIFICATION_RIGHT;
using tesseract::JUSTIFICATION_UNKNOWN;

static const char *ParagraphJustificationToString(tesseract::ParagraphJustification justification) {
  switch (justification) {
    case JUSTIFICATION_LEFT:
      return "LEFT";
    case JUSTIFICATION_RIGHT:
      return "RIGHT";
    case JUSTIFICATION_CENTER:
      return "CENTER";
    default:
      return "UNKNOWN";
  }
}

bool ParagraphModel::ValidFirstLine(int lmargin, int lindent, int rindent, int rmargin) const {
  switch (justification_) {
    case JUSTIFICATION_LEFT:
      return NearlyEqual(lmargin + lindent, margin_ + first_indent_, tolerance_);
    case JUSTIFICATION_RIGHT:
      return NearlyEqual(rmargin + rindent, margin_ + first_indent_, tolerance_);
    case JUSTIFICATION_CENTER:
      return NearlyEqual(lindent, rindent, tolerance_ * 2);
    default:
      // shouldn't happen
      return false;
  }
}

bool ParagraphModel::ValidBodyLine(int lmargin, int lindent, int rindent, int rmargin) const {
  switch (justification_) {
    case JUSTIFICATION_LEFT:
      return NearlyEqual(lmargin + lindent, margin_ + body_indent_, tolerance_);
    case JUSTIFICATION_RIGHT:
      return NearlyEqual(rmargin + rindent, margin_ + body_indent_, tolerance_);
    case JUSTIFICATION_CENTER:
      return NearlyEqual(lindent, rindent, tolerance_ * 2);
    default:
      // shouldn't happen
      return false;
  }
}

bool ParagraphModel::Comparable(const ParagraphModel &other) const {
  if (justification_ != other.justification_) {
    return false;
  }
  if (justification_ == JUSTIFICATION_CENTER || justification_ == JUSTIFICATION_UNKNOWN) {
    return true;
  }
  int tolerance = (tolerance_ + other.tolerance_) / 4;
  return NearlyEqual(margin_ + first_indent_, other.margin_ + other.first_indent_, tolerance) &&
         NearlyEqual(margin_ + body_indent_, other.margin_ + other.body_indent_, tolerance);
}

std::string ParagraphModel::ToString() const {
  const char *alignment = ParagraphJustificationToString(justification_);
  return fmt::format("margin: {}, first_indent: {}, body_indent: {}, alignment: {}",
           margin_, first_indent_, body_indent_, alignment);
}

} // namespace tesseract
