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

#include "helpers.h"  // for chomp_string, mupdf imports, etc.: see also the header collision comment in there (MSVC-specific).
#include "host.h"     // tesseract/export.h, windows.h for MAX_PATH
#include "serialis.h" // for TFile
#include "tprintf.h"
#include "fopenutf8.h"

#include <fmt/base.h>
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
#include <type_traits>

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
ParamsVector &GlobalParams() {
  static ParamsVector global_params("global"); // static auto-inits at startup
  return global_params;
}

// -- local helper functions --

static inline bool strieq(const char *s1, const char *s2) {
	return strcasecmp(s1, s2) == 0;
}

static inline bool SafeAtoi(const char* str, int* val) {
	char* endptr = nullptr;
	*val = strtol(str, &endptr, 10);
	return endptr != nullptr && *endptr == '\0';
}

static inline bool is_legal_fpval(double val) {
	return !std::isnan(val) && val != HUGE_VAL && val != -HUGE_VAL;
}

static inline bool is_single_word(const char* s) {
	if (!*s)
		return false;
	while (isalpha(*s))
		s++;
	while (isspace(*s))
		s++;
	return (!*s); // string must be at the end now...
}

static inline bool is_optional_whitespace(const char* s) {
  if (!*s)
    return false;
  while (isspace(*s))
    s++;
  return (!*s); // string must be at the end now...
}

using statistics_uint_t = decltype(Param::access_counts_t().reading);
using statistics_lumpsum_uint_t = decltype(Param::access_counts_t().prev_sum_reading);

// increment value, prevent overflow, a.k.a. wrap-around, i.e. clip to maximum value
template <class T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
static inline void safe_inc(T& sum) {
  sum++;
  // did a wrap-around just occur? if so, compensate by rewinding to max value.
  if (sum == T(0))
    sum--;
}

// add value to sum, prevent overflow, a.k.a. wrap-around, i.e. clip sum to maximum value
template <class SumT, class ValT, typename = std::enable_if_t<std::is_unsigned<SumT>::value && std::is_unsigned<ValT>::value>>
static inline void safe_add(SumT &sum, const ValT value) {
  // conditional check is shaped to work in overflow conditions
  if (sum < SumT(0) - 1 - value) // sum + value < max?  ==>  sum < max - value?
    sum += value;
  else                           // clip/limit ==>  sum = max
    sum = SumT(0) - 1;
}

// --- end of helper functions set ---

static ParamSetBySourceType default_source_type = PARAM_VALUE_IS_SET_BY_ASSIGN;

/**
 * The default application source_type starts out as PARAM_VALUE_IS_SET_BY_ASSIGN.
 * Discerning applications may want to set the default source type to PARAM_VALUE_IS_SET_BY_APPLICATION
 * or PARAM_VALUE_IS_SET_BY_CONFIGFILE, depending on where the main workflow is currently at,
 * while the major OCR tesseract APIs will set source type to PARAM_VALUE_IS_SET_BY_CORE_RUN
 * (if the larger, embedding, application hasn't already).
 *
 * The purpose here is to be able to provide improved diagnostics reports about *who* did *what* to
 * *which* parameters *when* exactly.
 */
void set_current_application_default_param_source_type(ParamSetBySourceType source_type) {
  default_source_type = source_type;
}

/**
 * Produces the current default application source type; intended to be used internally by our parameters support library code.
 */
ParamSetBySourceType get_current_application_default_param_source_type() {
  return default_source_type;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ParamHash
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ParamComparer
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    std::string s = fmt::format("{} param name '{}' collision: double definition of param '{}'", ParamUtils::GetApplicationName(), name, name);
		throw new std::logic_error(s);
	}
}
static void check_and_report_name_collisions(const char *name, std::vector<ParamPtr> &table) {
	for (Param *p : table) {
		if (ParamHash()(p->name_str(), name)) {
            std::string s = fmt::format("{} param name '{}' collision: double definition of param '{}'", ParamUtils::GetApplicationName(), name, name);
			throw new std::logic_error(s);
		}
	}
}
#else
#define check_and_report_name_collisions(name, table)     ((void)0)
#endif


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ParamsVector
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

ParamsVector::~ParamsVector() {
  if (is_params_owner_) {
    // we are the owner of all these Param instances, so we should destroy them here!
    for (auto i : params_) {
      ParamPtr p = i.second;
      delete p;
    }
  }
  params_.clear();
}

ParamsVector::ParamsVector(const char *title) :
  title_(title)
{
	params_.reserve(256);
}

// Note: std::initializer_list<ParamRef> causes compiler errors: error C2528: 'abstract declarator': you cannot create a pointer to a reference 
// See also: https://skycoders.wordpress.com/2014/06/08/in-c-pointer-to-reference-is-illegal/
// hence this is a bug:
//   ParamsVector::ParamsVector(const char *title, std::initializer_list<ParamRef> vecs) : ......

ParamsVector::ParamsVector(const char *title, std::initializer_list<ParamPtr> vecs) :
	title_(title)
{
	params_.reserve(256);

	for (ParamPtr i : vecs) {
		add(i);
	}
}

void ParamsVector::mark_as_all_params_owner() {
  is_params_owner_ = true;
}

void ParamsVector::add(ParamPtr param_ref) {
	check_and_report_name_collisions(param_ref->name_str(), params_);
	params_.insert({param_ref->name_str(), param_ref});
}

void ParamsVector::add(Param &param_ref) {
	add(&param_ref);
}

void ParamsVector::add(std::initializer_list<ParamPtr> vecs) {
	for (ParamPtr i : vecs) {
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
	ParamType accepted_types_mask
) const {
	auto l = params_.find(name);
	if (l != params_.end()) {
		ParamPtr p = (*l).second;
		if ((p->type() & accepted_types_mask) != 0) {
			return p;
		}
	}
	return nullptr;
}


template <>
IntParam *ParamsVector::find<IntParam>(
	const char *name
) const {
	return static_cast<IntParam *>(find(name, INT_PARAM));
}

template <>
BoolParam *ParamsVector::find<BoolParam>(
	const char *name
) const {
	return static_cast<BoolParam *>(find(name, BOOL_PARAM));
}

template <>
DoubleParam *ParamsVector::find<DoubleParam>(
	const char *name
) const {
	return static_cast<DoubleParam *>(find(name, DOUBLE_PARAM));
}

template <>
StringParam *ParamsVector::find<StringParam>(
	const char *name
) const {
	return static_cast<StringParam *>(find(name, STRING_PARAM));
}

template <>
Param* ParamsVector::find<Param>(
  const char* name
) const {
  return find(name, ANY_TYPE_PARAM);
}


std::vector<ParamPtr> ParamsVector::as_list(		
	ParamType accepted_types_mask
) const {
	std::vector<ParamPtr> lst;
	for (auto i : params_) {
		ParamPtr p = i.second;
		if ((p->type() & accepted_types_mask) != 0) {
			lst.push_back(p);
		}
	}
	return lst;
}

const char* ParamsVector::title() const {
	return title_.c_str();
}
void ParamsVector::change_title(const char* title) {
	title_ = title ? title : "";
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ParamsVectorSet
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

ParamsVectorSet::~ParamsVectorSet() {
	collection_.clear();
}

ParamsVectorSet::ParamsVectorSet() {
}

ParamsVectorSet::ParamsVectorSet(std::initializer_list<ParamsVector *> vecs) {
	for (ParamsVector *i : vecs) {
		add(i);
	}
}

void ParamsVectorSet::add(ParamsVector &vec_ref) {
	collection_.push_back(&vec_ref);
}

void ParamsVectorSet::add(ParamsVector *vec_ref) {
        collection_.push_back(vec_ref);
}

void ParamsVectorSet::add(std::initializer_list<ParamsVector *> vecs) {
	for (ParamsVector *i : vecs) {
		add(i);
	}
}

Param *ParamsVectorSet::find(
	const char *name, 
	ParamType accepted_types_mask
) const {
	for (ParamsVector *vec : collection_) {
		auto l = vec->params_.find(name);
		if (l != vec->params_.end()) {
			ParamPtr p = (*l).second;
			if ((p->type() & accepted_types_mask) != 0) {
				return p;
			}
		}
	}
	return nullptr;
}

template <>
IntParam *ParamsVectorSet::find<IntParam>(
	const char *name
) const {
	return static_cast<IntParam *>(find(name, INT_PARAM));
}

template <>
BoolParam *ParamsVectorSet::find<BoolParam>(
	const char *name
) const {
	return static_cast<BoolParam *>(find(name, BOOL_PARAM));
}

template <>
DoubleParam *ParamsVectorSet::find<DoubleParam>(
	const char *name
) const {
	return static_cast<DoubleParam *>(find(name, DOUBLE_PARAM));
}

template <>
StringParam *ParamsVectorSet::find<StringParam>(
	const char *name
) const {
	return static_cast<StringParam *>(find(name, STRING_PARAM));
}

template <>
Param* ParamsVectorSet::find<Param>(
  const char* name
) const {
  return static_cast<Param*>(find(name, ANY_TYPE_PARAM));
}

std::vector<ParamPtr> ParamsVectorSet::as_list(		
	ParamType accepted_types_mask
) const {
	std::vector<ParamPtr> lst;
	for (ParamsVector *vec : collection_) {
		for (auto i : vec->params_) {
			ParamPtr p = i.second;
			if ((p->type() & accepted_types_mask) != 0) {
				lst.push_back(p);
			}
		}
	}
	return lst;
}

ParamsVector ParamsVectorSet::flattened_copy(
  ParamType accepted_types_mask
) const {
  ParamsVector rv("muster");

  std::vector<ParamPtr> srclst = as_list(accepted_types_mask);
  for (ParamPtr ref : srclst) {
    rv.add(ref);
  }
  return rv;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Param
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

Param::Param(const char *name, const char *comment, ParamsVector &owner, bool init)
    : owner_(owner),
      name_(name),
      info_(comment),
      init_(init),
      // debug_(false),
      set_(false),
      set_to_non_default_value_(false),
      locked_(false),
      error_(false),

      type_(UNKNOWN_PARAM),
      set_mode_(PARAM_VALUE_IS_DEFAULT),
      setter_(nullptr),
      access_counts_({0, 0, 0, 0})
{
  debug_ = (strstr(name, "debug") != nullptr) || (strstr(name, "display") != nullptr);

  owner.add(this);
}

const char *Param::name_str() const noexcept {
  return name_;
}
const char *Param::info_str() const noexcept {
  return info_;
}
bool Param::is_init() const noexcept {
  return init_;
}
bool Param::is_debug() const noexcept {
  return debug_;
}
bool Param::is_set() const noexcept {
  return set_;
}
bool Param::is_set_to_non_default_value() const noexcept {
  return set_to_non_default_value_;
}
bool Param::is_locked() const noexcept {
  return locked_;
}
bool Param::has_faulted() const noexcept {
  return error_;
}

void Param::lock(bool locking) {
  locked_ = locking;
}
void Param::fault() noexcept {
  safe_inc(access_counts_.faulting);
  error_ = true;
}

void Param::reset_fault() noexcept {
  error_ = false;
}

ParamSetBySourceType Param::set_mode() const noexcept {
  return set_mode_;
}
Param *Param::is_set_by() const noexcept {
  return setter_;
}

ParamsVector &Param::owner() const noexcept {
  return owner_;
}

const Param::access_counts_t &Param::access_counts() const noexcept {
  return access_counts_;
}

void Param::reset_access_counts() noexcept {
  safe_add(access_counts_.prev_sum_reading, access_counts_.reading);
  safe_add(access_counts_.prev_sum_writing, access_counts_.writing);
  safe_add(access_counts_.prev_sum_changing, access_counts_.changing);

  access_counts_.reading = 0;
  access_counts_.writing = 0;
  access_counts_.changing = 0;
}

std::string Param::formatted_value_str() const {
  return value_str(VALSTR_PURPOSE_DATA_FORMATTED_4_DISPLAY);
}

std::string Param::raw_value_str() const {
  return value_str(VALSTR_PURPOSE_RAW_DATA_4_INSPECT);
}

std::string Param::formatted_default_value_str() const {
  return value_str(VALSTR_PURPOSE_DEFAULT_DATA_FORMATTED_4_DISPLAY);
}

std::string Param::raw_default_value_str() const {
  return value_str(VALSTR_PURPOSE_RAW_DEFAULT_DATA_4_INSPECT);
}

std::string Param::value_type_str() const {
  return value_str(VALSTR_PURPOSE_TYPE_INFO);
}

void Param::set_value(const std::string &v, ParamSetBySourceType source_type, ParamPtr source) {
  set_value(v.c_str(), source_type, source);
}

void Param::operator=(const char *value) {
  set_value(value, get_current_application_default_param_source_type(), nullptr);
}
void Param::operator=(const std::string &value) {
  set_value(value.c_str(), get_current_application_default_param_source_type(), nullptr);
}

ParamType Param::type() const noexcept {
  return type_;
}

#if 0
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
		throw new std::logic_error(fmt::format("{} param '{}' error: failed to get value from variant input arg", ParamUtils::GetApplicationName(), name_));
}
#endif




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IntParam
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#define THE_4_HANDLERS_PROTO                                                    \
  const char *name, const char *comment, ParamsVector &owner, bool init,        \
  ParamOnModifyFunction on_modify_f, ParamOnValidateFunction on_validate_f,     \
  ParamOnParseFunction on_parse_f, ParamOnFormatFunction on_format_f

void IntParam_ParamOnModifyFunction(IntParam &target, const int32_t old_value, int32_t &new_value, const int32_t default_value, ParamSetBySourceType source_type, ParamPtr optional_setter) {
  // nothing to do
  return;
}

void IntParam_ParamOnValidateFunction(IntParam &target, const int32_t old_value, int32_t &new_value, const int32_t default_value, ParamSetBySourceType source_type) {
  // nothing to do
  return;
}

void IntParam_ParamOnParseFunction(IntParam &target, int32_t &new_value, const std::string &source_value_str, unsigned int &pos, ParamSetBySourceType source_type) {
  const char *vs = source_value_str.c_str();
  char *endptr = nullptr;
  // https://stackoverflow.com/questions/25315191/need-to-clean-up-errno-before-calling-function-then-checking-errno?rq=3
#if defined(_MSC_VER)
  _set_errno(E_OK);
#else
  errno = E_OK;
#endif
  auto parsed_value = strtol(vs, &endptr, 10);
  auto ec = errno;
  int32_t val = int32_t(parsed_value);
  bool good = (endptr != nullptr && ec == E_OK);
  std::string errmsg;
  if (good) {
    // check to make sure the tail is legal: whitespace only.
    // This also takes care of utter parse failure (when not already signaled via `errno`) when strtol() returns 0 and sets `endptr == vs`.
    while (isspace(*endptr))
      endptr++;
    good = (*endptr == '\0');

    // check if our parsed value is out of legal range: we check the type conversion as that is faster than checking against [INT32_MIN, INT32_MAX].
    if (val != parsed_value && ec == E_OK) {
      good = false;
      ec = ERANGE;
    }
  } else {
    // failed to parse value.
    if (!endptr)
      endptr = (char *)vs;
  }
  if (!good) {
    target.fault();
    if (ec != E_OK) {
      if (ec == ERANGE) {
        errmsg = fmt::format("the parser stopped and reported an integer value overflow (ERANGE); we accept decimal values between {} and {}.", std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
      } else {
        errmsg = fmt::format("the parser stopped and reported \"{}\" (errno: {})", strerror(ec), ec);
      }
    } else if (endptr > vs) {
      errmsg = fmt::format("the parser stopped early: the tail end (\"{}\") of the value string remains", endptr);
    } else {
      errmsg = "the parser was unable to parse anything at all";
    }
    tprintError("ERROR: error parsing {} parameter '{}' value (\"{}\") to {}; {}. The parameter value will not be adjusted: the preset value ({}) will be used instead.\n", ParamUtils::GetApplicationName(), target.name_str(), source_value_str, target.value_type_str(), errmsg, target.formatted_value_str());

    // This value parse handler thus decides to NOT have a value written; we therefore signal a fault state right now: these are (non-fatal) non-silent errors.
    //
    // CODING TIP:
    // 
    // When writing your own parse handlers, when you encounter truly very minor recoverable mistakes, you may opt to have such very minor mistakes be *slient*
    // by writing a WARNING message instead of an ERROR-level one and *not* invoking fault() -- such *silent mistakes* will consequently also not be counted
    // in the parameter fault statistics!
    // 
    // IFF you want such minor mistakes to be counted anyway, we suggest to invoke `fault(); reset_fault();` which has the side-effect of incrementing the
    // error statistic without having ending up with a signaled fault state for the given parameter.
    // Here, today, however, we want the parse error to be non-silent and follow the behaviour as stated in the error message above: by signaling the fault state
    // before we leave, the remainder of this parameter write attempt will be aborted/skipped, as stated above.
    target.fault();
    //target.reset_fault();    -- commented out; here only as part of the CODING TIP above.

    // Finally, we should set the "parsed value" (`new_value`) to a sane value, despite our failure to parse the incoming number.
    // Hence we produce the previously value as that is the best sane value we currently know; the default value being the other option for this choice.
    new_value = target.value();
  } else {
    new_value = val;
  }
  pos = endptr - vs;
}

std::string IntParam_ParamOnFormatFunction(const IntParam &source, const int32_t value, const int32_t default_value, Param::ValueFetchPurpose purpose) {
  switch (purpose) {
      // Fetches the (raw, parseble for re-use via set_value()) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_FORMATTED_4_DISPLAY:
      // Fetches the (raw, parseble for re-use via set_value() or storing to serialized text data format files) value of the param as a string.
      // 
      // NOTE: The part where the documentation says this variant MUST update the parameter usage statistics is
      // handled by the Param class code itself; no need for this callback to handle that part of the deal.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_4_USE:
      return std::to_string(value);

      // Fetches the (raw, parseble for re-use via set_value()) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DEFAULT_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DEFAULT_DATA_FORMATTED_4_DISPLAY:
      return std::to_string(default_value);

      // Return string representing the type of the parameter value, e.g. "integer".
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_TYPE_INFO:
      return "integer";

    default:
      assert(0);
      return nullptr;
  }
}

IntParam::ValueTypedParam(const int32_t value, THE_4_HANDLERS_PROTO)
    : Param(name, comment, owner, init),
      on_modify_f_(on_modify_f ? on_modify_f : IntParam_ParamOnModifyFunction),
      on_validate_f_(on_validate_f ? on_validate_f : IntParam_ParamOnValidateFunction),
      on_parse_f_(on_parse_f ? on_parse_f : IntParam_ParamOnParseFunction),
      on_format_f_(on_format_f ? on_format_f : IntParam_ParamOnFormatFunction),
      value_(value),
      default_(value)
{
  type_ = INT_PARAM;
}

IntParam::operator int32_t() const {
  return value();
}

void IntParam::operator=(const int32_t value) {
  set_value(value, get_current_application_default_param_source_type(), nullptr);
}

void IntParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
  unsigned int pos = 0;
  std::string vs(v);
  int32_t vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, source_type); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    set_value(vv, source_type, source);
  }
}

