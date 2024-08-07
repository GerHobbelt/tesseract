///////////////////////////////////////////////////////////////////////
// File:        lm_state.cpp
// Description: Structures and functionality for capturing the state of
//              segmentation search guided by the language model.
// Author:      Rika Antonova
//
// (C) Copyright 2012, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////

#include <tesseract/preparation.h> // compiler config, etc.

#include "lm_state.h"

namespace tesseract {

void ViterbiStateEntry::Print(const char *msg) const {
  std::string s = fmt::format("{} ViterbiStateEntry", msg);
  if (updated) {
    s += fmt::format("(NEW)");
  }
  if (this->debug_str != nullptr) {
    s += fmt::format(" str={}", this->debug_str->c_str());
  }
  s += fmt::format(" with ratings_sum={} length={} cost={}", this->ratings_sum, this->length, this->cost);
  if (this->top_choice_flags) {
    s += fmt::format(" top_choice_flags={}", this->top_choice_flags);
  }
  if (!this->Consistent()) {
    s += fmt::format(" inconsistent=(punc {} case {} chartype {} script {} font {})",
            this->consistency_info.NumInconsistentPunc(),
            this->consistency_info.NumInconsistentCase(),
            this->consistency_info.NumInconsistentChartype(),
            this->consistency_info.inconsistent_script, 
            this->consistency_info.inconsistent_font);
  }
  if (this->dawg_info) {
    s += fmt::format(" permuter={}", this->dawg_info->permuter);
  }
  if (this->ngram_info) {
    s += fmt::format(" ngram_cl_cost={} context={} ngram pruned={}",
            this->ngram_info->ngram_and_classifier_cost, 
            this->ngram_info->context.c_str(),
            this->ngram_info->pruned);
  }
  if (this->associate_stats.shape_cost > 0.0f) {
    s += fmt::format(" shape_cost={}", this->associate_stats.shape_cost);
  }
  tprintDebug("{} {}\n", s, XHeightConsistencyEnumName[this->consistency_info.xht_decision]);
}

/// Clears the viterbi search state back to its initial conditions.
void LanguageModelState::Clear() {
  viterbi_state_entries.clear();
  viterbi_state_entries_prunable_length = 0;
  viterbi_state_entries_prunable_max_cost = FLT_MAX;
  viterbi_state_entries_length = 0;
}

void LanguageModelState::Print(const char *msg) {
  tprintDebug("{} VSEs (max_cost={} prn_len={} tot_len={}):\n", msg,
          viterbi_state_entries_prunable_max_cost, 
          viterbi_state_entries_prunable_length,
          viterbi_state_entries_length);
  ViterbiStateEntry_IT vit(&viterbi_state_entries);
  for (vit.mark_cycle_pt(); !vit.cycled_list(); vit.forward()) {
    vit.data()->Print("");
  }
}

} // namespace tesseract
