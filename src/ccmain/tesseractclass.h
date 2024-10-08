///////////////////////////////////////////////////////////////////////
// File:        tesseractclass.h
// Description: The Tesseract class. It holds/owns everything needed
//              to run Tesseract on a single language, and also a set of
//              sub-Tesseracts to run sub-languages. For thread safety, *every*
//              global variable goes in here, directly, or indirectly.
//              This makes it safe to run multiple Tesseracts in different
//              threads in parallel, and keeps the different language
//              instances separate.
// Author:      Ray Smith
//
// (C) Copyright 2008, Google Inc.
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

#ifndef TESSERACT_CCMAIN_TESSERACTCLASS_H_
#define TESSERACT_CCMAIN_TESSERACTCLASS_H_

#include <tesseract/preparation.h> // compiler config, etc.

#include "control.h"               // for ACCEPTABLE_WERD_TYPE
#include "debugpixa.h"             // for DebugPixa
#include "devanagari_processing.h" // for ShiroRekhaSplitter
#if !DISABLED_LEGACY_ENGINE
#  include "docqual.h" // for GARBAGE_LEVEL
#endif
#include "genericvector.h"   // for PointerVector
#include "pageres.h"         // for WERD_RES (ptr only), PAGE_RES (pt...
#include <tesseract/params.h>          // for BOOL_VAR_H, BoolParam, DoubleParam
#include "points.h"          // for FCOORD
#include "ratngs.h"          // for ScriptPos, WERD_CHOICE (ptr only)
#include "tessdatamanager.h" // for TessdataManager
#include "textord.h"         // for Textord
#include "wordrec.h"         // for Wordrec
#include "imagefind.h"       // for ImageFind
#include "linefind.h"        // for LineFinder
#include "genericvector.h"   // for PointerVector (ptr only)

#include <tesseract/publictypes.h> // for OcrEngineMode, PageSegMode, OEM_L...
#include <tesseract/unichar.h>     // for UNICHAR_ID
#include <tesseract/memcost_estimate.h>  // for ImageCostEstimate

#include <leptonica/allheaders.h> // for pixDestroy, pixGetWidth, pixGetHe...

#include <cstdint> // for int16_t, int32_t, uint16_t
#include <cstdio>  // for FILE
#include <map> 

namespace tesseract {

class BLOCK_LIST;
class ETEXT_DESC;
struct OSResults;
class PAGE_RES;
class PAGE_RES_IT;
class ROW;
class SVMenuNode;
class TBOX;
class TO_BLOCK_LIST;
class WERD;
class WERD_CHOICE;
class WERD_RES;
class BLOBNBOX;
class BLOBNBOX_CLIST;
class BLOB_CHOICE_LIST;
class TO_BLOCK_LIST;
class MutableIterator;
class ParagraphModel;
class PARA_LIST;
struct PARA;
class RowInfo;

class ColumnFinder;
class DocumentData;

#if !DISABLED_LEGACY_ENGINE
class EquationDetect;
#endif // !DISABLED_LEGACY_ENGINE

class ImageData;
class LSTMRecognizer;
class OrientationDetector;
class ScriptDetector;
class Tesseract;

// Top-level class for all tesseract global instance data.
// This class either holds or points to all data used by an instance
// of Tesseract, including the memory allocator. When this is
// complete, Tesseract will be thread-safe. UNTIL THEN, IT IS NOT!
//
// NOTE to developers: Do not create cyclic dependencies through this class!
// The directory dependency tree must remain a tree! To keep this clean,
// lower-level code (eg in ccutil, the bottom level) must never need to
// know about the content of a higher-level directory.
// The following scheme will grant the easiest access to lower-level
// global members without creating a cyclic dependency:
//
// Class Hierarchy (^ = inheritance):
//
//             CCUtil (ccutil/ccutil.h)
//                         ^      Members include: UNICHARSET
//           CCStruct (ccstruct/ccstruct.h)
//                         ^       Members include: Image
//           Classify (classify/classify.h)
//                         ^       Members include: Dict
//             WordRec (wordrec/wordrec.h)
//                         ^       Members include: WERD*, DENORM*
//        Tesseract (ccmain/tesseractclass.h)
//                                 Members include: Pix*
//
// Other important classes:
//
//  TessBaseAPI (tesseract/baseapi.h)
//                                 Members include: BLOCK_LIST*, PAGE_RES*,
//                                 Tesseract*, ImageThresholder*
//  Dict (dict/dict.h)
//                                 Members include: Image* (private)
//
// NOTE: that each level contains members that correspond to global
// data that is defined (and used) at that level, not necessarily where
// the type is defined so for instance:
// BOOL_VAR_H(textord_show_blobs);
// goes inside the Textord class, not the cc_util class.

// A collection of various variables for statistics and debugging.
struct TesseractStats {
  TesseractStats()
      : adaption_word_number(0)
      , doc_blob_quality(0)
      , doc_outline_errs(0)
      , doc_char_quality(0)
      , good_char_count(0)
      , doc_good_char_quality(0)
      , word_count(0)
      , dict_words(0)
      , tilde_crunch_written(false)
      , last_char_was_newline(true)
      , last_char_was_tilde(false)
      , write_results_empty_block(true) {}

  int32_t adaption_word_number;
  int16_t doc_blob_quality;
  int16_t doc_outline_errs;
  int16_t doc_char_quality;
  int16_t good_char_count;
  int16_t doc_good_char_quality;
  int32_t word_count;    // count of word in the document
  int32_t dict_words;    // number of dicitionary words in the document
  std::string dump_words_str; // accumulator used by dump_words()
  // Flags used by write_results()
  bool tilde_crunch_written;
  bool last_char_was_newline;
  bool last_char_was_tilde;
  bool write_results_empty_block;
};

// Struct to hold all the pointers to relevant data for processing a word.
struct WordData {
  WordData() : word(nullptr), row(nullptr), block(nullptr), prev_word(nullptr) {}
  explicit WordData(const PAGE_RES_IT &page_res_it)
      : word(page_res_it.word())
      , row(page_res_it.row()->row)
      , block(page_res_it.block()->block)
      , prev_word(nullptr) {}
  WordData(BLOCK *block_in, ROW *row_in, WERD_RES *word_res)
      : word(word_res), row(row_in), block(block_in), prev_word(nullptr) {}

  WERD_RES *word;
  ROW *row;
  BLOCK *block;
  WordData *prev_word;
  PointerVector<WERD_RES> lang_words;
};

// Definition of a Tesseract WordRecognizer. The WordData provides the context
// of row/block, in_word holds an initialized, possibly pre-classified word,
// that the recognizer may or may not consume (but if so it sets
// *in_word=nullptr) and produces one or more output words in out_words, which
// may be the consumed in_word, or may be generated independently. This api
// allows both a conventional tesseract classifier to work, or a line-level
// classifier that generates multiple words from a merged input.
using WordRecognizer = void (Tesseract::*)(const WordData &, WERD_RES **,
                                           PointerVector<WERD_RES> *);

class TESS_API Tesseract: public Wordrec {
public:
  Tesseract() : Tesseract(nullptr) {};
  Tesseract(Tesseract *parent);
  virtual ~Tesseract() override;

  // Return appropriate dictionary
  virtual Dict &getDict() override;

  // Clear as much used memory as possible without resetting the adaptive
  // classifier or losing any other classifier data.
  void Clear(bool invoked_by_destructor = false);
  // Clear all memory of adaption for this and all subclassifiers.
  void ResetAdaptiveClassifier();
  // Clear the document dictionary for this and all subclassifiers.
  void ResetDocumentDictionary();

