///////////////////////////////////////////////////////////////////////
// File:        lstmtraining.cpp
// Description: Training program for LSTM-based networks.
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

#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/debugheap.h>

#include <cerrno>
#include <locale> // for std::locale::classic
#if defined(__USE_GNU)
#  include <cfenv> // for feenableexcept
#endif
#include "common/commontraining.h"
#include "unicharset/fileio.h"             // for LoadFileLinesToStrings
#include "unicharset/lstmtester.h"
#include "unicharset/lstmtrainer.h"
#include <tesseract/params.h>
#include <tesseract/tprintf.h>
#include "unicharset/unicharset_training_utils.h"

using namespace tesseract;

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

INT_VAR(training_debug_interval, 0, "How often to display the alignment.");
STRING_VAR(training_net_spec, "", "Network specification");
INT_VAR(training_net_mode, 192, "Controls network behavior.");
INT_VAR(training_perfect_sample_delay, 0, "How many imperfect samples between perfect ones.");
DOUBLE_VAR(training_target_error_rate, 0.01, "Final error rate in percent.");
DOUBLE_VAR(training_weight_range, 0.1, "Range of initial random weights.");
DOUBLE_VAR(training_learning_rate, 10.0e-4, "Weight factor for new deltas.");
BOOL_VAR(training_reset_learning_rate, false,
                       "Resets all stored learning rates to the value specified by --learning_rate.");
DOUBLE_VAR(training_momentum, 0.5, "Decay factor for repeating deltas.");
DOUBLE_VAR(training_adam_beta, 0.999, "Decay factor for repeating deltas.");
INT_VAR(training_max_image_MB, 6000, "Max memory to use for images.");
STRING_VAR(training_continue_from, "", "Existing model to extend");
STRING_VAR(training_model_output, "lstmtrain", "Basename for output models");
STRING_VAR(training_train_listfile, "",
                         "File listing training files in lstmf training format.");
STRING_VAR(training_eval_listfile, "", "File listing eval files in lstmf training format.");
#if defined(__USE_GNU) || defined(_MSC_VER)
BOOL_VAR(training_debug_float, false, "Raise error on certain float errors.");
#endif
BOOL_VAR(training_stop_training, false, "Just convert the training model to a runtime model.");
BOOL_VAR(training_convert_to_int, false, "Convert the recognition model to an integer model.");
BOOL_VAR(training_sequential_training, false,
                       "Use the training files sequentially instead of round-robin.");
INT_VAR(training_append_index, -1,
                      "Index in continue_from Network at which to"
                      " attach the new network defined by net_spec");
BOOL_VAR(training_debug_network, false, "Get info on distribution of weight values");
INT_VAR(training_max_iterations, 0, "If set, exit after this many iterations");
STRING_VAR(training_traineddata, "", "Combined Dawgs/Unicharset/Recoder for language model");
STRING_VAR(training_old_traineddata, "",
                         "When changing the character set, this specifies the old"
                         " character set that is to be replaced");
BOOL_VAR(training_randomly_rotate, false,
                       "Train OSD and randomly turn training samples upside-down");

// Number of training images to train between calls to MaintainCheckpoints.
const int kNumPagesPerBatch = 100;

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

// Apart from command-line flags, input is a collection of lstmf files, that
// were previously created using tesseract with the lstm.train config file.
// The program iterates over the inputs, feeding the data to the network,
// until the error rate reaches a specified target or max_iterations is reached.
#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_lstm_training_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  int rv = ParseArguments(&argc, &argv);
  if (rv >= 0) {
    return rv;
  }
#if defined(__USE_GNU)
  if (training_debug_float) {
    // Raise SIGFPE for unwanted floating point calculations.
    feenableexcept(FE_DIVBYZERO | FE_OVERFLOW | FE_INVALID);
  }
#elif defined(_MSC_VER)
  if (training_debug_float) {
	  // Raise SIGFPE for unwanted floating point calculations.
	  // 
	  // See also https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/control87-controlfp-control87-2?view=msvc-170
	  _control87(0, (0
		  // | _EM_INEXACT     //     inexact (precision)
  		  // | _EM_UNDERFLOW   //     underflow
		  | _EM_OVERFLOW    //     overflow
		  | _EM_ZERODIVIDE  //     zero divide
		  | _EM_INVALID     //     invalid
		  // | _EM_DENORMAL    //     Denormal 
		  ));
  }
#endif
  if (training_model_output.empty()) {
    tprintError("Must provide a --model_output!\n");
    return EXIT_FAILURE;
  }
  if (training_traineddata.empty()) {
    tprintError("Must provide a --traineddata, see training documentation\n");
    return EXIT_FAILURE;
  }

  // Check write permissions.
  std::string test_file = training_model_output;
  test_file += "_wtest";
  FILE *f = fopen(test_file.c_str(), "wb");
  if (f != nullptr) {
    fclose(f);
    if (remove(test_file.c_str()) != 0) {
      tprintError("Failed to remove {}: {}\n", test_file, strerror(errno));
      return EXIT_FAILURE;
    }
  } else {
    tprintError("Model output cannot be written: {}\n", strerror(errno));
    return EXIT_FAILURE;
  }

  // Setup the trainer.
  std::string checkpoint_file = training_model_output;
  checkpoint_file += "_checkpoint";
  std::string checkpoint_bak = checkpoint_file + ".bak";
  tesseract::LSTMTrainer trainer(training_model_output, checkpoint_file,
                                 training_debug_interval,
                                 static_cast<int64_t>(training_max_image_MB) * 1048576);
