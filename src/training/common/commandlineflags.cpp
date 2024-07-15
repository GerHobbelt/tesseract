// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <tesseract/preparation.h> // compiler config, etc.

#include "commandlineflags.h"
#include <tesseract/baseapi.h> // TessBaseAPI::Version
#include <cmath>               // for std::isnan, NAN
#include <locale>              // for std::locale::classic
#include <sstream>             // for std::stringstream
#include <vector>              // for std::vector
#include "errcode.h"
#include "helpers.h"
#include <tesseract/tprintf.h>

namespace tesseract {
static bool IntFlagExists(const char *flag_name, int32_t *value) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<IntParam *> empty;
  auto *p =
      ParamUtils::FindParam<IntParam>(full_flag_name.c_str(), GlobalParams()->int_params_c(), empty);
  if (p == nullptr) {
    return false;
  }
  *value = (int32_t)(*p);
  return true;
}

static bool DoubleFlagExists(const char *flag_name, double *value) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<DoubleParam *> empty;
  auto *p = ParamUtils::FindParam<DoubleParam>(full_flag_name.c_str(),
                                               GlobalParams()->double_params_c(), empty);
  if (p == nullptr) {
    return false;
  }
  *value = static_cast<double>(*p);
  return true;
}

static bool BoolFlagExists(const char *flag_name, bool *value) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<BoolParam *> empty;
  auto *p =
      ParamUtils::FindParam<BoolParam>(full_flag_name.c_str(), GlobalParams()->bool_params_c(), empty);
  if (p == nullptr) {
    return false;
  }
  *value = bool(*p);
  return true;
}

static bool StringFlagExists(const char *flag_name, const char **value) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<StringParam *> empty;
  auto *p = ParamUtils::FindParam<StringParam>(full_flag_name.c_str(),
                                               GlobalParams()->string_params_c(), empty);
  *value = (p != nullptr) ? p->c_str() : nullptr;
  return p != nullptr;
}

static void SetIntFlagValue(const char *flag_name, const int32_t new_val) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<IntParam *> empty;
  auto *p =
      ParamUtils::FindParam<IntParam>(full_flag_name.c_str(), GlobalParams()->int_params(), empty);
  ASSERT_HOST(p != nullptr);
  p->set_value(new_val);
}

static void SetDoubleFlagValue(const char *flag_name, const double new_val) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<DoubleParam *> empty;
  auto *p = ParamUtils::FindParam<DoubleParam>(full_flag_name.c_str(),
                                               GlobalParams()->double_params(), empty);
  ASSERT_HOST(p != nullptr);
  p->set_value(new_val);
}

static void SetBoolFlagValue(const char *flag_name, const bool new_val) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<BoolParam *> empty;
  auto *p =
      ParamUtils::FindParam<BoolParam>(full_flag_name.c_str(), GlobalParams()->bool_params(), empty);
  ASSERT_HOST(p != nullptr);
  p->set_value(new_val);
}

static void SetStringFlagValue(const char *flag_name, const char *new_val) {
  std::string full_flag_name("FLAGS_");
  full_flag_name += flag_name;
  std::vector<StringParam *> empty;
  auto *p = ParamUtils::FindParam<StringParam>(full_flag_name.c_str(),
                                               GlobalParams()->string_params(), empty);
  ASSERT_HOST(p != nullptr);
  p->set_value(std::string(new_val));
}

static bool SafeAtoi(const char *str, int *val) {
  char *endptr = nullptr;
  *val = strtol(str, &endptr, 10);
  return endptr != nullptr && *endptr == '\0';
}

static bool SafeAtod(const char *str, double *val) {
  double d = NAN;
  std::stringstream stream(str);
  // Use "C" locale for reading double value.
  stream.imbue(std::locale::classic());
  stream >> d;
  *val = 0;
  bool success = !std::isnan(d);
  if (success) {
    *val = d;
  }
  return success;
}

static void PrintCommandLineFlags() {
  const char *kFlagNamePrefix = "FLAGS_";
  const int kFlagNamePrefixLen = strlen(kFlagNamePrefix);
  for (auto &param : GlobalParams()->int_params_c()) {
    if (!strncmp(param->name_str(), kFlagNamePrefix, kFlagNamePrefixLen)) {
      tprintDebug("  --{}  {}  (type:int default:{})\n",
             param->name_str() + kFlagNamePrefixLen,
             param->info_str(), int32_t(*param));
    }
  }
  for (auto &param : GlobalParams()->double_params_c()) {
    if (!strncmp(param->name_str(), kFlagNamePrefix,
                 kFlagNamePrefixLen)) {
      tprintDebug("  --{}  {}  (type:double default:{})\n",
             param->name_str() + kFlagNamePrefixLen,
             param->info_str(),
             static_cast<double>(*param));
    }
  }
  for (auto &param : GlobalParams()->bool_params_c()) {
    if (!strncmp(param->name_str(), kFlagNamePrefix, kFlagNamePrefixLen)) {
      tprintDebug("  --{}  {}  (type:bool default:{})\n",
             param->name_str() + kFlagNamePrefixLen,
             param->info_str(),
             bool(*param) ? "true" : "false");
    }
  }
  for (auto &param : GlobalParams()->string_params_c()) {
    if (!strncmp(param->name_str(), kFlagNamePrefix,
                 kFlagNamePrefixLen)) {
      tprintDebug("  --{}  {}  (type:string default:{})\n",
             param->name_str() + kFlagNamePrefixLen,
             param->info_str(),
             param->c_str());
    }
  }
}

