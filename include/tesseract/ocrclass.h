// SPDX-License-Identifier: Apache-2.0
/**********************************************************************
 * File:        ocrclass.h
 * Description: Class definitions and constants for the OCR API.
 * Author:      Hewlett-Packard Co
 *
 * (C) Copyright 1996, Hewlett-Packard Co.
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

/**********************************************************************
 * This file contains typedefs for all the structures used by
 * the HP OCR interface.
 * The structures are designed to allow them to be used with any
 * structure alignment up to 8.
 **********************************************************************/

#ifndef CCUTIL_OCRCLASS_H_
#define CCUTIL_OCRCLASS_H_

#include <chrono>
#include <ctime>

namespace tesseract {

  class TBOX;

/**********************************************************************
 * EANYCODE_CHAR
 * Description of a single character. The character code is defined by
 * the character set of the current font.
 * Output text is sent as an array of these structures.
 * Spaces and line endings in the output are represented in the
 * structures of the surrounding characters. They are not directly
 * represented as characters.
 * The first character in a word has a positive value of blanks.
 * Missing information should be set to the defaults in the comments.
 * If word bounds are known, but not character bounds, then the top and
 * bottom of each character should be those of the word. The left of the
 * first and right of the last char in each word should be set. All other
 * lefts and rights should be set to -1.
 * If set, the values of right and bottom are left+width and top+height.
 * Most of the members come directly from the parameters to ocr_append_char.
 * The formatting member uses the enhancement parameter and combines the
 * line direction stuff into the top 3 bits.
 * The coding is 0=RL char, 1=LR char, 2=DR NL, 3=UL NL, 4=DR Para,
 * 5=UL Para, 6=TB char, 7=BT char. API users do not need to know what
 * the coding is, only that it is backwards compatible with the previous
 * version.
 **********************************************************************/

struct EANYCODE_CHAR { /*single character */
  // It should be noted that the format for char_code for version 2.0 and beyond
  // is UTF8 which means that ASCII characters will come out as one structure
  // but other characters will be returned in two or more instances of this
  // structure with a single byte of the  UTF8 code in each, but each will have
  // the same bounding box. Programs which want to handle languages with
  // different characters sets will need to handle extended characters
  // appropriately, but *all* code needs to be prepared to receive UTF8 coded
  // characters for characters such as bullet and fancy quotes.
  uint16_t char_code; /*character itself */
  int16_t left;       /*of char (-1) */
  int16_t right;      /*of char (-1) */
  int16_t top;        /*of char (-1) */
  int16_t bottom;     /*of char (-1) */
  int16_t font_index; /*what font (0) */
  uint8_t confidence; /*0=perfect, 100=reject (0/100) */
  uint8_t point_size; /*of char, 72=i inch, (10) */
  int8_t blanks;      /*no of spaces before this char (1) */
  uint8_t formatting; /*char formatting (0) */
};

/**********************************************************************
 * ETEXT_DESC
 * Description of the output of the OCR engine.
 * This structure is used as both a progress monitor and the final
 * output header, since it needs to be a valid progress monitor while
 * the OCR engine is storing its output to shared memory.
 * During progress, all the buffer info is -1.
 * Progress starts at 0 and increases to 100 during OCR. No other constraint.
 * Additionally the progress callback contains the bounding box of the word that
 * is currently being processed.
 * Every progress callback, the OCR engine must set ocr_alive to 1.
 * The HP side will set ocr_alive to 0. Repeated failure to reset
 * to 1 indicates that the OCR engine is dead.
 * If the cancel function is not null then it is called with the number of
 * user words found. If it returns true then operation is cancelled.
 **********************************************************************/
class ETEXT_DESC;

/// Return true when the session should be canceled.
///
/// Notes: the cancel signal is not "sticky", i.e. persisted. If the cancel is meant to be permanent,
/// until the application terminates, then you are advised to set the `ETEXT_DESC::abort_the_action`
/// flag to `true` as well: once that flag is set, all subsequent cancel checks are supposed to signal
/// and this callback will not be invoked again.
using CANCEL_FUNC = bool (*)(ETEXT_DESC *self, int word_count);

/// This callback may be used to report the session's progress.
///
/// Notes: as we expect userland code to use their own enhanced derived class instance for the monitor,
/// where ETEXT_DESC is the inherited base class, we also anticipate enhanced behaviour of the
/// progress callback itself:
/// we only invoke the progress callback when either `ETEXT_DESC::previous_progress` equals NaN or when
/// `ETEXT_DESC::previous_progress` and `ETEXT_DESC::progress` differ by 0.1 or more, i.e. 0.1%, which
/// we designate "important enough to notify the outside world".
///
/// As your userland progress callback handler may be more elaborate and/or have other rate limiting
/// features built in, we expect the progress callback to copy/update the `ETEXT_DESC::previous_progress`
/// value itself: we don't touch it so you have full control over rate limiting the progress reports.
///
/// For an example use of this, see the tesseract CLI and its monitor implementation in
/// `tesseract.cpp::CLI_Monitor`.
/// 
using PROGRESS_FUNC = void (*)(ETEXT_DESC *self, int left, int right, int top, int bottom);

/**
 * Progress monitor covers word recognition and layout analysis.
 *
 * See Ray comment in https://github.com/tesseract-ocr/tesseract/pull/27
 */
class ETEXT_DESC { // output header
public:
  float progress{0.0}; /// percent complete increasing (0-100)
  float previous_progress{NAN}; /// internal tracker used by exec_progress_func() et al

  volatile int8_t ocr_alive{0}; /// watchdog flag: ocr engine sets to 1, (async) monitor resets to 0

  CANCEL_FUNC cancel{nullptr};  /// returns true to cancel
  PROGRESS_FUNC progress_callback{nullptr};  /// called whenever progress increases. See also PROGRESS_FUNC notes.

  std::chrono::steady_clock::time_point end_time;
  ///< Time to stop. Expected to be set only by call to set_deadline_msecs().

  /// This flag signals tesseract to abort the current operation. It is checked by calling the kick_watchdog_and_check_for_cancel() method.
  volatile bool abort_the_action{false};

  ETEXT_DESC();

  void reset_values_to_Factory_Defaults();

  // Sets the end time to be deadline_msecs milliseconds from now. Any `deadline_msecs` value <= 0 will disable the deadline by setting an deadline in infinity.
  void set_deadline_msecs(int32_t deadline_msecs);

  // Returns false if we've not passed the end_time, or have not set a deadline.
  bool deadline_exceeded() const;

  // Return true when cancel state has been flagged through whatever means.
  bool kick_watchdog_and_check_for_cancel(int word_count = 0);

  // increment progress by 'one point'.
  //
  // Note: uses a exponential degradation to smooth the progress towards 100%
  // without knowing how often this method will be invoked. Guarantees to
  // never rise above 99.9%, i.e. you must explicitly set progress to 100%
  // when done; it cannot be achieved through this call.
  ETEXT_DESC &bump_progress() noexcept;
  // Ditto, but possibly increase the progress faster using the ratio
  // part_count/whole_count; conditions may apply...
  ETEXT_DESC &bump_progress(int part_count, int whole_count) noexcept;

  ETEXT_DESC &set_progress(float percentage) noexcept;

  ETEXT_DESC &exec_progress_func(int left, int right, int top, int bottom);
  ETEXT_DESC &exec_progress_func(const TBOX *box);
  ETEXT_DESC &exec_progress_func(const TBOX &box);
  ETEXT_DESC &exec_progress_func();
};

} // namespace tesseract

#endif // CCUTIL_OCRCLASS_H_