template<>
void IntParam::set_value(int32_t value, ParamSetBySourceType source_type, ParamPtr source) {
  safe_inc(access_counts_.writing);
  // ^^^^^^^ --
  // Our 'writing' statistic counts write ATTEMPTS, in reailty.
  // Any real change is tracked by the 'changing' statistic (see further below)!

  reset_fault();
  // when we fail the validation horribly, the validator will throw an exception and thus abort the (write) action.
  // non-fatal errors may be signaled, in which case the write operation is aborted/skipped, or not signaled (a.k.a. 'silent')
  // in which case the write operation proceeds as if nothing untoward happened inside on_validate_f.
  on_validate_f_(*this, value_, value, default_, source_type);
  if (!has_faulted()) {
    // however, when we failed the validation only in the sense of the value being adjusted/restricted by the validator,
    // then we must set the value as set by the validator anyway, so nothing changes in our workflow here.

    set_ = (source_type > PARAM_VALUE_IS_RESET);
    set_to_non_default_value_ = (value != default_);

    if (value != value_) {
      on_modify_f_(*this, value_, value, default_, source_type, source);
      if (!has_faulted() && value != value_) {
        safe_inc(access_counts_.changing);
        value_ = value;
      }
    }
  }
  // any signaled fault will be visible outside...
}

template <>
int32_t IntParam::value() const noexcept {
  safe_inc(access_counts_.reading);
  return value_;
}

// Optionally the `source_vec` can be used to source the value to reset the parameter to.
// When no source vector is specified, or when the source vector does not specify this
// particular parameter, then our value is reset to the default value which was
// specified earlier in our constructor.
void IntParam::ResetToDefault(const ParamsVectorSet *source_vec, ParamSetBySourceType source_type) {
  if (source_vec != nullptr) {
    IntParam *source = source_vec->find<IntParam>(name_str());
    if (source != nullptr) {
      set_value(source->value(), PARAM_VALUE_IS_RESET, source);
      return;
    }
  }
  set_value(default_, PARAM_VALUE_IS_RESET, nullptr);
}

std::string IntParam::value_str(ValueFetchPurpose purpose) const {
  if (purpose == VALSTR_PURPOSE_DATA_4_USE)
    safe_inc(access_counts_.reading);
  return on_format_f_(*this, value_, default_, purpose);
}

IntParam::ParamOnModifyFunction IntParam::set_on_modify_handler(IntParam::ParamOnModifyFunction on_modify_f) {
  IntParam::ParamOnModifyFunction rv = on_modify_f_;
  if (!on_modify_f)
    on_modify_f = IntParam_ParamOnModifyFunction;
  on_modify_f_ = on_modify_f;
  return rv;
}
void IntParam::clear_on_modify_handler() {
  on_modify_f_ = IntParam_ParamOnModifyFunction;
}
IntParam::ParamOnValidateFunction IntParam::set_on_validate_handler(IntParam::ParamOnValidateFunction on_validate_f) {
  IntParam::ParamOnValidateFunction rv = on_validate_f_;
  if (!on_validate_f)
    on_validate_f = IntParam_ParamOnValidateFunction;
  on_validate_f_ = on_validate_f;
  return rv;
}
void IntParam::clear_on_validate_handler() {
  on_validate_f_ = IntParam_ParamOnValidateFunction;
}
IntParam::ParamOnParseFunction IntParam::set_on_parse_handler(IntParam::ParamOnParseFunction on_parse_f) {
  IntParam::ParamOnParseFunction rv = on_parse_f_;
  if (!on_parse_f)
    on_parse_f = IntParam_ParamOnParseFunction;
  on_parse_f_ = on_parse_f;
  return rv;
}
void IntParam::clear_on_parse_handler() {
  on_parse_f_ = IntParam_ParamOnParseFunction;
}
IntParam::ParamOnFormatFunction IntParam::set_on_format_handler(IntParam::ParamOnFormatFunction on_format_f) {
  IntParam::ParamOnFormatFunction rv = on_format_f_;
  if (!on_format_f)
    on_format_f = IntParam_ParamOnFormatFunction;
  on_format_f_ = on_format_f;
  return rv;
}
void IntParam::clear_on_format_handler() {
  on_format_f_ = IntParam_ParamOnFormatFunction;
}

#if 0
bool IntParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
	int32_t val = 0;
	return SafeAtoi(v, &val) && set_value(val, source_type, source);
}
bool IntParam::set_value(bool v, ParamSetBySourceType source_type, ParamPtr source) {
	int32_t val = !!v;
	return set_value(val, source_type, source);
}
bool IntParam::set_value(double v, ParamSetBySourceType source_type, ParamPtr source) {
	v = roundf(v);
	if (v < INT32_MIN || v > INT32_MAX)
		return false;

	int32_t val = v;
	return set_value(val, source_type, source);
}
#endif




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BoolParam
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void BoolParam_ParamOnModifyFunction(BoolParam &target, const bool old_value, bool &new_value, const bool default_value, ParamSetBySourceType source_type, ParamPtr optional_setter) {
  // nothing to do
  return;
}

void BoolParam_ParamOnValidateFunction(BoolParam &target, const bool old_value, bool &new_value, const bool default_value, ParamSetBySourceType source_type) {
  // nothing to do
  return;
}

