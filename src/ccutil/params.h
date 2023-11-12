/**********************************************************************
 * File:        params.h
 * Description: Class definitions of the *_VAR classes for tunable constants.
 * Author:      Ray Smith
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

#ifndef PARAMS_H
#define PARAMS_H

#include <tesseract/export.h> // for TESS_API
#include "tprintf.h"          // for printf (when debugging this code)
#include "mupdf/assertions.h" // for ASSERT

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <variant>
#include <functional>
#include <unordered_map>


namespace tesseract {

class Param;
class IntParam;
class BoolParam;
class StringParam;
class DoubleParam;
class TFile;


enum ParamType {
	UNKNOWN_PARAM = 0,

	INT_PARAM = 1,
	BOOL_PARAM = 2,
	DOUBLE_PARAM = 4,
	STRING_PARAM = 8,

	ANY_TYPE_PARAM = 15,
};

enum ParamSetBySourceType {
	PARAM_VALUE_IS_DEFAULT = 0,

	PARAM_VALUE_IS_RESET,
	PARAM_VALUE_IS_SET_BY_ASSIGN,			// 'indirect' write: value is copied over from elsewhere via operator=.
	PARAM_VALUE_IS_SET_BY_PARAM,			// 'indirect' write: other Param's OnChange code set the param value, whatever it is now.
	PARAM_VALUE_IS_SET_BY_APPLICATION,		// 'explicit' write: user / application code set the param value, whatever it is now.
};

// Enum for constraints on what kind of params should be set by SetParam().
enum SetParamConstraint {
  SET_PARAM_CONSTRAINT_NONE,
  SET_PARAM_CONSTRAINT_DEBUG_ONLY,
  SET_PARAM_CONSTRAINT_NON_DEBUG_ONLY,
  SET_PARAM_CONSTRAINT_INIT_ONLY,
  SET_PARAM_CONSTRAINT_NON_INIT_ONLY,
};


// Custom equivalent of std::hash<Param> + std::equal_to<Param> for std::unordered_map<const char *key, Param & value>.
class ParamHash
{
public:
	// hash:
	std::size_t operator()(const Param& s) const noexcept;
	std::size_t operator()(const char * s) const noexcept;

	// equal_to:
	bool operator()( const Param& lhs, const Param& rhs ) const noexcept;
	// equal_to:
	bool operator()( const char * lhs, const char * rhs ) const noexcept;
};

// Custom implementation of class Compare for std::sort.
class ParamComparer
{
public:
	// compare as a-less-b? for purposes of std::sort et al:
	bool operator()( const Param& lhs, const Param& rhs ) const;
	// compare as a-less-b? for purposes of std::sort et al:
	bool operator()( const char * lhs, const char * rhs ) const;
};

typedef Param & ParamRef;
typedef Param * ParamPtr;


// reduce noise by using macros to help set up the member prototypes

#define SOURCE_TYPE																\
		ParamSetBySourceType source_type = PARAM_VALUE_IS_SET_BY_APPLICATION

#define SOURCE_REF																\
		ParamSetBySourceType source_type = PARAM_VALUE_IS_SET_BY_APPLICATION,		\
		ParamPtr source = nullptr


typedef std::unordered_map<
	const char * /* key */, 
	ParamPtr /* value */, 
	ParamHash /* hash value calc */, 
	ParamHash /* equality check */
> ParamsHashTableType;


class ParamsVector {
private:
	ParamsHashTableType params_;
	std::string title_;

public:
	ParamsVector(const char *title = nullptr);
	ParamsVector(const char *title, std::initializer_list<ParamRef> vecs);
	
	~ParamsVector();

	void add(ParamPtr param_ref);
	void add(ParamRef param_ref);
	void add(std::initializer_list<ParamRef> vecs);

	void remove(ParamPtr param_ref);
	void remove(ParamRef param_ref);

	const char *title() const { 
		return title_.c_str(); 
	}
	void change_title(const char *title) {
		title_ = title ? title : "";
	}

	ParamPtr find(
		const char *name, 
		SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
		ParamType accepted_types_mask = ANY_TYPE_PARAM
	) const;

	template <class T>
	T *find(
		const char *name, 
		SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE
	) const;

	std::vector<ParamPtr> as_list(		
		SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
		ParamType accepted_types_mask = ANY_TYPE_PARAM
	) const;

	friend class ParamsVectors;
};


