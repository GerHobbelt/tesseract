///////////////////////////////////////////////////////////////////////
// File:        wordlist2dawg.cpp
// Description: Program to generate a DAWG from a word list file
// Author:      Thomas Kielbus
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

// Given a file that contains a list of words (one word per line) this program
// generates the corresponding squished DAWG file.

#include <tesseract/preparation.h> // compiler config, etc.

#include "classify.h"
#include "common/commontraining.h"     // CheckSharedLibraryVersion
#include "dawg.h"
#include "dict.h"
#include "helpers.h"
#include "serialis.h"
#include "trie.h"
#include "unicharset.h"

using namespace tesseract;

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_wordlist2dawg_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  if (argc > 1 && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))) {
    tprintDebug("{}\n", tesseract::TessBaseAPI::Version());
    return EXIT_SUCCESS;
  } else if (!(argc == 4 || (argc == 5 && strcmp(argv[1], "-t") == 0) ||
               (argc == 6 && strcmp(argv[1], "-r") == 0))) {
    tprintDebug(
        "Usage: {} -v | --version |\n"
        "       {} [-t | -r [reverse policy] ] word_list_file"
        " dawg_file unicharset_file\n",
        argv[0], argv[0]);
    return EXIT_FAILURE;
  }
  tesseract::Classify classify;
  int argv_index = 0;
  if (argc == 5) {
    ++argv_index;
  }
  tesseract::Trie::RTLReversePolicy reverse_policy = tesseract::Trie::RRP_DO_NO_REVERSE;
  if (argc == 6) {
    ++argv_index;
    int tmp_int;
    sscanf(argv[++argv_index], "%d", &tmp_int);
    reverse_policy = static_cast<tesseract::Trie::RTLReversePolicy>(tmp_int);
    tprintDebug("Set reverse_policy to {}\n", tesseract::Trie::get_reverse_policy_name(reverse_policy));
  }
  const char *wordlist_filename = argv[++argv_index];
  const char *dawg_filename = argv[++argv_index];
  const char *unicharset_file = argv[++argv_index];
  tprintDebug("Loading unicharset from '{}'\n", unicharset_file);
  if (!classify.getDict().getUnicharset().load_from_file(unicharset_file)) {
    tprintError("Failed to load unicharset from '{}'\n", unicharset_file);
    return EXIT_FAILURE;
  }
  const UNICHARSET &unicharset = classify.getDict().getUnicharset();
  if (argc == 4 || argc == 6) {
    tesseract::Trie trie(
        // the first 3 arguments are not used in this case
        tesseract::DAWG_TYPE_WORD, "", SYSTEM_DAWG_PERM, unicharset.size(),
        classify.getDict().dawg_debug_level);
    tprintDebug("Reading word list from '{}'\n", wordlist_filename);
    if (!trie.read_and_add_word_list(wordlist_filename, unicharset, reverse_policy)) {
      tprintError("Failed to add word list from '{}'\n", wordlist_filename);
      return EXIT_FAILURE;
    }
    tprintDebug("Reducing Trie to SquishedDawg\n");
    std::unique_ptr<tesseract::SquishedDawg> dawg(trie.trie_to_dawg());
    if (dawg && dawg->NumEdges() > 0) {
      tprintDebug("Writing squished DAWG to '{}'\n", dawg_filename);
      dawg->write_squished_dawg(dawg_filename);
    } else {
      tprintWarn("Dawg is empty, skip producing the output file\n");
    }
  } else if (argc == 5) {
    tprintDebug("Loading dawg DAWG from '{}'\n", dawg_filename);
    tesseract::SquishedDawg words(dawg_filename,
                                  // these 3 arguments are not used in this case
                                  tesseract::DAWG_TYPE_WORD, "", SYSTEM_DAWG_PERM,
                                  classify.getDict().dawg_debug_level);
    tprintDebug("Checking word list from '{}'\n", wordlist_filename);
    words.check_for_words(wordlist_filename, unicharset, true);
  } else { // should never get here
    tprintError("Invalid command-line options.\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
