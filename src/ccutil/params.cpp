/**********************************************************************
 * File:        params.cpp
 * Description: Initialization and setting of Tesseract parameters.
 * Author:      Ray Smith
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

#include "params.h"

#include "helpers.h"  // for chomp_string, mupdf imports, etc.: see also the header collision comment in there (MSVC-specific).
#include "host.h"     // tesseract/export.h, windows.h for MAX_PATH
#include "serialis.h" // for TFile
#include "tprintf.h"
#include "fopenutf8.h"

#include <fmt/core.h>
#include <fmt/format.h>

#include <climits> // for INT_MIN, INT_MAX
#include <cmath>   // for NAN, std::isnan
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale>  // for std::locale::classic
#include <sstream> // for std::stringstream
#include <functional>
#include <exception>
#include <cctype>  // for std::toupper

#if defined(HAVE_MUPDF)
#include "mupdf/assertions.h"
#endif

#ifdef _WIN32
#  include <windows.h>
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#else
#  include <strings.h>
#endif


namespace tesseract {

TESS_API
tesseract::ParamsVectors *GlobalParams() {
  static tesseract::ParamsVectors global_params; // static auto-inits at startup
  return &global_params;
}


static inline bool strieq(const char *s1, const char *s2) {
	return strcasecmp(s1, s2) == 0;
}

// Note about Param names, i.e. Variable Names:
// 
// - accept both `-` and `_` in key names, e.g. user-specified 'debug-all' would match 'debug_all'
//   in the database.
// - names are matched case-*in*sensitive and must be ASCII. Unicode characters in Variable Names 
//   are not supported.

// calculate hash:
std::size_t ParamHash::operator()(const Param& s) const noexcept {
	const char * str = s.name_str();
	return ParamHash()(str);
}
// calculate hash:
std::size_t ParamHash::operator()(const char * s) const noexcept {
	ASSERT0(s != nullptr);
	uint32_t h = 1;
	for (const char *p = s; *p ; p++) {
		uint32_t c = std::toupper(static_cast<unsigned char>(*p));
		if (c == '-')
			c = '_';
		h *= 31397;
		h += c;
	}
	return h;
}

// equal_to:
bool ParamHash::operator()( const Param& lhs, const Param& rhs ) const noexcept {
	ASSERT0(lhs.name_str() != nullptr);
	ASSERT0(rhs.name_str() != nullptr);
	return ParamHash()(lhs.name_str(), rhs.name_str());
}
// equal_to:
bool ParamHash::operator()( const char * lhs, const char * rhs ) const noexcept {
	ASSERT0(lhs != nullptr);
	ASSERT0(rhs != nullptr);
	for ( ; *lhs ; lhs++, rhs++) {
		uint32_t c = std::toupper(static_cast<unsigned char>(*lhs));
		if (c == '-')
			c = '_';
		uint32_t d = std::toupper(static_cast<unsigned char>(*rhs));
		if (d == '-')
			d = '_';
		if (c != d)
			return false;
	}
	ASSERT0(*lhs == 0);
	return *rhs == 0;
}

// compare as a-less-b? for purposes of std::sort et al:
bool ParamComparer::operator()( const Param& lhs, const Param& rhs ) const {
	return ParamComparer()(lhs.name_str(), rhs.name_str());
}
// compare as a-less-b? for purposes of std::sort et al:
bool ParamComparer::operator()( const char * lhs, const char * rhs ) const {
	ASSERT0(lhs != nullptr);
	ASSERT0(rhs != nullptr);
	for ( ; *lhs ; lhs++, rhs++) {
		uint32_t c = std::toupper(static_cast<unsigned char>(*lhs));
		if (c == '-')
			c = '_';
		uint32_t d = std::toupper(static_cast<unsigned char>(*rhs));
		if (d == '-')
			d = '_';
		// long names come before short names; otherwise sort A->Z
		if (c != d)
			return d == 0 ? true : (c < d);
	}
	ASSERT0(*lhs == 0);
	return *rhs == 0;
}


#ifndef NDEBUG
static void check_and_report_name_collisions(const char *name, const ParamsHashTableType &table) {
	if (table.contains(name)) {
		throw new std::logic_error(fmt::format("tesseract param name '{}' colliion: double definition of param '{}'", name));
	}
}
static void check_and_report_name_collisions(const char *name, std::vector<ParamPtr> &table) {
	for (Param *p : table) {
		if (ParamHash()(p->name_str(), name)) {
			throw new std::logic_error(fmt::format("tesseract param name '{}' colliion: double definition of param '{}'", name));
		}
	}
}
#else
#define check_and_report_name_collisions(name, table)     ((void)0)
#endif


ParamsVector::~ParamsVector() {
	params_.clear();
}

ParamsVector::ParamsVector(const char *title) :
	title_(title)
{
	params_.reserve(256);
}

ParamsVector::ParamsVector(const char *title, std::initializer_list<ParamRef> vecs) :
	title_(title) 
{
	params_.reserve(256);

	for (ParamRef i : vecs) {
		add(i);
	}
}

void ParamsVector::add(ParamPtr param_ref) {
	check_and_report_name_collisions(param_ref->name_str(), params_);
	params_.insert({param_ref->name_str(), param_ref});
}

void ParamsVector::add(Param &param_ref) {
	add(&param_ref);
}

void ParamsVector::add(std::initializer_list<ParamRef> vecs) {
	for (ParamRef i : vecs) {
		add(i);
	}
}

void ParamsVector::remove(ParamPtr param_ref) {
	params_.erase(param_ref->name_str());
}

void ParamsVector::remove(ParamRef param_ref) {
	remove(&param_ref);
}


Param *ParamsVector::find(
	const char *name, 
	SetParamConstraint constraint,
	ParamType accepted_types_mask
) const {
	auto l = params_.find(name);
	if (l != params_.end()) {
		ParamPtr p = (*l).second;
		if (p->constraint_ok(constraint) && (p->type() & accepted_types_mask) != 0) {
			return p;
		}
	}
	return nullptr;
}


template <>
IntParam *ParamsVector::find<IntParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<IntParam *>(find(name, constraint, INT_PARAM));
}

template <>
BoolParam *ParamsVector::find<BoolParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<BoolParam *>(find(name, constraint, BOOL_PARAM));
}

template <>
DoubleParam *ParamsVector::find<DoubleParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<DoubleParam *>(find(name, constraint, DOUBLE_PARAM));
}

template <>
StringParam *ParamsVector::find<StringParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<StringParam *>(find(name, constraint, STRING_PARAM));
}


std::vector<ParamPtr> ParamsVector::as_list(		
	SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
	ParamType accepted_types_mask = ANY_TYPE_PARAM
) const {
	std::vector<ParamPtr> lst;
	for (auto i : params_) {
		ParamPtr p = i.second;
		if (p->constraint_ok(constraint) && (p->type() & accepted_types_mask) != 0) {
			lst.push_back(p);
		}
	}
	return lst;
}


ParamsVectors::~ParamsVectors() {
	collection_.clear();
}

ParamsVectors::ParamsVectors() {
}

ParamsVectors::ParamsVectors(std::initializer_list<ParamsVector &> vecs) {
	for (ParamsVector i : vecs) {
		add(i);
	}
}

void ParamsVectors::add(ParamsVector &vec_ref) {
	collection_.push_back(&vec_ref);
}

void ParamsVectors::add(std::initializer_list<ParamsVector &> vecs) {
	for (ParamsVector &i : vecs) {
		add(i);
	}
}

Param *ParamsVectors::find(
	const char *name, 
	SetParamConstraint constraint,
	ParamType accepted_types_mask
) const {
	for (ParamsVector *vec : collection_) {
		auto l = vec->params_.find(name);
		if (l != vec->params_.end()) {
			ParamPtr p = (*l).second;
			if (p->constraint_ok(constraint) && (p->type() & accepted_types_mask) != 0) {
				return p;
			}
		}
	}
	return nullptr;
}

template <>
IntParam *ParamsVectors::find<IntParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<IntParam *>(find(name, constraint, INT_PARAM));
}

template <>
BoolParam *ParamsVectors::find<BoolParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<BoolParam *>(find(name, constraint, BOOL_PARAM));
}

template <>
DoubleParam *ParamsVectors::find<DoubleParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<DoubleParam *>(find(name, constraint, DOUBLE_PARAM));
}

template <>
StringParam *ParamsVectors::find<StringParam>(
	const char *name, 
	SetParamConstraint constraint
) const {
	return static_cast<StringParam *>(find(name, constraint, STRING_PARAM));
}

std::vector<ParamPtr> ParamsVectors::as_list(		
	SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
	ParamType accepted_types_mask = ANY_TYPE_PARAM
) const {
	std::vector<ParamPtr> lst;
	for (ParamsVector *vec : collection_) {
		for (auto i : vec->params_) {
			ParamPtr p = i.second;
			if (p->constraint_ok(constraint) && (p->type() & accepted_types_mask) != 0) {
				lst.push_back(p);
			}
		}
	}
	return lst;
}


bool Param::set_value(const ParamValueContainer &v, ParamSetBySourceType source_type, ParamPtr source) {
	if (const int32_t* val = std::get_if<int32_t>(&v)) 
		return set_value(*val, source_type, source);
	else if (const bool* val = std::get_if<bool>(&v)) 
		return set_value(*val, source_type, source);
	else if (const double* val = std::get_if<double>(&v)) 
		return set_value(*val, source_type, source);
	else if (const std::string* val = std::get_if<std::string>(&v)) 
		return set_value(*val, source_type, source);
	else
		throw new std::logic_error(fmt::format("tesseract param '{}' error: failed to get value from variant input arg", name_));
}


bool IntParam::set_value(int32_t value, ParamSetBySourceType source_type, ParamPtr source) {
	access_counts_.writing++;
	if (value != value_ && value != default_)
		access_counts_.changing++;

	if (!!on_modify_f_) {
		ParamValueContainer old(value_);
		ParamValueContainer now(value);

		value_ = value;

		on_modify_f_(name_, *this, source_type, source, old, now);
	}
	else {
		value_ = value;
	}
}

bool IntParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
	int32_t val = atoi(v);
	return set_value(val, source_type, source);
}
bool IntParam::set_value(bool v, ParamSetBySourceType source_type, ParamPtr source) {
	int32_t val = v;
	return set_value(val, source_type, source);
}
bool IntParam::set_value(double v, ParamSetBySourceType source_type, ParamPtr source) {
	if (v < INT32_MIN || v > INT32_MAX)
		return false;

	int32_t val = roundf(v);
	return set_value(val, source_type, source);
}


bool ParamUtils::ReadParamsFile(const char *file, SetParamConstraint constraint,
                                ParamsVectors *member_params) {
  TFile fp;
  if (!fp.Open(file, nullptr)) {
    tprintError("read_params_file: Can't open/read file {}\n", file);
    return true;
  }
  return ReadParamsFromFp(&fp, constraint, member_params);
}

bool ParamUtils::ReadParamsFromFp(TFile *fp,
								  SetParamConstraint constraint, 
                                  ParamsVectors *member_params) {
#define LINE_SIZE 4096
  char line[LINE_SIZE]; // input line
  bool anyerr = false;  // true if any error
  bool foundit;         // found parameter
  char *nameptr;        // name field
  char *valptr;         // value field

  while (fp->FGets(line, LINE_SIZE) != nullptr) {
	  // trimRight:
	  for (nameptr = line + strlen(line) - 1; nameptr >= line && std::isspace(*nameptr); nameptr--) {
		  ;
	  }
	  nameptr[1] = 0;
	  // trimLeft:
	  for (nameptr = line; *nameptr && std::isspace(*nameptr); nameptr++) {
		  ;
	  }
	if (line[0] && line[0] != '#') {
      // jump over variable name
      for (valptr = line; *valptr && std::isspace(*valptr); valptr++) {
        ;
      }
      if (*valptr) {    // found blank
        *valptr = '\0'; // make name a string
        do {
          valptr++; // find end of blanks
        } while (std::isspace(*valptr));
      }
      foundit = SetParam(line, valptr, constraint, member_params);

      if (!foundit) {
        anyerr = true; // had an error
        tprintError("Parameter not found: {}\n", line);
      }
    }
  }
  return anyerr;
}





// permanent lookup table:
std::vector<std::string> ReportFile::_processed_file_paths;


ReportFile::ReportFile(const char *path)
{
	if (!path || !*path) {
		_f = nullptr;
		return;
	}

	_f = nullptr;

	if (strieq(path, "/dev/stdout") || strieq(path, "stdout") || strieq(path, "-") || strieq(path, "1"))
		_f = stdout;
	else if (strieq(path, "/dev/stderr") || strieq(path, "stderr") || strieq(path, "+") || strieq(path, "2"))
		_f = stderr;
	else {
		bool first = true;
		for (std::string &i : _processed_file_paths) {
			if (strieq(i.c_str(), path)) {
				first = false;
				break;
			}
		}
		_f = fopenUtf8(path, first ? "w" : "a");
		if (!_f) {
			tprintError("Cannot produce parameter usage report file: '{}'\n", path);
		}
		else if (first) {
			_processed_file_paths.push_back(path);
		}
	}
}

ReportFile::~ReportFile() {
	if (_f) {
		if (_f != stdout && _f != stderr) {
			fclose(_f);
		} else {
			fflush(_f);
		}
	}
}

FILE * ReportFile::operator()() const {
	return _f;
}





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


// When `section_title` is NULL, this will report the lump sum parameter usage for the entire run.
// When `section_title` is NOT NULL, this will only report the parameters that were actually used (R/W) during the last section of the run, i.e.
// since the previous invocation of this reporting method (or when it hasn't been called before: the start of the application).
void ParamUtils::ReportParamsUsageStatistics(FILE *f, const ParamsVectors *member_params, const char *section_title)
{
  bool is_section_subreport = (section_title != nullptr);

  std::unique_ptr<ParamsReportWriter> writer;

  if (f != nullptr) {
    writer.reset(new ParamsReportFileDuoWriter(f));
  } else {
    writer.reset(new ParamsReportDefaultWriter());
  }

  writer->Write(fmt::format("\n\nTesseract Parameter Usage Statistics{}: which params have been relevant?\n"
                            "----------------------------------------------------------------------\n\n",
                            (section_title != nullptr ? fmt::format(" for section: {}", section_title) : "")));

  // first collect all parameters and sort them according to these criteria:
  // - global / (class)local
  // - name

  const ParamsVectors* globals = GlobalParams();

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
	  	fprintf(stderr, "Apparently you have double-defined Tesseract Variable: '%s'! Fix that in the source code!\n", a.p->name_str());
	    ASSERT0(!"Apparently you have double-defined a Tesseract Variable.");
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
IntParam *FindParam(const char *name, ParamsVectors *globals, ParamsVectors *locals, const IntParam *DUMMY, ParamType accepted_types_mask) {
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


bool ParamUtils::SetParam(const char *name, const char *value, SetParamConstraint constraint,
                          ParamsVectors *member_params) {
  // Look for the parameter among string parameters.
  auto *sp = FindParam<StringParam>(name, GlobalParams()->string_params(), member_params->string_params());
  if (sp != nullptr && sp->constraint_ok(constraint)) {
    sp->set_value(value);
  }
  if (*value == '\0') {
    return (sp != nullptr);
  }

  // Look for the parameter among int parameters.
  auto *ip = FindParam<IntParam>(name, GlobalParams()->int_params(), member_params->int_params());
  if (ip && ip->constraint_ok(constraint)) {
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
  auto *bp = FindParam<BoolParam>(name, GlobalParams()->bool_params(), member_params->bool_params());
  if (bp != nullptr && bp->constraint_ok(constraint)) {
    if (*value == 'T' || *value == 't' || *value == 'Y' || *value == 'y' || *value == '1') {
      bp->set_value(true);
    } else if (*value == 'F' || *value == 'f' || *value == 'N' || *value == 'n' || *value == '0') {
      bp->set_value(false);
    }
  }

  // Look for the parameter among double parameters.
  auto *dp = FindParam<DoubleParam>(name, GlobalParams(), member_params);
  if (dp != nullptr && dp->constraint_ok(constraint)) {
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

bool ParamUtils::GetParamAsString(const char *name, const ParamsVectors *member_params,
                                  std::string *value) {
  // Look for the parameter among string parameters.
  auto *sp =
      FindParam<StringParam>(name, GlobalParams()->string_params_c(), member_params->string_params_c());
  if (sp) {
    *value = sp->c_str();
    return true;
  }
  // Look for the parameter among int parameters.
  auto *ip = FindParam<IntParam>(name, GlobalParams()->int_params_c(), member_params->int_params_c());
  if (ip) {
    *value = std::to_string(int32_t(*ip));
    return true;
  }
  // Look for the parameter among bool parameters.
  auto *bp = FindParam<BoolParam>(name, GlobalParams()->bool_params_c(), member_params->bool_params_c());
  if (bp != nullptr) {
    *value = bool(*bp) ? "1" : "0";
    return true;
  }
  // Look for the parameter among double parameters.
  auto *dp =
      FindParam<DoubleParam>(name, GlobalParams()->double_params_c(), member_params->double_params_c());
  if (dp != nullptr) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << double(*dp);
    *value = stream.str();
    return true;
  }
  return false;
}

void ParamUtils::PrintParams(FILE *fp, const ParamsVectors *member_params, bool print_info) {
  int num_iterations = (member_params == nullptr) ? 1 : 2;
  // When printing to stdout info text is included.
  // Info text is omitted when printing to a file (would result in an invalid config file).
  if (!fp)
	  fp = stdout;
  bool printing_to_stdio = (fp == stdout || fp == stderr);
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  for (int v = 0; v < num_iterations; ++v) {
    const ParamsVectors *vec = (v == 0) ? GlobalParams() : member_params;
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
void ParamUtils::ResetToDefaults(ParamsVectors *member_params) {
	for (Param *param : GlobalParams()->as_list()) {
		param->ResetToDefault();
	}
	if (member_params != nullptr) {
		for (Param *param : member_params->as_list()) {
			param->ResetToDefault();
		}
	}
}

// Find the flag name in the list of global flags.
// int32_t flag
int32_t int_val;
if (IntFlagExists(lhs.c_str(), &int_val)) {
	if (rhs != nullptr) {
		if (!strlen(rhs)) {
			// Bad input of the format --int_flag=
			tprintError("Bad argument: {}\n", (*argv)[i]);
			exit(1);
		}
		if (!SafeAtoi(rhs, &int_val)) {
			tprintError("Could not parse int from {} in flag {}\n", rhs, (*argv)[i]);
			exit(1);
		}
	} else {
		// We need to parse the next argument
		if (i + 1 >= *argc) {
			tprintError("Could not find value argument for flag {}\n", lhs.c_str());
			exit(1);
		} else {
			++i;
			if (!SafeAtoi((*argv)[i], &int_val)) {
				tprintError("Could not parse int32_t from {}\n", (*argv)[i]);
				exit(1);
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
			exit(1);
		}
		if (!SafeAtod(rhs, &double_val)) {
			tprintError("Could not parse double from {} in flag {}\n", rhs, (*argv)[i]);
			exit(1);
		}
	} else {
		// We need to parse the next argument
		if (i + 1 >= *argc) {
			tprintError("Could not find value argument for flag {}\n", lhs.c_str());
			exit(1);
		} else {
			++i;
			if (!SafeAtod((*argv)[i], &double_val)) {
				tprintError("Could not parse double from {}\n", (*argv)[i]);
				exit(1);
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
			exit(1);
		}
		if (!strcmp(rhs, "false") || !strcmp(rhs, "0")) {
			bool_val = false;
		} else if (!strcmp(rhs, "true") || !strcmp(rhs, "1")) {
			bool_val = true;
		} else {
			tprintError("Could not parse bool from flag {}\n", (*argv)[i]);
			exit(1);
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
			exit(1);
		} else {
			string_val = (*argv)[++i];
		}
	}
	SetStringFlagValue(lhs.c_str(), string_val);
	continue;
}

} // namespace tesseract
