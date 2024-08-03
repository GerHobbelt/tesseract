///////////////////////////////////////////////////////////////////////
// File:        lstmeval.cpp
// Description: Evaluation program for LSTM-based networks.
// Author:      Ray Smith
//
// (C) Copyright 2016, Google Inc.
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

#include <tesseract/preparation.h> // compiler config, etc.

#include "common/commontraining.h"
#include "unicharset/lstmtester.h"
#include <tesseract/tprintf.h>

using namespace tesseract;

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

STRING_VAR(lstmeval_model, "", "Name of model file (training or recognition)");
// v--- the next few flags are also referenced in lstmtraining.cpp et al
STRING_VAR(lstmeval_traineddata, "",
                         "If model is a training checkpoint, then traineddata must "
                         "be the traineddata file that was given to the trainer");
STRING_VAR(lstmeval_eval_listfile, "", "File listing sample files in lstmf training format.");
INT_VAR(lstmeval_max_image_MB, 2000, "Max memory to use for images.");
INT_VAR(lstmeval_verbosity, 1, "Amount of diagnosting information to output (0-2).");

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_lstm_eval_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  tesseract::TessBaseAPI api;

  int rv = ParseArguments(api, &argc, &argv);
  if (rv >= 0) {
    return rv;
  }
  if (lstmeval_model.empty()) {
    tprintError("Must provide a --model!\n");
    return EXIT_FAILURE;
  }
  if (lstmeval_eval_listfile.empty()) {
    tprintError("Must provide a --eval_listfile!\n");
    return EXIT_FAILURE;
  }
  tesseract::TessdataManager mgr;
  if (!mgr.Init(lstmeval_model.c_str())) {
    if (lstmeval_traineddata.empty()) {
      tprintError("Must supply --traineddata to eval a training checkpoint!\n");
      return EXIT_FAILURE;
    }
    tprintWarn("{} is not a recognition model, trying training checkpoint...\n", lstmeval_model.c_str());
    if (!mgr.Init(lstmeval_traineddata.c_str())) {
      tprintError("Failed to load language model from {}!\n", lstmeval_traineddata.c_str());
      return EXIT_FAILURE;
    }
    std::vector<char> model_data;
    if (!tesseract::LoadDataFromFile(lstmeval_model.c_str(), &model_data)) {
      tprintError("Failed to load model from: {}\n", lstmeval_model.c_str());
      return EXIT_FAILURE;
    }
    mgr.OverwriteEntry(tesseract::TESSDATA_LSTM, &model_data[0], model_data.size());
  }
  tesseract::LSTMTester tester(static_cast<int64_t>(lstmeval_max_image_MB) * 1048576);
#ifndef NDEBUG
  tester.SetDebug(1);
#endif
  if (!tester.LoadAllEvalData(lstmeval_eval_listfile.c_str())) {
    tprintError("Failed to load eval data from: {}\n", lstmeval_eval_listfile.c_str());
    return EXIT_FAILURE;
  }
  double errs = 0.0;
  std::string result = tester.RunEvalSync(0, &errs, mgr,
                                          /*training_stage (irrelevant)*/ 0, lstmeval_verbosity);
  tprintInfo("{}\n", result);
  return EXIT_SUCCESS;
} /* main */
