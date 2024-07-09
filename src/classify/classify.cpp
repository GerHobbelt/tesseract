///////////////////////////////////////////////////////////////////////
// File:        classify.cpp
// Description: classify class.
// Author:      Samuel Charron
//
// (C) Copyright 2006, Google Inc.
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

#include "classify.h"

#if DISABLED_LEGACY_ENGINE

#  include <string.h>

namespace tesseract {

Classify::Classify()
    : INT_MEMBER(classify_debug_level, 0, "Classify debug level (0..3)", params())
    , INT_MEMBER(tess_debug_lstm, 0, "Debug LSTM internals (0..2)", params())

    , BOOL_MEMBER(classify_bln_numeric_mode, 0, "Assume the input is numbers [0-9].", params())

    , DOUBLE_MEMBER(classify_max_rating_ratio, 1.5, "Veto ratio between classifier ratings",
                  params())

    , DOUBLE_MEMBER(classify_max_certainty_margin, 5.5,
                  "Veto difference between classifier certainties", params())

    , dict_(this) {}

Classify::~Classify() {}

} // namespace tesseract

#else // DISABLED_LEGACY_ENGINE not defined

#  include <cstring>
#  include "fontinfo.h"
#  include "intproto.h"
#  include "mfoutline.h"
#  include "scrollview.h"
#  include "shapeclassifier.h"
#  include "shapetable.h"
#  include "unicity_table.h"

