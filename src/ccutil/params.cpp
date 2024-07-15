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

#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/params.h>

namespace tesseract {

#if 0









// When `section_title` is NULL, this will report the lump sum parameter usage for the entire run.
// When `section_title` is NOT NULL, this will only report the parameters that were actually used (R/W) during the last section of the run, i.e.
// since the previous invocation of this reporting method (or when it hasn't been called before: the start of the application).
void ParamUtils::ReportParamsUsageStatistics(FILE *f, const ParamsVectors *member_params, int section_level, const char *section_title)
{
  std::unique_ptr<ParamsReportWriter> writer;

  TPrintGroupLinesTillEndOfScope push;

  if (f != nullptr) {
    writer.reset(new ParamsReportFileDuoWriter(f));
  } else {
    writer.reset(new ParamsReportDefaultWriter());
  }
  ReportParamsUsageStatistics(*writer, member_params, section_level, section_title);
}


// When `section_title` is NULL, this will report the lump sum parameter usage for the entire run.
// When `section_title` is NOT NULL, this will only report the parameters that were actually used (R/W) during the last section of the run, i.e.
// since the previous invocation of this reporting method (or when it hasn't been called before: the start of the application).
void ParamUtils::ReportParamsUsageStatistics(ParamsReportWriter &writer, const ParamsVectors *member_params, int section_level, const char *section_title)
{
  bool is_section_subreport = (section_title != nullptr);

  writer.Write(fmt::format("\n\n"
                            "{}Tesseract Parameter Usage Statistics{}: which params have been relevant?\n"
                            "----------------------------------------------------------------------\n"
                            "(WR legenda: `.`: zero/nil; `w`: written once, `W`: ~ twice or more; `r` = read once, `R`: ~ twice or more)\n"
                            "\n\n",
                            (is_section_subreport ? std::string(std::max(1, section_level + 1), '#').append(" ") : ""),
                            (is_section_subreport ? fmt::format(" for section: {}", section_title) : "")));

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