  /**
   * Clear and free up everything inside, returning the instance to a state
   * equivalent to having just being freshly constructed, with one important
   * distinction:
   *
   * - WipeSqueakyCleanForReUse() will *not* destroy any diagnostics/trace data
   *   cached in the running instance: the goal is to thus be able to produce
   *   diagnostics reports which span multiple rounds of OCR activity, executed
   *   in the single lifespan of the Tesseract instance.
   *
   * Once WipeSqueakyCleanForReUse() has been used, proceed just as when a
   * Tesseract instance has been constructed just now: the same restrictions and
   * conditions exist, once again.
   */
  void WipeSqueakyCleanForReUse(bool invoked_by_destructor = false);

  /**
   * Returns `true` when the current Tesseract instance has been initialized with 
   * language-specific data, which must be wiped if we want to re-use this instance
   * for an independent subsequent run.
   * 
   * Companion function of WipeSqueakyCleanForReUse(): together these allow u
   * keep a long-running Tesseract active and collect diagnostics spanning multiple
   * OCR sessions executed within the lifetime of the Tesseract instance.
   */
  bool RequiresWipeBeforeIndependentReUse() const;

  void ResyncVariablesInternally();

#if !DISABLED_LEGACY_ENGINE
  // Set the equation detector.
  void SetEquationDetect(EquationDetect *detector);
#endif // !DISABLED_LEGACY_ENGINE

  // Simple accessors.
  const FCOORD &reskew() const {
    return reskew_;
  }

  float gradient() const {
    return gradient_;
  }

  // Destroy any existing pix and return a pointer to the pointer.
  void set_pix_binary(Image pix);
  Image pix_binary() const {
    return pix_binary_;
  }
  Image pix_grey() const {
    return pix_grey_;
  }
  void set_pix_grey(Image grey_pix);
  Image pix_original() const {
    return pix_original_;
  }
  // Takes ownership of the given original_pix.
  void set_pix_original(Image original_pix);

  Image GetPixForDebugView();

  void ClearPixForDebugView();

  void ReportDebugInfo();

#if !GRAPHICS_DISABLED
  bool SupportsInteractiveScrollView() const {
    return interactive_display_mode;
  }
#else
  constexpr bool SupportsInteractiveScrollView() const {
    return false;
  }
#endif

  // Return a memory capacity cost estimate for the given image / current original image.
  //
  // (unless overridden by the `pix` argument) uses the current original image for the estimate,
  // i.e. tells you the cost estimate of this run:
  ImageCostEstimate EstimateImageMemoryCost(const Pix* pix = nullptr /* default: use pix_original() data */) const;

  // Helper, which may be invoked after SetInputImage() or equivalent has been called:
  // reports the cost estimate for the current instance/image via `tprintf()` and returns
  // `true` when the cost is expected to be too high.
  bool CheckAndReportIfImageTooLarge(const Pix* pix = nullptr /* default: use pix_original() data */) const;
  bool CheckAndReportIfImageTooLarge(int width, int height) const;

  // Returns a pointer to a Pix representing the best available resolution image
  // of the page, with best available bit depth as second priority. Result can
  // be of any bit depth, but never color-mapped, as that has always been
  // removed. Note that in grey and color, 0 is black and 255 is
  // white. If the input was binary, then black is 1 and white is 0.
  // To tell the difference pixGetDepth() will return 32, 8 or 1.
  // In any case, the return value is a borrowed Pix, and should not be
  // deleted or pixDestroyed.
  Image BestPix() const;

  void set_pix_thresholds(Image thresholds);
  Image pix_thresholds() {
	  return pix_thresholds_;
  }
  int source_resolution() const {
    return source_resolution_;
  }
  void set_source_resolution(int ppi);
  int ImageWidth() const;
  int ImageHeight() const;
  Image scaled_color() const {
    return scaled_color_;
  }
  int scaled_factor() const {
    return scaled_factor_;
  }
  void SetScaledColor(int factor, Image color);
  const Textord &textord() const {
    return textord_;
  }
  Textord *mutable_textord() {
    return &textord_;
  }

  bool right_to_left() const {
    return right_to_left_;
  }
  int num_sub_langs() const {
    return sub_langs_.size();
  }
  Tesseract *get_sub_lang(int index) const;

  // Returns true if any language uses Tesseract (as opposed to LSTM).
  bool AnyTessLang() const;
  // Returns true if any language uses the LSTM.
  bool AnyLSTMLang() const;

  void SetBlackAndWhitelist();

  // Perform steps to prepare underlying binary image/other data structures for
  // page segmentation. Uses the strategy specified in the global variable
  // pageseg_devanagari_split_strategy for perform splitting while preparing for
  // page segmentation.
  void PrepareForPageseg();

  // Perform steps to prepare underlying binary image/other data structures for
  // Tesseract OCR. The current segmentation is required by this method.
  // Uses the strategy specified in the global variable
  // ocr_devanagari_split_strategy for performing splitting while preparing for
  // Tesseract OCR.
  void PrepareForTessOCR(BLOCK_LIST *block_list, OSResults *osr);

  int SegmentPage(const char *input_file, BLOCK_LIST *blocks, Tesseract *osd_tess, OSResults *osr);
  void SetupWordScripts(BLOCK_LIST *blocks);
  int AutoPageSeg(PageSegMode pageseg_mode, BLOCK_LIST *blocks, TO_BLOCK_LIST *to_blocks,
                  BLOBNBOX_LIST *diacritic_blobs, Tesseract *osd_tess, OSResults *osr);
  ColumnFinder *SetupPageSegAndDetectOrientation(PageSegMode pageseg_mode, BLOCK_LIST *blocks,
                                                 Tesseract *osd_tess, OSResults *osr,
                                                 TO_BLOCK_LIST *to_blocks, Image *photo_mask_pix,
                                                 Image *music_mask_pix);
  // par_control.cpp
  void PrerecAllWordsPar(const std::vector<WordData> &words);

  //// linerec.cpp
  // Generates training data for training a line recognizer, eg LSTM.
  // Breaks the page into lines, according to the boxes, and writes them to a
  // serialized DocumentData based on output_basename.
  // Return true if successful, false if an error occurred.
  bool TrainLineRecognizer(const char *input_imagename, const std::string &output_basename,
                           BLOCK_LIST *block_list);
  // Generates training data for training a line recognizer, eg LSTM.
  // Breaks the boxes into lines, normalizes them, converts to ImageData and
  // appends them to the given training_data.
  void TrainFromBoxes(const std::vector<TBOX> &boxes, const std::vector<std::string> &texts,
                      BLOCK_LIST *block_list, DocumentData *training_data);

  // Returns an Imagedata containing the image of the given textline,
  // and ground truth boxes/truth text if available in the input.
  // The image is not normalized in any way.
  ImageData *GetLineData(const TBOX &line_box, const std::vector<TBOX> &boxes,
                         const std::vector<std::string> &texts, int start_box, int end_box,
                         const BLOCK &block);
  // Helper gets the image of a rectangle, using the block.re_rotation() if
  // needed to get to the image, and rotating the result back to horizontal
  // layout. (CJK characters will be on their left sides) The vertical text flag
  // is set in the returned ImageData if the text was originally vertical, which
  // can be used to invoke a different CJK recognition engine. The revised_box
  // is also returned to enable calculation of output bounding boxes.
  ImageData *GetRectImage(const TBOX &box, const BLOCK &block, int padding,
                          TBOX *revised_box) const;
  // Recognizes a word or group of words, converting to WERD_RES in *words.
  // Analogous to classify_word_pass1, but can handle a group of words as well.
  void LSTMRecognizeWord(const BLOCK &block, ROW *row, WERD_RES *word,
                         PointerVector<WERD_RES> *words);
  // Apply segmentation search to the given set of words, within the constraints
  // of the existing ratings matrix. If there is already a best_choice on a word
  // leaves it untouched and just sets the done/accepted etc flags.
  void SearchWords(PointerVector<WERD_RES> *words);