namespace tesseract {
Classify::Classify()
    : BOOL_MEMBER(allow_blob_division, true, "Use divisible blobs chopping", params())
    , BOOL_MEMBER(prioritize_division, false, "Prioritize blob division over chopping",
                  params())
    , BOOL_MEMBER(classify_enable_learning, true, "Enable adaptive classifier", params())
    , INT_MEMBER(classify_debug_level, 0, "Classify debug level (0..3)", params())
	, INT_MEMBER(tess_debug_lstm, 0, "Debug LSTM internals (0..2)", params())
	, INT_MEMBER(classify_norm_method, character, "Normalization Method   ...", params())
    , DOUBLE_MEMBER(classify_char_norm_range, 0.2, "Character Normalization Range ...",
                    params())
    , DOUBLE_MEMBER(classify_max_rating_ratio, 1.5, "Veto ratio between classifier ratings",
                    params())
    , DOUBLE_MEMBER(classify_max_certainty_margin, 5.5,
                    "Veto difference between classifier certainties", params())
    , BOOL_MEMBER(tess_cn_matching, 0, "Character Normalized Matching", params())
    , BOOL_MEMBER(tess_bn_matching, 0, "Baseline Normalized Matching", params())
    , BOOL_MEMBER(classify_enable_adaptive_matcher, 1, "Enable adaptive classifier", params())
    , BOOL_MEMBER(classify_use_pre_adapted_templates, 0, "Use pre-adapted classifier templates",
                  params())
    , BOOL_MEMBER(classify_save_adapted_templates, 0, "Save adapted templates to a file",
                  params())
    , BOOL_MEMBER(classify_enable_adaptive_debugger, 0, "Enable match debugger", params())
    , BOOL_MEMBER(classify_nonlinear_norm, 0, "Non-linear stroke-density normalization",
                  params())
    , INT_MEMBER(matcher_debug_level, 0, "Matcher Debug Level (0..3)", params())
    , INT_MEMBER(matcher_debug_flags, 0, "Matcher Debug Flags", params())
    , INT_MEMBER(classify_learning_debug_level, 0, "Learning Debug Level (0..4)", params())
    , DOUBLE_MEMBER(matcher_good_threshold, 0.125, "Good Match (0-1)", params())
    , DOUBLE_MEMBER(matcher_reliable_adaptive_result, 0.0, "Great Match (0-1)", params())
    , DOUBLE_MEMBER(matcher_perfect_threshold, 0.02, "Perfect Match (0-1)", params())
    , DOUBLE_MEMBER(matcher_bad_match_pad, 0.15, "Bad Match Pad (0-1)", params())
    , DOUBLE_MEMBER(matcher_rating_margin, 0.1, "New template margin (0-1)", params())
    , DOUBLE_MEMBER(matcher_avg_noise_size, 12.0, "Avg. noise blob length", params())
    , INT_MEMBER(matcher_permanent_classes_min, 1, "Min # of permanent classes", params())
    , INT_MEMBER(matcher_min_examples_for_prototyping, 3, "Reliable Config Threshold",
                 params())
    , INT_MEMBER(matcher_sufficient_examples_for_prototyping, 5,
                 "Enable adaption even if the ambiguities have not been seen", params())
    , DOUBLE_MEMBER(matcher_clustering_max_angle_delta, 0.015,
                    "Maximum angle delta for prototype clustering", params())
    , DOUBLE_MEMBER(classify_misfit_junk_penalty, 0.0,
                    "Penalty to apply when a non-alnum is vertically out of "
                    "its expected textline position",
                    params())
    , DOUBLE_MEMBER(rating_scale, 1.5, "Rating scaling factor", params())
    , DOUBLE_MEMBER(tessedit_class_miss_scale, 0.00390625, "Scale factor for features not used",
                    params())
    , DOUBLE_MEMBER(classify_adapted_pruning_factor, 2.5,
                    "Prune poor adapted results this much worse than best result", params())
    , DOUBLE_MEMBER(classify_adapted_pruning_threshold, -1.0,
                    "Threshold at which classify_adapted_pruning_factor starts", params())
    , INT_MEMBER(classify_adapt_proto_threshold, 230,
                 "Threshold for good protos during adaptive 0-255", params())
    , INT_MEMBER(classify_adapt_feature_threshold, 230,
                 "Threshold for good features during adaptive 0-255", params())
    , BOOL_MEMBER(disable_character_fragments, true,
                  "Do not include character fragments in the"
                  " results of the classifier",
                  params())
    , DOUBLE_MEMBER(classify_character_fragments_garbage_certainty_threshold, -3.0,
                    "Exclude fragments that do not look like whole"
                    " characters from training and adaption",
                    params())
    , BOOL_MEMBER(classify_debug_character_fragments, false,
                  "Bring up graphical debugging windows for fragments training", params())
    , BOOL_MEMBER(matcher_debug_separate_windows, false,
                  "Use two different windows for debugging the matching: "
                  "One for the protos and one for the features.",
                  params())
    , STRING_MEMBER(classify_learn_debug_str, "", "Class str to debug learning", params())
    , INT_MEMBER(classify_class_pruner_threshold, 229, "Class Pruner Threshold 0-255",
                 params())
    , INT_MEMBER(classify_class_pruner_multiplier, 15,
                 "Class Pruner Multiplier 0-255:       ", params())
    , INT_MEMBER(classify_cp_cutoff_strength, 7,
                 "Class Pruner CutoffStrength:         ", params())
    , INT_MEMBER(classify_integer_matcher_multiplier, 10,
                 "Integer Matcher Multiplier  0-255:   ", params())
    , BOOL_MEMBER(classify_bln_numeric_mode, 0, "Assume the input is numbers [0-9].",
                  params())
    , DOUBLE_MEMBER(speckle_large_max_size, 0.30, "Max large speckle size", params())
    , DOUBLE_MEMBER(speckle_rating_penalty, 10.0, "Penalty to add to worst rating for noise",
                    params())
    , im_(&classify_debug_level)
    , dict_(this) {
  using namespace std::placeholders; // for _1, _2
  fontinfo_table_.set_clear_callback(std::bind(FontInfoDeleteCallback, _1));

  InitFeatureDefs(&feature_defs_);
}

Classify::~Classify() {
  EndAdaptiveClassifier();
}

// Takes ownership of the given classifier, and uses it for future calls
// to CharNormClassifier.
void Classify::SetStaticClassifier(ShapeClassifier *static_classifier) {
  delete static_classifier_;
  static_classifier_ = static_classifier;
}

// Moved from speckle.cpp
// Adds a noise classification result that is a bit worse than the worst
// current result, or the worst possible result if no current results.
void Classify::AddLargeSpeckleTo(int blob_length, BLOB_CHOICE_LIST *choices) {
  BLOB_CHOICE_IT bc_it(choices);
  // If there is no classifier result, we will use the worst possible certainty
  // and corresponding rating.
  float certainty = -getDict().certainty_scale;
  float rating = rating_scale * blob_length;
  if (!choices->empty() && blob_length > 0) {
    bc_it.move_to_last();
    BLOB_CHOICE *worst_choice = bc_it.data();
    // Add speckle_rating_penalty to worst rating, matching old value.
    rating = worst_choice->rating() + speckle_rating_penalty;
    // Compute the rating to correspond to the certainty. (Used to be kept
    // the same, but that messes up the language model search.)
    certainty = -rating * getDict().certainty_scale / (rating_scale * blob_length);
  }
  auto *blob_choice = new BLOB_CHOICE(UNICHAR_SPACE, rating, certainty, -1, 0.0f, FLT_MAX, 0,
                                      BCC_SPECKLE_CLASSIFIER);
  bc_it.add_to_end(blob_choice);
}

// Returns true if the blob is small enough to be a large speckle.
bool Classify::LargeSpeckle(const TBLOB &blob) {
  double speckle_size = kBlnXHeight * speckle_large_max_size;
  TBOX bbox = blob.bounding_box();
  return bbox.width() < speckle_size && bbox.height() < speckle_size;
}

} // namespace tesseract

#endif // DISABLED_LEGACY_ENGINE