void BoolParam_ParamOnParseFunction(BoolParam &target, bool &new_value, const std::string &source_value_str, unsigned int &pos, ParamSetBySourceType source_type) {
  const char *vs = source_value_str.c_str();
  char *endptr = nullptr;
  // https://stackoverflow.com/questions/25315191/need-to-clean-up-errno-before-calling-function-then-checking-errno?rq=3
#if defined(_MSC_VER)
  _set_errno(E_OK);
#else
  errno = E_OK;
#endif
  // We accept decimal, hex and octal numbers here, not just the ubiquitous 0, 1 and -1. `+5` also implies TRUE as far as we are concerned. We are tolerant on our input here, not pedantic, *by design*.
  // However, we do restrict our values to the 32-bit signed range: this is picked for the tolerated numeric value range as that equals the IntPAram (int32_t) one, but granted: this range restriction is
  // a matter of taste and arguably arbitrary. We've pondered limiting the accepted numerical values to the range of an int8_t (-128 .. + 127) but the ulterior goal here is to stay as close to the int32_t
  // IntParam value parser code as possible, so int32_t range it is....
  auto parsed_value = strtol(vs, &endptr, 0);
  auto ec = errno;
  int32_t val = int32_t(parsed_value);
  bool good = (endptr != nullptr && ec == E_OK);
  std::string errmsg;
  if (good) {
    // check to make sure the tail is legal: whitespace only.
    // This also takes care of utter parse failure (when not already signaled via `errno`) when strtol() returns 0 and sets `endptr == vs`.
    while (isspace(*endptr))
      endptr++;
    good = (*endptr == '\0');

    // check if our parsed value is out of legal range: we check the type conversion as that is faster than checking against [INT32_MIN, INT32_MAX].
    if (val != parsed_value && ec == E_OK) {
      good = false;
      ec = ERANGE;
    }
  } else {
    // failed to parse boolean value as numeric value (zero, non-zero). Try to parse as a word (true/false) or symbol (+/-) instead.
    const char *s = vs;
    while (isspace(*s))
      s++;
    endptr = (char *)vs;
    switch (tolower(s[0])) {
      case 't':
        // true; only valid when a single char or word:
        // (and, yes, we are very lenient: if some Smart Alec enters "Tamagotchi" as a value here, we consider that a valid equivalent to TRUE. Tolerant *by design*.)
        good = is_single_word(s);
        val = 1;
        break;

      case 'f':
        // false; only valid when a single char or word:
        // (and, yes, we are very lenient again: if some Smart Alec enters "Favela" as a value here, we consider that a valid equivalent to FALSE. Tolerant *by design*. Bite me.)
        good = is_single_word(s);
        val = 0;
        break;

      case 'y':
      case 'j':
        // yes / ja; only valid when a single char or word:
        good = is_single_word(s);
        val = 1;
        break;

      case 'n':
        // no; only valid when a single char or word:
        good = is_single_word(s);
        val = 0;
        break;

      case 'x':
      case '+':
        // on; only valid when alone:
        good = is_optional_whitespace(s + 1);
        val = 1;
        break;

      case '-':
      case '.':
        // off; only valid when alone:
        good = is_optional_whitespace(s + 1);
        val = 0;
        break;

      default:
        // we reject everything else as not-a-boolean-value.
        good = false;
        break;
    }

    if (good) {
      endptr += strlen(endptr);
    }
  }

  if (!good) {
    target.fault();
    if (ec != E_OK) {
      if (ec == ERANGE) {
        errmsg = fmt::format("the parser stopped and reported an integer value overflow (ERANGE); while we expect a boolean value (ideally 1/0/-1), we accept decimal values between {} and {} where any non-zero value equals TRUE.", std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
      } else {
        errmsg = fmt::format("the parser stopped and reported \"{}\" (errno: {}) and we were unable to otherwise parse the given value as a boolean word ([T]rue/[F]alse/[Y]es/[J]a/[N]o) or boolean symbol (+/-/.) ", strerror(ec), ec);
      }
    } else if (endptr > vs) {
      errmsg = fmt::format("the parser stopped early: the tail end (\"{}\") of the value string remains", endptr);
    } else {
      errmsg = "the parser was unable to parse anything at all";
    }
    tprintError("ERROR: error parsing {} parameter '{}' value (\"{}\") to {}; {}. The parameter value will not be adjusted: the preset value ({}) will be used instead.\n", ParamUtils::GetApplicationName(), target.name_str(), source_value_str, target.value_type_str(), errmsg, target.formatted_value_str());

    // This value parse handler thus decides to NOT have a value written; we therefore signal a fault state right now: these are (non-fatal) non-silent errors.
    //
    // CODING TIP:
    //
    // When writing your own parse handlers, when you encounter truly very minor recoverable mistakes, you may opt to have such very minor mistakes be *slient*
    // by writing a WARNING message instead of an ERROR-level one and *not* invoking fault() -- such *silent mistakes* will consequently also not be counted
    // in the parameter fault statistics!
    //
    // IFF you want such minor mistakes to be counted anyway, we suggest to invoke `fault(); reset_fault();` which has the side-effect of incrementing the
    // error statistic without having ending up with a signaled fault state for the given parameter.
    // Here, today, however, we want the parse error to be non-silent and follow the behaviour as stated in the error message above: by signaling the fault state
    // before we leave, the remainder of this parameter write attempt will be aborted/skipped, as stated above.
    target.fault();
    // target.reset_fault();    -- commented out; here only as part of the CODING TIP above.

    // Finally, we should set the "parsed value" (`new_value`) to a sane value, despite our failure to parse the incoming number.
    // Hence we produce the previously value as that is the best sane value we currently know; the default value being the other option for this choice.
    new_value = target.value();
  }
  pos = endptr - vs;
}

std::string BoolParam_ParamOnFormatFunction(const BoolParam &source, const bool value, const bool default_value, Param::ValueFetchPurpose purpose) {
  switch (purpose) {
      // Fetches the (raw, parseble for re-use via set_value()) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_FORMATTED_4_DISPLAY:
      // Fetches the (raw, parseble for re-use via set_value() or storing to serialized text data format files) value of the param as a string.
      //
      // NOTE: The part where the documentation says this variant MUST update the parameter usage statistics is
      // handled by the Param class code itself; no need for this callback to handle that part of the deal.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_4_USE:
      return value ? "true" : "false";

      // Fetches the (raw, parseble for re-use via set_value()) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DEFAULT_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DEFAULT_DATA_FORMATTED_4_DISPLAY:
      return default_value ? "true" : "false";

      // Return string representing the type of the parameter value, e.g. "integer".
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_TYPE_INFO:
      return "boolean";

    default:
      assert(0);
      return nullptr;
  }
}

BoolParam::ValueTypedParam(const bool value, THE_4_HANDLERS_PROTO)
    : Param(name, comment, owner, init),
      on_modify_f_(on_modify_f ? on_modify_f : BoolParam_ParamOnModifyFunction),
      on_validate_f_(on_validate_f ? on_validate_f : BoolParam_ParamOnValidateFunction),
      on_parse_f_(on_parse_f ? on_parse_f : BoolParam_ParamOnParseFunction),
      on_format_f_(on_format_f ? on_format_f : BoolParam_ParamOnFormatFunction),
      value_(value),
      default_(value) {
  type_ = BOOL_PARAM;
}

BoolParam::operator bool() const {
  return value();
}

void BoolParam::operator=(const bool value) {
  set_value(value, get_current_application_default_param_source_type(), nullptr);
}

void BoolParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
  unsigned int pos = 0;
  std::string vs(v);
  bool vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, source_type); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    set_value(vv, source_type, source);
  }
}

template <>
void BoolParam::set_value(bool value, ParamSetBySourceType source_type, ParamPtr source) {
  safe_inc(access_counts_.writing);
  // ^^^^^^^ --
  // Our 'writing' statistic counts write ATTEMPTS, in reailty.
  // Any real change is tracked by the 'changing' statistic (see further below)!

  reset_fault();
  // when we fail the validation horribly, the validator will throw an exception and thus abort the (write) action.
  // non-fatal errors may be signaled, in which case the write operation is aborted/skipped, or not signaled (a.k.a. 'silent')
  // in which case the write operation proceeds as if nothing untoward happened inside on_validate_f.
  on_validate_f_(*this, value_, value, default_, source_type);
  if (!has_faulted()) {
    // however, when we failed the validation only in the sense of the value being adjusted/restricted by the validator,
    // then we must set the value as set by the validator anyway, so nothing changes in our workflow here.

    set_ = (source_type > PARAM_VALUE_IS_RESET);
    set_to_non_default_value_ = (value != default_);

    if (value != value_) {
      on_modify_f_(*this, value_, value, default_, source_type, source);
      if (!has_faulted() && value != value_) {
        safe_inc(access_counts_.changing);
        value_ = value;
      }
    }
  }
  // any signaled fault will be visible outside...
}

template <>
bool BoolParam::value() const noexcept {
  safe_inc(access_counts_.reading);
  return value_;
}

// Optionally the `source_vec` can be used to source the value to reset the parameter to.
// When no source vector is specified, or when the source vector does not specify this
// particular parameter, then our value is reset to the default value which was
// specified earlier in our constructor.
void BoolParam::ResetToDefault(const ParamsVectorSet *source_vec, ParamSetBySourceType source_type) {
  if (source_vec != nullptr) {
    BoolParam *source = source_vec->find<BoolParam>(name_str());
    if (source != nullptr) {
      set_value(source->value(), PARAM_VALUE_IS_RESET, source);
      return;
    }
  }
  set_value(default_, PARAM_VALUE_IS_RESET, nullptr);
}

std::string BoolParam::value_str(ValueFetchPurpose purpose) const {
  if (purpose == VALSTR_PURPOSE_DATA_4_USE)
    safe_inc(access_counts_.reading);
  return on_format_f_(*this, value_, default_, purpose);
}

BoolParam::ParamOnModifyFunction BoolParam::set_on_modify_handler(BoolParam::ParamOnModifyFunction on_modify_f) {
  BoolParam::ParamOnModifyFunction rv = on_modify_f_;
  if (!on_modify_f)
    on_modify_f = BoolParam_ParamOnModifyFunction;
  on_modify_f_ = on_modify_f;
  return rv;
}
void BoolParam::clear_on_modify_handler() {
  on_modify_f_ = BoolParam_ParamOnModifyFunction;
}
BoolParam::ParamOnValidateFunction BoolParam::set_on_validate_handler(BoolParam::ParamOnValidateFunction on_validate_f) {
  BoolParam::ParamOnValidateFunction rv = on_validate_f_;
  if (!on_validate_f)
    on_validate_f = BoolParam_ParamOnValidateFunction;
  on_validate_f_ = on_validate_f;
  return rv;
}
void BoolParam::clear_on_validate_handler() {
  on_validate_f_ = BoolParam_ParamOnValidateFunction;
}
BoolParam::ParamOnParseFunction BoolParam::set_on_parse_handler(BoolParam::ParamOnParseFunction on_parse_f) {
  BoolParam::ParamOnParseFunction rv = on_parse_f_;
  if (!on_parse_f)
    on_parse_f = BoolParam_ParamOnParseFunction;
  on_parse_f_ = on_parse_f;
  return rv;
}
void BoolParam::clear_on_parse_handler() {
  on_parse_f_ = BoolParam_ParamOnParseFunction;
}
BoolParam::ParamOnFormatFunction BoolParam::set_on_format_handler(BoolParam::ParamOnFormatFunction on_format_f) {
  BoolParam::ParamOnFormatFunction rv = on_format_f_;
  if (!on_format_f)
    on_format_f = BoolParam_ParamOnFormatFunction;
  on_format_f_ = on_format_f;
  return rv;
}
void BoolParam::clear_on_format_handler() {
  on_format_f_ = BoolParam_ParamOnFormatFunction;
}

#if 0
bool BoolParam::set_value(bool v, ParamSetBySourceType source_type, ParamPtr source) {
	bool val = (v != 0);
	return set_value(val, source_type, source);
}

// based on https://stackoverflow.com/questions/13698927/compare-double-to-zero-using-epsilon
#define inline_constexpr   inline
static inline_constexpr double epsilon_plus()
{
  const double a = 0.0;
  return std::nextafter(a, std::numeric_limits<double>::max());
}
static inline_constexpr double epsilon_minus()
{
  const double a = 0.0;
  return std::nextafter(a, std::numeric_limits<double>::lowest());
}
static bool is_zero(const double b)
{
  return epsilon_minus() <= b
    && epsilon_plus() >= b;
}