  //// control.h /////////////////////////////////////////////////////////
  bool ProcessTargetWord(const TBOX &word_box, const TBOX &target_word_box, const char *word_config,
                         int pass);
  // Sets up the words ready for whichever engine is to be run
  void SetupAllWordsPassN(int pass_n, const TBOX *target_word_box, const char *word_config,
                          PAGE_RES *page_res, std::vector<WordData> *words);
  // Sets up the single word ready for whichever engine is to be run.
  void SetupWordPassN(int pass_n, WordData *word);
  // Runs word recognition on all the words.
  bool RecogAllWordsPassN(int pass_n, ETEXT_DESC *monitor, PAGE_RES_IT *pr_it,
                          std::vector<WordData> *words);
  bool recog_all_words(PAGE_RES *page_res, ETEXT_DESC *monitor, const TBOX *target_word_box,
                       const char *word_config, int dopasses);
  void rejection_passes(PAGE_RES *page_res, ETEXT_DESC *monitor, const TBOX *target_word_box,
                        const char *word_config);
  void bigram_correction_pass(PAGE_RES *page_res);
  void blamer_pass(PAGE_RES *page_res);
  // Sets script positions and detects smallcaps on all output words.
  void script_pos_pass(PAGE_RES *page_res);
  // Helper to recognize the word using the given (language-specific) tesseract.
  // Returns positive if this recognizer found more new best words than the
  // number kept from best_words.
  int RetryWithLanguage(const WordData &word_data, WordRecognizer recognizer,
                        WERD_RES **in_word, PointerVector<WERD_RES> *best_words);

protected:
  // Helper chooses the best combination of words, transferring good ones from
  // new_words to best_words. To win, a new word must have (better rating and
  // certainty) or (better permuter status and rating within rating ratio and
  // certainty within certainty margin) than current best.
  // All the new_words are consumed (moved to best_words or deleted.)
  // The return value is the number of new_words used minus the number of
  // best_words that remain in the output.
  int SelectBestWords(double rating_ratio, double certainty_margin,
                             PointerVector<WERD_RES>* new_words,
                             PointerVector<WERD_RES>* best_words);
  // Factored helper computes the rating, certainty, badness and validity of
  // the permuter of the words in [first_index, end_index).
  void EvaluateWordSpan(const PointerVector<WERD_RES>& words, unsigned int first_index, unsigned int end_index,
                               float* rating, float* certainty, bool* bad, bool* valid_permuter);
  // Helper finds the gap between the index word and the next.
  void WordGap(const PointerVector<WERD_RES>& words, unsigned int index, TDimension* right, TDimension* next_left);

public:
  // Moves good-looking "noise"/diacritics from the reject list to the main
  // blob list on the current word. Returns true if anything was done, and
  // sets make_next_word_fuzzy if blob(s) were added to the end of the word.
  bool ReassignDiacritics(int pass, PAGE_RES_IT *pr_it, bool *make_next_word_fuzzy);
  // Attempts to put noise/diacritic outlines into the blobs that they overlap.
  // Input: a set of noisy outlines that probably belong to the real_word.
  // Output: outlines that overlapped blobs are set to nullptr and put back into
  // the word, either in the blobs or in the reject list.
  void AssignDiacriticsToOverlappingBlobs(const std::vector<C_OUTLINE *> &outlines, int pass,
                                          WERD *real_word, PAGE_RES_IT *pr_it,
                                          std::vector<bool> *word_wanted,
                                          std::vector<bool> *overlapped_any_blob,
                                          std::vector<C_BLOB *> *target_blobs);
  // Attempts to assign non-overlapping outlines to their nearest blobs or
  // make new blobs out of them.
  void AssignDiacriticsToNewBlobs(const std::vector<C_OUTLINE *> &outlines, int pass,
                                  WERD *real_word, PAGE_RES_IT *pr_it,
                                  std::vector<bool> *word_wanted,
                                  std::vector<C_BLOB *> *target_blobs);
  // Starting with ok_outlines set to indicate which outlines overlap the blob,
  // chooses the optimal set (approximately) and returns true if any outlines
  // are desired, in which case ok_outlines indicates which ones.
  bool SelectGoodDiacriticOutlines(int pass, float certainty_threshold, PAGE_RES_IT *pr_it,
                                   C_BLOB *blob, const std::vector<C_OUTLINE *> &outlines,
                                   int num_outlines, std::vector<bool> *ok_outlines);
  // Classifies the given blob plus the outlines flagged by ok_outlines, undoes
  // the inclusion of the outlines, and returns the certainty of the raw choice.
  float ClassifyBlobPlusOutlines(const std::vector<bool> &ok_outlines,
                                 const std::vector<C_OUTLINE *> &outlines, int pass_n,
                                 PAGE_RES_IT *pr_it, C_BLOB *blob, std::string &best_str);
  // Classifies the given blob (part of word_data->word->word) as an individual
  // word, using languages, chopper etc, returning only the certainty of the
  // best raw choice, and undoing all the work done to fake out the word.
  float ClassifyBlobAsWord(int pass_n, PAGE_RES_IT *pr_it, C_BLOB *blob, std::string &best_str,
                           float *c2);
  // Generic function for classifying a word. Can be used either for pass1 or
  // pass2 according to the function passed to recognizer.
  // word_data holds the word to be recognized, and its block and row, and
  // pr_it points to the word as well, in case we are running LSTM and it wants
  // to output multiple words.
  // Recognizes in the current language, and if successful (a.k.a. accepted) that is all.
  // If recognition was not successful, tries all available languages until
  // it gets a successful result or runs out of languages. Keeps the best result,
  // where "best" is defined as: the first language that producs an *acceptable* result
  // (as determined by Dict::AcceptableResult() et al).
  void classify_word_and_language(int pass_n, PAGE_RES_IT *pr_it, WordData *word_data);
  void classify_word_pass1(const WordData &word_data, WERD_RES **in_word,
                           PointerVector<WERD_RES> *out_words);
  void recog_pseudo_word(PAGE_RES *page_res, // blocks to check
                         TBOX &selection_box);

  void fix_rep_char(PAGE_RES_IT *page_res_it);

  ACCEPTABLE_WERD_TYPE acceptable_word_string(const UNICHARSET &char_set, const char *s,
                                              const char *lengths);
  void match_word_pass_n(int pass_n, WERD_RES *word, ROW *row, BLOCK *block);
  void classify_word_pass2(const WordData &word_data, WERD_RES **in_word,
                           PointerVector<WERD_RES> *out_words);
  void ReportXhtFixResult(bool accept_new_word, float new_x_ht, WERD_RES *word, WERD_RES *new_word);
  bool RunOldFixXht(WERD_RES *word, BLOCK *block, ROW *row);
  bool TrainedXheightFix(WERD_RES *word, BLOCK *block, ROW *row);
  // Runs recognition with the test baseline shift and x-height and returns true
  // if there was an improvement in recognition result.
  bool TestNewNormalization(int original_misfits, float baseline_shift, float new_x_ht,
                            WERD_RES *word, BLOCK *block, ROW *row);
  bool recog_interactive(PAGE_RES_IT *pr_it);

  // Set fonts of this word.
  void set_word_fonts(WERD_RES *word, std::vector<int> font_choices = std::vector<int>());
  std::vector<int> score_word_fonts(WERD_RES *word);
  void score_word_fonts_by_letter(WERD_RES *word, std::map<int, std::map<int, std::map<int, int>>> & page_fonts_letter, int font_id);
  void font_recognition_pass(PAGE_RES *page_res);
  void italic_recognition_pass(PAGE_RES *page_res);
  void dictionary_correction_pass(PAGE_RES *page_res);
  bool check_debug_pt(WERD_RES *word, int location);

