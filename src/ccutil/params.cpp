/**********************************************************************
 * File:        params.cpp
 * Description: Initialization and setting of Tesseract parameters.
 * Author:      Ger Hobbelt
 *
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 **********************************************************************/

#include <tesseract/params.h>

namespace tesseract {





//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ParamUtils
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ParamUtils::ReadParamsFile(const std::string &file,
                                const ParamsVectorSet &member_params,
								                ParamSetBySourceType source_type,
								                ParamPtr source
) {
  TFile fp;
  if (!fp.Open(file.c_str(), nullptr)) {
    tprintError("read_params_file: Can't open/read file {}\n", file);
    return true;
  }
  return ReadParamsFromFp(&fp, member_params, source_type, source);
}

bool ParamUtils::ReadParamsFromFp(TFile *fp,
                                  const ParamsVectorSet &member_params,
                                  ParamSetBySourceType source_type,
                                  ParamPtr source) {
#define LINE_SIZE 4096
  char line[LINE_SIZE]; // input line
  bool anyerr = false;  // true if any error
  bool foundit;         // found parameter
  char *nameptr;        // name field
  char *valptr;         // value field
  unsigned linecounter = 0;

  while (fp->FGets(line, LINE_SIZE) != nullptr) {
    linecounter++;

    // trimRight:
    for (nameptr = line + strlen(line) - 1; nameptr >= line && std::isspace(*nameptr); nameptr--) {
      ;
    }
    nameptr[1] = 0;
    // trimLeft:
    for (nameptr = line; *nameptr && std::isspace(*nameptr); nameptr++) {
      ;
    }

    if (nameptr[0] && nameptr[0] != '#') {
      // jump over variable name
      for (valptr = nameptr; *valptr && !std::isspace(*valptr); valptr++) {
        ;
      }

      if (*valptr) {    // found blank
        *valptr = '\0'; // make name a string

        do {
          valptr++; // find end of blanks
        } while (std::isspace(*valptr));
      }
      foundit = SetParam(nameptr, valptr, member_params, source_type, source);

      if (!foundit) {
        anyerr = true; // had an error
        tprintError("Failure while processing parameter line: {}  {}\n", nameptr, valptr);
      }
    }
  }
  return anyerr;
}








//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ParamsReportWriter, et al
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

class ParamsReportWriter {
public:
  ParamsReportWriter(FILE *f)
      : file_(f) {}
  virtual ~ParamsReportWriter() = default;

  virtual void Write(const std::string message) = 0;

protected:
  FILE *file_;
};

class ParamsReportDefaultWriter : public ParamsReportWriter {
public:
  ParamsReportDefaultWriter() : ParamsReportWriter(nullptr) {}
  virtual ~ParamsReportDefaultWriter() = default;

  virtual void Write(const std::string message) {
    tprintDebug("{}", message);
  }

protected:
};

class ParamsReportFileDuoWriter : public ParamsReportWriter {
public:
  ParamsReportFileDuoWriter(FILE *f) : ParamsReportWriter(f) {
    is_separate_file_ = (f != nullptr && f != stderr && f != stdout);
  }
  virtual ~ParamsReportFileDuoWriter() = default;

