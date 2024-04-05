
///////////////////////////////////////////////////////////////////////
// File:        combine_tessdata.cpp
// Description: Creates a unified traineddata file from several
//              data files produced by the training process.
// Author:      Daria Antonova
//
// (C) Copyright 2009, Google Inc.
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

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h" // HAS_LIBICU
#endif

#include "common/commontraining.h" // CheckSharedLibraryVersion
#include "lstmrecognizer.h"
#include "tessdatamanager.h"

#include <cerrno>
#include <iostream> // std::cout

#if defined(HAVE_MUPDF)
#include "mupdf/fitz.h"           // fz_basename
#include "mupdf/helpers/dir.h"
#endif

using namespace tesseract;

static int list_components(TessdataManager &tm, const char *filename) {
  // Initialize TessdataManager with the data in the given traineddata file.
  if (filename != nullptr && !tm.Init(filename)) {
    tprintError("Failed to read {}\n", filename);
    return EXIT_FAILURE;
  }
  tm.Directory();
  return EXIT_SUCCESS;
}

static int list_network(TessdataManager &tm, const char *filename, int tess_debug_lstm) {
  if (filename != nullptr && !tm.Init(filename)) {
    tprintError("Failed to read {}\n", filename);
    return EXIT_FAILURE;
  }
  tesseract::TFile fp;
  if (tm.GetComponent(tesseract::TESSDATA_LSTM, &fp)) {
    tesseract::LSTMRecognizer recognizer;
    recognizer.SetDebug(tess_debug_lstm);
    if (!recognizer.DeSerialize(&tm, &fp)) {
      tprintError("Failed to deserialize LSTM in {}!\n", filename);
      return EXIT_FAILURE;
    }
    std::cout << "LSTM: network=" << recognizer.GetNetwork()
              << ", int_mode=" << recognizer.IsIntMode()
              << ", recoding=" << recognizer.IsRecoding()
              << ", iteration=" << recognizer.training_iteration()
              << ", sample_iteration=" << recognizer.sample_iteration()
              << ", null_char=" << recognizer.null_char()
              << ", learning_rate=" << recognizer.learning_rate()
              << ", momentum=" << recognizer.GetMomentum()
              << ", adam_beta=" << recognizer.GetAdamBeta() << '\n';

    std::cout << "Layer Learning Rates: ";
    auto layers = recognizer.EnumerateLayers();
    for (const auto &id : layers) {
      auto layer = recognizer.GetLayer(id);
      std::cout << id << "(" << layer->name() << ")"
                << "=" << recognizer.GetLayerLearningRate(id)
                << (layers[layers.size() - 1] != id ? ", " : "");
    }
    std::cout << "\n";
  }
  return EXIT_SUCCESS;
}

