/**********************************************************************
 * File:        rejctmap.cpp  (Formerly rejmap.c)
 * Description: REJ and REJMAP class functions.
 * Author:      Phil Cheatle
 *
 * (C) Copyright 1994, Hewlett-Packard Ltd.
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 *
 **********************************************************************/

#include <tesseract/preparation.h> // compiler config, etc.

#include "rejctmap.h"

#include <memory>

#include <tesseract/params.h>

namespace tesseract {

std::string REJ::full_print_to_string() const {
  std::string msg;
  msg += fmt::format("R_TESS_FAILURE: {}\n", flag(R_TESS_FAILURE) ? "T" : "F");
  msg += fmt::format("R_SMALL_XHT: {}\n", flag(R_SMALL_XHT) ? "T" : "F");
  msg += fmt::format("R_EDGE_CHAR: {}\n", flag(R_EDGE_CHAR) ? "T" : "F");
  msg += fmt::format("R_1IL_CONFLICT: {}\n", flag(R_1IL_CONFLICT) ? "T" : "F");
  msg += fmt::format("R_POSTNN_1IL: {}\n", flag(R_POSTNN_1IL) ? "T" : "F");
  msg += fmt::format("R_REJ_CBLOB: {}\n", flag(R_REJ_CBLOB) ? "T" : "F");
  msg += fmt::format("R_MM_REJECT: {}\n", flag(R_MM_REJECT) ? "T" : "F");
  msg += fmt::format("R_BAD_REPETITION: {}\n", flag(R_BAD_REPETITION) ? "T" : "F");
  msg += fmt::format("R_POOR_MATCH: {}\n", flag(R_POOR_MATCH) ? "T" : "F");
  msg += fmt::format("R_NOT_TESS_ACCEPTED: {}\n", flag(R_NOT_TESS_ACCEPTED) ? "T" : "F");
  msg += fmt::format("R_CONTAINS_BLANKS: {}\n", flag(R_CONTAINS_BLANKS) ? "T" : "F");
  msg += fmt::format("R_BAD_PERMUTER: {}\n", flag(R_BAD_PERMUTER) ? "T" : "F");
  msg += fmt::format("R_HYPHEN: {}\n", flag(R_HYPHEN) ? "T" : "F");
  msg += fmt::format("R_DUBIOUS: {}\n", flag(R_DUBIOUS) ? "T" : "F");
  msg += fmt::format("R_NO_ALPHANUMS: {}\n", flag(R_NO_ALPHANUMS) ? "T" : "F");
  msg += fmt::format("R_MOSTLY_REJ: {}\n", flag(R_MOSTLY_REJ) ? "T" : "F");
  msg += fmt::format("R_XHT_FIXUP: {}\n", flag(R_XHT_FIXUP) ? "T" : "F");
  msg += fmt::format("R_BAD_QUALITY: {}\n", flag(R_BAD_QUALITY) ? "T" : "F");
  msg += fmt::format("R_DOC_REJ: {}\n", flag(R_DOC_REJ) ? "T" : "F");
  msg += fmt::format("R_BLOCK_REJ: {}\n", flag(R_BLOCK_REJ) ? "T" : "F");
  msg += fmt::format("R_ROW_REJ: {}\n", flag(R_ROW_REJ) ? "T" : "F");
  msg += fmt::format("R_UNLV_REJ: {}\n", flag(R_UNLV_REJ) ? "T" : "F");
  msg += fmt::format("R_HYPHEN_ACCEPT: {}\n", flag(R_HYPHEN_ACCEPT) ? "T" : "F");
  msg += fmt::format("R_NN_ACCEPT: {}\n", flag(R_NN_ACCEPT) ? "T" : "F");
  msg += fmt::format("R_MM_ACCEPT: {}\n", flag(R_MM_ACCEPT) ? "T" : "F");
  msg += fmt::format("R_QUALITY_ACCEPT: {}\n", flag(R_QUALITY_ACCEPT) ? "T" : "F");
  msg += fmt::format("R_MINIMAL_REJ_ACCEPT: {}\n", flag(R_MINIMAL_REJ_ACCEPT) ? "T" : "F");
  return msg;
}

REJMAP &REJMAP::operator=(const REJMAP &source) {
  initialise(source.len);
  for (unsigned i = 0; i < len; i++) {
    ptr[i] = source.ptr[i];
  }
  return *this;
}

void REJMAP::initialise(uint16_t length) {
  ptr = std::make_unique<REJ[]>(length);
  len = length;
}

int16_t REJMAP::accept_count() const { // How many accepted?
  int16_t count = 0;
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      count++;
    }
  }
  return count;
}

bool REJMAP::recoverable_rejects() const { // Any non perm rejs?
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].recoverable()) {
      return true;
    }
  }
  return false;
}

bool REJMAP::quality_recoverable_rejects() const { // Any potential rejs?
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accept_if_good_quality()) {
      return true;
    }
  }
  return false;
}

void REJMAP::remove_pos( // Cut out an element
    uint16_t pos         // element to remove
) {
  ASSERT_HOST(pos < len);
  ASSERT_HOST(len > 0);

  len--;
  for (; pos < len; pos++) {
    ptr[pos] = ptr[pos + 1];
  }
}

std::string REJMAP::print_to_string() const {
  std::string msg;
  msg += '"';
  for (unsigned i = 0; i < len; i++) {
    msg += ptr[i].display_char();
  }
  msg += '"';
  return msg;
}

std::string REJMAP::full_print_to_string() const {
  std::string msg;
  for (unsigned i = 0; i < len; i++) {
    msg += ptr[i].full_print_to_string();
    msg += "\n";
  }
  return msg;
}

void REJMAP::rej_word_small_xht() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    ptr[i].setrej_small_xht();
  }
}

void REJMAP::rej_word_tess_failure() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    ptr[i].setrej_tess_failure();
  }
}

void REJMAP::rej_word_not_tess_accepted() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_not_tess_accepted();
    }
  }
}

void REJMAP::rej_word_contains_blanks() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_contains_blanks();
    }
  }
}

void REJMAP::rej_word_bad_permuter() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_bad_permuter();
    }
  }
}

void REJMAP::rej_word_xht_fixup() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_xht_fixup();
    }
  }
}

void REJMAP::rej_word_no_alphanums() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_no_alphanums();
    }
  }
}

void REJMAP::rej_word_mostly_rej() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_mostly_rej();
    }
  }
}

void REJMAP::rej_word_bad_quality() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_bad_quality();
    }
  }
}

void REJMAP::rej_word_doc_rej() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_doc_rej();
    }
  }
}

void REJMAP::rej_word_block_rej() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_block_rej();
    }
  }
}

void REJMAP::rej_word_row_rej() { // Reject whole word
  for (unsigned i = 0; i < len; i++) {
    if (ptr[i].accepted()) {
      ptr[i].setrej_row_rej();
    }
  }
}

} // namespace tesseract