  virtual void Write(const std::string message) {
    // only write via tprintDebug() -- which usually logs to stderr -- when the `f` file destination is an actual file, rather than stderr or stdout.
    // This prevents these report lines showing up in duplicate on the console.
    if (is_separate_file_) {
      tprintDebug("{}", message);
    }
    size_t len = message.length();
    if (fwrite(message.c_str(), 1, len, file_) != len) {
      tprintError("Failed to write params-report line to file. {}\n", strerror(errno));
    }
  }

protected:
  bool is_separate_file_;
};


static inline const char *type_as_str(ParamType type) {
	switch (type) {
	case INT_PARAM:
		return "[Integer]";
	case BOOL_PARAM:
		return "[Boolean]";
	case DOUBLE_PARAM:
		return "[Float]";
	case STRING_PARAM:
		return "[String]";
	case ANY_TYPE_PARAM:
		return "[ANY]";
	default:
		return "[???]";
	}
}




#if 0

// When `section_title` is NULL, this will report the lump sum parameter usage for the entire run.
// When `section_title` is NOT NULL, this will only report the parameters that were actually used (R/W) during the last section of the run, i.e.
// since the previous invocation of this reporting method (or when it hasn't been called before: the start of the application).
void ParamUtils::ReportParamsUsageStatistics(FILE *f, const ParamsVectorSet *member_params, const char *section_title)
{
  bool is_section_subreport = (section_title != nullptr);

  std::unique_ptr<ParamsReportWriter> writer;

  if (f != nullptr) {
    writer.reset(new ParamsReportFileDuoWriter(f));
  } else {
    writer.reset(new ParamsReportDefaultWriter());
  }

  writer->Write(fmt::format("\n\n{} Parameter Usage Statistics{}: which params have been relevant?\n"
                            "----------------------------------------------------------------------\n\n",
                            ParamUtils::GetApplicationName(), (section_title != nullptr ? fmt::format(" for section: {}", section_title) : "")));

  // first collect all parameters and sort them according to these criteria:
  // - global / (class)local
  // - name

  const ParamsVector* globals = GlobalParams();

  struct ParamInfo {
	  Param *p;
	  bool global;
  };
  
  std::vector<ParamInfo> params;
  {
	std::vector<Param *> ll = globals->as_list();
	params.reserve(ll.size());
	for (Param *i : ll) {
	  params.push_back({i, true});
	}
  }

  if (member_params != nullptr) {
	std::vector<Param *> ll = member_params->as_list();
	params.reserve(ll.size() + params.size());
	for (Param *i : ll) {
	  params.push_back({i, false});
	}
  }

  sort(params.begin(), params.end(), [](ParamInfo& a, ParamInfo& b)
  {
    int rv = (int) a.global - (int) b.global;
	if (rv == 0)
	{
		rv = (int) a.p->is_init() - (int) b.p->is_init();
	if (rv == 0)
	{
		rv = (int) b.p->is_debug() - (int) a.p->is_debug();
	if (rv == 0)
	{
	  rv = strcmp(b.p->name_str(), a.p->name_str());
#if !defined(NDEBUG)
	  if (rv == 0) 
	  {
	  	fprintf(stderr, "Apparently you have double-defined {} Variable: '%s'! Fix that in the source code!\n", ParamUtils::GetApplicationName(), a.p->name_str());
	    ASSERT0(!"Apparently you have double-defined a Variable.");
	  }
#endif
	}}}
    return (rv >= 0);
  });

  static const char* categories[] = { "(Global)", "(Local)" };
  static const char* sections[] = { "", "(Init)", "(Debug)", "(Init+Dbg)" };
  static const char* write_access[] = { ".", "w", "W" };
  static const char* read_access[] = { ".", "r", "R" };

  auto acc = [](int access) {
    if (access > 2)
      access = 2;
    return access;
  };

  if (!is_section_subreport) {
    // produce the final lump-sum overview report

    for (ParamInfo &item : params) {
      Param *p = item.p;
      p->reset_access_counts();
    }

    for (ParamInfo &item : params) {
      const Param* p = item.p;
      auto stats = p->access_counts();
      if (stats.prev_sum_reading > 0)
      {
		int section = ((int)p->is_init()) | (2 * (int)p->is_debug());
        writer->Write(fmt::format("* {:.<60} {:8}{:10} {}{} {:9} = {}\n", p->name_str(), categories[item.global], sections[section], write_access[acc(stats.prev_sum_writing)], read_access[acc(stats.prev_sum_reading)], type_as_str(p->type()), p->formatted_value_str()));
      }
    }

    if (report_all_variables)
    {
      writer->Write("\n\nUnused parameters:\n\n");

	  for (ParamInfo &item : params) {
  	    const Param* p = item.p;
		auto stats = p->access_counts();
        if (stats.prev_sum_reading <= 0)
        {
			int section = ((int)p->is_init()) | (2 * (int)p->is_debug());
			writer->Write(fmt::format("* {:.<60} {:8}{:10} {}{} {:9} = {}\n", p->name_str(), categories[item.global], sections[section], write_access[acc(stats.prev_sum_writing)], read_access[acc(stats.prev_sum_reading)], type_as_str(p->type()), p->formatted_value_str()));
		}
      }
    }
  }
  else {
    // produce the section-local report of used parameters

	for (ParamInfo &item : params) {
	  const Param* p = item.p;
	  auto stats = p->access_counts();
      if (stats.reading > 0)
      {
		  int section = ((int)p->is_init()) | (2 * (int)p->is_debug());
		  writer->Write(fmt::format("* {:.<60} {:8}{:10} {}{} {:9} = {}\n", p->name_str(), categories[item.global], sections[section], write_access[acc(stats.prev_sum_writing)], read_access[acc(stats.prev_sum_reading)], type_as_str(p->type()), p->formatted_value_str()));
	  }
    }

    // reset the access counts for the next section:
	for (ParamInfo &item : params) {
	  Param* p = item.p;
      p->reset_access_counts();
    }
  }
}

template<>
IntParam *FindParam(const char *name, ParamsVectorSet *globals, ParamsVectorSet *locals, const IntParam *DUMMY, ParamType accepted_types_mask) {
	if (!globals)
		globals = ::tesseract::GlobalParams();

	Param *p = globals->find(name);
	if (!p && locals != nullptr) {
		p = locals->find(name);
	}
	IntParam *rv = nullptr;
	if (p && p->type() == INT_PARAM)
		rv = static_cast<IntParam *>(p);

	if (rv)
		return rv;

	return nullptr;
}


bool ParamUtils::SetParam(const char *name, const char *value,
                          ParamsVectorSet *member_params) {
  // Look for the parameter among string parameters.
  auto *sp = FindParam<StringParam>(name, GlobalParams(), member_params);
  if (sp != nullptr) {
    sp->set_value(value);
  }
  if (*value == '\0') {
    return (sp != nullptr);
  }

  // Look for the parameter among int parameters.
  auto *ip = FindParam<IntParam>(name, GlobalParams(), member_params);
  if (ip) {
    int intval = INT_MIN;
    std::stringstream stream(value);
    stream.imbue(std::locale::classic());
    stream >> intval;
    if (intval == 0) {
      std::string sv(stream.str());
      if (!sv.empty() &&
          (sv[0] == 'T' || sv[0] == 't' || sv[0] == 'Y' || sv[0] == 'y')) {
        intval = 1;
      }
    }
    if (intval != INT_MIN) {
      ip->set_value(intval);
    }
  }

  // Look for the parameter among bool parameters.
  auto *bp = FindParam<BoolParam>(name, GlobalParams(), member_params);
  if (bp != nullptr && bp->constraint_ok(constraint)) {
    if (*value == 'T' || *value == 't' || *value == 'Y' || *value == 'y' || *value == '1') {
      bp->set_value(true);
    } else if (*value == 'F' || *value == 'f' || *value == 'N' || *value == 'n' || *value == '0') {
      bp->set_value(false);
    }
  }

  // Look for the parameter among double parameters.
  auto *dp = FindParam<DoubleParam>(name, GlobalParams(), member_params);
  if (dp != nullptr) {
    double doubleval = NAN;
    std::stringstream stream(value);
    stream.imbue(std::locale::classic());
    stream >> doubleval;
    if (!std::isnan(doubleval)) {
      dp->set_value(doubleval);
    }
  }
  return (sp || ip || bp || dp);
}

bool ParamUtils::GetParamAsString(const char *name, const ParamsVectorSet *member_params,
                                  std::string *value) {
  // Look for the parameter among string parameters.
  auto *sp = FindParam<StringParam>(name, GlobalParams(), member_params);
  if (sp) {
    *value = sp->c_str();
    return true;
  }
  // Look for the parameter among int parameters.
  auto *ip = FindParam<IntParam>(name, GlobalParams(), member_params);
  if (ip) {
    *value = std::to_string(int32_t(*ip));
    return true;
  }
  // Look for the parameter among bool parameters.
  auto *bp = FindParam<BoolParam>(name, GlobalParams(), member_params);
  if (bp != nullptr) {
    *value = bool(*bp) ? "1" : "0";
    return true;
  }
  // Look for the parameter among double parameters.
  auto *dp = FindParam<DoubleParam>(name, GlobalParams(), member_params);
  if (dp != nullptr) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << double(*dp);
    *value = stream.str();
    return true;
  }
  return false;
}