  //// superscript.cpp ////////////////////////////////////////////////////
  bool SubAndSuperscriptFix(WERD_RES *word_res);
  void GetSubAndSuperscriptCandidates(const WERD_RES *word, int *num_rebuilt_leading,
                                      ScriptPos *leading_pos, float *leading_certainty,
                                      int *num_rebuilt_trailing, ScriptPos *trailing_pos,
                                      float *trailing_certainty, float *avg_certainty,
                                      float *unlikely_threshold);
  WERD_RES *TrySuperscriptSplits(int num_chopped_leading, float leading_certainty,
                                 ScriptPos leading_pos, int num_chopped_trailing,
                                 float trailing_certainty, ScriptPos trailing_pos, WERD_RES *word,
                                 bool *is_good, int *retry_leading, int *retry_trailing);
  bool BelievableSuperscript(bool debug, const WERD_RES &word, float certainty_threshold,
                             int *left_ok, int *right_ok) const;

  //// output.h //////////////////////////////////////////////////////////

  void output_pass(PAGE_RES_IT &page_res_it, const TBOX *target_word_box);
  void write_results(PAGE_RES_IT &page_res_it, // full info
                     char newline_type,        // type of newline
                     bool force_eol            // override tilde crunch?
  );
  void set_unlv_suspects(WERD_RES *word);
  UNICHAR_ID get_rep_char(WERD_RES *word); // what char is repeated?
  bool acceptable_number_string(const char *s, const char *lengths);
  int16_t count_alphanums(const WERD_CHOICE &word);
  int16_t count_alphas(const WERD_CHOICE &word);

  // Read a configuration file carrying a set of tesseract parameters.
  // Any parameter (listed in the config file) which has not been set yet, will
  // be set, while already set-up parameters are silently skipped.
  // Thus we can establish an easy order of precendence, where
  // first-come-first-serve i.e. the first occurrence of a parameter
  // determines its value.
  //
  // Note: parameter values are 'released' for another round of initialization
  // like this by invoking one of the ReadyParametersForReinitialization() or
  // ResetParametersToFactoryDefault() methods.
  void read_config_file(const char *filename);

  // Set each to the specified parameters to the given value, iff the parameter
  // has not been set yet.
  // Invoking this call before invoking read_config_file() will override
  // the setting in the config file for each of the listed variables.
  //
  // Return false when unknown parameters are listed in the vector;
  // otherwise return true (already set parameters will have been skipped
  // silently).
  //
  // Note: parameter values are 'released' for another round of initialization
  // like this by invoking one of the ReadyParametersForReinitialization() or
  // ResetParametersToFactoryDefault() methods.
  bool InitParameters(const std::vector<std::string> &vars_vec,
                       const std::vector<std::string> &vars_values);

  // Tesseract parameter values are 'released' for another round of initialization
  // by way of InitParameters() and/or read_config_file().
  //
  // The current parameter values will not be altered by this call; use this
  // method if you want to keep the currently active parameter values as a kind
  // of 'good initial setup' for any subsequent teseract action.
  void ReadyParametersForReinitialization();

  // Tesseract parameter values are 'released' for another round of initialization
  // by way of InitParameters() and/or read_config_file().
  //
  // The current parameter values are reset to their factory defaults by this call.
  void ResetParametersToFactoryDefault();

  // Initialize for potentially a set of languages defined by the language
  // string and recursively any additional languages required by any language
  // traineddata file (via tessedit_load_sublangs in its config) that is loaded.
  // 
  // See init_tesseract_internal for args.
  int init_tesseract(const std::string &arg0, const std::string &textbase,
                     const std::vector<std::string> &configs,
                     TessdataManager *mgr);
  int init_tesseract(const std::string &datapath, const std::string &language, OcrEngineMode oem, TessdataManager *mgr);
  int init_tesseract(const std::string &datapath, const std::string &language, OcrEngineMode oem);
  int init_tesseract(const std::string &datapath, TessdataManager *mgr);

  // Common initialization for a single language.
  // 
  // arg0 is the datapath for the tessdata directory, which could be the
  // path of the tessdata directory with no trailing /, or (if tessdata
  // lives in the same directory as the executable, the path of the executable,
  // hence the name arg0.
  // 
  // textbase is an optional output file basename (used only for training)
  // 
  // language is the language code to load.
  // 
  // oem controls which engine(s) will operate on the image.
  // 
  // configs is a vector of optional config filenames to load variables from.
  // May be empty.
  // 
  // vars_vec is an optional vector of variables to set. May be empty.
  // 
  // vars_values is an optional corresponding vector of values for the variables
  // in vars_vec.
  int init_tesseract_internal(const std::string &arg0, const std::string &textbase,
                              const std::string &language, OcrEngineMode oem, 
						                  const std::vector<std::string> &configs,
                              TessdataManager *mgr);

#if !DISABLED_LEGACY_ENGINE
  // Set the universal_id member of each font to be unique among all
  // instances of the same font loaded.
  void SetupUniversalFontIds();
#endif

  void recognize_page(std::string &image_name);
  void end_tesseract();

  bool init_tesseract_lang_data(const std::string &arg0,
                                const std::string &language, OcrEngineMode oem, 
							                  const std::vector<std::string> &configs,
                                TessdataManager *mgr);

  void ParseLanguageString(const std::string &lang_str, std::vector<std::string> *to_load,
                           std::vector<std::string> *not_to_load);

  static BOXA *ParseRectsString(const char *rects_str);

  //// pgedit.h //////////////////////////////////////////////////////////
  SVMenuNode *build_menu_new();
#if !GRAPHICS_DISABLED
  void pgeditor_main(int width, int height, PAGE_RES *page_res);

  void process_image_event( // action in image win
      const SVEvent &event);
  bool process_cmd_win_event( // UI command semantics
      int32_t cmd_event,      // which menu item?
      const char *new_value   // any prompt data
  );
#endif // !GRAPHICS_DISABLED
  void debug_word(PAGE_RES *page_res, const TBOX &selection_box);
  void do_re_display(PAGE_RES *page_res, bool (tesseract::Tesseract::*word_painter)(PAGE_RES_IT *pr_it));
  bool word_display(PAGE_RES_IT *pr_it);
  bool word_bln_display(PAGE_RES_IT *pr_it);
  bool word_blank_and_set_display(PAGE_RES_IT *pr_its);
  bool word_set_display(PAGE_RES_IT *pr_it);

  // #if !GRAPHICS_DISABLED
  bool word_dumper(PAGE_RES_IT *pr_it);
  // #endif // !GRAPHICS_DISABLED
  void blob_feature_display(PAGE_RES *page_res, const TBOX &selection_box);
  //// reject.h //////////////////////////////////////////////////////////
  // make rej map for word
  void make_reject_map(WERD_RES *word, ROW *row, int16_t pass);
  bool one_ell_conflict(WERD_RES *word_res, bool update_map);
  int16_t first_alphanum_index(const char *word, const char *word_lengths);
  int16_t first_alphanum_offset(const char *word, const char *word_lengths);
  int16_t alpha_count(const char *word, const char *word_lengths);
  bool word_contains_non_1_digit(const char *word, const char *word_lengths);
  void dont_allow_1Il(WERD_RES *word);
  int16_t count_alphanums( // how many alphanums
      WERD_RES *word);
  void flip_0O(WERD_RES *word);
  bool non_0_digit(const UNICHARSET &ch_set, UNICHAR_ID unichar_id);
  bool non_O_upper(const UNICHARSET &ch_set, UNICHAR_ID unichar_id);
  bool repeated_nonalphanum_wd(WERD_RES *word, ROW *row);
  void nn_match_word( // Match a word
      WERD_RES *word, ROW *row);
  void nn_recover_rejects(WERD_RES *word, ROW *row);
  void set_done( // set done flag
      WERD_RES *word, int16_t pass);
  int16_t safe_dict_word(const WERD_RES *werd_res); // is best_choice in dict?
  void flip_hyphens(WERD_RES *word);
  void reject_I_1_L(WERD_RES *word);
  void reject_edge_blobs(WERD_RES *word);
  void reject_mostly_rejects(WERD_RES *word);
  //// adaptions.h ///////////////////////////////////////////////////////
  bool word_adaptable( // should we adapt?
      WERD_RES *word, uint16_t mode);