bool BoolParam::set_value(double v, ParamSetBySourceType source_type, ParamPtr source) {
  bool zero = is_zero(v);

	return set_value(!zero, source_type, source);
}
#endif




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DoubleParam
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void DoubleParam_ParamOnModifyFunction(DoubleParam &target, const double old_value, double &new_value, const double default_value, ParamSetBySourceType source_type, ParamPtr optional_setter) {
  // nothing to do
  return;
}

void DoubleParam_ParamOnValidateFunction(DoubleParam &target, const double old_value, double &new_value, const double default_value, ParamSetBySourceType source_type) {
  // nothing to do
  return;
}

void DoubleParam_ParamOnParseFunction(DoubleParam &target, double &new_value, const std::string &source_value_str, unsigned int &pos, ParamSetBySourceType source_type) {
  const char *vs = source_value_str.c_str();
  char *endptr = nullptr;
  // https://stackoverflow.com/questions/25315191/need-to-clean-up-errno-before-calling-function-then-checking-errno?rq=3
#if defined(_MSC_VER)
  _set_errno(E_OK);
#else
  errno = E_OK;
#endif
#if 01
  double val = NAN;
    std::istringstream stream(source_value_str);
    // Use "C" locale for reading double value.
    stream.imbue(std::locale::classic());
    stream >> val;
    auto ec = errno;
    auto spos = stream.tellg();
    endptr = (char *)vs + spos;
  bool good = (endptr != vs && ec == E_OK);
#else
  auto val = strtod(vs, &endptr);
  bool good = (endptr != nullptr && ec == E_OK);
#endif
  std::string errmsg;
  if (good) {
    // check to make sure the tail is legal: whitespace only.
    // This also takes care of utter parse failure (when not already signaled via `errno`) when strtol() returns 0 and sets `endptr == vs`.
    while (isspace(*endptr))
      endptr++;
    good = (*endptr == '\0');

    // check if our parsed value is out of legal range: we check the type conversion as that is faster than checking against [INT32_MIN, INT32_MAX].
    if (!is_legal_fpval(val) && ec == E_OK) {
      good = false;
      ec = ERANGE;
    }
  } else {
    // failed to parse value.
    if (!endptr)
      endptr = (char *)vs;
  }
  if (!good) {
    target.fault();
    if (ec != E_OK) {
      if (ec == ERANGE) {
        errmsg = fmt::format("the parser stopped and reported an floating point value overflow (ERANGE); we accept floating point values between {} and {}.", std::numeric_limits<double>::min(), std::numeric_limits<double>::max());
      } else {
        errmsg = fmt::format("the parser stopped and reported \"{}\" (errno: {})", strerror(ec), ec);
      }
    } else if (endptr > vs) {
      errmsg = fmt::format("the parser stopped early: the tail end (\"{}\") of the value string remains", endptr);
    } else {
      errmsg = "the parser was unable to parse anything at all";
    }
    tprintError("ERROR: error parsing {} parameter '{}' value (\"{}\") to {}; {}. The parameter value will not be adjusted: the preset value ({}) will be used instead.\n", ParamUtils::GetApplicationName(), target.name_str(), source_value_str, target.value_type_str(), errmsg, target.formatted_value_str());

    // This value parse handler thus decides to NOT have a value written; we therefore signal a fault state right now: these are (non-fatal) non-silent errors.
    //
    // CODING TIP:
    //
    // When writing your own parse handlers, when you encounter truly very minor recoverable mistakes, you may opt to have such very minor mistakes be *slient*
    // by writing a WARNING message instead of an ERROR-level one and *not* invoking fault() -- such *silent mistakes* will consequently also not be counted
    // in the parameter fault statistics!
    //
    // IFF you want such minor mistakes to be counted anyway, we suggest to invoke `fault(); reset_fault();` which has the side-effect of incrementing the
    // error statistic without having ending up with a signaled fault state for the given parameter.
    // Here, today, however, we want the parse error to be non-silent and follow the behaviour as stated in the error message above: by signaling the fault state
    // before we leave, the remainder of this parameter write attempt will be aborted/skipped, as stated above.
    target.fault();
    // target.reset_fault();    -- commented out; here only as part of the CODING TIP above.

    // Finally, we should set the "parsed value" (`new_value`) to a sane value, despite our failure to parse the incoming number.
    // Hence we produce the previously value as that is the best sane value we currently know; the default value being the other option for this choice.
    new_value = target.value();
  } else {
    new_value = val;
  }
  pos = endptr - vs;
}

std::string DoubleParam_ParamOnFormatFunction(const DoubleParam &source, const double value, const double default_value, Param::ValueFetchPurpose purpose) {
  switch (purpose) {
      // Fetches the (raw, parseble for re-use via set_value()) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_FORMATTED_4_DISPLAY:
      // Fetches the (raw, parseble for re-use via set_value() or storing to serialized text data format files) value of the param as a string.
      //
      // NOTE: The part where the documentation says this variant MUST update the parameter usage statistics is
      // handled by the Param class code itself; no need for this callback to handle that part of the deal.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_4_USE:
#if 0
      return std::to_string(value);   // always outputs %.6f format style values
#else
      char sbuf[40];
      snprintf(sbuf, sizeof(sbuf), "%1.f", value);
      sbuf[39] = 0;
      return sbuf;
#endif

      // Fetches the (raw, parseble for re-use via set_value()) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DEFAULT_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DEFAULT_DATA_FORMATTED_4_DISPLAY:
#if 0
      return std::to_string(default_value);   // always outputs %.6f format style values
#else
      char sdbuf[40];
      snprintf(sdbuf, sizeof(sdbuf), "%1.f", default_value);
      sdbuf[39] = 0;
      return sdbuf;
#endif

      // Return string representing the type of the parameter value, e.g. "integer".
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_TYPE_INFO:
      return "floating point";

    default:
      assert(0);
      return nullptr;
  }
}

DoubleParam::ValueTypedParam(const double value, THE_4_HANDLERS_PROTO)
    : Param(name, comment, owner, init),
      on_modify_f_(on_modify_f ? on_modify_f : DoubleParam_ParamOnModifyFunction),
      on_validate_f_(on_validate_f ? on_validate_f : DoubleParam_ParamOnValidateFunction),
      on_parse_f_(on_parse_f ? on_parse_f : DoubleParam_ParamOnParseFunction),
      on_format_f_(on_format_f ? on_format_f : DoubleParam_ParamOnFormatFunction),
      value_(value),
      default_(value) {
  type_ = DOUBLE_PARAM;
}

DoubleParam::operator double() const {
  return value();
}

void DoubleParam::operator=(const double value) {
  set_value(value, get_current_application_default_param_source_type(), nullptr);
}

void DoubleParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
  unsigned int pos = 0;
  std::string vs(v);
  double vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, source_type); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    set_value(vv, source_type, source);
  }
}

template <>
void DoubleParam::set_value(double value, ParamSetBySourceType source_type, ParamPtr source) {
  safe_inc(access_counts_.writing);
  // ^^^^^^^ --
  // Our 'writing' statistic counts write ATTEMPTS, in reailty.
  // Any real change is tracked by the 'changing' statistic (see further below)!

  reset_fault();
  // when we fail the validation horribly, the validator will throw an exception and thus abort the (write) action.
  // non-fatal errors may be signaled, in which case the write operation is aborted/skipped, or not signaled (a.k.a. 'silent')
  // in which case the write operation proceeds as if nothing untoward happened inside on_validate_f.
  on_validate_f_(*this, value_, value, default_, source_type);
  if (!has_faulted()) {
    // however, when we failed the validation only in the sense of the value being adjusted/restricted by the validator,
    // then we must set the value as set by the validator anyway, so nothing changes in our workflow here.

    set_ = (source_type > PARAM_VALUE_IS_RESET);
    set_to_non_default_value_ = (value != default_);

    if (value != value_) {
      on_modify_f_(*this, value_, value, default_, source_type, source);
      if (!has_faulted() && value != value_) {
        safe_inc(access_counts_.changing);
        value_ = value;
      }
    }
  }
  // any signaled fault will be visible outside...
}

template <>
double DoubleParam::value() const noexcept {
  safe_inc(access_counts_.reading);
  return value_;
}

// Optionally the `source_vec` can be used to source the value to reset the parameter to.
// When no source vector is specified, or when the source vector does not specify this
// particular parameter, then our value is reset to the default value which was
// specified earlier in our constructor.
void DoubleParam::ResetToDefault(const ParamsVectorSet *source_vec, ParamSetBySourceType source_type) {
  if (source_vec != nullptr) {
    DoubleParam *source = source_vec->find<DoubleParam>(name_str());
    if (source != nullptr) {
      set_value(source->value(), PARAM_VALUE_IS_RESET, source);
      return;
    }
  }
  set_value(default_, PARAM_VALUE_IS_RESET, nullptr);
}

std::string DoubleParam::value_str(ValueFetchPurpose purpose) const {
  if (purpose == VALSTR_PURPOSE_DATA_4_USE)
    safe_inc(access_counts_.reading);
  return on_format_f_(*this, value_, default_, purpose);
}

DoubleParam::ParamOnModifyFunction DoubleParam::set_on_modify_handler(DoubleParam::ParamOnModifyFunction on_modify_f) {
  DoubleParam::ParamOnModifyFunction rv = on_modify_f_;
  if (!on_modify_f)
    on_modify_f = DoubleParam_ParamOnModifyFunction;
  on_modify_f_ = on_modify_f;
  return rv;
}
void DoubleParam::clear_on_modify_handler() {
  on_modify_f_ = DoubleParam_ParamOnModifyFunction;
}
DoubleParam::ParamOnValidateFunction DoubleParam::set_on_validate_handler(DoubleParam::ParamOnValidateFunction on_validate_f) {
  DoubleParam::ParamOnValidateFunction rv = on_validate_f_;
  if (!on_validate_f)
    on_validate_f = DoubleParam_ParamOnValidateFunction;
  on_validate_f_ = on_validate_f;
  return rv;
}
void DoubleParam::clear_on_validate_handler() {
  on_validate_f_ = DoubleParam_ParamOnValidateFunction;
}
DoubleParam::ParamOnParseFunction DoubleParam::set_on_parse_handler(DoubleParam::ParamOnParseFunction on_parse_f) {
  DoubleParam::ParamOnParseFunction rv = on_parse_f_;
  if (!on_parse_f)
    on_parse_f = DoubleParam_ParamOnParseFunction;
  on_parse_f_ = on_parse_f;
  return rv;
}
void DoubleParam::clear_on_parse_handler() {
  on_parse_f_ = DoubleParam_ParamOnParseFunction;
}
DoubleParam::ParamOnFormatFunction DoubleParam::set_on_format_handler(DoubleParam::ParamOnFormatFunction on_format_f) {
  DoubleParam::ParamOnFormatFunction rv = on_format_f_;
  if (!on_format_f)
    on_format_f = DoubleParam_ParamOnFormatFunction;
  on_format_f_ = on_format_f;
  return rv;
}
void DoubleParam::clear_on_format_handler() {
  on_format_f_ = DoubleParam_ParamOnFormatFunction;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StringParam
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void StringParam_ParamOnModifyFunction(StringParam &target, const std::string &old_value, std::string &new_value, const std::string &default_value, ParamSetBySourceType source_type, ParamPtr optional_setter) {
  // nothing to do
  return;
}

void StringParam_ParamOnValidateFunction(StringParam &target, const std::string &old_value, std::string &new_value, const std::string &default_value, ParamSetBySourceType source_type) {
  // nothing to do
  return;
}

void StringParam_ParamOnParseFunction(StringParam &target, std::string &new_value, const std::string &source_value_str, unsigned int &pos, ParamSetBySourceType source_type) {
  // we accept anything for a string parameter!
  new_value = source_value_str;
  pos = source_value_str.size();
}

std::string StringParam_ParamOnFormatFunction(const StringParam &source, const std::string &value, const std::string &default_value, Param::ValueFetchPurpose purpose) {
  switch (purpose) {
      // Fetches the (raw, parseble for re-use via set_value()) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_FORMATTED_4_DISPLAY:
      // Fetches the (raw, parseble for re-use via set_value() or storing to serialized text data format files) value of the param as a string.
      //
      // NOTE: The part where the documentation says this variant MUST update the parameter usage statistics is
      // handled by the Param class code itself; no need for this callback to handle that part of the deal.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_4_USE:
      return value;

      // Fetches the (raw, parseble for re-use via set_value()) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DEFAULT_DATA_4_INSPECT:
      // Fetches the (formatted for print/display) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DEFAULT_DATA_FORMATTED_4_DISPLAY:
      return default_value;

      // Return string representing the type of the parameter value, e.g. "integer".
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_TYPE_INFO:
      return "string";

    default:
      assert(0);
      return nullptr;
  }
}

StringParam::StringTypedParam(const std::string &value, THE_4_HANDLERS_PROTO)
    : Param(name, comment, owner, init),
      on_modify_f_(on_modify_f ? on_modify_f : StringParam_ParamOnModifyFunction),
      on_validate_f_(on_validate_f ? on_validate_f : StringParam_ParamOnValidateFunction),
      on_parse_f_(on_parse_f ? on_parse_f : StringParam_ParamOnParseFunction),
      on_format_f_(on_format_f ? on_format_f : StringParam_ParamOnFormatFunction),
      value_(value),
      default_(value) {
  type_ = STRING_PARAM;
}

StringParam::StringTypedParam(const std::string *value, THE_4_HANDLERS_PROTO)
    : StringTypedParam(value == nullptr ? "" : *value, name, comment, owner, init, on_modify_f, on_validate_f, on_parse_f, on_format_f)
{}

StringParam::StringTypedParam(const char *value, THE_4_HANDLERS_PROTO)
    : StringTypedParam(std::string(value == nullptr ? "" : value), name, comment, owner, init, on_modify_f, on_validate_f, on_parse_f, on_format_f)
{}

StringParam::operator const std::string &() const {
  return value();
}

StringParam::operator const std::string *() const {
  return &value();
}

const char* StringParam::c_str() const {
  return value().c_str();
}

bool StringParam::empty() const noexcept {
  return value().empty();
}

// https://en.cppreference.com/w/cpp/feature_test#cpp_lib_string_contains
#if defined(__has_cpp_attribute) && __has_cpp_attribute(__cpp_lib_string_contains)  // C++23

bool StringParam::contains(char ch) const noexcept {
  return value().contains(ch);
}

bool StringParam::contains(const char *s) const noexcept {
  return value().contains(s);
}

bool StringParam::contains(const std::string &s) const noexcept {
  return value().contains(s);
}

#else

bool StringParam::contains(char ch) const noexcept {
  auto v = value();
  auto f = v.find(ch);
  return f != std::string::npos;
}

bool StringParam::contains(const char *s) const noexcept {
  auto v = value();
  auto f = v.find(s);
  return f != std::string::npos;
}

bool StringParam::contains(const std::string &s) const noexcept {
  auto v = value();
  auto f = v.find(s);
  return f != std::string::npos;
}

#endif

void StringParam::operator=(const std::string &value) {
  set_value(value, get_current_application_default_param_source_type(), nullptr);
}

void StringParam::operator=(const std::string *value) {
  set_value((value == nullptr ? "" : *value), get_current_application_default_param_source_type(), nullptr);
}

void StringParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
  unsigned int pos = 0;
  std::string vs(v == nullptr ? "" : v);
  std::string vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, source_type); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    set_value(vv, source_type, source);
  }
}

