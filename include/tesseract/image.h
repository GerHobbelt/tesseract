///////////////////////////////////////////////////////////////////////
// File:        image.h
// Description: Image wrapper.
//
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

#ifndef TESSERACT_CCSTRUCT_IMAGE_H_
#define TESSERACT_CCSTRUCT_IMAGE_H_

#include <tesseract/export.h>

struct Pix;

namespace tesseract {

class TESS_API Image {
public:
  Pix *pix_ = nullptr;

public:
  Image(Pix *pix);  // mark this one as 'explicit' to see all the places where the code transitions between old/leptonica/C and new/C++/Image class styles.
  Image(bool take_ownership, Pix *pix);

  // https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c21-if-you-define-or-delete-any-copy-move-or-destructor-function-define-or-delete-them-all
  // https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#note-106

  ~Image();
  Image();
  Image(const Image &src);
  Image &operator=(const Image &src);
  Image(Image &&src) noexcept;
  Image &operator=(Image &&src) noexcept;

  // service
  bool operator==(decltype(nullptr)) const { return pix_ == nullptr; }
  bool operator!=(decltype(nullptr)) const { return pix_ != nullptr; }
  explicit operator bool() const { return pix_ != nullptr; }
  operator Pix *() noexcept;
  operator const Pix *() const noexcept;
  explicit operator Pix **() noexcept;
  const Pix *operator->() const noexcept;
  Pix *operator->() noexcept;
  Image &operator=(Pix *pix);
  Image &operator=(Pix **pix);      // move semantics, C style
  Image &operator=(decltype(nullptr));

  // api
  Pix *clone2pix() const; // increases refcount
  Image cccclone() const; // increases refcount
  Image copy() const;     // does full copy
  void destroy();
  bool isZero() const;
  void replace(Pix* pix);
  void replace(Pix *&pix);
  Pix *ptr() noexcept;
  const Pix *ptr() const noexcept;

  // equivalent of `operator Pix**` i.e. hard cast to `Pix **` type.
  Pix **obtains() noexcept;
  // relinquish control of the PIX: pass it on to some-one else. Move semantics simile for C code style.
  Pix *relinquish() noexcept;

  // ops
  Image operator|(Image) const;
  Image &operator|=(Image);
  Image operator&(Image) const;
  Image &operator&=(Image);
};

} // namespace tesseract

#endif // TESSERACT_CCSTRUCT_IMAGE_H_
