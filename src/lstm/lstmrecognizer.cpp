///////////////////////////////////////////////////////////////////////
// File:        lstmrecognizer.cpp
// Description: Top-level line recognizer class for LSTM-based networks.
// Author:      Ray Smith
//
// (C) Copyright 2013, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
///////////////////////////////////////////////////////////////////////

// Include automatically generated configuration file if running autoconf.
#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#include "lstmrecognizer.h"

#include <leptonica/allheaders.h>
#include "dict.h"
#include "genericheap.h"
#include "helpers.h"
#include "imagedata.h"
#include "input.h"
#include "lstm.h"
#include "normalis.h"
#include "pageres.h"
#include "ratngs.h"
#include "recodebeam.h"
#include "scrollview.h"
#include "statistc.h"
#include "tprintf.h"
#include "tlog.h"

#include <unordered_set>
#include <vector>

namespace tesseract {

// Default ratio between dict and non-dict words.
static const double kDictRatio = 1.25;
// Default certainty offset to give the dictionary a chance.
static const double kCertOffset = -0.085;

//LSTMRecognizer::LSTMRecognizer(const std::string &language_data_path_prefix)
//    : LSTMRecognizer::LSTMRecognizer() {
//  ccutil_.language_data_path_prefix = language_data_path_prefix;
//}

LSTMRecognizer::LSTMRecognizer()
    : network_(nullptr)
    , training_flags_(0)
    , training_iteration_(0)
    , sample_iteration_(0)
    , null_char_(UNICHAR_BROKEN)
    , learning_rate_(0.0f)
    , momentum_(0.0f)
    , adam_beta_(0.0f)
    , dict_(nullptr)
    , search_(nullptr)
#if !GRAPHICS_DISABLED
    , debug_win_(nullptr)
#endif
{}

LSTMRecognizer::~LSTMRecognizer() {
  if (network_ != nullptr) {
    network_->Clean();
  }

  delete network_;
  delete dict_;
  delete search_;
}

void LSTMRecognizer::Clean() {
  network_->Clean();

  delete network_;
  network_ = nullptr;
  delete dict_;
  dict_ = nullptr;
  delete search_;
  search_ = nullptr;
}

// Loads a model from mgr, including the dictionary only if lang is not empty.
bool LSTMRecognizer::Load(const ParamsVectorSet &params, const std::string &lang,
                          TessdataManager *mgr) {
  TFile fp;
  if (!mgr->GetComponent(TESSDATA_LSTM, &fp)) {
    return false;
  }
  if (!DeSerialize(mgr, &fp)) {
    return false;
  }
  if (lang.empty()) {
    return true;
  }
  // Allow it to run without a dictionary.
  LoadDictionary(params, lang, mgr);
  return true;
}

// Writes to the given file. Returns false in case of error.
bool LSTMRecognizer::Serialize(const TessdataManager *mgr, TFile *fp) const {
  bool include_charsets = mgr == nullptr || !mgr->IsComponentAvailable(TESSDATA_LSTM_RECODER) ||
                          !mgr->IsComponentAvailable(TESSDATA_LSTM_UNICHARSET);
  if (!network_->Serialize(fp)) {
    return false;
  }
  if (include_charsets && !GetUnicharset().save_to_file(fp)) {
    return false;
  }
  if (!fp->Serialize(network_str_)) {
    return false;
  }
  if (!fp->Serialize(&training_flags_)) {
    return false;
  }
  if (!fp->Serialize(&training_iteration_)) {
    return false;
  }
  if (!fp->Serialize(&sample_iteration_)) {
    return false;
  }
  if (!fp->Serialize(&null_char_)) {
    return false;
  }
  if (!fp->Serialize(&adam_beta_)) {
    return false;
  }
  if (!fp->Serialize(&learning_rate_)) {
    return false;
  }
  if (!fp->Serialize(&momentum_)) {
    return false;
  }
  if (include_charsets && IsRecoding() && !recoder_.Serialize(fp)) {
    return false;
  }
  return true;
}

// Reads from the given file. Returns false in case of error.
bool LSTMRecognizer::DeSerialize(const TessdataManager *mgr, TFile *fp) {
  delete network_;
  network_ = Network::CreateFromFile(fp);
  if (network_ == nullptr) {
    return false;
  }
  bool include_charsets = mgr == nullptr || !mgr->IsComponentAvailable(TESSDATA_LSTM_RECODER) ||
                          !mgr->IsComponentAvailable(TESSDATA_LSTM_UNICHARSET);
  if (include_charsets && !ccutil_.unicharset.load_from_file(fp, false)) {
    return false;
  }
  if (!fp->DeSerialize(network_str_)) {
    return false;
  }
  if (!fp->DeSerialize(&training_flags_)) {
    return false;
  }
  if (!fp->DeSerialize(&training_iteration_)) {
    return false;
  }
  if (!fp->DeSerialize(&sample_iteration_)) {
    return false;
  }
  if (!fp->DeSerialize(&null_char_)) {
    return false;
  }
  if (!fp->DeSerialize(&adam_beta_)) {
    return false;
  }
  if (!fp->DeSerialize(&learning_rate_)) {
    return false;
  }
  if (!fp->DeSerialize(&momentum_)) {
    return false;
  }
  if (include_charsets && !LoadRecoder(fp)) {
    return false;
  }
  if (!include_charsets && !LoadCharsets(mgr)) {
    return false;
  }
  network_->SetRandomizer(&randomizer_);
  network_->CacheXScaleFactor(network_->XScaleFactor());
  return true;
}

// Loads the charsets from mgr.
bool LSTMRecognizer::LoadCharsets(const TessdataManager *mgr) {
  TFile fp;
  if (!mgr->GetComponent(TESSDATA_LSTM_UNICHARSET, &fp)) {
    return false;
  }
  if (!ccutil_.unicharset.load_from_file(&fp, false)) {
    return false;
  }
  if (!mgr->GetComponent(TESSDATA_LSTM_RECODER, &fp)) {
    return false;
  }
  if (!LoadRecoder(&fp)) {
    return false;
  }
  return true;
}

// Loads the Recoder.
bool LSTMRecognizer::LoadRecoder(TFile *fp) {
  if (IsRecoding()) {
    if (!recoder_.DeSerialize(fp)) {
      return false;
    }
    RecodedCharID code;
    recoder_.EncodeUnichar(UNICHAR_SPACE, &code);
    if (code(0) != UNICHAR_SPACE) {
      tprintError("Space was garbled in recoding!!\n");
      return false;
    }
  } else {
    recoder_.SetupPassThrough(GetUnicharset());
    training_flags_ |= TF_COMPRESS_UNICHARSET;
  }
  return true;
}

// Loads the dictionary if possible from the traineddata file.
// Prints a warning message, and returns false but otherwise fails silently
// and continues to work without it if loading fails.
// Note that dictionary load is independent from DeSerialize, but dependent
// on the unicharset matching. This enables training to deserialize a model
// from checkpoint or restore without having to go back and reload the
// dictionary.
// Some parameters have to be passed in (from langdata/config/api via Tesseract)
bool LSTMRecognizer::LoadDictionary(const ParamsVectorSet &params, const std::string &lang,
                                    TessdataManager *mgr) {
  delete dict_;
  dict_ = new Dict(&ccutil_);
  dict_->user_words_file.ResetFrom(params);
  dict_->user_words_suffix.ResetFrom(params);
  dict_->user_patterns_file.ResetFrom(params);
  dict_->user_patterns_suffix.ResetFrom(params);
  dict_->SetupForLoad(Dict::GlobalDawgCache());
  dict_->LoadLSTM(lang, mgr);
  if (dict_->FinishLoad()) {
    return true; // Success.
  }
  tprintError("Failed to load any lstm-specific dictionaries for lang {}!!\n", lang);
  delete dict_;
  dict_ = nullptr;
  return false;
}

// Recognizes the line image, contained within image_data, returning the
// ratings matrix and matching box_word for each WERD_RES in the output.
void LSTMRecognizer::RecognizeLine(const ImageData &image_data,
                                   float invert_threshold,
                                   double worst_dict_cert, const TBOX &line_box,
                                   PointerVector<WERD_RES> *words, int lstm_choice_mode,
                                   int lstm_choice_amount) {
  NetworkIO outputs;
  float scale_factor = 0.0;
  NetworkIO inputs;
  if (!RecognizeLine(image_data, invert_threshold, false, false, line_box, &scale_factor, &inputs, &outputs)) {
    return;
  }
  if (search_ == nullptr) {
    search_ = new RecodeBeamSearch(recoder_, null_char_, SimpleTextOutput(), dict_);
	search_->SetDebug(HasDebug() - 1);
  }
  search_->excludedUnichars.clear();
  search_->Decode(outputs, kDictRatio, kCertOffset, worst_dict_cert, &GetUnicharset(), lstm_choice_mode);
  search_->ExtractBestPathAsWords(line_box, scale_factor, &GetUnicharset(), words);
  if (lstm_choice_mode) {
    search_->extractSymbolChoices(&GetUnicharset());
    for (int i = 0; i < lstm_choice_amount; ++i) {
      search_->DecodeSecondaryBeams(outputs, kDictRatio, kCertOffset, worst_dict_cert, &GetUnicharset());
      search_->extractSymbolChoices(&GetUnicharset());
    }
    search_->segmentTimestepsByCharacters();
    unsigned char_it = 0;
    for (size_t i = 0; i < words->size(); ++i) {
      for (int j = 0; j < words->at(i)->end; ++j) {
        if (char_it < search_->ctc_choices.size()) {
          words->at(i)->CTC_symbol_choices.push_back(search_->ctc_choices[char_it]);
        }
        if (char_it < search_->segmentedTimesteps.size()) {
          words->at(i)->segmented_timesteps.push_back(search_->segmentedTimesteps[char_it]);
        }
        ++char_it;
      }
      words->at(i)->timesteps =
          search_->combineSegmentedTimesteps(&words->at(i)->segmented_timesteps);
    }
    search_->segmentedTimesteps.clear();
    search_->ctc_choices.clear();
    search_->excludedUnichars.clear();
  }
}

// Helper computes min and mean best results in the output.
void LSTMRecognizer::OutputStats(const NetworkIO &outputs, float *min_output, float *mean_output, float *sd) {
  const int kOutputScale = INT8_MAX;
  STATS stats(0, kOutputScale);
  for (int t = 0; t < outputs.Width(); ++t) {
    int best_label = outputs.BestLabel(t, nullptr);
    if (best_label != null_char_) {
      float best_output = outputs.f(t)[best_label];
      stats.add(static_cast<int>(kOutputScale * best_output), 1);
    }
  }
  // If the output is all nulls it could be that the photometric interpretation
  // is wrong, so make it look bad, so the other way can win, even if not great.
  if (stats.get_total() == 0) {
    *min_output = 0.0f;
    *mean_output = 0.0f;
    *sd = 1.0f;
  } else {
    *min_output = static_cast<float>(stats.min_bucket()) / kOutputScale;
    *mean_output = stats.mean() / kOutputScale;
    *sd = stats.sd() / kOutputScale;
  }
}

// Recognizes the image_data, returning the labels,
// scores, and corresponding pairs of start, end x-coords in coords.
bool LSTMRecognizer::RecognizeLine(const ImageData &image_data,
                                   float invert_threshold,
                                   bool re_invert, bool upside_down, 
                                   const TBOX &line_box, 
                                   float *scale_factor,
                                   NetworkIO *inputs, NetworkIO *outputs) {
  // This ensures consistent recognition results.
  SetRandomSeed();
  int min_width = network_->XScaleFactor();
  Image pix = Input::PrepareLSTMInputs(image_data, network_, min_width, &randomizer_, scale_factor);
  if (pix == nullptr) {
    tprintError("Line cannot be recognized!!\n");
    return false;
  }
  // Maximum width of image to train on.
  const int kMaxImageWidth = 128 * pixGetHeight(pix);
  if (network_->IsTraining() && pixGetWidth(pix) > kMaxImageWidth) {
    tprintError("Image too large to learn!! Size = {}x{}\n", pixGetWidth(pix), pixGetHeight(pix));
    pix.destroy();
    return false;
  }
  if (upside_down) {
    pixRotate180(pix, pix);
  }
  // Reduction factor from image to coords.
  *scale_factor = min_width / *scale_factor;
  inputs->set_int_mode(IsIntMode());
  if (HasDebug()) {
    tprintDebug("Scale_factor:{}, upside_down:{}, invert_threshold:{}, int_mode:{}\n",
        *scale_factor, upside_down, invert_threshold, inputs->int_mode());
  }
  SetRandomSeed();
  Input::PreparePixInput(network_->InputShape(), pix, &randomizer_, inputs);
  network_->Forward(HasDebug(), *inputs, nullptr, &scratch_space_, outputs);
  // Check for auto inversion.
  if (invert_threshold > 0.0f) {
    float pos_min, pos_mean, pos_sd;
    OutputStats(*outputs, &pos_min, &pos_mean, &pos_sd);
    if (HasDebug()) {
      tprintDebug("OutputStats: pos_min:{}, pos_mean:{}, pos_sd:{}, invert_threshold:{}{}\n",
          pos_min, pos_mean, pos_sd, invert_threshold, (pos_mean < invert_threshold ? " --> Run again inverted and see if it is any better." : " --> OK"));
    }
    if (pos_mean < invert_threshold) {
      // Run again inverted and see if it is any better.
      NetworkIO inv_inputs, inv_outputs;
      inv_inputs.set_int_mode(IsIntMode());
      SetRandomSeed();
      Image inv_pix = pixClone(pix);
      pixInvert(inv_pix, pix);
      Input::PreparePixInput(network_->InputShape(), inv_pix, &randomizer_, &inv_inputs);
      network_->Forward(HasDebug(), inv_inputs, nullptr, &scratch_space_, &inv_outputs);
      float inv_min, inv_mean, inv_sd;
      OutputStats(inv_outputs, &inv_min, &inv_mean, &inv_sd);
      if (HasDebug() || 1) {
        tprintDebug("Inverting image OutputStats: {} :: old min={}, old mean={}, old sd={}, inv min={}, inv mean={}, inv sd={}\n",
            (inv_mean > pos_mean ? "Inverted did better. Use inverted data" : "Inverting was not an improvement, so undo and run again, so the outputs match the best forward result"),
            pos_min, pos_mean, pos_sd, inv_min, inv_mean, inv_sd);
      }
      if (inv_mean > pos_mean) {
        // Inverted did better. Use inverted data.
        *outputs = std::move(inv_outputs);
        *inputs = std::move(inv_inputs);
      } else if (re_invert) {
        // Inverting was not an improvement, so undo and run again, so the
        // outputs match the best forward result.
        SetRandomSeed();
        network_->Forward(HasDebug(), *inputs, nullptr, &scratch_space_, outputs);
      }
      inv_pix.destroy();
    }
  }

  pix.destroy();
  if (HasDebug()) {
    std::vector<int> labels, coords;
    LabelsFromOutputs(*outputs, &labels, &coords);
#if !GRAPHICS_DISABLED
    DisplayForward(*inputs, labels, coords, line_box, "LSTMForward", debug_win_);
#endif
    DebugActivationPath(*outputs, labels, coords);
  }
  return true;
}

// Converts an array of labels to utf-8, whether or not the labels are
// augmented with character boundaries.
std::string LSTMRecognizer::DecodeLabels(const std::vector<int> &labels) {
  std::string result;
  unsigned end = 1;
  for (unsigned start = 0; start < labels.size(); start = end) {
    if (labels[start] == null_char_) {
      end = start + 1;
    } else {
      result += DecodeLabel(labels, start, &end, nullptr);
    }
  }
  return result;
}

#if !GRAPHICS_DISABLED

// Displays the forward results in a window with the characters and
// boundaries as determined by the labels and label_coords.
void LSTMRecognizer::DisplayForward(const NetworkIO &inputs, const std::vector<int> &labels,
                                    const std::vector<int> &label_coords,
                                    const TBOX &line_box, const char *window_name,
                                    ScrollViewReference &window) {
  Image input_pix = inputs.ToPix();
  Network::ClearWindow(false, window_name, pixGetWidth(input_pix), pixGetHeight(input_pix), window);
  int line_height = Network::DisplayImage(input_pix, "LSTMRecognizer::DisplayForward", window);
  DisplayLSTMOutput(labels, label_coords, line_height, line_box, window);
}

// Displays the labels and cuts at the corresponding xcoords.
// Size of labels should match xcoords.
void LSTMRecognizer::DisplayLSTMOutput(const std::vector<int> &labels,
                                       const std::vector<int> &xcoords,
                                       int height, const TBOX &line_box, 
                                       ScrollViewReference &window) {
  int x_scale = network_->XScaleFactor();
  window->TextAttributes("Arial", height / 4, false, false, false);
  int x_offset = line_box.left();
  int y_offset = line_box.bottom();
  window->SetXYOffset(x_offset, y_offset);
  unsigned int end = 1;
  for (unsigned int start = 0; start < labels.size(); start = end) {
    int xpos = xcoords[start] * x_scale;
    if (labels[start] == null_char_) {
      end = start + 1;
      window->Pen(Diagnostics::RED);
    } else {
      window->Pen(Diagnostics::GREEN);
      const char *str = DecodeLabel(labels, start, &end, nullptr);
      if (*str == '\\') {
        str = "\\\\";
      }
      xpos = xcoords[(start + end) / 2] * x_scale;
      window->Text(xpos, height, str);
    }
    window->Line(xpos, 0, xpos, height * 3 / 2);
  }
  window->SetXYOffset(0, 0);
  window->UpdateWindow();
}

#endif // !GRAPHICS_DISABLED

// Prints debug output detailing the activation path that is implied by the
// label_coords.
void LSTMRecognizer::DebugActivationPath(const NetworkIO &outputs, const std::vector<int> &labels,
                                         const std::vector<int> &xcoords) {
  if (xcoords[0] > 0) {
    DebugActivationRange(outputs, "<null>", null_char_, 0, xcoords[0]);
  }
  unsigned end = 1;
  for (unsigned start = 0; start < labels.size(); start = end) {
    if (labels[start] == null_char_) {
      end = start + 1;
      DebugActivationRange(outputs, "<null>", null_char_, xcoords[start], xcoords[end]);
      continue;
    } else {
      int decoded;
      const char *label = DecodeLabel(labels, start, &end, &decoded);
      DebugActivationRange(outputs, label, labels[start], xcoords[start], xcoords[start + 1]);
      for (unsigned i = start + 1; i < end; ++i) {
        DebugActivationRange(outputs, DecodeSingleLabel(labels[i]), labels[i], xcoords[i],
                             xcoords[i + 1]);
      }
    }
  }
}

// Prints debug output detailing activations and 2nd choice over a range
// of positions.
void LSTMRecognizer::DebugActivationRange(const NetworkIO &outputs, const char *label,
                                          int best_choice, int x_start, int x_end) {
  tprintDebug("{}={} On [{}, {}), scores=", label, best_choice, x_start, x_end);
  double max_score = 0.0;
  double mean_score = 0.0;
  const int width = x_end - x_start;
  for (int x = x_start; x < x_end; ++x) {
    const float *line = outputs.f(x);
    const double score = line[best_choice] * 100.0;
    if (score > max_score) {
      max_score = score;
    }
    mean_score += score / width;
    int best_c = 0;
    double best_score = 0.0;
    for (int c = 0; c < outputs.NumFeatures(); ++c) {
      if (c != best_choice && line[c] > best_score) {
        best_c = c;
        best_score = line[c];
      }
    }
    tprintDebug(" {}({}={}={})", score, DecodeSingleLabel(best_c), best_c, best_score * 100.0);
  }
  tprintDebug(", Mean={}, max={}\n", mean_score, max_score);
}

// Helper returns true if the null_char is the winner at t, and it beats the
// null_threshold, or the next choice is space, in which case we will use the
// null anyway.
#if 0 // TODO: unused, remove if still unused after 2020.
static bool NullIsBest(const NetworkIO& output, float null_thr,
                       int null_char, int t) {
  if (output.f(t)[null_char] >= null_thr) return true;
  if (output.BestLabel(t, null_char, null_char, nullptr) != UNICHAR_SPACE)
    return false;
  return output.f(t)[null_char] > output.f(t)[UNICHAR_SPACE];
}
#endif

// Converts the network output to a sequence of labels. Outputs labels, scores
// and start xcoords of each char, and each null_char_, with an additional
// final xcoord for the end of the output.
// The conversion method is determined by internal state.
void LSTMRecognizer::LabelsFromOutputs(const NetworkIO &outputs, std::vector<int> *labels,
                                       std::vector<int> *xcoords) {
  if (SimpleTextOutput()) {
    LabelsViaSimpleText(outputs, labels, xcoords);
  } else {
    LabelsViaReEncode(outputs, labels, xcoords);
  }
}

// As LabelsViaCTC except that this function constructs the best path that
// contains only legal sequences of subcodes for CJK.
void LSTMRecognizer::LabelsViaReEncode(const NetworkIO &output, std::vector<int> *labels,
                                       std::vector<int> *xcoords) {
  if (search_ == nullptr) {
    search_ = new RecodeBeamSearch(recoder_, null_char_, SimpleTextOutput(), dict_);
	search_->SetDebug(HasDebug() - 1);
  }
  search_->Decode(output, 1.0, 0.0, RecodeBeamSearch::kMinCertainty, nullptr /* unicharset */, 2 /* 0 */);
  search_->ExtractBestPathAsLabels(labels, xcoords);
}

// Converts the network output to a sequence of labels, with scores, using
// the simple character model (each position is a char, and the null_char_ is
// mainly intended for tail padding.)
void LSTMRecognizer::LabelsViaSimpleText(const NetworkIO &output, std::vector<int> *labels,
                                         std::vector<int> *xcoords) {
  labels->clear();
  xcoords->clear();
  const int width = output.Width();
  for (int t = 0; t < width; ++t) {
    float score = 0.0f;
    const int label = output.BestLabel(t, &score);
    if (label != null_char_) {
      labels->push_back(label);
      xcoords->push_back(t);
    }
  }
  xcoords->push_back(width);
}

// Returns a string corresponding to the label starting at start. Sets *end
// to the next start and if non-null, *decoded to the unichar id.
const char *LSTMRecognizer::DecodeLabel(const std::vector<int> &labels, unsigned start, unsigned *end,
                                        int *decoded) {
  *end = start + 1;
  if (IsRecoding()) {
    // Decode labels via recoder_.
    RecodedCharID code;
    if (labels[start] == null_char_) {
      if (decoded != nullptr) {
        code.Set(0, null_char_);
        *decoded = recoder_.DecodeUnichar(code);
      }
      return "<null>";
    }
    unsigned index = start;
    while (index < labels.size() && code.length() < RecodedCharID::kMaxCodeLen) {
      code.Set(code.length(), labels[index++]);
      while (index < labels.size() && labels[index] == null_char_) {
        ++index;
      }
      int uni_id = recoder_.DecodeUnichar(code);
      // If the next label isn't a valid first code, then we need to continue
      // extending even if we have a valid uni_id from this prefix.
      if (uni_id != INVALID_UNICHAR_ID &&
          (index == labels.size() || code.length() == RecodedCharID::kMaxCodeLen ||
           recoder_.IsValidFirstCode(labels[index]))) {
        *end = index;
        if (decoded != nullptr) {
          *decoded = uni_id;
        }
        if (uni_id == UNICHAR_SPACE) {
          return " ";
        }
        return GetUnicharset().get_normed_unichar(uni_id);
      }
    }
    return "<Undecodable>";
  } else {
    if (decoded != nullptr) {
      *decoded = labels[start];
    }
    if (labels[start] == null_char_) {
      return "<null>";
    }
    if (labels[start] == UNICHAR_SPACE) {
      return " ";
    }
    return GetUnicharset().get_normed_unichar(labels[start]);
  }
}

// Returns a string corresponding to a given single label id, falling back to
// a default of ".." for part of a multi-label unichar-id.
const char *LSTMRecognizer::DecodeSingleLabel(int label) {
  if (label == null_char_) {
    return "<null>";
  }
  if (IsRecoding()) {
    // Decode label via recoder_.
    RecodedCharID code;
    code.Set(0, label);
    label = recoder_.DecodeUnichar(code);
    if (label == INVALID_UNICHAR_ID) {
      return ".."; // Part of a bigger code.
    }
  }
  if (label == UNICHAR_SPACE) {
    return " ";
  }
  return GetUnicharset().get_normed_unichar(label);
}


void LSTMRecognizer::SetDataPathPrefix(const std::string &language_data_path_prefix) {
  ccutil_.language_data_path_prefix = language_data_path_prefix;
}

void LSTMRecognizer::CopyDebugParameters(CCUtil *src, Dict *dict_src) {
  if (src != nullptr && &ccutil_ != src) {
      ccutil_.ambigs_debug_level = (int)src->ambigs_debug_level;
      ccutil_.use_ambigs_for_adaption = (bool)src->use_ambigs_for_adaption;
  }

  if (dict_ != nullptr && dict_ != dict_src) {
      dict_->user_words_file = dict_src->user_words_file.value();
      dict_->user_words_suffix = dict_src->user_words_suffix.value();
      dict_->user_patterns_file = dict_src->user_patterns_file.value();
      dict_->user_patterns_suffix = dict_src->user_patterns_suffix.value();
      dict_->load_system_dawg = dict_src->load_system_dawg.value();
      dict_->load_freq_dawg = dict_src->load_freq_dawg.value();
      dict_->load_unambig_dawg = dict_src->load_unambig_dawg.value();
      dict_->load_punc_dawg = dict_src->load_punc_dawg.value();
      dict_->load_number_dawg = dict_src->load_number_dawg.value();
      dict_->load_bigram_dawg = dict_src->load_bigram_dawg.value();
      dict_->xheight_penalty_subscripts = dict_src->xheight_penalty_subscripts.value();
      dict_->xheight_penalty_inconsistent = dict_src->xheight_penalty_inconsistent.value();
      dict_->segment_penalty_dict_frequent_word = dict_src->segment_penalty_dict_frequent_word.value();
      dict_->segment_penalty_dict_case_ok = dict_src->segment_penalty_dict_case_ok.value();
      dict_->segment_penalty_dict_case_bad = dict_src->segment_penalty_dict_case_bad.value();
      dict_->segment_penalty_dict_nonword = dict_src->segment_penalty_dict_nonword.value();
      dict_->segment_penalty_garbage = dict_src->segment_penalty_garbage.value();
      dict_->output_ambig_words_file = dict_src->output_ambig_words_file.value();
      dict_->dawg_debug_level = dict_src->dawg_debug_level.value();
      dict_->hyphen_debug_level = dict_src->hyphen_debug_level.value();
      dict_->use_only_first_uft8_step = dict_src->use_only_first_uft8_step.value();
      dict_->certainty_scale = dict_src->certainty_scale.value();
      dict_->stopper_nondict_certainty_base = dict_src->stopper_nondict_certainty_base.value();
      dict_->stopper_phase2_certainty_rejection_offset = dict_src->stopper_phase2_certainty_rejection_offset.value();
      dict_->stopper_smallword_size = dict_src->stopper_smallword_size.value();
      dict_->stopper_certainty_per_char = dict_src->stopper_certainty_per_char.value();
      dict_->stopper_allowable_character_badness = dict_src->stopper_allowable_character_badness.value();
      dict_->stopper_debug_level = dict_src->stopper_debug_level.value();
      dict_->stopper_no_acceptable_choices = dict_src->stopper_no_acceptable_choices.value();
      dict_->tessedit_truncate_wordchoice_log = dict_src->tessedit_truncate_wordchoice_log.value();
      dict_->word_to_debug = dict_src->word_to_debug.value();
      dict_->segment_nonalphabetic_script = dict_src->segment_nonalphabetic_script.value();
      dict_->save_doc_words = dict_src->save_doc_words.value();
      dict_->doc_dict_pending_threshold = dict_src->doc_dict_pending_threshold.value();
      dict_->doc_dict_certainty_threshold = dict_src->doc_dict_certainty_threshold.value();
      dict_->max_permuter_attempts = dict_src->max_permuter_attempts.value();
  }
}

} // namespace tesseract.