// an (ad-hoc?) collection of ParamsVector instances.
class ParamsVectors {
private:
	std::vector<ParamsVector *> collection_;

public:
	ParamsVectors();
	ParamsVectors(std::initializer_list<ParamsVector &> vecs);
	
	~ParamsVectors();

	void add(ParamsVector &vec_ref);
	void add(std::initializer_list<ParamsVector &> vecs);

	ParamPtr find(
		const char *name, 
		SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
		ParamType accepted_types_mask = ANY_TYPE_PARAM
	) const;

	template <class T>
	T *find(
		const char *name, 
		SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE
	) const;

	std::vector<ParamPtr> as_list(		
		SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
		ParamType accepted_types_mask = ANY_TYPE_PARAM
	) const;
};


// Utility functions for working with Tesseract parameters.
class TESS_API ParamUtils {
public:
  // Reads a file of parameter definitions and set/modify the values therein.
  // If the filename begins with a `+` or `-`, the Variables will be
  // ORed or ANDed with any current values.
  // 
  // Blank lines and lines beginning # are ignored.
  // 
  // Variable names are followed by one of more whitespace characters,
  // followed by the Value, which spans the rest of line.
  //
  // Any Variables listed in the file, which do not match the given
  // constraint are ignored, but are reported via `tprintf()` as ignored,
  // unless you set `quietly_ignore`.
  static bool ReadParamsFile(const char *file, // filename to read
	  ParamsVectors *set = nullptr, 
	  SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
	  SOURCE_REF,
	  bool quietly_ignore = false
  );

  // Read parameters from the given file pointer.
  // Otherwise identical to ReadParamsFile().
  static bool ReadParamsFromFp(TFile *fp,
	  ParamsVectors *set = nullptr, 
	  SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
	  SOURCE_REF,
	  bool quietly_ignore = false
  );

  // Set a parameter to have the given value.
  //
  // A Variable, which does not match the given constraint, is ignored, 
  // but is reported via `tprintf()` as ignored,
  // unless you set `quietly_ignore`.
  static bool SetParam(
	  const char *name, const char *value, 
	  ParamsVectors *set = nullptr, 
	  SetParamConstraint constraint = SET_PARAM_CONSTRAINT_NONE,
	  SOURCE_REF,
	  bool quietly_ignore = false
  );

  // Produces a pointer (reference) to the parameter with the given name (of the
  // appropriate type) if it was found in any of the given vectors.
  // When `set` is empty, the `GlobalParams()` vector will be assumed 
  // instead.
  //
  // Returns nullptr when param is not known. Prints a message via `tprintf()`
  // to report this fact, unless you set `quietly_ignore`.
  template <class T>
  static T *FindParam(
	  const char *name, 
	  const ParamsVectors *set = nullptr, 
	  ParamType accepted_types_mask = ANY_TYPE_PARAM,
	  bool quietly_ignore = false
  );

  // Fetches the value of the named param as a string. Returns false if not
  // found. Prints a message via `tprintf()` to report this fact (see also `FindParam()`).
  //
  // When `set` is empty, the `GlobalParams()` vector will be assumed instead.
  static bool GetParamAsString(
	  std::string *value_ref, const char *name, 
	  const ParamsVectors *set = nullptr, 
	  ParamType accepted_types_mask = ANY_TYPE_PARAM,
	  bool quietly_ignore = false
  );

  // Print parameters to the given file.
  static void PrintParams(FILE *fp, const ParamsVectors *set = nullptr, bool print_info = true);

  // Report parameters' usage statistics, i.e. report which params have been
  // set, modified and read/checked until now during this run-time's lifetime.
  //
  // Use this method for run-time 'discovery' about which tesseract parameters
  // are actually *used* during your particular usage of the library, ergo
  // answering the question:
  // "Which of all those parameters are actually *relevant* to my use case today?"
  // 
  // When `section_title` is NULL, this will report the lump sum parameter usage
  // for the entire run. When `section_title` is NOT NULL, this will only report
  // the parameters that were actually used (R/W) during the last section of the
  // run, i.e. since the previous invocation of this reporting method (or when
  // it hasn't been called before: the start of the application).
  //
  // Unless `f` is stdout/stderr, this method reports via `tprintf()` as well. 
  // When `f` is a valid handle, then the report is written to the given FILE, 
  // which may be stdout/stderr.
  //
  // When `set` is empty, the `GlobalParams()` vector will be assumed instead.
  static void ReportParamsUsageStatistics(FILE *fp, const ParamsVectors *set = nullptr, const char *section_title = nullptr);

