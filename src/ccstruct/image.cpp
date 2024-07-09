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

Image& Image::operator =(Pix* pix) {
  if (pix_ != nullptr) {
  	// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
	//
	// DO NOT invoke pixDestroy on the existing pix_ member pointer! While this would be
	// the obvious thing to do when `Image` were an *owning* class for the leptonica Pix 
	// pointer, IT IS NOT.    Doing this cost me dearly, as it only showed up later
	// when running tesseract bulk tests and then as a *random* Segmentation fault,
	// which happened due to double-free (this pixDestroy() would not know about the
	// outside pixDestroy() later on), combined with Windows radom address relocation
	// (which serves to help protect against fixed-address/offset assuming hacking tools / trojans / viruses)
	// this resulted in a rather random occurring Segfault (random occurrence while running
	// the *exact* same test over and over again...)   That was > 7 days lost.
	// -------------------------------------------------------------------------------------
	//
	// this inadvertently triggering a double-free: while `Image` may LOOK like it's a class 
	// that *owns* the Pix pointer, it DOES NOT: it is merely a C++ wrapper around a leptonica 
	// pointer and currently little more than that.
    //pixDestroy(&pix_);
  }
  pix_ = pix;
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

}
