/**********************************************************************
 * File:        params.h
 * Description: Class definitions of the *_VAR classes for tunable constants.
 * Author:      Ray Smith
 * Author:      Ger Hobbelt
 * 
 * UTF8 detect helper statement: «bloody MSVC»
 *
 * (C) Copyright 1991, Hewlett-Packard Ltd.
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

#ifndef TESSERACT_PARAMS_H
#define TESSERACT_PARAMS_H

#include <tesseract/export.h> // for TESS_API
#include "../../src/ccutil/params.h"

namespace tesseract {

class TFile;


// --------------------------------------------------------------------------------------------------

#if 0
// Utility functions for working with Tesseract parameters.
class TESS_API ParamUtils {
public:
  // Read parameters from the given file pointer.
  // Otherwise identical to ReadParamsFile().
  static bool ReadParamsFromFp(TFile *fp,
                               const ParamsVectorSet &set,
                               ParamSetBySourceType source_type = PARAM_VALUE_IS_SET_BY_APPLICATION,
                               ParamPtr source = nullptr);

};
#endif

	// --------------------------------------------------------------------------------------------------


	template <size_t SMALL_STRING_ALLOCSIZE = 16>
class CString {
public:
  CString() noexcept
      : buffer(small_buffer),
        allocsize(sizeof(small_buffer)),
        str_start_offset(0) {
    small_buffer[0] = 0;
  }
  CString(size_t allocate_size, size_t alloc_offset = 0)
      : buffer(nullptr),
        allocsize(allocate_size),
        str_start_offset(alloc_offset) {
    if (allocate_size <= SMALL_STRING_ALLOCSIZE) {
      buffer = small_buffer;
      allocsize = sizeof(small_buffer);
    } else {
      buffer = (char *)malloc(allocate_size * sizeof(buffer[0]));
      if (!buffer) {
        throw std::bad_alloc("bad CString alloc: ran out of heap space?");
      }
    }
    if (str_start_offset >= allocsize) {
      throw std::out_of_range("CString: initial alloc_offset is out of acceptable range");
    }
    memset(buffer, 0, str_start_offset + 1);
  }
  CString(const char *str, size_t alloc_offset = 0)
      : buffer(nullptr),
        allocsize(sizeof(small_buffer)),
        str_start_offset(alloc_offset) {
    if (!str || !*str) {
      if (str_start_offset >= allocsize) {
        throw std::out_of_range("CString: initial alloc_offset is out of acceptable range");
      }
      buffer = small_buffer;
      memset(buffer, 0, str_start_offset + 1);
    } else {
      size_t slen = strlen(str);
      allocsize = slen + 1 + str_start_offset;
      if (allocsize <= SMALL_STRING_ALLOCSIZE) {
        buffer = small_buffer;
        allocsize = sizeof(small_buffer);
        if (str_start_offset >= allocsize) {
          throw std::out_of_range("CString: initial alloc_offset is out of acceptable range");
        }
      } else {
        if (str_start_offset >= allocsize) {
          throw std::out_of_range("CString: initial alloc_offset is out of acceptable range");
        }
        buffer = (char *)malloc(allocsize * sizeof(buffer[0]));
        if (!buffer) {
          throw std::bad_alloc("bad CString alloc: ran out of heap space?");
        }
      }
      memset(buffer, 0, str_start_offset + 1);
      strcpy(data(), str);
    }
  }
  CString(const std::string &str, size_t alloc_offset = 0)
      : CString(str.c_str(), alloc_offset) {
  }

  ~CString() {
    if (buffer != small_buffer) {
      free(buffer);
    }
  }

  // trim leading and trailing whitespace.
  void Trim() {
    TrimLeft();
    TrimRight();
  }
  // trim leading whitespace.
  void TrimLeft() {
    char *p = data();
    while (isspace(*p))
      p++;
    str_start_offset = p - buffer;
  };
  // trim trailing whitespace.
  void TrimRight() {
    char *p = data();
    char *e = p + length();
    e--;
    while (e >= p && isspace(*e))
      e--;
    e++;
    *e = 0;
  }

  // return a pointer to the internal string buffer. Contrary to std::string::data(), this pointer points to an editable area of datasize() bytes.
  char *data() {
    return buffer + str_start_offset;
  }
  // return the number of bytes available in the internal string buffer.
  size_t datasize() const {
    return allocsize - str_start_offset;
  }

  const char *c_str() const {
    return buffer + str_start_offset;
  }
  size_t length() const {
    return strlen(buffer + str_start_offset);
  }

  void resize(size_t alloc_size) {
    // do not clip content: adjust size if necessary:
    size_t slen = length() + str_start_offset + 1;
    if (alloc_size < slen) {
      alloc_size = slen;
    }
    if (alloc_size <= SMALL_STRING_ALLOCSIZE) {
      if (buffer != small_buffer) {
        memcpy(small_buffer, buffer, slen);
        free(buffer);
        buffer = small_buffer;
        allocsize = sizeof(small_buffer);
      }
    } else {
      allocsize = alloc_size;
      if (buffer == small_buffer) {
        buffer = (char *)malloc(allocsize * sizeof(buffer[0]));
        if (!buffer) {
          throw std::bad_alloc("bad CString alloc: ran out of heap space?");
        }
        memcpy(buffer, small_buffer, slen);
      } else {
        buffer = (char *)realloc(buffer, allocsize * sizeof(buffer[0]));
        if (!buffer) {
          throw std::bad_alloc("bad CString alloc: ran out of heap space?");
        }
      }
    }
  }

  void append(const char *str) {
    if (!str || !*str)
      return;
    size_t dlen = length();
    size_t slen = strlen(str) + 1;
    if (datasize() - dlen < slen) {
      resize(allocsize + slen);
    }
    strcpy(data() + dlen, str);
  }
  void append(const CString &str) {
    append(str.c_str());
  }
  void append(const std::string &str) {
    append(str.c_str());
  }

protected:
  char small_buffer[SMALL_STRING_ALLOCSIZE];
  size_t allocsize;
  size_t str_start_offset;
  char *buffer;
};


// A simple FILE/stdio wrapper class which supports reading from stdin or regular file.
class ConfigFile {
public:
  // Parse '-', 'stdin' and '1' as STDIN, or open a regular text file in UTF8 read mode.
  //
  // An error line is printed via `tprintf()` when the given path turns out not to be valid.
  ConfigFile(const char *path);
  ConfigFile(const std::string &path)
      : ConfigFile(path.c_str()) {}
  ~ConfigFile();

  FILE *operator()() const;

  operator bool() const {
    return _f != nullptr;
  };

  bool ReadLine(CString<4096> &line);

private:
  FILE *_f;
};

// --------------------------------------------------------------------------------------------------


extern BOOL_VAR_H(stream_filelist);
extern STRING_VAR_H(document_title);
#ifdef HAVE_LIBCURL
extern INT_VAR_H(curl_timeout);
extern STRING_VAR_H(curl_cookiefile);
#endif
extern INT_VAR_H(debug_all);
extern BOOL_VAR_H(debug_misc);
extern BOOL_VAR_H(verbose_process);
#if !GRAPHICS_DISABLED
extern BOOL_VAR_H(scrollview_support);
#endif
extern STRING_VAR_H(vars_report_file);
extern BOOL_VAR_H(report_all_variables);
extern DOUBLE_VAR_H(allowed_image_memory_capacity);
extern BOOL_VAR_H(two_pass);

} // namespace tesseract

#endif