  // Resets all parameters back to default values;
  static void ResetToDefaults(ParamsVectors *set, SOURCE_TYPE);

  // Parse '-', 'stdout' and '1' as STDIN, '+', 'stderr', and '2' as STDERR, or open a regular text file in UTF8 write mode.
  //
  // MAY return NULL, e.g. when path is empty. This is considered a legal/valid use & behaviour.
  // On the other hand, an error line is printed via `tprintf()` when the given path is non-empty and
  // turns out not to be valid.
  // Either way, a return value of NULL implies that default behaviour (output via `tprintf()`) is assumed.
  static FILE* OpenReportFile(const char *path);
};


// The very first time we call this one during the current run, we CREATE/OVERWRITE the target file.
// Iff we happen to invoke this call multiple times for the same target file, any subsequent call
// will APPEND to the target file.
//
// NOTE: as we need to track the given file path, this will incur a very minor heap memory leak
// as we won't ever release the memory allocated for that string in `_processed_file_paths`.
class ReportFile {
public:
	ReportFile(const char *path);
	ReportFile(const std::string &path) : ReportFile(path.c_str()) {}
	~ReportFile();

	FILE * operator()() const;

private:
	FILE *_f;

	// We assume we won't be processing very many file paths through here, so a linear scan through
	// the set-processed-to-date is considered good enough/best re performance, hence std::vector 
	// sufices for the tracking list.
	static std::vector<std::string> _processed_file_paths;
};


class Param;

typedef std::variant<bool, int, double, std::string> ParamValueContainer;

typedef std::function<void (
	const char * /* name */, 
	ParamRef /* target */, 
	ParamSetBySourceType /* source type */,
    ParamPtr /* optional setter/parent */, 
	ParamValueContainer & /* old value */, 
	ParamValueContainer & /* new value */
)> ParamOnModifyFunction;


// Definition of various parameter types.
class Param {
public:
  ~Param() = default;

  const char *name_str() const {
    return name_;
  }
  const char *info_str() const {
    return info_;
  }
  bool is_init() const {
    return init_;
  }
  bool is_debug() const {
    return debug_;
  }
  bool constraint_ok(SetParamConstraint constraint) const {
    return (constraint == SET_PARAM_CONSTRAINT_NONE ||
            (constraint == SET_PARAM_CONSTRAINT_DEBUG_ONLY && this->is_debug()) ||
            (constraint == SET_PARAM_CONSTRAINT_NON_DEBUG_ONLY && !this->is_debug()) ||
            (constraint == SET_PARAM_CONSTRAINT_INIT_ONLY && this->is_init()) ||
            (constraint == SET_PARAM_CONSTRAINT_NON_INIT_ONLY && !this->is_init()));
  }

  ParamSetBySourceType set_mode() const {
	  return set_mode_;
  }
  Param *is_set_by() const {
	  return setter_;
  }

  // We track Param/Variable setup/changes/usage through this administrative struct.
  // It helps us to diagnose and report which tesseract Params (Variables) are actually
  // USED in which program section and in the program as a whole, while we can also 
  // diagnose which variables have been set up during this session, and by *whom*, as
  // some Params will have been modified due to others having been set, e.g. `debug_all`.
  typedef struct access_counts {
    // the current section's counts
    int reading;  
    int writing;
	int changing;

    // the sum of the previous section's counts
    int prev_sum_reading;
    int prev_sum_writing;
	int prev_sum_changing;
  } access_counts_t;

  access_counts_t access_counts() const {
    return access_counts_;
  }

  void reset_access_counts() {
    access_counts_.prev_sum_reading += access_counts_.reading;
    access_counts_.prev_sum_writing += access_counts_.writing;
	access_counts_.prev_sum_changing += access_counts_.changing;

    access_counts_.reading = 0;
    access_counts_.writing = 0;
	access_counts_.changing = 0;
  }

  virtual std::string formatted_value_str() const = 0;

  virtual bool set_value(const char *v, SOURCE_REF) = 0;
  virtual bool set_value(int32_t v, SOURCE_REF) = 0;
  virtual bool set_value(bool v, SOURCE_REF) = 0;
  virtual bool set_value(double v, SOURCE_REF) = 0;
  bool set_value(const ParamValueContainer &v, SOURCE_REF);
  bool set_value(const std::string &v, SOURCE_REF) {
	  return set_value(v.c_str(), source_type, source);
  }