#if !defined(NDEEBUG)
  trainer.SetDebug(1);
#endif
  if (!trainer.InitCharSet(training_traineddata.c_str())) {
    tprintError("Failed to read {}\n", training_traineddata.c_str());
    return EXIT_FAILURE;
  }

  // Reading something from an existing model doesn't require many flags,
  // so do it now and exit.
  if (training_stop_training || training_debug_network) {
    if (!trainer.TryLoadingCheckpoint(training_continue_from.c_str(), nullptr)) {
      tprintError("Failed to read continue from: {}\n", training_continue_from.c_str());
      return EXIT_FAILURE;
    }
    if (training_debug_network) {
      trainer.DebugNetwork();
    } else {
      if (training_convert_to_int) {
        trainer.ConvertToInt();
      }
      if (!trainer.SaveTraineddata(training_model_output.c_str())) {
        tprintError("Failed to write recognition model : {}\n", training_model_output.c_str());
      }
    }
    return EXIT_SUCCESS;
  }

  // Get the list of files to process.
  if (training_train_listfile.empty()) {
    tprintError("Must supply a list of training filenames! --train_listfile\n");
    return EXIT_FAILURE;
  }
  std::vector<std::string> filenames;
  if (!tesseract::LoadFileLinesToStrings(training_train_listfile.c_str(), &filenames)) {
    tprintError("Failed to load list of training filenames from {}\n", training_train_listfile.c_str());
    return EXIT_FAILURE;
  }

  // Checkpoints always take priority if they are available.
  if (trainer.TryLoadingCheckpoint(checkpoint_file.c_str(), nullptr) ||
      trainer.TryLoadingCheckpoint(checkpoint_bak.c_str(), nullptr)) {
    tprintDebug("Successfully restored trainer from {}\n", checkpoint_file.c_str());
  } else {
    if (!training_continue_from.empty()) {
      // Load a past model file to improve upon.
      if (!trainer.TryLoadingCheckpoint(training_continue_from.c_str(),
                                        training_append_index >= 0 ? training_continue_from.c_str()
                                                                : training_old_traineddata.c_str())) {
        tprintError("Failed to continue from: {}\n", training_continue_from.c_str());
        return EXIT_FAILURE;
      }
      tprintDebug("Continuing from {}\n", training_continue_from.c_str());
      if (training_reset_learning_rate) {
        trainer.SetLearningRate(training_learning_rate);
        tprintDebug("Set learning rate to {}\n", static_cast<float>(training_learning_rate));
      }
      trainer.InitIterations();
    }
    if (training_continue_from.empty() || training_append_index >= 0) {
      if (training_append_index >= 0) {
        tprintDebug("Appending a new network to an old one!!");
        if (training_continue_from.empty()) {
          tprintError("Must set --continue_from for appending!\n");
          return EXIT_FAILURE;
        }
      }
      // We are initializing from scratch.
      if (!trainer.InitNetwork(training_net_spec.c_str(), training_append_index, training_net_mode,
                               training_weight_range, training_learning_rate, training_momentum,
                               training_adam_beta)) {
        tprintError("Failed to create network from spec: {}\n", training_net_spec.c_str());
        return EXIT_FAILURE;
      }
      trainer.set_perfect_delay(training_perfect_sample_delay);
    }
  }
  if (!trainer.LoadAllTrainingData(
          filenames,
          training_sequential_training ? tesseract::CS_SEQUENTIAL : tesseract::CS_ROUND_ROBIN,
          training_randomly_rotate)) {
    tprintError("Load of images failed!!\n");
    return EXIT_FAILURE;
  }

  tesseract::LSTMTester tester(static_cast<int64_t>(training_max_image_MB) * 1048576);
  tesseract::TestCallback tester_callback = nullptr;
  if (!training_eval_listfile.empty()) {
    using namespace std::placeholders; // for _1, _2, _3...
    if (!tester.LoadAllEvalData(training_eval_listfile.c_str())) {
      tprintError("Failed to load eval data from: {}\n", training_eval_listfile.c_str());
      return EXIT_FAILURE;
    }
    tester_callback = std::bind(&tesseract::LSTMTester::RunEvalAsync, &tester, _1, _2, _3, _4);
  }

  int max_iterations = training_max_iterations;
  if (max_iterations < 0) {
    // A negative value is interpreted as epochs
    max_iterations = filenames.size() * (-max_iterations);
  } else if (max_iterations == 0) {
    // "Infinite" iterations.
    max_iterations = INT_MAX;
  }

  do {
    // Train a few.
    int iteration = trainer.training_iteration();
    for (int target_iteration = iteration + kNumPagesPerBatch;
         iteration < target_iteration && iteration < max_iterations;
         iteration = trainer.training_iteration()) {
      trainer.TrainOnLine(&trainer, false);
    }
    std::stringstream log_str;
    log_str.imbue(std::locale::classic());
    trainer.MaintainCheckpoints(tester_callback, log_str);
    tprintDebug("{}\n", log_str.str());
  } while (trainer.best_error_rate() > training_target_error_rate &&
           (trainer.training_iteration() < max_iterations));
  tprintInfo("Finished! Selected model with minimal training error rate (BCER) = {}\n",
          trainer.best_error_rate());
  return EXIT_SUCCESS;
} /* main */