// Main program to combine/extract/overwrite tessdata components
// in [lang].traineddata files.
//
// To combine all the individual tessdata components (unicharset, DAWGs,
// classifier templates, ambiguities, language configs) located at, say,
// /home/$USER/temp/eng.* run:
//
//   combine_tessdata /home/$USER/temp/eng.
//
// The result will be a combined tessdata file /home/$USER/temp/eng.traineddata
//
// Specify option -e if you would like to extract individual components
// from a combined traineddata file. For example, to extract language config
// file and the unicharset from tessdata/eng.traineddata run:
//
//   combine_tessdata -e tessdata/eng.traineddata
//   /home/$USER/temp/eng.config /home/$USER/temp/eng.unicharset
//
// The desired config file and unicharset will be written to
// /home/$USER/temp/eng.config /home/$USER/temp/eng.unicharset
//
// Specify option -o to overwrite individual components of the given
// [lang].traineddata file. For example, to overwrite language config
// and unichar ambiguities files in tessdata/eng.traineddata use:
//
//   combine_tessdata -o tessdata/eng.traineddata
//   /home/$USER/temp/eng.config /home/$USER/temp/eng.unicharambigs
//
// As a result, tessdata/eng.traineddata will contain the new language config
// and unichar ambigs, plus all the original DAWGs, classifier teamples, etc.
//
// Note: the file names of the files to extract to and to overwrite from should
// have the appropriate file suffixes (extensions) indicating their tessdata
// component type (.unicharset for the unicharset, .unicharambigs for unichar
// ambigs, etc). See k*FileSuffix variable in ccutil/tessdatamanager.h.
//
// Specify option -u to unpack all the components to the specified path:
//
//   combine_tessdata -u tessdata/eng.traineddata /home/$USER/temp/eng.
//
// This will create  /home/$USER/temp/eng.* files with individual tessdata
// components from tessdata/eng.traineddata.
//
#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_combine_tessdata_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  int tess_debug_lstm = 0;

  auto usage_f = [](const char* exename) {
    tprintInfo(
        "Usage for combining tessdata components:\n"
        "  {} language_data_path_prefix\n"
        "  (e.g. {} tessdata/eng.)\n\n",
        exename, exename);
    tprintInfo(
        "Usage for extracting tessdata components:\n"
        "  {} -e traineddata_file [output_component_file...]\n"
        "  (e.g. {} -e eng.traineddata eng.unicharset)\n\n",
        exename, exename);
    tprintInfo(
        "Usage for overwriting tessdata components:\n"
        "  {} -o traineddata_file [input_component_file...]\n"
        "  (e.g. {} -o eng.traineddata eng.unicharset)\n\n",
        exename, exename);
    tprintInfo(
        "Usage for unpacking all tessdata components:\n"
        "  {} -u traineddata_file output_path_prefix\n"
        "  (e.g. {} -u eng.traineddata tmp/eng.)\n\n",
        exename, exename);
    tprintInfo(
        "Usage for listing the network information\n"
        "  {} -l traineddata_file\n"
        "  (e.g. {} -l eng.traineddata)\n\n",
        exename, exename);
    tprintInfo(
        "Usage for listing directory of components:\n"
        "  {} -d traineddata_file\n\n",
        exename);
    tprintInfo(
        "NOTE: Above two flags may combined as -dl or -ld to get both outputs.\n\n"
    );
    tprintInfo(
        "Usage for compacting LSTM component to int:\n"
        "  {} -c traineddata_file\n\n",
        exename);
    tprintInfo(
        "Usage for transforming the proprietary .traineddata file to a zip archive:\n"
        "  {} -t traineddata_file\n\n",
        exename);
  };

  for (int err_round = 0;; err_round++) {
    int rv = tesseract::ParseCommandLineFlags("unicharset dawgfile wordlistfile", usage_f, &argc, &argv);
    if (rv > 0)
      return rv;
    if (rv == 0)
      return err_round;

    if (argc < 4) {
      tesseract::tprintError("Not enough parameters specified on commandline.\n");
      argc = 1;
      continue;
    }
    if (argc > 4) {
      tesseract::tprintError("Too many parameters specified on commandline.\n");
      argc = 1;
      continue;
    }

    tesseract::TessdataManager tm;

    if (argc == 2) {
      tprintDebug("Combining tessdata files\n");
      std::string lang = argv[1];
      const char* last = &argv[1][strlen(argv[1]) - 1];
      if (*last != '.') {
        lang += '.';
      }
      std::string output_file = lang;
      output_file += kTrainedDataSuffix;
      if (!tm.CombineDataFiles(lang.c_str(), output_file.c_str())) {
        tprintError("Error combining tessdata files into {}\n", output_file);
      }
      else {
        tprintDebug("Output {} created successfully.\n", output_file);
      }
    }
    else if (argc >= 4 &&
            (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "-u") == 0)) {
      // Initialize TessdataManager with the data in the given traineddata file.
      if (!tm.Init(argv[2])) {
        tprintError("Failed to read {}\n", argv[2]);
        return EXIT_FAILURE;
      }
      tprintDebug("Extracting tessdata components from {}\n", argv[2]);
      if (strcmp(argv[1], "-e") == 0) {
        for (int i = 3; i < argc; ++i) {
          errno = 0;
          if (tm.ExtractToFile(argv[i])) {
            tprintDebug("Wrote {}\n", argv[i]);
          }
          else if (errno == 0) {
            tprintError("Not extracting {}, since this component"
                " is not present\n",
                argv[i]);
            return EXIT_FAILURE;
          }
          else {
            tprintError("Could not extract {}: {}\n", argv[i], strerror(errno));
            return EXIT_FAILURE;
          }
        }
      }
      else { // extract all the components
        for (int i = 0; i < tesseract::TESSDATA_NUM_ENTRIES; ++i) {
          std::string filename = argv[3];
          const char* last = &argv[3][strlen(argv[3]) - 1];
          if (*last != '.') {
            filename += '.';
          }
          filename += tesseract::kTessdataFileSuffixes[i];
          errno = 0;
          if (tm.ExtractToFile(filename.c_str())) {
            tprintDebug("Wrote {}\n", filename);
          }
          else if (errno != 0) {
            tprintError("Could not extract {}: {}\n", filename,
                   strerror(errno));
            return EXIT_FAILURE;
          }
        }
      }
    }
    else if (argc >= 4 && strcmp(argv[1], "-o") == 0) {
      // Rename the current traineddata file to a temporary name.
      const char* new_traineddata_filename = argv[2];
      std::string traineddata_filename = new_traineddata_filename;
      traineddata_filename += ".__tmp__";
      if (rename(new_traineddata_filename, traineddata_filename.c_str()) != 0) {
        tprintError("Failed to create a temporary file {}\n",
                traineddata_filename);
        return EXIT_FAILURE;
      }

      // Initialize TessdataManager with the data in the given traineddata file.
      tm.Init(traineddata_filename.c_str());

      // Write the updated traineddata file.
      tm.OverwriteComponents(new_traineddata_filename, argv + 3, argc - 3);
    }
    else if (argc == 3 && strcmp(argv[1], "-c") == 0) {
      if (!tm.Init(argv[2])) {
        tprintError("Failed to read {}\n", argv[2]);
        return EXIT_FAILURE;
      }
      tesseract::TFile fp;
      if (!tm.GetComponent(tesseract::TESSDATA_LSTM, &fp)) {
        tprintError("No LSTM Component found in {}!\n", argv[2]);
        return EXIT_FAILURE;
      }
      tesseract::LSTMRecognizer recognizer;
      recognizer.SetDebug(tess_debug_lstm);
      if (!recognizer.DeSerialize(&tm, &fp)) {
        tprintError("Failed to deserialize LSTM in {}!\n", argv[2]);
        return EXIT_FAILURE;
      }
      recognizer.ConvertToInt();
      std::vector<char> lstm_data;
      fp.OpenWrite(&lstm_data);
      ASSERT_HOST(recognizer.Serialize(&tm, &fp));
      tm.OverwriteEntry(tesseract::TESSDATA_LSTM, &lstm_data[0],
                        lstm_data.size());
      if (!tm.SaveFile(argv[2], nullptr)) {
        tprintError("Failed to write modified traineddata:{}!\n", argv[2]);
        return EXIT_FAILURE;
      }
    }
    else if (argc == 3 && strcmp(argv[1], "-t") == 0) {
#if defined(HAVE_LIBARCHIVE)
      if (!tm.Init(argv[2])) {
        tprintError("Failed to read %s\n", argv[2]);
        return EXIT_FAILURE;
      }
      if (!tm.SaveFile(argv[2], nullptr)) {
        tprintError("Failed to tranform traineddata:%s!\n", argv[2]);
        return EXIT_FAILURE;
      }
#else
      tprintError("Failed to load libarchive. Is tesseract compiled with libarchive support?\n");
#endif
    }
    else if (argc == 3 && strcmp(argv[1], "-d") == 0) {
      return list_components(tm, argv[2]);
    }
    else if (argc == 3 && strcmp(argv[1], "-l") == 0) {
      return list_network(tm, argv[2], tess_debug_lstm);
    }
    else if (argc == 3 && strcmp(argv[1], "-dl") == 0) {
      int result = list_components(tm, argv[2]);
      if (result == EXIT_SUCCESS) {
        result = list_network(tm, nullptr, tess_debug_lstm);
      }
      return result;
    }
    else if (argc == 3 && strcmp(argv[1], "-ld") == 0) {
      int result = list_network(tm, argv[2], tess_debug_lstm);
      if (result == EXIT_SUCCESS) {
        result = list_components(tm, nullptr);
      }
      return result;
    }
    else {
      tprintError("Unsupported command '{}' or bad number of arguments ({}).\n", argv[1], argc - 1);
      argc = 1;
      continue;
    }
    tm.Directory();
    return EXIT_SUCCESS;
  }
}
