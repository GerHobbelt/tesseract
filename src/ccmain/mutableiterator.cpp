///////////////////////////////////////////////////////////////////////
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
//
///////////////////////////////////////////////////////////////////////

#include <tesseract/preparation.h> // compiler config, etc.

#include "mutableiterator.h"

namespace tesseract {

// Destructor.
// It is defined here, so the compiler can create a single vtable
// instead of weak vtables in every compilation unit.
MutableIterator::~MutableIterator() = default;

MutableIterator &MutableIterator::operator = (const MutableIterator &source) = default;
MutableIterator::MutableIterator(const MutableIterator &source) = default;
MutableIterator::MutableIterator(MutableIterator &&source) noexcept = default;

} // namespace tesseract.