int ParseCommandLineFlags(const char *usage, int* argc, const char ***argv, const bool remove_flags) {
  if (*argc == 1) {
    tprintDebug("USAGE: {}\n", usage);
    PrintCommandLineFlags();
    return 0;
  }

  if (*argc > 1 && (!strcmp((*argv)[1], "-v") || !strcmp((*argv)[1], "--version"))) {
    tprintDebug("{}\n", TessBaseAPI::Version());
    return 0;
  }

  int i;
  for (i = 1; i < *argc; ++i) {
    const char *current_arg = (*argv)[i];
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
      tprintDebug("Usage:\n  {} [OPTION ...]\n\n", usage);
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
      tprintError("Bad argument: {}\n", (*argv)[i]);
      return 1;
    }

    // Find the flag name in the list of global flags.
    // int32_t flag
    int32_t int_val;
    if (IntFlagExists(lhs.c_str(), &int_val)) {
      if (rhs != nullptr) {
        if (!strlen(rhs)) {
          // Bad input of the format --int_flag=
          tprintError("Bad argument: {}\n", (*argv)[i]);
          return 1;
        }
        if (!SafeAtoi(rhs, &int_val)) {
          tprintError("Could not parse int from {} in flag {}\n", rhs, (*argv)[i]);
          return 1;
        }
      } else {
        // We need to parse the next argument
        if (i + 1 >= *argc) {
          tprintError("Could not find value argument for flag {}\n", lhs.c_str());
          return 1;
        } else {
          ++i;
          if (!SafeAtoi((*argv)[i], &int_val)) {
            tprintError("Could not parse int32_t from {}\n", (*argv)[i]);
            return 1;
          }
        }
      }
      SetIntFlagValue(lhs.c_str(), int_val);
      continue;
    }

    // double flag
    double double_val;
    if (DoubleFlagExists(lhs.c_str(), &double_val)) {
      if (rhs != nullptr) {
        if (!strlen(rhs)) {
          // Bad input of the format --double_flag=
          tprintError("Bad argument: {}\n", (*argv)[i]);
          return 1;
        }
        if (!SafeAtod(rhs, &double_val)) {
          tprintError("Could not parse double from {} in flag {}\n", rhs, (*argv)[i]);
          return 1;
        }
      } else {
        // We need to parse the next argument
        if (i + 1 >= *argc) {
          tprintError("Could not find value argument for flag {}\n", lhs.c_str());
          return 1;
        } else {
          ++i;
          if (!SafeAtod((*argv)[i], &double_val)) {
            tprintError("Could not parse double from {}\n", (*argv)[i]);
            return 1;
          }
        }
      }
      SetDoubleFlagValue(lhs.c_str(), double_val);
      continue;
    }

    // Bool flag. Allow input forms --flag (equivalent to --flag=true),
    // --flag=false, --flag=true, --flag=0 and --flag=1
    bool bool_val;
    if (BoolFlagExists(lhs.c_str(), &bool_val)) {
      if (rhs == nullptr) {
        // --flag form
        bool_val = true;
      } else {
        if (!strlen(rhs)) {
          // Bad input of the format --bool_flag=
          tprintError("Bad argument: {}\n", (*argv)[i]);
          return 1;
        }
        if (!strcmp(rhs, "false") || !strcmp(rhs, "0")) {
          bool_val = false;
        } else if (!strcmp(rhs, "true") || !strcmp(rhs, "1")) {
          bool_val = true;
        } else {
          tprintError("Could not parse bool from flag {}\n", (*argv)[i]);
          return 1;
        }
      }
      SetBoolFlagValue(lhs.c_str(), bool_val);
      continue;
    }

    // string flag
    const char *string_val;
    if (StringFlagExists(lhs.c_str(), &string_val)) {
      if (rhs != nullptr) {
        string_val = rhs;
      } else {
        // Pick the next argument
        if (i + 1 >= *argc) {
          tprintError("Could not find string value for flag {}\n", lhs.c_str());
          return 1;
        } else {
          string_val = (*argv)[++i];
        }
      }
      SetStringFlagValue(lhs.c_str(), string_val);
      continue;
    }

    // Flag was not found. Exit with an error message.
    tprintError("Non-existent flag {}\n", (*argv)[i]);
    return 1;
  } // for each argv
  if (remove_flags) {
    (*argv)[i - 1] = (*argv)[0];
    (*argv) += (i - 1);
    (*argc) -= (i - 1);
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
