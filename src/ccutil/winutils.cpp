// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.

#include "winutils.h"

#ifdef _WIN32

#include <windows.h>

namespace tesseract {
namespace winutils {

// Note that UTF conversion functions overwrite std::string[size()]
// with CharT(), which is allowed by the C++ standard.

std::wstring Utf8ToUtf16(const char* utf8Str) {
  const auto sizeWithNull = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, nullptr, 0);
  if (sizeWithNull <= 0) {
    return {};
  }

  std::wstring result(sizeWithNull - 1, 0);

  if (!MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, &result[0], sizeWithNull)) {
    return {};
  }

  return result;
}

std::string Utf16ToUtf8(const wchar_t* utf16Str) {
  const auto sizeWithNull = WideCharToMultiByte(CP_UTF8, 0, utf16Str, -1, nullptr, 0, nullptr, nullptr);
  if (sizeWithNull <= 0) {
    return {};
  }

  std::string result(sizeWithNull - 1, 0);

  if (!WideCharToMultiByte(CP_UTF8, 0, utf16Str, -1, &result[0], sizeWithNull, nullptr, nullptr)) {
    return {};
  }

  return result;
}

// convert input name from ANSI encoding to utf-8
std::string AnsiToUtf8(const char *AnsiStr) {
  auto str16_len = MultiByteToWideChar(CP_ACP, 0, AnsiStr, -1, nullptr, 0);
  wchar_t *uni16_str = new WCHAR[str16_len];
  str16_len = MultiByteToWideChar(CP_ACP, 0, AnsiStr, -1, uni16_str, str16_len);
  std::string utf8_str = Utf16ToUtf8(uni16_str);
  delete[] uni16_str;
  return utf8_str;
}

} // namespace winutils
} // namespace tesseract

#endif // _WIN32