template <>
void StringParam::set_value(const std::string &val, ParamSetBySourceType source_type, ParamPtr source) {
  safe_inc(access_counts_.writing);
  // ^^^^^^^ --
  // Our 'writing' statistic counts write ATTEMPTS, in reailty.
  // Any real change is tracked by the 'changing' statistic (see further below)!

  std::string value(val);
  reset_fault();
  // when we fail the validation horribly, the validator will throw an exception and thus abort the (write) action.
  // non-fatal errors may be signaled, in which case the write operation is aborted/skipped, or not signaled (a.k.a. 'silent')
  // in which case the write operation proceeds as if nothing untoward happened inside on_validate_f.
  on_validate_f_(*this, value_, value, default_, source_type);
  if (!has_faulted()) {
    // however, when we failed the validation only in the sense of the value being adjusted/restricted by the validator,
    // then we must set the value as set by the validator anyway, so nothing changes in our workflow here.

    set_ = (source_type > PARAM_VALUE_IS_RESET);
    set_to_non_default_value_ = (value != default_);

    if (value != value_) {
      on_modify_f_(*this, value_, value, default_, source_type, source);
      if (!has_faulted() && value != value_) {
        safe_inc(access_counts_.changing);
        value_ = value;
      }
    }
  }
  // any signaled fault will be visible outside...
}

template <>
const std::string &StringParam::value() const noexcept {
  safe_inc(access_counts_.reading);
  return value_;
}

// Optionally the `source_vec` can be used to source the value to reset the parameter to.
// When no source vector is specified, or when the source vector does not specify this
// particular parameter, then our value is reset to the default value which was
// specified earlier in our constructor.
void StringParam::ResetToDefault(const ParamsVectorSet *source_vec, ParamSetBySourceType source_type) {
  if (source_vec != nullptr) {
    StringParam *source = source_vec->find<StringParam>(name_str());
    if (source != nullptr) {
      set_value(source->value(), PARAM_VALUE_IS_RESET, source);
      return;
    }
  }
  set_value(default_, PARAM_VALUE_IS_RESET, nullptr);
}

std::string StringParam::value_str(ValueFetchPurpose purpose) const {
  if (purpose == VALSTR_PURPOSE_DATA_4_USE)
    safe_inc(access_counts_.reading);
  return on_format_f_(*this, value_, default_, purpose);
}

StringParam::ParamOnModifyFunction StringParam::set_on_modify_handler(StringParam::ParamOnModifyFunction on_modify_f) {
  StringParam::ParamOnModifyFunction rv = on_modify_f_;
  if (!on_modify_f)
    on_modify_f = StringParam_ParamOnModifyFunction;
  on_modify_f_ = on_modify_f;
  return rv;
}
void StringParam::clear_on_modify_handler() {
  on_modify_f_ = StringParam_ParamOnModifyFunction;
}
StringParam::ParamOnValidateFunction StringParam::set_on_validate_handler(StringParam::ParamOnValidateFunction on_validate_f) {
  StringParam::ParamOnValidateFunction rv = on_validate_f_;
  if (!on_validate_f)
    on_validate_f = StringParam_ParamOnValidateFunction;
  on_validate_f_ = on_validate_f;
  return rv;
}
void StringParam::clear_on_validate_handler() {
  on_validate_f_ = StringParam_ParamOnValidateFunction;
}
StringParam::ParamOnParseFunction StringParam::set_on_parse_handler(StringParam::ParamOnParseFunction on_parse_f) {
  StringParam::ParamOnParseFunction rv = on_parse_f_;
  if (!on_parse_f)
    on_parse_f = StringParam_ParamOnParseFunction;
  on_parse_f_ = on_parse_f;
  return rv;
}
void StringParam::clear_on_parse_handler() {
  on_parse_f_ = StringParam_ParamOnParseFunction;
}
StringParam::ParamOnFormatFunction StringParam::set_on_format_handler(StringParam::ParamOnFormatFunction on_format_f) {
  StringParam::ParamOnFormatFunction rv = on_format_f_;
  if (!on_format_f)
    on_format_f = StringParam_ParamOnFormatFunction;
  on_format_f_ = on_format_f;
  return rv;
}
void StringParam::clear_on_format_handler() {
  on_format_f_ = StringParam_ParamOnFormatFunction;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// StringSetParam
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void StringSetParam_ParamOnModifyFunction(StringSetParam &target, const std::vector<std::string> &old_value, std::vector<std::string> &new_value, const std::vector<std::string> &default_value, ParamSetBySourceType source_type, ParamPtr optional_setter) {
  // nothing to do
  return;
}

void StringSetParam_ParamOnValidateFunction(StringSetParam &target, const std::vector<std::string> &old_value, std::vector<std::string> &new_value, const std::vector<std::string> &default_value, ParamSetBySourceType source_type) {
  // nothing to do
  return;
}

void StringSetParam_ParamOnParseFunction(StringSetParam &target, std::vector<std::string> &new_value, const std::string &source_value_str, unsigned int &pos, ParamSetBySourceType source_type) {
  const BasicVectorParamParseAssistant &assistant = target.get_assistant();

  // create a modifiable copy of the `source_value_str`; we use a small-strings optimization approach similar to std::string internally.
  const char *svs = source_value_str.c_str();
  const int MAX_SMALLSIZE = 1022;
  char small_buf[MAX_SMALLSIZE + 2];
  const auto slen = strlen(svs);
  char *vs;
  if (slen <= MAX_SMALLSIZE) {
    vs = small_buf;
  } else {
    vs = new char[slen + 2];
  }
  // The value string will have a NUL sentinel at both ends while we process it.
  // This helps simplify and speed up the suffix checks below.
  *vs++ = 0;
  strcpy(vs, svs);
  assert(vs[slen] == 0);

  // start parsing: `vs` points 1 NUL sentinel past the start of the allocated buffer space.

  // skip leading whitespace and any prefix:
  char *s = vs;
  while (isspace(*s))
    s++;
  bool has_display_prefix = false;
  const char *prefix = assistant.fmt_display_prefix.c_str();
  auto prefix_len = strlen(prefix);
  if (prefix_len && 0 == strncmp(s, prefix, prefix_len)) {
    s += prefix_len;
    while (isspace(*s))
      s++;
    has_display_prefix = true;
  } else {
    prefix = assistant.fmt_data_prefix.c_str();
    prefix_len = strlen(prefix);
    if (prefix_len && 0 == strncmp(s, prefix, prefix_len)) {
      s += prefix_len;
      while (isspace(*s))
        s++;
    }
  }
  // plug in a new before-start sentinel!
  // (We can do this safely as we allocated buffer space for this extra sentinel *and* started above by effectively writing a NUL sentinel at string position/index -1!)
  s[-1] = 0;

  // now perform the mirror action by checking and skipping any trailing whitespace and suffix!
  // (When the source string doesn't contain anything else, this code still works great for it will hit the *start sentinnel*!)
  char *e = s + strlen(s) - 1;
  while (isspace(*e))
    e--;
  // plug in a new sentinel!
  *++e = 0;

  const char *suffix = has_display_prefix ? assistant.fmt_display_postfix.c_str() : assistant.fmt_data_postfix.c_str();
  auto suffix_len = strlen(suffix);
  e -= suffix_len;
  if (suffix_len && e >= s && 0 == strcmp(e, suffix)) {
    e--;
    while (isspace(*e))
      e--;
    e++;

    // plug in a new sentinel!
    e[0] = 0;
  } else {
    e += suffix_len;
  }

  // now `s` points at the first value in the input string and `e` points at the end sentinel, just beyond the last value in the input string.
  assert(s == e ? *s == 0 : *s != 0);
  new_value.clear();
  const char *delimiters = assistant.parse_separators.c_str();
  while (s < e) {
    // leading whitespace removel; only relevant for the 2nd element and beyond as we already stripped leading whitespace for the first element in the prefix-skipping code above.
    while (isspace(*s))
      s++;
    auto n = strcspn(s, delimiters);
    char *ele = s + n;
    // plug in a new end-of-element sentinel!
    *ele++ = 0;
    if (n) {
      // there's actual content here, so we can expect trailing whitespace to follow it: trim it.
      char *we = ele - 2;
      while (isspace(*we))
        we--;
      we++;
      // plug in a new end-of-element sentinel!
      *we = 0;
    }
    // we DO NOT accept empty (string) element values!
    if (*s) {
      new_value.push_back(s);
    }
    s = ele;
  }
  // All done, no boogers.
  pos = slen;
}

static inline std::string fmt_stringset_vector(const std::vector<std::string> &value, const char *prefix, const char *suffix, const char *separator) {
  std::string rv;
  rv = prefix;
  for (const std::string &elem : value) {
    rv += elem;
    rv += separator;
  }
  if (value.size()) {
    // we pushed one separator too many: roll back
    for (size_t i = 0, en = strlen(separator); i < en; i++) {
      (void)rv.pop_back();
    }
  }
  rv += suffix;
  return rv;
}

std::string StringSetParam_ParamOnFormatFunction(const StringSetParam &source, const std::vector<std::string> &value, const std::vector<std::string> &default_value, Param::ValueFetchPurpose purpose) {
  const BasicVectorParamParseAssistant &assistant = source.get_assistant();
  switch (purpose) {
      // Fetches the (raw, parseble for re-use via set_value()) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DATA_4_INSPECT:
      // Fetches the (raw, parseble for re-use via set_value() or storing to serialized text data format files) value of the param as a string.
      //
      // NOTE: The part where the documentation says this variant MUST update the parameter usage statistics is
      // handled by the Param class code itself; no need for this callback to handle that part of the deal.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_4_USE:
      return fmt_stringset_vector(value, assistant.fmt_data_prefix.c_str(), assistant.fmt_data_postfix.c_str(), assistant.fmt_data_separator.c_str());

      // Fetches the (formatted for print/display) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_FORMATTED_4_DISPLAY:
      return fmt_stringset_vector(value, assistant.fmt_display_prefix.c_str(), assistant.fmt_display_postfix.c_str(), assistant.fmt_display_separator.c_str());

      // Fetches the (raw, parseble for re-use via set_value()) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DEFAULT_DATA_4_INSPECT:
      return fmt_stringset_vector(default_value, assistant.fmt_data_prefix.c_str(), assistant.fmt_data_postfix.c_str(), assistant.fmt_data_separator.c_str());

      // Fetches the (formatted for print/display) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DEFAULT_DATA_FORMATTED_4_DISPLAY:
      return fmt_stringset_vector(default_value, assistant.fmt_display_prefix.c_str(), assistant.fmt_display_postfix.c_str(), assistant.fmt_display_separator.c_str());

      // Return string representing the type of the parameter value, e.g. "integer".
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_TYPE_INFO:
      return "set of strings";

    default:
      assert(0);
      return nullptr;
  }
}

StringSetParam::BasicVectorTypedParam(const std::vector<std::string> &value, const BasicVectorParamParseAssistant &assistant, THE_4_HANDLERS_PROTO)
    : Param(name, comment, owner, init),
      on_modify_f_(on_modify_f ? on_modify_f : StringSetParam_ParamOnModifyFunction),
      on_validate_f_(on_validate_f ? on_validate_f : StringSetParam_ParamOnValidateFunction),
      on_parse_f_(on_parse_f ? on_parse_f : StringSetParam_ParamOnParseFunction),
      on_format_f_(on_format_f ? on_format_f : StringSetParam_ParamOnFormatFunction),
      value_(value),
      default_(value),
      assistant_(assistant) {
  type_ = STRING_SET_PARAM;
}

StringSetParam::BasicVectorTypedParam(const char *value, const BasicVectorParamParseAssistant &assistant, THE_4_HANDLERS_PROTO)
    : BasicVectorTypedParam(std::vector<std::string>(), assistant, name, comment, owner, init, on_modify_f, on_validate_f, on_parse_f, on_format_f)
{
  unsigned int pos = 0;
  std::string vs(value == nullptr ? "" : value);
  std::vector<std::string> vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, PARAM_VALUE_IS_DEFAULT); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    //set_value(vv, PARAM_VALUE_IS_DEFAULT, nullptr);
    value_ = vv;
  }
}

