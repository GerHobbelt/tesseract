// Copyright 2011 Google Inc. All Rights Reserved.
// Author: rays@google.com (Ray Smith)

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//  Filename: classifier_tester.cpp
//  Purpose:  Tests a character classifier on data as formatted for training,
//            but doesn't have to be the same as the training data.
//  Author:   Ray Smith

#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/baseapi.h>
#include <algorithm>
#include <cstdio>
#include "common/commontraining.h"
#include "common/mastertrainer.h"
#include <tesseract/params.h>
#include "tessclassifier.h"
#include "tesseractclass.h"

#include "tesseract/capi_training_tools.h"


using namespace tesseract;

#if !DISABLED_LEGACY_ENGINE

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

STRING_VAR(test_classifier, "", "Classifier to test");
STRING_VAR(test_lang, "eng", "Language to test");
STRING_VAR(test_tessdata_dir, "", "Directory of traineddata files");
INT_VAR(test_report_level, 0, "The amount of diagnostics reporting you wish to see while run the test. 0 = no output. 1 = bottom-line error rate. 2 = bottom-line error rate + time. 3 = font-level error rate + time. 4 = list of all errors + short classifier debug output on 16 errors. 5 = list of all errors + short classifier debug output on 25 errors.");

enum ClassifierName { CN_PRUNER, CN_FULL, CN_COUNT };

static const char *names[] = {"pruner", "full"};

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

static tesseract::ShapeClassifier *InitializeClassifier(tesseract::TessBaseAPI &api, const char *classifer_name,
                                                        const UNICHARSET &unicharset, int argc,
                                                        const char **argv) {
  // Decode the classifier string.
  ClassifierName classifier = CN_COUNT;
  for (int c = 0; c < CN_COUNT; ++c) {
    if (strcmp(classifer_name, names[c]) == 0) {
      classifier = static_cast<ClassifierName>(c);
      break;
    }
  }
  if (classifier == CN_COUNT) {
    tprintError("Invalid classifier name:{}\n", test_classifier.c_str());
    return nullptr;
  }

  // We need to initialize tesseract to test.
  tesseract::OcrEngineMode engine_mode = tesseract::OEM_TESSERACT_ONLY;
  tesseract::Tesseract *tesseract = nullptr;
  tesseract::Classify *classify = nullptr;
  if (classifier == CN_PRUNER || classifier == CN_FULL) {
    if (api.InitOem(test_tessdata_dir.c_str(), test_lang.c_str(), engine_mode) < 0) {
      tprintError("Tesseract initialization failed!\n");
      return nullptr;
    }
    tesseract = &api.tesseract();
    classify = static_cast<tesseract::Classify *>(tesseract);
    if (classify->shape_table() == nullptr) {
      tprintError("Tesseract must contain a ShapeTable!\n");
      return nullptr;
    }
  }
  tesseract::ShapeClassifier *shape_classifier = nullptr;

  if (classifier == CN_PRUNER) {
    shape_classifier = new tesseract::TessClassifier(true, classify);
  } else if (classifier == CN_FULL) {
    shape_classifier = new tesseract::TessClassifier(false, classify);
  }
  tprintDebug("Testing classifier {}:\n", classifer_name);
  return shape_classifier;
}

// This program has complex setup requirements, so here is some help:
// Two different modes, tr files and serialized mastertrainer.
// From tr files:
//   classifier_tester -U unicharset -F font_properties -X xheights
//     -classifier x -lang lang [-output_trainer trainer] *.tr
// From a serialized trainer:
//  classifier_tester -input_trainer trainer [-lang lang] -classifier x
//
// In the first case, the unicharset must be the unicharset from within
// the classifier under test, and the font_properties and xheights files must
// match the files used during training.
// In the second case, the trainer file must have been prepared from
// some previous run of shapeclustering, mftraining, or classifier_tester
// using the same conditions as above, ie matching unicharset/font_properties.
//
// Available values of classifier (x above) are:
// pruner   : Tesseract class pruner only.
// full     : Tesseract full classifier.
//            with an input trainer.)
#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_classifier_tester_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  tesseract::TessBaseAPI api;

  int rv = ParseArguments(api, &argc, &argv);
  if (rv >= 0) {
    return rv;
  }
  std::string file_prefix;
  auto trainer = tesseract::LoadTrainingData(argv + 1, false, nullptr, file_prefix);
  // Decode the classifier string.
  tesseract::ShapeClassifier *shape_classifier =
      InitializeClassifier(api, test_classifier.c_str(), trainer->unicharset(), argc, argv);
  if (shape_classifier == nullptr) {
    tprintError("Classifier init failed!:{}\n", test_classifier.c_str());
    return EXIT_FAILURE;
  }

  // We want to test junk as well if it is available.
  // trainer->IncludeJunk();
  // We want to test with replicated samples too.
  trainer->ReplicateAndRandomizeSamplesIfRequired();

  trainer->TestClassifierOnSamples(tesseract::CT_UNICHAR_TOP1_ERR,
                                   test_report_level, false,
                                   shape_classifier, nullptr);
  delete shape_classifier;

  return EXIT_SUCCESS;
} /* main */

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_classifier_tester_main(int argc, const char** argv)
#endif
{
	tesseract::tprintError("the {} tool is not supported in this build.\n", fz_basename(argv[0]));
	return EXIT_FAILURE;
}

#endif
