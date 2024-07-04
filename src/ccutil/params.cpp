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

#if 0









class ParamsReportDefaultWriter : public ParamsReportWriter,
                                  protected TPrintGroupLinesTillEndOfScope {
public:
  ParamsReportDefaultWriter()
      : ParamsReportWriter(nullptr), TPrintGroupLinesTillEndOfScope() {}
  virtual ~ParamsReportDefaultWriter() = default;

  virtual void Write(const std::string message) {
    tprintDebug("{}", message);
  }

protected:
};

class ParamsReportFileDuoWriter : public ParamsReportWriter,
                                  protected TPrintGroupLinesTillEndOfScope {
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

  writer->Write(fmt::format("\n\n"
                            "## Tesseract Parameter Usage Statistics{}: which params have been relevant?\n"
                            "----------------------------------------------------------------------\n"
                            "(WR legenda: `.`: zero/nil; `w`: written once, `W`: ~ twice or more; `r` = read once, `R`: ~ twice or more)\n"
                            "\n\n",
                            (section_title != nullptr ? fmt::format(" for section: {}", section_title) : "")));

  // first collect all parameter names:

  typedef enum {
    INT_PARAM = 0,
    BOOL_PARAM,
    DOUBLE_PARAM,
    STRING_PARAM,
  } param_type_t;

  typedef struct param_info {
    const char* name;
    bool global;
    param_type_t type;
    Param* ref;
  } param_info_t;

  std::vector<param_info_t> param_names;

  if (member_params != nullptr) {
    for (auto p : member_params->int_params_c()) {
      param_names.push_back({ p->name_str(), false, INT_PARAM, p });
    }
    for (auto p : member_params->bool_params_c()) {
      param_names.push_back({ p->name_str(), false, BOOL_PARAM, p });
    }
    for (auto p : member_params->string_params_c()) {
      param_names.push_back({ p->name_str(), false, STRING_PARAM, p });
    }
    for (auto p : member_params->double_params_c()) {
      param_names.push_back({ p->name_str(), false, DOUBLE_PARAM, p });
    }
  }









#endif


} // namespace tesseract