StringSetParam::operator const std::vector<std::string> &() const {
  return value();
}

StringSetParam::operator const std::vector<std::string> *() const {
  return &value();
}

const char *StringSetParam::c_str() const {
  return value_str(VALSTR_PURPOSE_DATA_4_USE).c_str();
}

bool StringSetParam::empty() const noexcept {
  return value().empty();
}

void StringSetParam::operator=(const std::vector<std::string> &value) {
  set_value(value, get_current_application_default_param_source_type(), nullptr);
}

void StringSetParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
  unsigned int pos = 0;
  std::string vs(v == nullptr ? "" : v);
  std::vector<std::string> vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, source_type); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    set_value(vv, source_type, source);
  }
}

template <>
void StringSetParam::set_value(const std::vector<std::string> &val, ParamSetBySourceType source_type, ParamPtr source) {
  safe_inc(access_counts_.writing);
  // ^^^^^^^ --
  // Our 'writing' statistic counts write ATTEMPTS, in reailty.
  // Any real change is tracked by the 'changing' statistic (see further below)!

  std::vector<std::string> value(val);
  reset_fault();
  // when we fail the validation horribly, the validator will throw an exception and thus abort the (write) action.
  // non-fatal errors may be signaled, in which case the write operation is aborted/skipped, or not signaled (a.k.a. 'silent')
  // in which case the write operation proceeds as if nothing untoward happened inside on_validate_f.
  on_validate_f_(*this, value_, value, default_, source_type);
  if (!has_faulted()) {
    // however, when we failed the validation only in the sense of the value being adjusted/restricted by the validator,
    // then we must set the value as set by the validator anyway, so nothing changes in our workflow here.

    set_ = (source_type > PARAM_VALUE_IS_RESET);
    set_to_non_default_value_ = (value != default_);

    if (value != value_) {
      on_modify_f_(*this, value_, value, default_, source_type, source);
      if (!has_faulted() && value != value_) {
        safe_inc(access_counts_.changing);
        value_ = value;
      }
    }
  }
  // any signaled fault will be visible outside...
}

template <>
const std::vector<std::string> &StringSetParam::value() const noexcept {
  safe_inc(access_counts_.reading);
  return value_;
}

// Optionally the `source_vec` can be used to source the value to reset the parameter to.
// When no source vector is specified, or when the source vector does not specify this
// particular parameter, then our value is reset to the default value which was
// specified earlier in our constructor.
void StringSetParam::ResetToDefault(const ParamsVectorSet *source_vec, ParamSetBySourceType source_type) {
  if (source_vec != nullptr) {
    StringSetParam *source = source_vec->find<StringSetParam>(name_str());
    if (source != nullptr) {
      set_value(source->value(), PARAM_VALUE_IS_RESET, source);
      return;
    }
  }
  set_value(default_, PARAM_VALUE_IS_RESET, nullptr);
}

template<>
std::string StringSetParam::value_str(ValueFetchPurpose purpose) const {
  if (purpose == VALSTR_PURPOSE_DATA_4_USE)
    safe_inc(access_counts_.reading);
  return on_format_f_(*this, value_, default_, purpose);
}

StringSetParam::ParamOnModifyFunction StringSetParam::set_on_modify_handler(StringSetParam::ParamOnModifyFunction on_modify_f) {
  StringSetParam::ParamOnModifyFunction rv = on_modify_f_;
  if (!on_modify_f)
    on_modify_f = StringSetParam_ParamOnModifyFunction;
  on_modify_f_ = on_modify_f;
  return rv;
}
void StringSetParam::clear_on_modify_handler() {
  on_modify_f_ = StringSetParam_ParamOnModifyFunction;
}
StringSetParam::ParamOnValidateFunction StringSetParam::set_on_validate_handler(StringSetParam::ParamOnValidateFunction on_validate_f) {
  StringSetParam::ParamOnValidateFunction rv = on_validate_f_;
  if (!on_validate_f)
    on_validate_f = StringSetParam_ParamOnValidateFunction;
  on_validate_f_ = on_validate_f;
  return rv;
}
void StringSetParam::clear_on_validate_handler() {
  on_validate_f_ = StringSetParam_ParamOnValidateFunction;
}
StringSetParam::ParamOnParseFunction StringSetParam::set_on_parse_handler(StringSetParam::ParamOnParseFunction on_parse_f) {
  StringSetParam::ParamOnParseFunction rv = on_parse_f_;
  if (!on_parse_f)
    on_parse_f = StringSetParam_ParamOnParseFunction;
  on_parse_f_ = on_parse_f;
  return rv;
}
void StringSetParam::clear_on_parse_handler() {
  on_parse_f_ = StringSetParam_ParamOnParseFunction;
}
StringSetParam::ParamOnFormatFunction StringSetParam::set_on_format_handler(StringSetParam::ParamOnFormatFunction on_format_f) {
  StringSetParam::ParamOnFormatFunction rv = on_format_f_;
  if (!on_format_f)
    on_format_f = StringSetParam_ParamOnFormatFunction;
  on_format_f_ = on_format_f;
  return rv;
}
void StringSetParam::clear_on_format_handler() {
  on_format_f_ = StringSetParam_ParamOnFormatFunction;
}

#if 0
std::string StringSetParam::formatted_value_str() const {
  std::string rv = "\u00AB";
  rv += value_;
  rv += "\u00BB";
  return std::move(rv);
}
#endif









//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IntSetParam
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void IntSetParam_ParamOnModifyFunction(IntSetParam &target, const std::vector<int32_t> &old_value, std::vector<int32_t> &new_value, const std::vector<int32_t> &default_value, ParamSetBySourceType source_type, ParamPtr optional_setter) {
  // nothing to do
  return;
}

void IntSetParam_ParamOnValidateFunction(IntSetParam &target, const std::vector<int32_t> &old_value, std::vector<int32_t> &new_value, const std::vector<int32_t> &default_value, ParamSetBySourceType source_type) {
  // nothing to do
  return;
}