  virtual void ResetToDefault(SOURCE_TYPE) = 0;
  virtual void ResetFrom(const ParamsVectors &vec, SOURCE_TYPE) = 0;

  Param(const Param &o) = delete;
  Param(Param &&o) = delete;

  Param &operator=(const Param &other) = delete;
  Param &operator=(Param &&other) = delete;

  ParamType type() const {
	  return type_;
  }

protected:
  Param(const char *name, const char *comment, bool init, ParamOnModifyFunction on_modify_f = 0) : 
	  name_(name), 
	  info_(comment), 
	  init_(init), 
	  type_(UNKNOWN_PARAM), 
	  default_(true),
	  set_mode_(PARAM_VALUE_IS_DEFAULT),
	  setter_(nullptr),
	  on_modify_f_(on_modify_f),
	  access_counts_({0,0,0})
  {
    debug_ = (strstr(name, "debug") != nullptr) || (strstr(name, "display") != nullptr);
  }

protected:
  const char *name_; // name of this parameter
  const char *info_; // for menus

  ParamOnModifyFunction on_modify_f_;
  
  ParamValueContainer value_;
  ParamValueContainer default_;
  ParamType type_;

  ParamSetBySourceType set_mode_;
  Param *setter_;

  bool init_;        // needs to be set before init
  bool debug_;
  
  mutable access_counts_t access_counts_;
};


class IntParam : public Param {
public:
  IntParam(int32_t value, const char *name, const char *comment, bool init, ParamOnModifyFunction on_modify_f = 0)
      : Param(name, comment, init, on_modify_f) {
    value_ = value;
    default_ = value;
	type_ = INT_PARAM;
    access_counts_.writing++;
  }
  ~IntParam() = default;

  operator int32_t() const {
      access_counts_.reading++;
      return value_;
  }
  void operator=(int32_t value) {
	  set_value(value, PARAM_VALUE_IS_SET_BY_ASSIGN);
  }

  virtual bool set_value(const char *v, SOURCE_REF);
  virtual bool set_value(int32_t v, SOURCE_REF);
  virtual bool set_value(bool v, SOURCE_REF);
  virtual bool set_value(double v, SOURCE_REF);

  int32_t value() const {
	  access_counts_.reading++;
	  return value_;
  }
  virtual void ResetToDefault(SOURCE_TYPE) {
	  set_value(default_, source_type);
  }
  virtual void ResetFrom(const ParamsVectors *vec, SOURCE_TYPE) {
	IntParam *param = vec->find<IntParam>(name_);
    if (param) {
	  set_value(*param, source_type);
    }
	else {
	  ResetToDefault(source_type);
	}
  }

  virtual std::string formatted_value_str() const override {
    return std::to_string(value_);
  }

  IntParam(const IntParam &o) = delete;
  IntParam(IntParam &&o) = delete;

  IntParam &operator=(const IntParam &other) = delete;
  IntParam &operator=(IntParam &&other) = delete;

private:
  int32_t value_;
  int32_t default_;
};

class BoolParam : public Param {
public:
  BoolParam(bool value, const char *name, const char *comment, bool init, ParamsVectors *vec)
      : Param(name, comment, init) {
    value_ = value;
    default_ = value;
	type_ = BOOL_PARAM;
	access_counts_.writing++;
  }
  ~BoolParam() {
  }
  operator bool() const {
      access_counts_.reading++;
      return value_;
  }
  void operator=(bool value) {
      access_counts_.writing++;
	  if (value != value_ && value != default_)
		  access_counts_.changing++;
	  value_ = value;
  }

  virtual bool set_value(const char *v, SOURCE_REF);
  virtual bool set_value(int32_t v, SOURCE_REF);
  virtual bool set_value(bool v, SOURCE_REF);
  virtual bool set_value(double v, SOURCE_REF);

  void set_value(bool value) {
      access_counts_.writing++;
	  if (value != value_ && value != default_)
		  access_counts_.changing++;
	  value_ = value;
  }
  bool value() const {
	  access_counts_.reading++;
	  return value_;
  }
  void ResetToDefault() {
      access_counts_.writing++;
      value_ = default_;
  }
  void ResetFrom(const ParamsVectors *vec) {
    auto *param = vec->find(name_);
    if (param) {
#if !defined(NDEBUG)
        ::tesseract::tprintf("overriding param {}={} by ={}\n", name_, formatted_value_str(), (*param).formatted_value_str());
#endif
        access_counts_.writing++;
		ASSERT0(param->type() == BOOL_PARAM);
		BoolParam *p = static_cast<BoolParam *>(param);
		bool value = *p;
		if (value != value_ && value != default_)
			access_counts_.changing++;
		value_ = value;
    }
  }

