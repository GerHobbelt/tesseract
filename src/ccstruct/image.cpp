///////////////////////////////////////////////////////////////////////
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
///////////////////////////////////////////////////////////////////////

// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.

#include "image.h"

#include <leptonica/allheaders.h>

namespace tesseract {

Image::Image(Pix *pix)
    : pix_(pix) {}

Image::Image(Pix *&pix)
    : pix_(pix) {
  pix = nullptr;
}

Image Image::clone() const {
  return pix_ ? pixClone(pix_) : nullptr;
}

Image Image::copy() const {
  return pixCopy(nullptr, pix_);
}

void Image::destroy() {
  pixDestroy(&pix_);
}

bool Image::isZero() const {
  l_int32 r = 1;
  if (pix_ != nullptr) {
    pixZero(pix_, &r);
  }
  return r == 1;
}

void Image::replace(Pix *pix) {
  if (pix_ != nullptr) {
    pixDestroy(&pix_);
  }
  pix_ = pix;
}

void Image::replace(Pix *&pix) {
  if (pix_ != nullptr) {
    pixDestroy(&pix_);
  }
  pix_ = pix;
  pix = nullptr;
}

Image &Image::operator=(Pix *pix) {
  if (pix_ != nullptr) {
   pixDestroy(&pix_);
  }
  pix_ = pix;
  return *this;
}

Image &Image::operator=(Pix *&pix) {
  if (pix_ != nullptr) {
     pixDestroy(&pix_);
  }
  pix_ = pix;
  pix = nullptr;
  return *this;
}

Image::Image(const Image &src) {
  // copy constructor: clone the source
  if (this != &src) {
    if (pix_)
      pixDestroy(&pix_);
    if (src.pix_)
      pix_ = pixClone(src);
    else
      pix_ = nullptr;
  }
}
Image::Image(Image &&src) noexcept {
  if (this != &src) {
    if (pix_)
      pixDestroy(&pix_);
    if (src.pix_) {
      pix_ = src.pix_;
      src.pix_ = nullptr;
    } else {
      pix_ = nullptr;
    }
  }
}

Image::~Image() {
  if (pix_)
    pixDestroy(&pix_);
}

Image &Image::operator=(const Image &src) {
  if (this != &src) {
    if (pix_)
      pixDestroy(&pix_);
    if (src.pix_)
      pix_ = pixClone(src);
    else
      pix_ = nullptr;
  }
  return *this;
}

Image &Image::operator=(Image &&src) noexcept {
  if (this != &src) {
    if (pix_)
      pixDestroy(&pix_);
    if (src.pix_) {
      pix_ = src.pix_;
      src.pix_ = nullptr;
    } else {
      pix_ = nullptr;
    }
  }
  return *this;
}

Image &Image::operator=(decltype(nullptr)) {
  if (pix_)
    pixDestroy(&pix_);
  return *this;
}

Image Image::operator|(Image i) const {
  return pixOr(nullptr, pix_, i);
}

Image &Image::operator|=(Image i) {
  pixOr(pix_, pix_, i);
  return *this;
}

Image Image::operator&(Image i) const {
  return pixAnd(nullptr, pix_, i);
}

Image &Image::operator&=(Image i) {
  pixAnd(pix_, pix_, i);
  return *this;
}

} // namespace tesseract