void IntSetParam_ParamOnParseFunction(IntSetParam &target, std::vector<int32_t> &new_value, const std::string &source_value_str, unsigned int &pos, ParamSetBySourceType source_type) {
  const BasicVectorParamParseAssistant &assistant = target.get_assistant();

  // create a modifiable copy of the `source_value_str`; we use a small-strings optimization approach similar to std::string internally.
  const char *svs = source_value_str.c_str();
  const int MAX_SMALLSIZE = 1022;
  char small_buf[MAX_SMALLSIZE + 2];
  const auto slen = strlen(svs);
  char *vs;
  // the std::unique_ptr ensures this heap space is properly released at end of scope, no matter what happens next in this code.
  std::unique_ptr<char> vs_dropper;
  if (slen <= MAX_SMALLSIZE) {
    vs = small_buf;
  } else {
    vs_dropper.reset(new char[slen + 2]);
    vs = vs_dropper.get();
  }
  // The value string will have a NUL sentinel at both ends while we process it.
  // This helps simplify and speed up the suffix checks below.
  *vs++ = 0;
  strcpy(vs, svs);
  assert(vs[slen] == 0);

  // start parsing: `vs` points 1 NUL sentinel past the start of the allocated buffer space.

  // skip leading whitespace and any prefix:
  char *s = vs;
  while (isspace(*s))
    s++;
  bool has_display_prefix = false;
  const char *prefix = assistant.fmt_display_prefix.c_str();
  auto prefix_len = strlen(prefix);
  if (prefix_len && 0 == strncmp(s, prefix, prefix_len)) {
    s += prefix_len;
    while (isspace(*s))
      s++;
    has_display_prefix = true;
  } else {
    prefix = assistant.fmt_data_prefix.c_str();
    prefix_len = strlen(prefix);
    if (prefix_len && 0 == strncmp(s, prefix, prefix_len)) {
      s += prefix_len;
      while (isspace(*s))
        s++;
    }
  }
  // plug in a new before-start sentinel!
  // (We can do this safely as we allocated buffer space for this extra sentinel *and* started above by effectively writing a NUL sentinel at string position/index -1!)
  s[-1] = 0;

  // now perform the mirror action by checking and skipping any trailing whitespace and suffix!
  // (When the source string doesn't contain anything else, this code still works great for it will hit the *start sentinnel*!)
  char *e = s + strlen(s) - 1;
  while (isspace(*e))
    e--;
  // plug in a new sentinel!
  *++e = 0;

  const char *suffix = has_display_prefix ? assistant.fmt_display_postfix.c_str() : assistant.fmt_data_postfix.c_str();
  auto suffix_len = strlen(suffix);
  e -= suffix_len;
  if (suffix_len && e >= s && 0 == strcmp(e, suffix)) {
    e--;
    while (isspace(*e))
      e--;
    e++;

    // plug in a new sentinel!
    e[0] = 0;
  } else {
    e += suffix_len;
  }

  // now `s` points at the first value in the input string and `e` points at the end sentinel, just beyond the last value in the input string.
  assert(s == e ? *s == 0 : *s != 0);
  new_value.clear();
  const char *delimiters = assistant.parse_separators.c_str();
  while (s < e) {
    // leading whitespace removel; only relevant for the 2nd element and beyond as we already stripped leading whitespace for the first element in the prefix-skipping code above.
    while (isspace(*s))
      s++;
    auto n = strcspn(s, delimiters);
    char *ele = s + n;
    // plug in a new end-of-element sentinel!
    *ele++ = 0;
    if (n) {
      // there's actual content here, so we can expect trailing whitespace to follow it: trim it.
      char *we = ele - 2;
      while (isspace(*we))
        we--;
      we++;
      // plug in a new end-of-element sentinel!
      *we = 0;
    }
    // we DO NOT accept empty (string) element values!
    if (*s) {
      // IntParam_ParamOnParseFunction(...) derivative chunk, parsing a single integer:
      char *endptr = nullptr;
      // https://stackoverflow.com/questions/25315191/need-to-clean-up-errno-before-calling-function-then-checking-errno?rq=3
#if defined(_MSC_VER)
      _set_errno(E_OK);
#else
      errno = E_OK;
#endif
      auto parsed_value = strtol(s, &endptr, 10);
      auto ec = errno;
      int32_t val = int32_t(parsed_value);
      bool good = (endptr != nullptr && ec == E_OK);
      std::string errmsg;
      if (good) {
        // check to make sure the tail is legal: all whitespace has been stripped already, so the tail must be empty!
        // This also takes care of utter parse failure (when not already signaled via `errno`) when strtol() returns 0 and sets `endptr == s`.
        good = (*endptr == '\0');

        // check if our parsed value is out of legal range: we check the type conversion as that is faster than checking against [INT32_MIN, INT32_MAX].
        if (val != parsed_value && ec == E_OK) {
          good = false;
          ec = ERANGE;
        }
      } else {
        // failed to parse value.
        if (!endptr)
          endptr = s;
      }

      if (!good) {
        pos = endptr - vs;

        // produce a sensible snippet of this element plus what follows...
        //
        // Don't use `pos` as that one points half-way into the current element, at the start of the error.
        // We however want to show the overarching 'element plus 'tail', including the current element,
        // which failed to parse.
        std::string tailstr(svs + (s - vs));
        // sane heuristic for a tail? say... 40 characters, tops?
        if (tailstr.size() > 40) {
          tailstr.resize(40 - 18);
          tailstr += " ...(continued)...";
        }

        target.fault();
        if (ec != E_OK) {
          if (ec == ERANGE) {
            errmsg = fmt::format("the parser stopped at item #{} (\"{}\") and reported an integer value overflow (ERANGE); we accept decimal values between {} and {}.", new_value.size(), tailstr, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
          } else {
            errmsg = fmt::format("the parser stopped at item #{} (\"{}\") and reported \"{}\" (errno: {})", new_value.size(), tailstr, strerror(ec), ec);
          }
        } else if (endptr > vs) {
          errmsg = fmt::format("the parser stopped early at item #{} (\"{}\"): the tail end (\"{}\") of the element value string remains", new_value.size(), tailstr, endptr);
        } else {
          errmsg = fmt::format("the parser was unable to parse anything at all at item #{} (\"{}\")", new_value.size(), tailstr);
        }
        tprintError("ERROR: error parsing {} parameter '{}' value (\"{}\") to {}; {}. The parameter value will not be adjusted: the preset value ({}) will be used instead.\n", ParamUtils::GetApplicationName(), target.name_str(), source_value_str, target.value_type_str(), errmsg, target.formatted_value_str());

        return; // Note: vs_dropper will take care of our heap-allocated scratch buffer for us.
      }

      new_value.push_back(val);
    }
    s = ele;
  }
  // All done, no boogers.
  pos = slen;
}

static inline std::string fmt_stringset_vector(const std::vector<int32_t> &value, const char *prefix, const char *suffix, const char *separator) {
  std::string rv;
  rv = prefix;
  for (int32_t elem : value) {
    rv += elem;
    rv += separator;
  }
  if (value.size()) {
    // we pushed one separator too many: roll back
    for (size_t i = 0, en = strlen(separator); i < en; i++) {
      (void)rv.pop_back();
    }
  }
  rv += suffix;
  return rv;
}

std::string IntSetParam_ParamOnFormatFunction(const IntSetParam &source, const std::vector<int32_t> &value, const std::vector<int32_t> &default_value, Param::ValueFetchPurpose purpose) {
  const BasicVectorParamParseAssistant &assistant = source.get_assistant();
  switch (purpose) {
      // Fetches the (raw, parseble for re-use via set_value()) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DATA_4_INSPECT:
      // Fetches the (raw, parseble for re-use via set_value() or storing to serialized text data format files) value of the param as a string.
      //
      // NOTE: The part where the documentation says this variant MUST update the parameter usage statistics is
      // handled by the Param class code itself; no need for this callback to handle that part of the deal.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_4_USE:
      return fmt_stringset_vector(value, assistant.fmt_data_prefix.c_str(), assistant.fmt_data_postfix.c_str(), assistant.fmt_data_separator.c_str());

      // Fetches the (formatted for print/display) value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DATA_FORMATTED_4_DISPLAY:
      return fmt_stringset_vector(value, assistant.fmt_display_prefix.c_str(), assistant.fmt_display_postfix.c_str(), assistant.fmt_display_separator.c_str());

      // Fetches the (raw, parseble for re-use via set_value()) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_RAW_DEFAULT_DATA_4_INSPECT:
      return fmt_stringset_vector(default_value, assistant.fmt_data_prefix.c_str(), assistant.fmt_data_postfix.c_str(), assistant.fmt_data_separator.c_str());

      // Fetches the (formatted for print/display) default value of the param as a string.
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_DEFAULT_DATA_FORMATTED_4_DISPLAY:
      return fmt_stringset_vector(default_value, assistant.fmt_display_prefix.c_str(), assistant.fmt_display_postfix.c_str(), assistant.fmt_display_separator.c_str());

      // Return string representing the type of the parameter value, e.g. "integer".
    case Param::ValueFetchPurpose::VALSTR_PURPOSE_TYPE_INFO:
      return "set of integers";

    default:
      assert(0);
      return nullptr;
  }
}

IntSetParam::BasicVectorTypedParam(const std::vector<int32_t> &value, const BasicVectorParamParseAssistant &assistant, THE_4_HANDLERS_PROTO)
    : Param(name, comment, owner, init),
      on_modify_f_(on_modify_f ? on_modify_f : IntSetParam_ParamOnModifyFunction),
      on_validate_f_(on_validate_f ? on_validate_f : IntSetParam_ParamOnValidateFunction),
      on_parse_f_(on_parse_f ? on_parse_f : IntSetParam_ParamOnParseFunction),
      on_format_f_(on_format_f ? on_format_f : IntSetParam_ParamOnFormatFunction),
      value_(value),
      default_(value),
      assistant_(assistant) {
  type_ = STRING_SET_PARAM;
}

IntSetParam::BasicVectorTypedParam(const char *value, const BasicVectorParamParseAssistant &assistant, THE_4_HANDLERS_PROTO)
    : BasicVectorTypedParam(std::vector<int32_t>(), assistant, name, comment, owner, init, on_modify_f, on_validate_f, on_parse_f, on_format_f) {
  unsigned int pos = 0;
  std::string vs(value == nullptr ? "" : value);
  std::vector<int32_t> vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, PARAM_VALUE_IS_DEFAULT); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    // set_value(vv, PARAM_VALUE_IS_DEFAULT, nullptr);
    value_ = vv;
  }
}

IntSetParam::operator const std::vector<int32_t> &() const {
  return value();
}

IntSetParam::operator const std::vector<int32_t> *() const {
  return &value();
}

const char *IntSetParam::c_str() const {
  return value_str(VALSTR_PURPOSE_DATA_4_USE).c_str();
}

bool IntSetParam::empty() const noexcept {
  return value().empty();
}

void IntSetParam::operator=(const std::vector<int32_t> &value) {
  set_value(value, get_current_application_default_param_source_type(), nullptr);
}

void IntSetParam::set_value(const char *v, ParamSetBySourceType source_type, ParamPtr source) {
  unsigned int pos = 0;
  std::string vs(v == nullptr ? "" : v);
  std::vector<int32_t> vv;
  reset_fault();
  on_parse_f_(*this, vv, vs, pos, source_type); // minor(=recoverable) errors shall have signalled by calling fault()
  // when a signaled parse error occurred, we won't write the (faulty/undefined) value:
  if (!has_faulted()) {
    set_value(vv, source_type, source);
  }
}

template <>
void IntSetParam::set_value(const std::vector<int32_t> &val, ParamSetBySourceType source_type, ParamPtr source) {
  safe_inc(access_counts_.writing);
  // ^^^^^^^ --
  // Our 'writing' statistic counts write ATTEMPTS, in reailty.
  // Any real change is tracked by the 'changing' statistic (see further below)!

  std::vector<int32_t> value(val);
  reset_fault();
  // when we fail the validation horribly, the validator will throw an exception and thus abort the (write) action.
  // non-fatal errors may be signaled, in which case the write operation is aborted/skipped, or not signaled (a.k.a. 'silent')
  // in which case the write operation proceeds as if nothing untoward happened inside on_validate_f.
  on_validate_f_(*this, value_, value, default_, source_type);
  if (!has_faulted()) {
    // however, when we failed the validation only in the sense of the value being adjusted/restricted by the validator,
    // then we must set the value as set by the validator anyway, so nothing changes in our workflow here.

    set_ = (source_type > PARAM_VALUE_IS_RESET);
    set_to_non_default_value_ = (value != default_);

    if (value != value_) {
      on_modify_f_(*this, value_, value, default_, source_type, source);
      if (!has_faulted() && value != value_) {
        safe_inc(access_counts_.changing);
        value_ = value;
      }
    }
  }
  // any signaled fault will be visible outside...
}

template <>
const std::vector<int32_t> &IntSetParam::value() const noexcept {
  safe_inc(access_counts_.reading);
  return value_;
}

// Optionally the `source_vec` can be used to source the value to reset the parameter to.
// When no source vector is specified, or when the source vector does not specify this
// particular parameter, then our value is reset to the default value which was
// specified earlier in our constructor.
void IntSetParam::ResetToDefault(const ParamsVectorSet *source_vec, ParamSetBySourceType source_type) {
  if (source_vec != nullptr) {
    IntSetParam *source = source_vec->find<IntSetParam>(name_str());
    if (source != nullptr) {
      set_value(source->value(), PARAM_VALUE_IS_RESET, source);
      return;
    }
  }
  set_value(default_, PARAM_VALUE_IS_RESET, nullptr);
}

template <>
std::string IntSetParam::value_str(ValueFetchPurpose purpose) const {
  if (purpose == VALSTR_PURPOSE_DATA_4_USE)
    safe_inc(access_counts_.reading);
  return on_format_f_(*this, value_, default_, purpose);
}

IntSetParam::ParamOnModifyFunction IntSetParam::set_on_modify_handler(IntSetParam::ParamOnModifyFunction on_modify_f) {
  IntSetParam::ParamOnModifyFunction rv = on_modify_f_;
  if (!on_modify_f)
    on_modify_f = IntSetParam_ParamOnModifyFunction;
  on_modify_f_ = on_modify_f;
  return rv;
}
void IntSetParam::clear_on_modify_handler() {
  on_modify_f_ = IntSetParam_ParamOnModifyFunction;
}
IntSetParam::ParamOnValidateFunction IntSetParam::set_on_validate_handler(IntSetParam::ParamOnValidateFunction on_validate_f) {
  IntSetParam::ParamOnValidateFunction rv = on_validate_f_;
  if (!on_validate_f)
    on_validate_f = IntSetParam_ParamOnValidateFunction;
  on_validate_f_ = on_validate_f;
  return rv;
}
void IntSetParam::clear_on_validate_handler() {
  on_validate_f_ = IntSetParam_ParamOnValidateFunction;
}
IntSetParam::ParamOnParseFunction IntSetParam::set_on_parse_handler(IntSetParam::ParamOnParseFunction on_parse_f) {
  IntSetParam::ParamOnParseFunction rv = on_parse_f_;
  if (!on_parse_f)
    on_parse_f = IntSetParam_ParamOnParseFunction;
  on_parse_f_ = on_parse_f;
  return rv;
}
void IntSetParam::clear_on_parse_handler() {
  on_parse_f_ = IntSetParam_ParamOnParseFunction;
}
IntSetParam::ParamOnFormatFunction IntSetParam::set_on_format_handler(IntSetParam::ParamOnFormatFunction on_format_f) {
  IntSetParam::ParamOnFormatFunction rv = on_format_f_;
  if (!on_format_f)
    on_format_f = IntSetParam_ParamOnFormatFunction;
  on_format_f_ = on_format_f;
  return rv;
}
void IntSetParam::clear_on_format_handler() {
  on_format_f_ = IntSetParam_ParamOnFormatFunction;
}