  virtual std::string formatted_value_str() const override {
    return value_ ? "true" : "false";
  }

  BoolParam(const BoolParam &o) = delete;
  BoolParam(BoolParam &&o) = delete;

  BoolParam &operator=(const BoolParam &other) = delete;
  BoolParam &operator=(BoolParam &&other) = delete;

private:
  bool value_;
  bool default_;

  // Pointer to the vector that contains this param (not owned by this class).
  ParamsVectors *params_vec_;
};

class StringParam : public Param {
public:
  StringParam(const char *value, const char *name, const char *comment, bool init,
              ParamsVectors *vec)
      : Param(name, comment, init) {
    value_ = value;
    default_ = value;
	type_ = STRING_PARAM;
	access_counts_.writing++;
  }
  ~StringParam() {
  }
  operator std::string &() {
      access_counts_.reading++;
      return value_;
  }
  const char *c_str() const {
      access_counts_.reading++;
      return value_.c_str();
  }
  bool contains(char c) const {
      access_counts_.reading++;
      return value_.find(c) != std::string::npos;
  }
  bool empty() const {
      access_counts_.reading++;
      return value_.empty();
  }
  bool operator==(const std::string &other) const {
      access_counts_.reading++;
      return value_ == other;
  }
  void operator=(const std::string &value) {
      access_counts_.writing++;
	  if (value != value_ && value != default_)
		  access_counts_.changing++;
	  value_ = value;
  }

  virtual bool set_value(const char *v, SOURCE_REF);
  virtual bool set_value(int32_t v, SOURCE_REF);
  virtual bool set_value(bool v, SOURCE_REF);
  virtual bool set_value(double v, SOURCE_REF);

  void set_value(const std::string &value) {
      access_counts_.writing++;
	  if (value != value_ && value != default_)
		  access_counts_.changing++;
	  value_ = value;
  }
  const std::string &value() const {
	  access_counts_.reading++;
	  return value_;
  }
  void ResetToDefault() {
      access_counts_.writing++;
      value_ = default_;
  }
  void ResetFrom(const ParamsVectors *vec) {
    auto *param = vec->find(name_);
    if (param) {
#if !defined(NDEBUG) && 0
        ::tesseract::tprintf("overriding param {}={} by ={}\n", name_, formatted_value_str(), (*param).formatted_value_str());
#endif
        access_counts_.writing++;
		ASSERT0(param->type() == STRING_PARAM);
		StringParam *p = static_cast<StringParam *>(param);
		std::string value = *p;
		if (value != value_ && value != default_)
			access_counts_.changing++;
		value_ = value;
    }
  }

  virtual std::string formatted_value_str() const override {
    std::string rv = (const char *)u8"«";
    rv += value_;
    rv += (const char *)u8"»";
    return rv;
  }

  StringParam(const StringParam &o) = delete;
  StringParam(StringParam &&o) = delete;

  StringParam &operator=(const StringParam &other) = delete;
  StringParam &operator=(StringParam &&other) = delete;

private:
  std::string value_;
  std::string default_;

  // Pointer to the vector that contains this param (not owned by this class).
  ParamsVectors *params_vec_;
};

class DoubleParam : public Param {
public:
  DoubleParam(double value, const char *name, const char *comment, bool init, ParamsVectors *vec)
      : Param(name, comment, init) {
    value_ = value;
    default_ = value;
	type_ = DOUBLE_PARAM;
	access_counts_.writing++;
  }
  ~DoubleParam() {
  }
  operator double() const {
      access_counts_.reading++;
      return value_;
  }
  void operator=(double value) {
      access_counts_.writing++;
	  if (value != value_ && value != default_)
		  access_counts_.changing++;
	  value_ = value;
  }

  virtual bool set_value(const char *v, SOURCE_REF);
  virtual bool set_value(int32_t v, SOURCE_REF);
  virtual bool set_value(bool v, SOURCE_REF);
  virtual bool set_value(double v, SOURCE_REF);