void ParamUtils::PrintParams(FILE *fp, const ParamsVectorSet *member_params, bool print_info) {
  int num_iterations = (member_params == nullptr) ? 1 : 2;
  // When printing to stdout info text is included.
  // Info text is omitted when printing to a file (would result in an invalid config file).
  if (!fp)
	  fp = stdout;
  bool printing_to_stdio = (fp == stdout || fp == stderr);
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  for (int v = 0; v < num_iterations; ++v) {
    const ParamsVectorSet *vec = (v == 0) ? GlobalParams() : member_params;
    for (auto int_param : vec->int_params_c()) {
      if (print_info) {
        stream << int_param->name_str() << '\t' << (int32_t)(*int_param) << '\t'
              << int_param->info_str() << '\n';
      } else {
        stream << int_param->name_str() << '\t' << (int32_t)(*int_param) << '\n';
      }
    }
    for (auto bool_param : vec->bool_params_c()) {
      if (print_info) {
        stream << bool_param->name_str() << '\t' << bool(*bool_param) << '\t'
              << bool_param->info_str() << '\n';
      } else {
        stream << bool_param->name_str() << '\t' << bool(*bool_param) << '\n';
      }
    }
    for (auto string_param : vec->string_params_c()) {
      if (print_info) {
        stream << string_param->name_str() << '\t' << string_param->c_str() << '\t'
              << string_param->info_str() << '\n';
      } else {
        stream << string_param->name_str() << '\t' << string_param->c_str() << '\n';
      }
    }
    for (auto double_param : vec->double_params_c()) {
      if (print_info) {
        stream << double_param->name_str() << '\t' << (double)(*double_param) << '\t'
              << double_param->info_str() << '\n';
      } else {
        stream << double_param->name_str() << '\t' << (double)(*double_param) << '\n';
      }
    }
  }
  if (printing_to_stdio)
  {
	  tprintDebug("{}", stream.str().c_str());
  }
  else
  {
      fprintf(fp, "%s", stream.str().c_str());
  }
}