#if 0
std::string IntSetParam::formatted_value_str() const {
  std::string rv = "\u00AB";
  rv += value_;
  rv += "\u00BB";
  return std::move(rv);
}
#endif





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

template <>
IntParam* ParamUtils::FindParam<IntParam>(
    const char* name,
    const ParamsVectorSet& set
) {
  return set.find<IntParam>(name);
}

template <>
BoolParam* ParamUtils::FindParam<BoolParam>(
    const char* name,
    const ParamsVectorSet& set
) {
  return set.find<BoolParam>(name);
}

template <>
DoubleParam* ParamUtils::FindParam<DoubleParam>(
    const char* name,
    const ParamsVectorSet& set
) {
  return set.find<DoubleParam>(name);
}

template <>
StringParam* ParamUtils::FindParam<StringParam>(
    const char* name,
    const ParamsVectorSet& set
) {
  return set.find<StringParam>(name);
}

template <>
StringSetParam *ParamUtils::FindParam<StringSetParam>(
    const char *name,
    const ParamsVectorSet &set) {
  return set.find<StringSetParam>(name);
}

template <>
IntSetParam *ParamUtils::FindParam<IntSetParam>(
    const char *name,
    const ParamsVectorSet &set) {
  return set.find<IntSetParam>(name);
}

template <>
Param* ParamUtils::FindParam<Param>(
    const char* name,
    const ParamsVectorSet& set
) {
  return set.find<Param>(name);
}

Param* ParamUtils::FindParam(
  const char* name,
  const ParamsVectorSet& set,
  ParamType accepted_types_mask
) {
  return set.find(name, accepted_types_mask);
}


#if 0
template <ParamDerivativeType T>
T* ParamUtils::FindParam(
  const char* name,
  const ParamsVector& set
) {
  ParamsVectorSet pvec({ &set });

  return FindParam<T>(
    name,
    pvec
  );
}
#endif


Param* ParamUtils::FindParam(
  const char* name,
  const ParamsVector& set,
  ParamType accepted_types_mask
) {
  ParamsVectorSet pvec;
  const ParamsVector* set_ptr = &set;
  pvec.add(const_cast<ParamsVector*>(set_ptr));

  return FindParam(
    name,
    pvec,
    accepted_types_mask
  );
}


template <>
bool ParamUtils::SetParam<int32_t>(
    const char *name, const int32_t value,
    const ParamsVectorSet &set,
    ParamSetBySourceType source_type, ParamPtr source) {
  {
    IntParam *param = FindParam<IntParam>(name, set);
    if (param != nullptr) {
      param->set_value(value, source_type, source);
      return !param->has_faulted();
    }
  }
  {
    Param *param = FindParam<Param>(name, set);
    if (param != nullptr) {
      switch (param->type()) {
        case INT_PARAM:
          assert(0);
          break;

        case BOOL_PARAM: {
          BoolParam *bp = static_cast<BoolParam *>(param);
          bp->set_value(value != 0, source_type, source);
          return !bp->has_faulted();
        }

        case DOUBLE_PARAM: {
          DoubleParam *dp = static_cast<DoubleParam *>(param);
          dp->set_value(value, source_type, source);
          return !dp->has_faulted();
        }

        case STRING_PARAM:
        case CUSTOM_PARAM:
        case CUSTOM_SET_PARAM:
        default: {
          std::string vs = fmt::format("{}", value);
          param->set_value(vs, source_type, source);
          return !param->has_faulted();
        }

        case STRING_SET_PARAM: {
          std::vector<std::string> v;
          std::string vs = fmt::format("{}", value);
          v.push_back(vs);
          StringSetParam *p = static_cast<StringSetParam *>(param);
          p->set_value(v, source_type, source);
          return !p->has_faulted();
        }

        case INT_SET_PARAM: {
          std::vector<int32_t> iv;
          iv.push_back(value);
          IntSetParam *ivp = static_cast<IntSetParam *>(param);
          ivp->set_value(iv, source_type, source);
          return !ivp->has_faulted();
        }

        case BOOL_SET_PARAM: {
          std::vector<bool> bv;
          bv.push_back(value != 0);
          BoolSetParam *bvp = static_cast<BoolSetParam *>(param);
          bvp->set_value(bv, source_type, source);
          return !bvp->has_faulted();
        }

        case DOUBLE_SET_PARAM: {
          std::vector<double> dv;
          dv.push_back(value);
          DoubleSetParam *dvp = static_cast<DoubleSetParam *>(param);
          dvp->set_value(dv, source_type, source);
          return !dvp->has_faulted();
        }
      }
    }
  }
  return false;
}

template <>
bool ParamUtils::SetParam<bool>(
    const char* name, const bool value,
    const ParamsVectorSet& set,
    ParamSetBySourceType source_type, ParamPtr source
) {
  {
    BoolParam* param = FindParam<BoolParam>(name, set);
    if (param != nullptr) {
      param->set_value(value, source_type, source);
      return !param->has_faulted();
    }
  }
  {
    Param* param = FindParam<Param>(name, set);
    if (param != nullptr) {
      switch (param->type()) {
        case BOOL_PARAM:
          assert(0);
          break;

        case INT_PARAM: {
          IntParam *bp = static_cast<IntParam *>(param);
          bp->set_value(value, source_type, source);
          return !bp->has_faulted();
        }

        case DOUBLE_PARAM: {
          DoubleParam *dp = static_cast<DoubleParam *>(param);
          dp->set_value(value, source_type, source);
          return !dp->has_faulted();
        }

        case STRING_PARAM:
        case CUSTOM_PARAM:
        case CUSTOM_SET_PARAM:
        default: {
          const char *vs = (value ? "true" : "false");
          param->set_value(vs, source_type, source);
          return !param->has_faulted();
        }

        case STRING_SET_PARAM: {
          std::vector<std::string> v;
          const char *vs = (value ? "true" : "false");
          v.push_back(vs);
          StringSetParam *p = static_cast<StringSetParam *>(param);
          p->set_value(v, source_type, source);
          return !p->has_faulted();
        }

        case INT_SET_PARAM: {
          std::vector<int32_t> iv;
          iv.push_back(value);
          IntSetParam *ivp = static_cast<IntSetParam *>(param);
          ivp->set_value(iv, source_type, source);
          return !ivp->has_faulted();
        }

        case BOOL_SET_PARAM: {
          std::vector<bool> bv;
          bv.push_back(value);
          BoolSetParam *bvp = static_cast<BoolSetParam *>(param);
          bvp->set_value(bv, source_type, source);
          return !bvp->has_faulted();
        }

        case DOUBLE_SET_PARAM: {
          std::vector<double> dv;
          dv.push_back(value);
          DoubleSetParam *dvp = static_cast<DoubleSetParam *>(param);
          dvp->set_value(dv, source_type, source);
          return !dvp->has_faulted();
        }
      }
    }
  }
  return false;
}

template <>
bool ParamUtils::SetParam<double>(
    const char* name, const double value,
    const ParamsVectorSet& set,
    ParamSetBySourceType source_type, ParamPtr source
) {
  {
    DoubleParam* param = FindParam<DoubleParam>(name, set);
    if (param != nullptr) {
      param->set_value(value, source_type, source);
      return !param->has_faulted();
    }
  }
  {
    Param* param = FindParam<Param>(name, set);
    if (param != nullptr) {
      switch (param->type()) {
        case DOUBLE_PARAM:
          assert(0);
          break;

        case BOOL_PARAM: {
          BoolParam *bp = static_cast<BoolParam *>(param);
          // reckon with the inaccuracy/noise inherent in IEEE754 calculus.
          bool v = (value > -FLT_EPSILON && value < FLT_EPSILON);
          bp->set_value(v, source_type, source);
          return !bp->has_faulted();
        }

        case INT_PARAM: {
          IntParam *dp = static_cast<IntParam *>(param);
          auto v = round(value);
          if (v < INT32_MIN || v > INT32_MAX)
            return false;
          dp->set_value(int32_t(v), source_type, source);
          return !dp->has_faulted();
        }

        case STRING_PARAM:
        case CUSTOM_PARAM:
        case CUSTOM_SET_PARAM:
        default: {
          std::string vs = fmt::format("{}", value);
          param->set_value(vs, source_type, source);
          return !param->has_faulted();
        }

        case STRING_SET_PARAM: {
          std::vector<std::string> v;
          std::string vs = fmt::format("{}", value);
          v.push_back(vs);
          StringSetParam *p = static_cast<StringSetParam *>(param);
          p->set_value(v, source_type, source);
          return !p->has_faulted();
        }

        case INT_SET_PARAM: {
          std::vector<int32_t> iv;
          auto v = round(value);
          if (v < INT32_MIN || v > INT32_MAX)
            return false;
          iv.push_back(v);
          IntSetParam *ivp = static_cast<IntSetParam *>(param);
          ivp->set_value(iv, source_type, source);
          return !ivp->has_faulted();
        }

        case BOOL_SET_PARAM: {
          std::vector<bool> bv;
          // reckon with the inaccuracy/noise inherent in IEEE754 calculus.
          bool v = (value > -FLT_EPSILON && value < FLT_EPSILON);
          bv.push_back(v);
          BoolSetParam *bvp = static_cast<BoolSetParam *>(param);
          bvp->set_value(bv, source_type, source);
          return !bvp->has_faulted();
        }

        case DOUBLE_SET_PARAM: {
          std::vector<double> dv;
          dv.push_back(value);
          DoubleSetParam *dvp = static_cast<DoubleSetParam *>(param);
          dvp->set_value(dv, source_type, source);
          return !dvp->has_faulted();
        }
      }
    }
  }
  return false;
}

bool ParamUtils::SetParam(
    const char* name, const std::string &value,
    const ParamsVectorSet& set,
    ParamSetBySourceType source_type, ParamPtr source
) {
  {
    StringParam* param = FindParam<StringParam>(name, set);
    if (param != nullptr) {
      param->set_value(value, source_type, source);
      return !param->has_faulted();
    }
  }
  {
    Param* param = FindParam<Param>(name, set);
    if (param != nullptr) {
      param->set_value(value, source_type, source);
      return !param->has_faulted();
    }
  }
  return false;
}

bool ParamUtils::SetParam(
    const char* name, const char *value,
    const ParamsVectorSet& set,
    ParamSetBySourceType source_type, ParamPtr source
) {
  Param* param = FindParam(name, set, ANY_TYPE_PARAM);
  if (param != nullptr) {
    param->set_value(value, source_type, source);
    return !param->has_faulted();
  }
  return false;
}


#if 0
template <ParamAcceptableValueType T>
bool ParamUtils::SetParam(
	const char* name, const T value,
	ParamsVector& set,
	ParamSetBySourceType source_type, ParamPtr source
) {
	ParamsVectorSet pvec({ &set });
	return SetParam<T>(name, value, pvec, source_type, source);
}
#endif


bool ParamUtils::SetParam(
  const char* name, const char* value,
  ParamsVector& set,
  ParamSetBySourceType source_type, ParamPtr source
) {
  ParamsVectorSet pvec({ &set });
  return SetParam(name, value, pvec, source_type, source);
}


void ParamUtils::PrintParams(FILE* fp, const ParamsVectorSet& set, bool print_info ) {

}

void ParamUtils::ReportParamsUsageStatistics(FILE* fp, const ParamsVectorSet& set, const char* section_title ) {

}

void ParamUtils::ResetToDefaults(const ParamsVectorSet& set, ParamSetBySourceType source_type) {

}






//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConfigFile
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

ConfigFile::ConfigFile(const char *path)
{
	if (!path || !*path) {
		_f = nullptr;
		return;
	}

	_f = nullptr;

	if (strieq(path, "/dev/stdin") || strieq(path, "stdin") || strieq(path, "-") || strieq(path, "1"))
		_f = stdin;
	else {
		_f = fopenUtf8(path, "r");
		if (!_f) {
			tprintError("Cannot open file: '{}'\n", path);
		}
	}
}

ConfigFile::~ConfigFile() {
	if (_f) {
		if (_f != stdin) {
			fclose(_f);
		} else {
			fflush(_f);
		}
	}
}

FILE *ConfigFile::operator()() const {
	return _f;
}






//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ReportFile
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

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