  void set_value(double value) {
      access_counts_.writing++;
	  if (value != value_ && value != default_)
		  access_counts_.changing++;
	  value_ = value;
  }
  double value() const {
	  access_counts_.reading++;
	  return value_;
  }
  void ResetToDefault() {
      access_counts_.writing++;
      value_ = default_;
  }
  void ResetFrom(const ParamsVectors *vec) {
    auto *param = vec->find(name_);
    if (param) {
#if !defined(NDEBUG) && 0
        ::tesseract::tprintf("overriding param {}={} by ={}\n", name_, formatted_value_str(), (*param).formatted_value_str());
#endif
        access_counts_.writing++;
		ASSERT0(param->type() == DOUBLE_PARAM);
		DoubleParam *p = static_cast<DoubleParam *>(param);
		double value = *p;
		if (value != value_ && value != default_)
			access_counts_.changing++;
		value_ = value;
    }
  }

  virtual std::string formatted_value_str() const override {
#if 0
    return std::to_string(value_);   // always outputs %.6f format style values
#else
    char sbuf[40];
    snprintf(sbuf, sizeof(sbuf), "%1.f", value_);
    sbuf[39] = 0;
    return sbuf;
#endif
  }

  DoubleParam(const DoubleParam &o) = delete;
  DoubleParam(DoubleParam &&o) = delete;

  DoubleParam &operator=(const DoubleParam &other) = delete;
  DoubleParam &operator=(DoubleParam &&other) = delete;

private:
  double value_;
  double default_;

  // Pointer to the vector that contains this param (not owned by this class).
  ParamsVectors *params_vec_;
};


// remove the macros to help set up the member prototypes

#undef SOURCE_TYPE															
#undef SOURCE_REF																



// Global parameter lists.
//
// To avoid the problem of undetermined order of static initialization
// global_params are accessed through the GlobalParams function that
// initializes the static pointer to global_params only on the first time
// GlobalParams() is called.
//
// TODO(daria): remove GlobalParams() when all global Tesseract
// parameters are converted to members.
TESS_API
ParamsVectors *GlobalParams();


/*************************************************************************
 * Note on defining parameters.
 *
 * The values of the parameters defined with *_INIT_* macros are guaranteed
 * to be loaded from config files before Tesseract initialization is done
 * (there is no such guarantee for parameters defined with the other macros).
 *************************************************************************/

#define INT_VAR_H(name) ::tesseract::IntParam name

#define BOOL_VAR_H(name) ::tesseract::BoolParam name

#define STRING_VAR_H(name) ::tesseract::StringParam name

#define DOUBLE_VAR_H(name) ::tesseract::DoubleParam name

#define INT_VAR(name, val, comment) \
  ::tesseract::IntParam name(val, #name, comment, false, ::tesseract::GlobalParams())

#define BOOL_VAR(name, val, comment) \
  ::tesseract::BoolParam name(val, #name, comment, false, ::tesseract::GlobalParams())

#define STRING_VAR(name, val, comment) \
  ::tesseract::StringParam name(val, #name, comment, false, ::tesseract::GlobalParams())

#define DOUBLE_VAR(name, val, comment) \
  ::tesseract::DoubleParam name(val, #name, comment, false, ::tesseract::GlobalParams())

#define INT_MEMBER(name, val, comment, vec) name(val, #name, comment, false, vec)

#define BOOL_MEMBER(name, val, comment, vec) name(val, #name, comment, false, vec)

#define STRING_MEMBER(name, val, comment, vec) name(val, #name, comment, false, vec)

#define DOUBLE_MEMBER(name, val, comment, vec) name(val, #name, comment, false, vec)

#define INT_INIT_MEMBER(name, val, comment, vec) name(val, #name, comment, true, vec)

#define BOOL_INIT_MEMBER(name, val, comment, vec) name(val, #name, comment, true, vec)

#define STRING_INIT_MEMBER(name, val, comment, vec) name(val, #name, comment, true, vec)

#define DOUBLE_INIT_MEMBER(name, val, comment, vec) name(val, #name, comment, true, vec)

// ------------------------------------

extern BOOL_VAR_H(stream_filelist);
extern STRING_VAR_H(document_title);
#ifdef HAVE_LIBCURL
extern INT_VAR_H(curl_timeout);
extern STRING_VAR_H(curl_cookiefile);
#endif
extern INT_VAR_H(debug_all);
extern BOOL_VAR_H(debug_misc);
extern BOOL_VAR_H(verbose_process);
extern BOOL_VAR_H(scrollview_support);
extern STRING_VAR_H(vars_report_file);
extern BOOL_VAR_H(report_all_variables);
extern DOUBLE_VAR_H(allowed_image_memory_capacity);

} // namespace tesseract

#endif
