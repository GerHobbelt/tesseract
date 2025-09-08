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

#include <tesseract/image.h>

#include <leptonica/allheaders.h>

#include <algorithm>
#include <utility>

namespace tesseract {

#pragma optimize("", off)

Image::Image()
    : pix_(nullptr) {
}

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

#if 0
// this is essentially a NIL op, or rather: identical to the basic assignment, as
// it goes something like this: take original pix_, clone it in here, thus we have
// a fresh PIX pointer, which is wrapped in a TEMPORARY Image, which takes ownership
// (without cloning it ;-) ) and then returns that temporary instance to the outside,
// so on the path towards reaching the target all this does is injecting one more
// clone + destroy, thanks to the temporary Image instance.
//
// The one that was INTENDED is now named clone2pix(): see above: no more temporary
// (intermediate) Image instance and thus no ownership being gobbled up by temporaries
// while the cloned PIX is on its way to the receiver!
Image Image::clone() const {
  if (pix_)
    return pixClone(pix_);
  return Image();
}
#endif

Pix *Image::clone2pix() const {
  if (pix_)
    return pixClone(pix_);
  return nullptr;
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
    pix_ = pixClone(const_cast<PIX *>(src.ptr()));
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

Pix *Image::ptr() noexcept {
  return pix_;
}

const Pix *Image::ptr() const noexcept {
  return pix_;
}

Image::operator const Pix *() const noexcept {
  return pix_;
}

Image::operator Pix *() noexcept {
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

Pix *Image::operator->() noexcept {
  return pix_;
}

const Pix *Image::operator->() const noexcept {
  return pix_;
}

Image &Image::operator=(const Image &src) {
  // copy assignment: share ownership
  if (pix_ != src.pix_) {
    if (pix_)
      pixDestroy(&pix_);
    if (src.pix_)
      pix_ = pixClone(const_cast<PIX *>(src.ptr()));
  }
  return *this;
}

Image &Image::operator=(Image &&src) noexcept {
  // move assignment: take ownership by SWAPPING: that way we guarantee our `noexcept` clause.
  std::swap(pix_, src.pix_);
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