  //// tfacepp.cpp ///////////////////////////////////////////////////////
  void recog_word_recursive(WERD_RES *word);
  void recog_word(WERD_RES *word);
  void split_and_recog_word(WERD_RES *word);
  void split_word(WERD_RES *word, unsigned split_pt, WERD_RES **right_piece,
                  BlamerBundle **orig_blamer_bundle) const;
  void join_words(WERD_RES *word, WERD_RES *word2, BlamerBundle *orig_bb) const;
  //// fixspace.cpp ///////////////////////////////////////////////////////
  bool digit_or_numeric_punct(WERD_RES *word, int char_position);
  int16_t eval_word_spacing(WERD_RES_LIST &word_res_list);
  void match_current_words(WERD_RES_LIST &words, ROW *row, BLOCK *block);
  int16_t fp_eval_word_spacing(WERD_RES_LIST &word_res_list);
  void fix_noisy_space_list(WERD_RES_LIST &best_perm, ROW *row, BLOCK *block);
  void fix_fuzzy_space_list(WERD_RES_LIST &best_perm, ROW *row, BLOCK *block);
  void fix_sp_fp_word(WERD_RES_IT &word_res_it, ROW *row, BLOCK *block);
  void fix_fuzzy_spaces(   // find fuzzy words
      ETEXT_DESC *monitor, // progress monitor
      int32_t word_count,  // count of words in doc
      PAGE_RES *page_res);
  void dump_words(WERD_RES_LIST &perm, int16_t score, int16_t mode, bool improved);
  bool fixspace_thinks_word_done(WERD_RES *word);
  int16_t worst_noise_blob(WERD_RES *word_res, float *worst_noise_score);
  float blob_noise_score(TBLOB *blob);
  void break_noisiest_blob_word(WERD_RES_LIST &words);
  //// docqual.cpp ////////////////////////////////////////////////////////
#if !DISABLED_LEGACY_ENGINE
  GARBAGE_LEVEL garbage_word(WERD_RES *word, bool ok_dict_word);
  bool potential_word_crunch(WERD_RES *word, GARBAGE_LEVEL garbage_level, bool ok_dict_word);
  void tilde_crunch(PAGE_RES_IT &page_res_it);
#endif
  void unrej_good_quality_words( // unreject potential
      PAGE_RES_IT &page_res_it);
  void doc_and_block_rejection( // reject big chunks
      PAGE_RES_IT &page_res_it, bool good_quality_doc);
#if !DISABLED_LEGACY_ENGINE
  void quality_based_rejection(PAGE_RES_IT &page_res_it, bool good_quality_doc);
#endif
  void convert_bad_unlv_chs(WERD_RES *word_res);
  void tilde_delete(PAGE_RES_IT &page_res_it);
  int16_t word_blob_quality(WERD_RES *word);
  void word_char_quality(WERD_RES *word, int16_t *match_count, int16_t *accepted_match_count);
  void unrej_good_chs(WERD_RES *word);
  int16_t count_outline_errs(char c, int16_t outline_count);
  int16_t word_outline_errs(WERD_RES *word);
#if !DISABLED_LEGACY_ENGINE
  bool terrible_word_crunch(WERD_RES *word, GARBAGE_LEVEL garbage_level);
#endif
  CRUNCH_MODE word_deletable(WERD_RES *word, int16_t &delete_mode);
  int16_t failure_count(WERD_RES *word);
  bool noise_outlines(TWERD *word);
  //// pagewalk.cpp ///////////////////////////////////////////////////////
  void process_selected_words(PAGE_RES *page_res, // blocks to check
                                                  // function to call
                              TBOX &selection_box,
                              bool (tesseract::Tesseract::*word_processor)(PAGE_RES_IT *pr_it));
  //// tessbox.cpp ///////////////////////////////////////////////////////
#if !DISABLED_LEGACY_ENGINE
  void tess_add_doc_word(      // test acceptability
      WERD_CHOICE *word_choice // after context
  );
  void tess_segment_pass_n(int pass_n, WERD_RES *word);
  bool tess_acceptable_word(const WERD_RES &word);
#endif

  //// applybox.cpp //////////////////////////////////////////////////////
  // Applies the box file based on the image name filename, and resegments
  // the words in the block_list (page), with:
  // blob-mode: one blob per line in the box file, words as input.
  // word/line-mode: one blob per space-delimited unit after the #, and one word
  // per line in the box file. (See comment above for box file format.)
  // If find_segmentation is true, (word/line mode) then the classifier is used
  // to re-segment words/lines to match the space-delimited truth string for
  // each box. In this case, the input box may be for a word or even a whole
  // text line, and the output words will contain multiple blobs corresponding
  // to the space-delimited input string.
  // With find_segmentation false, no classifier is needed, but the chopper
  // can still be used to correctly segment touching characters with the help
  // of the input boxes.
  // In the returned PAGE_RES, the WERD_RES are setup as they would be returned
  // from normal classification, i.e. with a word, chopped_word, rebuild_word,
  // seam_array, denorm, box_word, and best_state, but NO best_choice or
  // raw_choice, as they would require a UNICHARSET, which we aim to avoid.
  // Instead, the correct_text member of WERD_RES is set, and this may be later
  // converted to a best_choice using CorrectClassifyWords. CorrectClassifyWords
  // is not required before calling ApplyBoxTraining.
  PAGE_RES *ApplyBoxes(const char *filename, bool find_segmentation, BLOCK_LIST *block_list);

  // Any row xheight that is significantly different from the median is set
  // to the median.
  void PreenXHeights(BLOCK_LIST *block_list);

  // Builds a PAGE_RES from the block_list in the way required for ApplyBoxes:
  // All fuzzy spaces are removed, and all the words are maximally chopped.
  PAGE_RES *SetupApplyBoxes(const std::vector<TBOX> &boxes, BLOCK_LIST *block_list);
  // Tests the chopper by exhaustively running chop_one_blob.
  // The word_res will contain filled chopped_word, seam_array, denorm,
  // box_word and best_state for the maximally chopped word.
  void MaximallyChopWord(const std::vector<TBOX> &boxes, BLOCK *block, ROW *row,
                         WERD_RES *word_res);
  // Gather consecutive blobs that match the given box into the best_state
  // and corresponding correct_text.
  // Fights over which box owns which blobs are settled by pre-chopping and
  // applying the blobs to box or next_box with the least non-overlap.
  // Returns false if the box was in error, which can only be caused by
  // failing to find an appropriate blob for a box.
  // This means that occasionally, blobs may be incorrectly segmented if the
  // chopper fails to find a suitable chop point.
  bool ResegmentCharBox(PAGE_RES *page_res, const TBOX *prev_box, const TBOX &box,
                        const TBOX *next_box, const char *correct_text);
  // Consume all source blobs that strongly overlap the given box,
  // putting them into a new word, with the correct_text label.
  // Fights over which box owns which blobs are settled by
  // applying the blobs to box or next_box with the least non-overlap.
  // Returns false if the box was in error, which can only be caused by
  // failing to find an overlapping blob for a box.
  bool ResegmentWordBox(BLOCK_LIST *block_list, const TBOX &box, const TBOX *next_box,
                        const char *correct_text);
  // Resegments the words by running the classifier in an attempt to find the
  // correct segmentation that produces the required string.
  void ReSegmentByClassification(PAGE_RES *page_res);
  // Converts the space-delimited string of utf8 text to a vector of UNICHAR_ID.
  // Returns false if an invalid UNICHAR_ID is encountered.
  bool ConvertStringToUnichars(const char *utf8, std::vector<UNICHAR_ID> *class_ids);
  // Resegments the word to achieve the target_text from the classifier.
  // Returns false if the re-segmentation fails.
  // Uses brute-force combination of up to kMaxGroupSize adjacent blobs, and
  // applies a full search on the classifier results to find the best classified
  // segmentation. As a compromise to obtain better recall, 1-1 ambigiguity
  // substitutions ARE used.
  bool FindSegmentation(const std::vector<UNICHAR_ID> &target_text, WERD_RES *word_res);
  // Recursive helper to find a match to the target_text (from text_index
  // position) in the choices (from choices_pos position).
  // Choices is an array of vectors of length choices_length, with each
  // element representing a starting position in the word, and the
  // vector holding classification results for a sequence of consecutive
  // blobs, with index 0 being a single blob, index 1 being 2 blobs etc.
  void SearchForText(const std::vector<BLOB_CHOICE_LIST *> *choices, int choices_pos,
                     unsigned choices_length, const std::vector<UNICHAR_ID> &target_text,
                     unsigned text_index, float rating, std::vector<int> *segmentation,
                     float *best_rating, std::vector<int> *best_segmentation);
  // Counts up the labelled words and the blobs within.
  // Deletes all unused or emptied words, counting the unused ones.
  // Resets W_BOL and W_EOL flags correctly.
  // Builds the rebuild_word and rebuilds the box_word.
  void TidyUp(PAGE_RES *page_res);
  // Logs a bad box by line in the box file and box coords.
  void ReportFailedBox(int boxfile_lineno, TBOX box, const char *box_ch, const char *err_msg);
  // Creates a fake best_choice entry in each WERD_RES with the correct text.
  void CorrectClassifyWords(PAGE_RES *page_res);
  // Call LearnWord to extract features for labelled blobs within each word.
  // Features are stored in an internal buffer.
  void ApplyBoxTraining(const std::string &fontname, PAGE_RES *page_res);

