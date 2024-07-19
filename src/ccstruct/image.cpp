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
    : pix_(pix) {
  // takes ownership of pix
}

Image::Image(bool take_ownership, Pix *pix) {
  if (!take_ownership) {
    if (pix) {
      // DOES NOT take ownership of pix, we own a CLONE. (refcounted by leptonica)
      pix_ = pixClone(pix);
    }
  } else {
    // take ownership, just like the regular constructor
    pix_ = pix;
  }
}

Image Image::clone() const {
  if (pix_)
    return pixClone(pix_);
  return Image();
}

Image Image::copy() const {
  if (pix_)
    return pixCopy(nullptr, pix_);
  return Image();
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

Image::Image(const Image &src) {
  // copy constructor: clone the source
  if (src.pix_)
    pix_ = pixClone(src);
}
Image::Image(Image &&src) noexcept {
  // move constructor: take ownership of the source
  pix_ = src.pix_;
  src.pix_ = nullptr;
}

Image::~Image() {
  if (pix_)
    pixDestroy(&pix_);
}

Image::operator Pix *() const noexcept {
  return pix_;
}

Image::operator Pix **() noexcept {
  return &pix_;
}

Pix **Image::obtains() noexcept {
  return &pix_;
}

Pix *Image::relinquish() noexcept {
  Pix *rv = pix_;
  pix_ = nullptr;
  return rv;
}

Pix *Image::operator->() const noexcept {
  return pix_;
}

Image &Image::operator=(const Image &src) {
  // copy assignment: share ownership
  if (this != &src) {
    if (pix_)
      pixDestroy(&pix_);
    if (src.pix_)
      pix_ = pixClone(src);
  }
  return *this;
}

Image &Image::operator=(Image &&src) noexcept {
  // move assignment: take ownership by SWAPPING: that way we guarantee our `noexcept` clause.
  auto p = pix_;
  pix_ = src.pix_;
  src.pix_ = p;
  return *this;
}

Image &Image::operator=(decltype(nullptr)) {
  if (pix_)
    pixDestroy(&pix_);
  return *this;
}

Image &Image::operator=(Pix *pix) {
  // check? -> care about the fluke scenario where C-style code receives a PIX pointer from us before, then feeds it back to us sans change, i.e. no refcount bump inside the C code:
  if (pix != pix_) {
    // takes ownership of the PIX
    if (pix_)
      pixDestroy(&pix_);
    pix_ = pix;
  }
  return *this;
}

Image &Image::operator=(Pix **pix) {
  // move semantics, C style
  // check? -> care about the fluke scenario where C-style code receives a PIX pointer-pointer from us before, then feeds it back to us sans change, i.e. no refcount bump inside the C code:
  if (!pix || *pix != pix_) {
    // takes ownership of the PIX
    if (pix_)
      pixDestroy(&pix_);
    if (pix) {
      pix_ = *pix;
      *pix = nullptr; // DO NOT swap like we do for the C++ move operations above: this is expected to cope with C-style code and assumptions abound!
    }
  }
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
