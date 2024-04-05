// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "commandlineflags.h"
#include <tesseract/baseapi.h> // TessBaseAPI::Version
#include <cmath>               // for std::isnan, NAN
#include <locale>              // for std::locale::classic
#include <sstream>             // for std::stringstream
#include <vector>              // for std::vector
#include "errcode.h"
#include "helpers.h"
#include "tprintf.h"

namespace tesseract {

static void PrintCommandLineFlags() {
  const char *kFlagNamePrefix = "FLAGS_";
  const int kFlagNamePrefixLen = strlen(kFlagNamePrefix);
  for (ParamPtr param : GlobalParams().as_list()) {
    if (!strncmp(param->name_str(), kFlagNamePrefix, kFlagNamePrefixLen)) {
      tprintInfo("  --{}  {}  (type:{} default:{})\n",
        param->name_str() + kFlagNamePrefixLen,
        param->info_str(), 
        param->value_type_str(),
	      param->formatted_value_str());
    }
  }
}

int ParseCommandLineFlags(const char *extra_usage, std::function<void(const char* exename)> extra_usage_f, int* argc_ref, const char ***argv_ref, const bool remove_flags, std::function<void()> print_version_f) {
  int argc = *argc_ref;
  const char** argv = *argv_ref;
  if (!extra_usage)
    extra_usage = "";
  const char* appname = ((argc > 0 && argv[0]) ? fz_basename(argv[0]) : "???");

  if (argc == 1) {
    tprintInfo("USAGE:\n  {} -v | --version | {}\n", appname, extra_usage);
    PrintCommandLineFlags();
    if (extra_usage_f) {
      tprintInfo("\n");
      extra_usage_f(appname);
    }
    return 0;
  }

  if (argc > 1 && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))) {
    if (print_version_f)
      print_version_f();
    tprintInfo("{} (tesseract) v{}\n", appname, TessBaseAPI::Version());
    return 0;
  }

  int i;
  for (i = 1; i < argc; ++i) {
    const char *current_arg = argv[i];
    // If argument does not start with a hyphen then break.
    if (current_arg[0] != '-') {
      break;
    }
    // Position current_arg after starting hyphens. We treat a sequence of
    // one or two consecutive hyphens identically.
    ++current_arg;
    if (current_arg[0] == '-') {
      ++current_arg;
    }
    // If this is asking for usage, print the help message and abort.
    if (!strcmp(current_arg, "help")) {
      tprintInfo("USAGE:\n  {} -v | --version | [OPTION ...] {}\n", appname, extra_usage);
      PrintCommandLineFlags();
      return 0;
    }
    if (!strcmp(current_arg, "help-extra")) {
      tprintInfo("USAGE:\n  {} -v | --version | [OPTION ...] {}\n", appname, extra_usage);
      PrintCommandLineFlags();
      return 0;
    }
    // Find the starting position of the value if it was specified in this
    // string.
    const char *equals_position = strchr(current_arg, '=');
    const char *rhs = nullptr;
    if (equals_position != nullptr) {
      rhs = equals_position + 1;
    }
    // Extract the flag name.
    std::string lhs;
    if (equals_position == nullptr) {
      lhs = current_arg;
    } else {
      lhs.assign(current_arg, equals_position - current_arg);
    }
    if (!lhs.length()) {
      tprintError("Bad argument: {}\n", argv[i]);
      return 1;
    }

    // Find the flag name in the list of global flags.
    std::string full_flag_name("FLAGS_");
    full_flag_name += lhs;
    auto *p = ParamUtils::FindParam<Param>(full_flag_name.c_str(), GlobalParams());
    if (p == nullptr) {
      // Flag was not found. Exit with an error message?

      // When the commandline option is a single character, it's probably
      // an application specific command. Keep it.
      if (lhs.length() == 1) {
        break;
      }

      tprintError("Non-existent flag '{}'\n", lhs);
      return 1;
    }

    // do not require rhs when parameter is the boolean type:
    if (rhs == nullptr) {
      // Pick the next argument
      if (i + 1 >= argc) {
        if (p->type() != BOOL_PARAM) {
          tprintError("Could not find value for flag {}\n", lhs);
          return 1;
        }
        else {
          // --flag form
          rhs = "true";
        }
      }
      else {
        rhs = argv[++i];
      }
    }
    if (p->type() == BOOL_PARAM && strlen(rhs) == 0) {
      // Bad input of the format --bool_flag=
      tprintError("Bad boolean flag '{}' argument: '{}'\n", lhs, rhs);
      return 1;
    }

    if (!p->set_value(rhs)) {
      tprintError("Could not parse value '{}' for flag '{}'\n", rhs, lhs);
      return 1;
    }
  } // for each argv
  if (remove_flags && i > 1) {
    (*argv_ref)[i - 1] = argv[0];
    (*argv_ref) += (i - 1);
    (*argc_ref) -= (i - 1);
  }
  return -1;		// continue executing the application
}


// as per https://stackoverflow.com/questions/15826188/what-most-correct-way-to-set-the-encoding-in-c

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)

#include <windows.h>

class AutoWin32ConsoleOutputCP {
public:
  explicit AutoWin32ConsoleOutputCP(UINT codeCP) {
    oldCCP_ = GetConsoleCP();
    oldCP_ = GetConsoleOutputCP();

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(codeCP);
  }
  ~AutoWin32ConsoleOutputCP() {
    SetConsoleOutputCP(oldCP_);
    SetConsoleCP(oldCCP_);
  }

  bool activated() {
    return !!(oldCP_ | oldCCP_);
  }

private:
  UINT oldCP_;
  UINT oldCCP_;
};

static AutoWin32ConsoleOutputCP autoWin32ConsoleOutputCP(CP_UTF8);

#endif // _WIN32

bool SetConsoleModeToUTF8(void) {
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
  return autoWin32ConsoleOutputCP.activated();
#else
  return true;
#endif
}

} // namespace tesseract