  //// fixxht.cpp ///////////////////////////////////////////////////////
  // Returns the number of misfit blob tops in this word.
  int CountMisfitTops(WERD_RES *word_res);
  // Returns a new x-height in pixels (original image coords) that is
  // maximally compatible with the result in word_res.
  // Returns 0.0f if no x-height is found that is better than the current
  // estimate.
  float ComputeCompatibleXheight(WERD_RES *word_res, float *baseline_shift);
  //// Data members ///////////////////////////////////////////////////////
  // TODO(ocr-team): Find and remove obsolete parameters.
  STRING_VAR_H(raw_input_image_path);
  STRING_VAR_H(segmentation_mask_input_image_path);
  STRING_VAR_H(visible_output_source_image_path);
  STRING_VAR_H(debug_output_base_path);
  STRING_VAR_H(debug_output_modes);
  STRING_VAR_H(output_base_path);
  STRING_VAR_H(output_base_filename);
  BOOL_VAR_H(tessedit_resegment_from_boxes);
  BOOL_VAR_H(tessedit_resegment_from_line_boxes);
  BOOL_VAR_H(tessedit_train_from_boxes);
  BOOL_VAR_H(tessedit_make_boxes_from_boxes);
  BOOL_VAR_H(tessedit_train_line_recognizer);
  BOOL_VAR_H(tessedit_dump_pageseg_images);
  DOUBLE_VAR_H(invert_threshold);
  INT_VAR_H(tessedit_pageseg_mode);
  INT_VAR_H(preprocess_graynorm_mode);
  INT_VAR_H(thresholding_method);
  BOOL_VAR_H(thresholding_debug);
  DOUBLE_VAR_H(thresholding_window_size);
  DOUBLE_VAR_H(thresholding_kfactor);
  DOUBLE_VAR_H(thresholding_tile_size);
  DOUBLE_VAR_H(thresholding_smooth_kernel_size);
  DOUBLE_VAR_H(thresholding_score_fraction);
  INT_VAR_H(tessedit_ocr_engine_mode);
  STRING_VAR_H(tessedit_char_blacklist);
  STRING_VAR_H(tessedit_char_whitelist);
  STRING_VAR_H(tessedit_char_unblacklist);
  BOOL_VAR_H(tessedit_ambigs_training);
  INT_VAR_H(pageseg_devanagari_split_strategy);
  INT_VAR_H(ocr_devanagari_split_strategy);
  STRING_VAR_H(tessedit_write_params_to_file);
  BOOL_VAR_H(tessedit_adaption_debug);
  INT_VAR_H(bidi_debug);
  INT_VAR_H(applybox_debug);
  INT_VAR_H(applybox_page);
  STRING_VAR_H(applybox_exposure_pattern);
  BOOL_VAR_H(applybox_learn_chars_and_char_frags_mode);
  BOOL_VAR_H(applybox_learn_ngrams_mode);
  BOOL_VAR_H(tessedit_display_outwords);
  BOOL_VAR_H(tessedit_dump_choices);
  BOOL_VAR_H(tessedit_timing_debug);
  BOOL_VAR_H(tessedit_fix_fuzzy_spaces);
  BOOL_VAR_H(tessedit_unrej_any_wd);
  BOOL_VAR_H(tessedit_fix_hyphens);
  BOOL_VAR_H(tessedit_enable_doc_dict);
  BOOL_VAR_H(tessedit_debug_fonts);
  INT_VAR_H(tessedit_font_id);
  BOOL_VAR_H(tessedit_debug_block_rejection);
  BOOL_VAR_H(tessedit_enable_bigram_correction);
  BOOL_VAR_H(tessedit_enable_dict_correction);
  INT_VAR_H(tessedit_bigram_debug);
  BOOL_VAR_H(enable_noise_removal);
  INT_VAR_H(debug_noise_removal);
  // Worst (min) certainty, for which a diacritic is allowed to make the base
  // character worse and still be included.
  DOUBLE_VAR_H(noise_cert_basechar);
  // Worst (min) certainty, for which a non-overlapping diacritic is allowed to
  // make the base character worse and still be included.
  DOUBLE_VAR_H(noise_cert_disjoint);
  // Worst (min) certainty, for which a diacritic is allowed to make a new
  // stand-alone blob.
  DOUBLE_VAR_H(noise_cert_punc);
  // Factor of certainty margin for adding diacritics to not count as worse.
  DOUBLE_VAR_H(noise_cert_factor);
  INT_VAR_H(noise_maxperblob);
  INT_VAR_H(noise_maxperword);
  INT_VAR_H(debug_x_ht_level);
  STRING_VAR_H(chs_leading_punct);
  STRING_VAR_H(chs_trailing_punct1);
  STRING_VAR_H(chs_trailing_punct2);
  DOUBLE_VAR_H(quality_rej_pc);
  DOUBLE_VAR_H(quality_blob_pc);
  DOUBLE_VAR_H(quality_outline_pc);
  DOUBLE_VAR_H(quality_char_pc);
  INT_VAR_H(quality_min_initial_alphas_reqd);
  INT_VAR_H(tessedit_tess_adaption_mode);
  BOOL_VAR_H(tessedit_minimal_rej_pass1);
  BOOL_VAR_H(tessedit_test_adaption);
  BOOL_VAR_H(test_pt);
  DOUBLE_VAR_H(test_pt_x);
  DOUBLE_VAR_H(test_pt_y);
  INT_VAR_H(multilang_debug_level);
  INT_VAR_H(paragraph_debug_level);
  BOOL_VAR_H(paragraph_text_based);
  BOOL_VAR_H(lstm_use_matrix);
  STRING_VAR_H(outlines_odd);
  STRING_VAR_H(outlines_2);
  BOOL_VAR_H(tessedit_good_quality_unrej);
  BOOL_VAR_H(tessedit_use_reject_spaces);
  DOUBLE_VAR_H(tessedit_reject_doc_percent);
  DOUBLE_VAR_H(tessedit_reject_block_percent);
  DOUBLE_VAR_H(tessedit_reject_row_percent);
  DOUBLE_VAR_H(tessedit_whole_wd_rej_row_percent);
  BOOL_VAR_H(tessedit_preserve_blk_rej_perfect_wds);
  BOOL_VAR_H(tessedit_preserve_row_rej_perfect_wds);
  BOOL_VAR_H(tessedit_dont_blkrej_good_wds);
  BOOL_VAR_H(tessedit_dont_rowrej_good_wds);
  INT_VAR_H(tessedit_preserve_min_wd_len);
  BOOL_VAR_H(tessedit_row_rej_good_docs);
  DOUBLE_VAR_H(tessedit_good_doc_still_rowrej_wd);
  BOOL_VAR_H(tessedit_reject_bad_qual_wds);
  BOOL_VAR_H(tessedit_debug_doc_rejection);
  BOOL_VAR_H(tessedit_debug_quality_metrics);
  BOOL_VAR_H(bland_unrej);
  DOUBLE_VAR_H(quality_rowrej_pc);
  BOOL_VAR_H(unlv_tilde_crunching);
  BOOL_VAR_H(hocr_font_info);
  BOOL_VAR_H(hocr_char_boxes);
  BOOL_VAR_H(hocr_images);
  BOOL_VAR_H(crunch_early_merge_tess_fails);
  BOOL_VAR_H(crunch_early_convert_bad_unlv_chs);
  DOUBLE_VAR_H(crunch_terrible_rating);
  BOOL_VAR_H(crunch_terrible_garbage);
  DOUBLE_VAR_H(crunch_poor_garbage_cert);
  DOUBLE_VAR_H(crunch_poor_garbage_rate);
  DOUBLE_VAR_H(crunch_pot_poor_rate);
  DOUBLE_VAR_H(crunch_pot_poor_cert);
  DOUBLE_VAR_H(crunch_del_rating);
  DOUBLE_VAR_H(crunch_del_cert);
  DOUBLE_VAR_H(crunch_del_min_ht);
  DOUBLE_VAR_H(crunch_del_max_ht);
  DOUBLE_VAR_H(crunch_del_min_width);
  DOUBLE_VAR_H(crunch_del_high_word);
  DOUBLE_VAR_H(crunch_del_low_word);
  DOUBLE_VAR_H(crunch_small_outlines_size);
  INT_VAR_H(crunch_rating_max);
  INT_VAR_H(crunch_pot_indicators);
  BOOL_VAR_H(crunch_leave_ok_strings);
  BOOL_VAR_H(crunch_accept_ok);
  BOOL_VAR_H(crunch_leave_accept_strings);
  BOOL_VAR_H(crunch_include_numerals);
  INT_VAR_H(crunch_leave_lc_strings);
  INT_VAR_H(crunch_leave_uc_strings);
  INT_VAR_H(crunch_long_repetitions);
  INT_VAR_H(crunch_debug);
  INT_VAR_H(fixsp_non_noise_limit);
  DOUBLE_VAR_H(fixsp_small_outlines_size);
  BOOL_VAR_H(tessedit_prefer_joined_punct);
  INT_VAR_H(fixsp_done_mode);
  INT_VAR_H(debug_fix_space_level);
  STRING_VAR_H(numeric_punctuation);
  INT_VAR_H(x_ht_acceptance_tolerance);
  INT_VAR_H(x_ht_min_change);
  INT_VAR_H(superscript_debug);
  DOUBLE_VAR_H(superscript_worse_certainty);
  DOUBLE_VAR_H(superscript_bettered_certainty);
  DOUBLE_VAR_H(superscript_scaledown_ratio);
  DOUBLE_VAR_H(subscript_max_y_top);
  DOUBLE_VAR_H(superscript_min_y_bottom);
  BOOL_VAR_H(tessedit_write_block_separators);
  BOOL_VAR_H(tessedit_write_rep_codes);
  BOOL_VAR_H(tessedit_write_unlv);
  BOOL_VAR_H(tessedit_create_txt);
  BOOL_VAR_H(tessedit_create_hocr);
  BOOL_VAR_H(tessedit_create_alto);
  BOOL_VAR_H(tessedit_create_page_xml);
  BOOL_VAR_H(page_xml_polygon);
  INT_VAR_H(page_xml_level);
  BOOL_VAR_H(tessedit_create_lstmbox);
  BOOL_VAR_H(tessedit_create_tsv);
  BOOL_VAR_H(tessedit_create_wordstrbox);
  BOOL_VAR_H(tessedit_create_pdf);
  BOOL_VAR_H(textonly_pdf);
  INT_VAR_H(jpg_quality);
  INT_VAR_H(user_defined_dpi);
  INT_VAR_H(min_characters_to_try);
  STRING_VAR_H(unrecognised_char);
  INT_VAR_H(suspect_level);
  INT_VAR_H(suspect_short_words);
  BOOL_VAR_H(suspect_constrain_1Il);
  DOUBLE_VAR_H(suspect_rating_per_ch);
  DOUBLE_VAR_H(suspect_accept_rating);
  BOOL_VAR_H(tessedit_minimal_rejection);
  BOOL_VAR_H(tessedit_zero_rejection);
  BOOL_VAR_H(tessedit_word_for_word);
  BOOL_VAR_H(tessedit_zero_kelvin_rejection);
  INT_VAR_H(tessedit_reject_mode);
  BOOL_VAR_H(tessedit_rejection_debug);
  BOOL_VAR_H(tessedit_flip_0O);
  DOUBLE_VAR_H(tessedit_lower_flip_hyphen);
  DOUBLE_VAR_H(tessedit_upper_flip_hyphen);
  BOOL_VAR_H(tsv_lang_info);
  BOOL_VAR_H(rej_trust_doc_dawg);
  BOOL_VAR_H(rej_1Il_use_dict_word);
  BOOL_VAR_H(rej_1Il_trust_permuter_type);
  BOOL_VAR_H(rej_use_tess_accepted);
  BOOL_VAR_H(rej_use_tess_blanks);
  BOOL_VAR_H(rej_use_good_perm);
  BOOL_VAR_H(rej_use_sensible_wd);
  BOOL_VAR_H(rej_alphas_in_number_perm);
  DOUBLE_VAR_H(rej_whole_of_mostly_reject_word_fract);
  INT_VAR_H(tessedit_image_border);
  STRING_VAR_H(ok_repeated_ch_non_alphanum_wds);
  STRING_VAR_H(conflict_set_I_l_1);
  INT_VAR_H(min_sane_x_ht_pixels);
  BOOL_VAR_H(tessedit_create_boxfile);
  INT_VAR_H(tessedit_page_number);
  BOOL_VAR_H(tessedit_write_images);
  BOOL_VAR_H(interactive_display_mode);
  STRING_VAR_H(file_type);
  BOOL_VAR_H(tessedit_override_permuter);
  STRING_VAR_H(tessedit_load_sublangs);
  STRING_VAR_H(languages_to_try);
  STRING_VAR_H(reactangles_to_process);
#if !DISABLED_LEGACY_ENGINE
  BOOL_VAR_H(tessedit_use_primary_params_model);
#endif
  // Min acceptable orientation margin (difference in scores between top and 2nd
  // choice in OSResults::orientations) to believe the page orientation.
  DOUBLE_VAR_H(min_orientation_margin);
  //BOOL_VAR_H(textord_tabfind_show_vlines);
  //BOOL_VAR_H(textord_tabfind_show_vlines_scrollview);
  BOOL_VAR_H(textord_use_cjk_fp_model);
  BOOL_VAR_H(poly_allow_detailed_fx);
  BOOL_VAR_H(tessedit_init_config_only);
#if !DISABLED_LEGACY_ENGINE
  BOOL_VAR_H(textord_equation_detect);
#endif // !DISABLED_LEGACY_ENGINE
  BOOL_VAR_H(textord_tabfind_vertical_text);
  BOOL_VAR_H(textord_tabfind_force_vertical_text);
  DOUBLE_VAR_H(textord_tabfind_vertical_text_ratio);
  DOUBLE_VAR_H(textord_tabfind_aligned_gap_fraction);
  INT_VAR_H(tessedit_parallelize);
  BOOL_VAR_H(preserve_interword_spaces);
  STRING_VAR_H(page_separator);
  INT_VAR_H(lstm_choice_mode);
  INT_VAR_H(lstm_choice_iterations);
  DOUBLE_VAR_H(lstm_rating_coefficient);
  BOOL_VAR_H(pageseg_apply_music_mask);
  DOUBLE_VAR_H(max_page_gradient_recognize);
  BOOL_VAR_H(scribe_save_binary_rotated_image);
  BOOL_VAR_H(scribe_save_grey_rotated_image);
  BOOL_VAR_H(scribe_save_original_rotated_image);
  STRING_VAR_H(debug_output_path);
  INT_VAR_H(debug_baseline_fit);
  INT_VAR_H(debug_baseline_y_coord);
  BOOL_VAR_H(debug_write_unlv);
  BOOL_VAR_H(debug_line_finding);
  BOOL_VAR_H(debug_image_normalization);
  BOOL_VAR_H(debug_display_page);
  BOOL_VAR_H(debug_display_page_blocks);
  BOOL_VAR_H(debug_display_page_baselines);
  BOOL_VAR_H(dump_segmented_word_images);
  BOOL_VAR_H(dump_osdetect_process_images);

