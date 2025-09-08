///////////////////////////////////////////////////////////////////////
// File:        filepath.cpp
// Description: Support for easily tracking a file path in several styles: original (as specified by user/application, the canonical path and a beautified display variant)
// Author:      Ger Hobbelt
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

#include <tesseract/filepath.h>

#include <string>
#include <filesystem>


namespace tesseract {

static inline char* new_strdup(const char* s) {
  auto slen = strlen(s);
  char *rv = new char[slen + 1];
  strcpy(rv, s);
  return rv;
}

FilePath::FilePath() {}
FilePath::FilePath(const char *path) {
  if (!path)
    return;
  orig_path = new_strdup(path);
}
FilePath::FilePath(const std::string &path) : FilePath(path.c_str()) {
}
FilePath::FilePath(const std::filesystem::path &filepath) : FilePath(filepath.string().c_str()) {
}

FilePath::~FilePath() {
  if (canonicalized)
    delete canonicalized;
  if (beautified_path)
    delete beautified_path;
  if (orig_path)
    delete orig_path;
}

const char *FilePath::original() const {
  if (orig_path)
    return orig_path;
  return nullptr;
}
const char *FilePath::unixified() {
  const char *s = orig_path;

  if (has_unixified_the_slot) {
    return s;
  } else {
    if (!s) {
      throw new std::exception("Cannot request unixified path when that path has not been set up yet.");
    }
    const char *delim = strchr(s, '\\');
    if (!delim) {
      has_unixified_the_slot = true;
      unixified_is_different = false;
      return s;
    }

    // 'nuke' the original path for this as we don't want to cache this one separately and that change is 'harmless' IMHO.
    char *p = orig_path;
    for(;;) {
      p = strchr(p, '\\');
      if (!p)
        break;
      *p++ = '/';
    }

    has_unixified_the_slot = true;
    unixified_is_different = true;
    return orig_path;
  }
}

const char *FilePath::normalized() {
  if (!has_canonicalized_slot) {
    // did user specify a path type, rather than a string for the path? If so, make sure we copy the original before we canonicalize it!
    if (!orig_path) {
      throw new std::exception("Cannot request normalized/canonicalized path when that path has not been set up yet.");
    }

    std::filesystem::path cn = orig_path;
    cn = std::filesystem::weakly_canonical(cn);
    canonicalized = new_strdup(cn.string().c_str());
    has_canonicalized_slot = true;
  }
  assert(canonicalized != nullptr);

  return canonicalized;
}

const char *FilePath::display(int max_dir_count, bool reduce_middle_instead_of_start_part) {
  // can we use the cached version or should we ditch it?
  if (beautified_path != nullptr && max_dir_count == beautify_slot_span && reduce_middle_instead_of_start_part == beautify_from_middle) {
    return beautified_path;
  }

  beautify_slot_span = max_dir_count;
  beautify_from_middle = reduce_middle_instead_of_start_part;

  delete beautified_path;
  beautified_path = nullptr;

  const char *s;

  // do we have a canonical flavor, or must we use the original as a base?
  if (canonicalized && beautify_slot_span > 0) {
    s = canonicalized;
  } else if (orig_path) {
    s = orig_path;
  } else {
    return "(...empty...)";
  }

  // do we need to reduce the path for beauty's sake?
  if (beautify_slot_span <= 0) {
    // beautified_path = s;
    return s;
  }

  // additional limiting heuristic: when beautify_slot_span is non-zero, hence a path width limit is specified,
  // we then assume that, for beauty's sake, every path particle is at most 15 characters wide: we TRY to keep
  // the beautified path within that length, by further reducing the number of path elements shown when
  // overflow is apparent.
  unsigned max_plen = max_dir_count * (15 + 1);
  auto slen = strlen(s);

  // are we already within bounds of what we like? If so, then there's no need for shortening...
  if (max_plen >= slen) {
    // beautified_path = s;
    return s;
  }

  // we know we have overflow, one way or the other, so we need to find out how and where to shorten this path now.

  char *sbuf = new char[slen + 2 + 6 /* "(...)/" */]; // +2 due to the way we use strspn() below: that only works if we have TWO NUL sentinels!
  sbuf += 6;                                          // first, we reserve space for the (...) shortening prefix
  memcpy(sbuf, s, slen);
  sbuf[slen] = 0;
  sbuf[slen + 1] = 0;

  // count the number of path elements & register their positions:
  std::vector<unsigned> elems;
  s = sbuf;
  while (*s) { // <-- that condition hits the *second* NUL sentinel once we reached the end!
    unsigned pos = strspn(s, "/\\:");
    if (pos > 0) {
      elems.push_back(s - sbuf);
    }
    s += pos + 1; // skip separator (or first NUL sentinel when we hit the end!)
  }

  unsigned elem_count = elems.size();
  if (!reduce_middle_instead_of_start_part) {
    unsigned i = elem_count;
    unsigned pos = slen;
    while (--i > 0) {
      // heuristic: print at least the filename + containing directory name, i.e. 2 elements
      if (6 + slen - pos > max_plen && elem_count - i > 2)
        break;
      if (elem_count - i > max_dir_count)
        break;
      pos = elems[i];
    }
    // correct for that last `--i` operation:
    i++;
    pos = elems[i];
    s = sbuf + pos;

    // now write the shortened string, starting with the 'this-is-shortened' prefix:
    sbuf -= 6;
    if (pos > 0) {
      memcpy(sbuf, "(...)/", 6);
      memmove(sbuf + 6, s, 1 + slen - pos);
    } else {
      memmove(sbuf, s, 1 + slen);
    }

    beautified_path = sbuf;
    return sbuf;
  } else {
    // reduce_middle_instead_of_start_part ... 'middle' being the middle element, unless our
    // heuristics below decide to skew this, as we like slightly more info to appear at the tail end
    // of the shortened path.
    unsigned i = elem_count;
    unsigned j = 0;
    unsigned tail_pos = slen;
    unsigned lead_pos = elems[0];
    while (tail_pos > lead_pos) {
      // heuristic: print at least the filename + containing directory name, i.e. 2 elements
      if (7 + lead_pos + slen - tail_pos > max_plen && elem_count - i >= 2) {
        assert(j > 0);
        // reduce lead part by one directory.
        --j;
        lead_pos = elems[j];
        break;
      }
      if (elem_count + j - i > max_dir_count) {
        break;
      }
      tail_pos = elems[--i];
      lead_pos = elems[++j];
    }
    s = sbuf;

    sbuf -= 6;

    if (tail_pos <= lead_pos + 7) {
      // no shortening after all...
      memmove(sbuf, s, 1 + slen);
      beautified_path = sbuf;
      return sbuf;
    }
    assert(tail_pos > lead_pos);

    // now write the shortened string, starting with the 'this-is-shortened' prefix:
    if (lead_pos) {
      memmove(sbuf, s, lead_pos);
    }
    memcpy(sbuf + lead_pos, "/(...)/", 7);
    memmove(sbuf + lead_pos + 7, s + tail_pos, 1 + slen - tail_pos);

    beautified_path = sbuf;
    return sbuf;
  }
}


} // namespace tesseract