// Resets all parameters back to default values;
void ParamUtils::ResetToDefaults(ParamsVectorSet *member_params) {
	for (Param *param : GlobalParams()->as_list()) {
		param->ResetToDefault();
	}
	if (member_params != nullptr) {
		for (Param *param : member_params->as_list()) {
			param->ResetToDefault();
		}
	}
}

#endif

static std::string params_appname_4_reporting = ParamUtils::GetApplicationName();

// Set the application name to be mentioned in libparameters' error messages.
void ParamUtils::SetApplicationName(const char* appname) {
  if (!appname || !*appname) {
    appname = "[?anonymous.app?]";

#if defined(_WIN32)
    {
      DWORD pathlen = MAX_PATH - 1;
      DWORD bufsize = pathlen + 1;
      LPSTR buffer = (LPSTR)malloc(bufsize * sizeof(buffer[0]));

      for (;;) {
        buffer[0] = 0;

        // On WinXP, if path length >= bufsize, the output is truncated and NOT
        // null-terminated.  On Vista and later, it will null-terminate the
        // truncated string. We call ReleaseBuffer on all OSes to be safe.
        pathlen = ::GetModuleFileNameA(NULL,
                                       buffer,
                                       bufsize);
        if (pathlen > 0 && pathlen < bufsize - 1)
          break;

        if (pathlen == 0 && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
          buffer[0] = 0;
          break;
        }
        bufsize *= 2;
        buffer = (LPSTR)realloc(buffer, bufsize * sizeof(buffer[0]));
      }
      if (buffer[0]) {
        appname = buffer;
      }

      free(buffer);
    }
#endif
  }

  params_appname_4_reporting = appname;
}

const std::string& ParamUtils::GetApplicationName() {
  return params_appname_4_reporting;
}


} // namespace tesseract