  //// ambigsrecog.cpp /////////////////////////////////////////////////////////
  FILE *init_recog_training(const char *filename);
  void recog_training_segmented(const char *filename, PAGE_RES *page_res,
                                volatile ETEXT_DESC *monitor, FILE *output_file);
  void ambigs_classify_and_output(const char *label, PAGE_RES_IT *pr_it, FILE *output_file);

  // debug PDF output helper methods:
  void AddPixDebugPage(const Image &pix, const char *title);
  void AddPixDebugPage(const Image &pix, const std::string &title);

  void AddPixCompedOverOrigDebugPage(const Image &pix, const TBOX& bbox, const char *title);
  void AddPixCompedOverOrigDebugPage(const Image &pix, const TBOX &bbox, const std::string &title);
  void AddPixCompedOverOrigDebugPage(const Image &pix, const char *title);
  void AddPixCompedOverOrigDebugPage(const Image &pix, const std::string &title);

  int PushNextPixDebugSection(const std::string &title); // sibling
  int PushSubordinatePixDebugSection(const std::string &title); // child
  void PopPixDebugSection(int handle = -1); // pop active; return focus to parent
  int GetPixDebugSectionLevel() const;

public:
  // Find connected components in the page and process a subset until finished or
  // a stopping criterion is met.
  // Returns the number of blobs used in making the estimate. 0 implies failure.
  int orientation_and_script_detection(const char* filename, OSResults* osr);

  // Filter and sample the blobs.
  // Returns a non-zero number of blobs if the page was successfully processed, or
  // zero if the page had too few characters to be reliable
  int os_detect(TO_BLOCK_LIST* port_blocks, OSResults* osr);

protected:
  // Detect orientation and script from a list of blobs.
  // Returns a non-zero number of blobs if the list was successfully processed, or
  // zero if the list had too few characters to be reliable.
  // If allowed_scripts is non-null and non-empty, it is a list of scripts that
  // constrains both orientation and script detection to consider only scripts
  // from the list.
  int os_detect_blobs(const std::vector<int>* allowed_scripts, BLOBNBOX_CLIST* blob_list, OSResults* osr);

  // Processes a single blob to estimate script and orientation.
  // Return true if estimate of orientation and script satisfies stopping
  // criteria.
  bool os_detect_blob(BLOBNBOX* bbox, OrientationDetector* o, ScriptDetector* s, OSResults* osr);

  // Detect and erase horizontal/vertical lines and picture regions from the
  // image, so that non-text blobs are removed from consideration.
  void remove_nontext_regions(BLOCK_LIST* blocks, TO_BLOCK_LIST* to_blocks);

public:
  // Main entry point for Paragraph Detection Algorithm.
  //
  // Given a set of equally spaced textlines (described by row_infos),
  // Split them into paragraphs.  See http://goto/paragraphstalk
  //
  // Output:
  //   row_owners - one pointer for each row, to the paragraph it belongs to.
  //   paragraphs - this is the actual list of PARA objects.
  //   models - the list of paragraph models referenced by the PARA objects.
  //            caller is responsible for deleting the models.
  void DetectParagraphs(std::vector<RowInfo>* row_infos,
                        std::vector<PARA*>* row_owners, PARA_LIST* paragraphs,
                        std::vector<ParagraphModel*>* models);

  // Given a MutableIterator to the start of a block, run DetectParagraphs on
  // that block and commit the results to the underlying ROW and BLOCK structs,
  // saving the ParagraphModels in models.  Caller owns the models.
  // We use unicharset during the function to answer questions such as "is the
  // first letter of this word upper case?"
  void DetectParagraphs(bool after_text_recognition,
                        const MutableIterator* block_start, std::vector<ParagraphModel*>* models);

public:
  Tesseract* get_parent_instance() const {
    return parent_instance_;
  }

protected:
  Tesseract* parent_instance_;      // reference to parent tesseract instance for sub-languages. Used, f.e., to allow using a single DebugPixa diagnostic channel for all languages tested on the input.

private:
  // The filename of a backup config file. If not null, then we currently
  // have a temporary debug config file loaded, and backup_config_file_
  // will be loaded, and set to null when debug is complete.
  const char *backup_config_file_;
  // The filename of a config file to read when processing a debug word via Tesseract::debug_word().
  std::string word_config_;
  // Image used for input to layout analysis and tesseract recognition.
  // May be modified by the ShiroRekhaSplitter to eliminate the top-line.
  Image pix_binary_;
  // Grey-level input image if the input was not binary, otherwise nullptr.
  Image pix_grey_;
  // Original input image. Color if the input was color.
  Image pix_original_;
  // Thresholds that were used to generate the thresholded image from grey.
  Image pix_thresholds_;
  // canvas copy of pix_binary for debug view painting; this image is always 32-bit depth RGBA.
  Image pix_for_debug_view_;
  // Debug images. If non-empty, will be written on destruction.
  DebugPixa pixa_debug_;
  // Input image resolution after any scaling. The resolution is not well
  // transmitted by operations on Pix, so we keep an independent record here.
  int source_resolution_;
  // The shiro-rekha splitter object which is used to split top-lines in
  // Devanagari words to provide a better word and grapheme segmentation.
  ShiroRekhaSplitter splitter_;
  // Image finder: located image/photo zones in a given image (scanned page).
  ImageFind image_finder_;
  LineFinder line_finder_;

  friend class ColumnFinder;
  friend class CCNonTextDetect;
  friend class ColPartitionGrid;
  friend class ColPartition;
  friend class StrokeWidth;

  // Page segmentation/layout
  Textord textord_;
  // True if the primary language uses right_to_left reading order.
  bool right_to_left_;
  Image scaled_color_;
  int scaled_factor_;
  FCOORD deskew_;
  FCOORD reskew_;
  float gradient_;
  TesseractStats stats_;
  // Sub-languages to be tried in addition to this.
  std::vector<Tesseract *> sub_langs_;
  // Most recently used Tesseract out of this and sub_langs_. The default
  // language for the next word.
  Tesseract *most_recently_used_;
  // The size of the font table, ie max possible font id + 1.
  int font_table_size_;
#if !DISABLED_LEGACY_ENGINE
  // Equation detector. Note: this pointer is NOT owned by the class.
  EquationDetect *equ_detect_;
#endif // !DISABLED_LEGACY_ENGINE
  // LSTM recognizer, if available.
  LSTMRecognizer *lstm_recognizer_;
  // Output "page" number (actually line number) using TrainLineRecognizer.
  int train_line_page_num_;
  /// internal use to help the (re)initialization process after a previous run.
  bool instance_has_been_initialized_; 
};

} // namespace tesseract

#endif // TESSERACT_CCMAIN_TESSERACTCLASS_H_
