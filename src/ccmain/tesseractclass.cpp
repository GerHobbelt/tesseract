///////////////////////////////////////////////////////////////////////
// File:        tesseractclass.cpp
// Description: The Tesseract class. It holds/owns everything needed
//              to run Tesseract on a single language, and also a set of
//              sub-Tesseracts to run sub-languages. For thread safety, *every*
//              variable that was previously global or static (except for
//              constant data, and some visual debugging flags) has been moved
//              in here, directly, or indirectly.
//              This makes it safe to run multiple Tesseracts in different
//              threads in parallel, and keeps the different language
//              instances separate.
//              Some global functions remain, but they are isolated re-entrant
//              functions that operate on their arguments. Functions that work
//              on variable data have been moved to an appropriate class based
//              mostly on the directory hierarchy. For more information see
//              slide 6 of "2ArchitectureAndDataStructures" in
// https://drive.google.com/file/d/0B7l10Bj_LprhbUlIUFlCdGtDYkE/edit?usp=sharing
//              Some global data and related functions still exist in the
//              training-related code, but they don't interfere with normal
//              recognition operation.
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

// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.

#include "tesseractclass.h"

#include <leptonica/allheaders.h>
#include "edgblob.h"
#if !DISABLED_LEGACY_ENGINE
#  include "equationdetect.h"
#endif
#include "lstmrecognizer.h"
#include "thresholder.h" // for ThresholdMethod
#include "global_params.h"

namespace tesseract {

Tesseract::Tesseract(Tesseract *parent)
    : parent_instance_(parent)
    , BOOL_MEMBER(tessedit_resegment_from_boxes, false,
                  "Take segmentation and labeling from box file", params())
    , BOOL_MEMBER(tessedit_resegment_from_line_boxes, false,
                  "Conversion of word/line box file to char box file", params())
    , BOOL_MEMBER(tessedit_train_from_boxes, false, "Generate training data from boxed chars",
                  params())
    , BOOL_MEMBER(tessedit_make_boxes_from_boxes, false, "Generate more boxes from boxed chars",
                  params())
    , BOOL_MEMBER(tessedit_train_line_recognizer, false,
                  "Break input into lines and remap boxes if present", params())
    , BOOL_MEMBER(tessedit_dump_pageseg_images, false,
                  "Dump intermediate images made during page segmentation", params())
    , DOUBLE_MEMBER(invert_threshold, 0.7,
                    "For lines with a mean confidence below this value, OCR is also tried with an inverted image.",
                    params())
    ,
    // The default for pageseg_mode is the old behaviour, so as not to
    // upset anything that relies on that.
    INT_MEMBER(tessedit_pageseg_mode, PSM_SINGLE_BLOCK,
               "Page seg mode: 0=osd only, 1=auto+osd, 2=auto_only, 3=auto, "
               "4=column, "
               "5=block_vert, 6=block, 7=line, 8=word, 9=word_circle, 10=char, "
               "11=sparse_text, 12=sparse_text+osd, 13=raw_line. "
               "(Values from PageSegMode enum in tesseract/publictypes.h)",
               params())
    , INT_MEMBER(preprocess_graynorm_mode, 0, 
                "Grayscale normalization mode: 0=no normalization, 1=thresholding + recognition (i.e. apply to all tasks), "
                "2=thresholding tasks (layout analysis) only, 3=character recognition only. "
                "The modes 1–3 apply non-linear normalization (nlnorm) on a grayscale version "
                "of the input image and replace it for the specified tasks.", 
                params())
    , INT_MEMBER(thresholding_method,
                 static_cast<int>(ThresholdMethod::Otsu),
                 "Thresholding method: 0 = Legacy Otsu, 1 = Adaptive Otsu, 2 = "
                 "Sauvola, 3 = Otsu on "
                 "adaptive normalized background, 4 = Masking and Otsu on "
                 "adaptive normalized background, 5 = Nlbin.",
                 params())
    , BOOL_MEMBER(thresholding_debug, false,
                  "Debug the thresholding process.",
                  params())
    , DOUBLE_MEMBER(thresholding_window_size, 0.33,
                    "Window size for measuring local statistics (to be "
                    "multiplied by image DPI). "
                    "This parameter is used by the Sauvola thresholding method.",
                    params())
    , DOUBLE_MEMBER(thresholding_kfactor, 0.34,
                    "Factor for reducing threshold due to variance. "
                    "This parameter is used by the Sauvola thresholding method. "
                    "Normal range: 0.2-0.5.",
                    params())
    , DOUBLE_MEMBER(thresholding_tile_size, 0.33,
                    "Desired tile size (to be multiplied by image DPI). "
                    "This parameter is used by the Adaptive Leptonica Otsu thresholding "
                    "method.",
                    params())
    , DOUBLE_MEMBER(thresholding_smooth_kernel_size, 0.01,
                    "Size of convolution kernel applied to threshold array "
                    "(to be multiplied by image DPI). Use 0 for no smoothing. "
                    "This parameter is used by the Adaptive Leptonica Otsu thresholding "
                    "method.",
                    params())
    , DOUBLE_MEMBER(thresholding_score_fraction, 0.1,
                    "Fraction of the max Otsu score. "
                    "This parameter is used by the Adaptive Leptonica Otsu thresholding "
                    "method. "
                    "For standard Otsu use 0.0, otherwise 0.1 is recommended.",
                    params())
    , INT_INIT_MEMBER(tessedit_ocr_engine_mode, tesseract::OEM_DEFAULT,
                      "Which OCR engine(s) to run (0: Tesseract, 1: LSTM, 2: both, 3: default). "
                      "Defaults to loading and running the most accurate "
                      "available.",
                      params())
    , STRING_MEMBER(tessedit_char_blacklist, "", "Blacklist of chars not to recognize.",
                    params())
    , STRING_MEMBER(tessedit_char_whitelist, "", "Whitelist of chars to recognize.", params())
    , STRING_MEMBER(tessedit_char_unblacklist, "",
                    "List of chars to override tessedit_char_blacklist.", params())
    , BOOL_MEMBER(tessedit_ambigs_training, false, "Perform training for ambiguities.",
                  params())
    , INT_MEMBER(pageseg_devanagari_split_strategy, tesseract::ShiroRekhaSplitter::NO_SPLIT,
                 "Which top-line splitting process to use for Devanagari "
                 "documents while performing page-segmentation. (0: no splitting (default), 1: minimal splitting, 2: maximal splitting)",
                 params())
    , INT_MEMBER(ocr_devanagari_split_strategy, tesseract::ShiroRekhaSplitter::NO_SPLIT,
                 "Which top-line splitting process to use for Devanagari "
                 "documents while performing ocr. (0: no splitting (default), 1: minimal splitting, 2: maximal splitting)",
                 params())
    , STRING_MEMBER(tessedit_write_params_to_file, "", "Write all parameters to the given file.",
                    params())
    , BOOL_MEMBER(tessedit_adaption_debug, false,
                  "Generate and print debug "
                  "information for adaption.",
                  params())
    , INT_MEMBER(bidi_debug, 0, "Debug level for BiDi.", params())
    , INT_MEMBER(applybox_debug, 1, "Debug level for apply boxes.", params())
    , INT_MEMBER(applybox_page, 0, "Page number to apply boxes from.", params())
    , STRING_MEMBER(applybox_exposure_pattern, ".exp",
                    "Exposure value follows "
                    "this pattern in the image filename. The name of the image "
                    "files are expected to be in the form "
                    "[lang].[fontname].exp[num].tif.",
                    params())
    , BOOL_MEMBER(applybox_learn_chars_and_char_frags_mode, false,
                  "Learn both character fragments (as is done in the "
                  "special low exposure mode) as well as unfragmented "
                  "characters.",
                  params())
    , BOOL_MEMBER(applybox_learn_ngrams_mode, false,
                  "Each bounding box "
                  "is assumed to contain ngrams. Only learn the ngrams "
                  "whose outlines overlap horizontally.",
                  params())
    , BOOL_MEMBER(tessedit_display_outwords, false, "Draw output words.", params())
    , BOOL_MEMBER(tessedit_dump_choices, false, "Dump char choices.", params())
    , BOOL_MEMBER(tessedit_timing_debug, false, "Print timing stats.", params())
    , BOOL_MEMBER(tessedit_fix_fuzzy_spaces, true, "Try to improve fuzzy spaces.", params())
    , BOOL_MEMBER(tessedit_unrej_any_wd, false, "Don't bother with word plausibility.",
                  params())
    , BOOL_MEMBER(tessedit_fix_hyphens, true, "Crunch double hyphens?", params())
    , BOOL_MEMBER(tessedit_enable_doc_dict, true, "Add discovered words to the document dictionary when found to be non-ambiguous through internal heuristic.",
                  params())
    , BOOL_MEMBER(tessedit_debug_fonts, false, "Output font info per char.", params())
    , INT_MEMBER(tessedit_font_id, 0, "Font ID to use or zero.", params())
    , BOOL_MEMBER(tessedit_debug_block_rejection, false, "Block and Row stats.", params())
    , BOOL_MEMBER(tessedit_enable_bigram_correction, true,
                  "Enable correction based on the word bigram dictionary.", params())
    , BOOL_MEMBER(tessedit_enable_dict_correction, false,
                  "Enable single word correction based on the dictionary.", params())
    , INT_MEMBER(tessedit_bigram_debug, 0, "Amount of debug output for bigram correction.",
                 params())
    , BOOL_MEMBER(enable_noise_removal, true,
                  "Remove and conditionally reassign small outlines when they "
                  "confuse layout analysis, determining diacritics vs noise.",
                  params())
    , INT_MEMBER(debug_noise_removal, 0, "Debug reassignment of small outlines.", params())
    , STRING_MEMBER(debug_output_path, "", "Path where to write debug diagnostics.",
                    params())
    ,
    // Worst (min) certainty, for which a diacritic is allowed to make the
    // base
    // character worse and still be included.
    DOUBLE_MEMBER(noise_cert_basechar, -8.0, "Hingepoint for base char certainty.", params())
    ,
    // Worst (min) certainty, for which a non-overlapping diacritic is allowed
    // to make the base character worse and still be included.
    DOUBLE_MEMBER(noise_cert_disjoint, -1.0, "Hingepoint for disjoint certainty.", params())
    ,
    // Worst (min) certainty, for which a diacritic is allowed to make a new
    // stand-alone blob.
    DOUBLE_MEMBER(noise_cert_punc, -3.0, "Threshold for new punc char certainty.", params())
    ,
    // Factor of certainty margin for adding diacritics to not count as worse.
    DOUBLE_MEMBER(noise_cert_factor, 0.375, "Scaling on certainty diff from Hingepoint.",
                  params())
    , INT_MEMBER(noise_maxperblob, 8, "Max diacritics to apply to a blob.", params())
    , INT_MEMBER(noise_maxperword, 16, "Max diacritics to apply to a word.", params())
    , INT_MEMBER(debug_x_ht_level, 0, "Reestimate x-height debug level (0..2).", params())
    , STRING_MEMBER(chs_leading_punct, "('`\"", "Leading punctuation.", params())
    , STRING_MEMBER(chs_trailing_punct1, ").,;:?!", "1st Trailing punctuation.", params())
    , STRING_MEMBER(chs_trailing_punct2, ")'`\"", "2nd Trailing punctuation.", params())
    , DOUBLE_MEMBER(quality_rej_pc, 0.08, "good_quality_doc lte rejection limit.", params())
    , DOUBLE_MEMBER(quality_blob_pc, 0.0, "good_quality_doc gte good blobs limit.", params())
    , DOUBLE_MEMBER(quality_outline_pc, 1.0, "good_quality_doc lte outline error limit.",
                    params())
    , DOUBLE_MEMBER(quality_char_pc, 0.95, "good_quality_doc gte good char limit.", params())
    , INT_MEMBER(quality_min_initial_alphas_reqd, 2, "alphas in a good word.", params())
    , INT_MEMBER(tessedit_tess_adaption_mode, 0x27, "Adaptation decision algorithm for tesseract. "
                 "(bit set where bit 0 = ADAPTABLE_WERD, bit 1 = ACCEPTABLE_WERD, "
                 "bit 2 = CHECK_DAWGS, bit 3 = CHECK_SPACES, bit 4 = CHECK_ONE_ELL_CONFLICT, "
                 "bit 5 = CHECK_AMBIG_WERD)",
                 params())
    , BOOL_MEMBER(tessedit_minimal_rej_pass1, false, "Do minimal rejection on pass 1 output.",
                  params())
    , BOOL_MEMBER(tessedit_test_adaption, false, "Test adaption criteria.", params())
    , BOOL_MEMBER(test_pt, false, "Test for point.", params())
    , DOUBLE_MEMBER(test_pt_x, 99999.99, "xcoord.", params())
    , DOUBLE_MEMBER(test_pt_y, 99999.99, "ycoord.", params())
    , INT_MEMBER(multilang_debug_level, 0, "Print multilang debug info. (0..1)", params())
    , INT_MEMBER(paragraph_debug_level, 0, "Print paragraph debug info. (0..3)", params())
    , BOOL_MEMBER(paragraph_text_based, true,
                  "Run paragraph detection on the post-text-recognition "
                  "(more accurate).",
                  params())
    , BOOL_MEMBER(lstm_use_matrix, 1, "Use ratings matrix/beam search with lstm.", params())
    , STRING_MEMBER(outlines_odd, "%| ", "Non standard number of outlines.", params())
    , STRING_MEMBER(outlines_2, "ij!?%\":;", "Non standard number of outlines.", params())
    , BOOL_MEMBER(tessedit_good_quality_unrej, true, "Reduce rejection on good docs.",
                  params())
    , BOOL_MEMBER(tessedit_use_reject_spaces, true, "Reject spaces?", params())
    , DOUBLE_MEMBER(tessedit_reject_doc_percent, 65.00, "%rej allowed before rej whole doc.",
                    params())
    , DOUBLE_MEMBER(tessedit_reject_block_percent, 45.00, "%rej allowed before rej whole block.",
                    params())
    , DOUBLE_MEMBER(tessedit_reject_row_percent, 40.00, "%rej allowed before rej whole row.",
                    params())
    , DOUBLE_MEMBER(tessedit_whole_wd_rej_row_percent, 70.00,
                    "Number of row rejects in whole word rejects "
                    "which prevents whole row rejection.",
                    params())
    , BOOL_MEMBER(tessedit_preserve_blk_rej_perfect_wds, true,
                  "Only rej partially rejected words in block rejection.", params())
    , BOOL_MEMBER(tessedit_preserve_row_rej_perfect_wds, true,
                  "Only rej partially rejected words in row rejection.", params())
    , BOOL_MEMBER(tessedit_dont_blkrej_good_wds, false, "Use word segmentation quality metric.",
                  params())
    , BOOL_MEMBER(tessedit_dont_rowrej_good_wds, false, "Use word segmentation quality metric.",
                  params())
    , INT_MEMBER(tessedit_preserve_min_wd_len, 2, "Only preserve wds longer than this.",
                 params())
    , BOOL_MEMBER(tessedit_row_rej_good_docs, true, "Apply row rejection to good docs.",
                  params())
    , DOUBLE_MEMBER(tessedit_good_doc_still_rowrej_wd, 1.1,
                    "rej good doc wd if more than this fraction rejected.", params())
    , BOOL_MEMBER(tessedit_reject_bad_qual_wds, true, "Reject all bad quality wds.", params())
    , BOOL_MEMBER(tessedit_debug_doc_rejection, false, "Print doc and Block character rejection page stats.", params())
    , BOOL_MEMBER(tessedit_debug_quality_metrics, false, "Print recognition quality report to debug channel.", params())
    , BOOL_MEMBER(bland_unrej, false, "unrej potential with no checks.", params())
    , DOUBLE_MEMBER(quality_rowrej_pc, 1.1, "good_quality_doc gte good char limit.", params())
    , BOOL_MEMBER(unlv_tilde_crunching, false, "Mark v.bad words for tilde crunch.", params())
    , BOOL_MEMBER(hocr_font_info, false, "Add font info to hocr output.", params())
    , BOOL_MEMBER(hocr_char_boxes, false, "Add coordinates for each character to hocr output.", params())
    , BOOL_MEMBER(hocr_images, false, "Add images to hocr output.", params())
    , BOOL_MEMBER(crunch_early_merge_tess_fails, true, "Before word crunch?", params())
    , BOOL_MEMBER(crunch_early_convert_bad_unlv_chs, false, "Take out ~^ early?", params())
    , DOUBLE_MEMBER(crunch_terrible_rating, 80.0, "crunch rating lt this.", params())
    , BOOL_MEMBER(crunch_terrible_garbage, true, "As it says.", params())
    , DOUBLE_MEMBER(crunch_poor_garbage_cert, -9.0, "crunch garbage cert lt this.", params())
    , DOUBLE_MEMBER(crunch_poor_garbage_rate, 60, "crunch garbage rating lt this.", params())
    , DOUBLE_MEMBER(crunch_pot_poor_rate, 40, "POTENTIAL crunch rating lt this.", params())
    , DOUBLE_MEMBER(crunch_pot_poor_cert, -8.0, "POTENTIAL crunch cert lt this.", params())
    , DOUBLE_MEMBER(crunch_del_rating, 60, "POTENTIAL crunch rating lt this.", params())
    , DOUBLE_MEMBER(crunch_del_cert, -10.0, "POTENTIAL crunch cert lt this.", params())
    , DOUBLE_MEMBER(crunch_del_min_ht, 0.7, "Del if word ht lt xht x this.", params())
    , DOUBLE_MEMBER(crunch_del_max_ht, 3.0, "Del if word ht gt xht x this.", params())
    , DOUBLE_MEMBER(crunch_del_min_width, 3.0, "Del if word width lt xht x this.", params())
    , DOUBLE_MEMBER(crunch_del_high_word, 1.5, "Del if word gt xht x this above bl.", params())
    , DOUBLE_MEMBER(crunch_del_low_word, 0.5, "Del if word gt xht x this below bl.", params())
    , DOUBLE_MEMBER(crunch_small_outlines_size, 0.6, "Small if lt xht x this.", params())
    , INT_MEMBER(crunch_rating_max, 10, "For adj length in rating per ch.", params())
    , INT_MEMBER(crunch_pot_indicators, 1, "How many potential indicators needed.", params())
    , BOOL_MEMBER(crunch_leave_ok_strings, true, "Don't touch sensible strings.", params())
    , BOOL_MEMBER(crunch_accept_ok, true, "Use acceptability in okstring.", params())
    , BOOL_MEMBER(crunch_leave_accept_strings, false, "Don't pot crunch sensible strings.",
                  params())
    , BOOL_MEMBER(crunch_include_numerals, false, "Fiddle alpha figures.", params())
    , INT_MEMBER(crunch_leave_lc_strings, 4, "Don't crunch words with long lower case strings.",
                 params())
    , INT_MEMBER(crunch_leave_uc_strings, 4, "Don't crunch words with long lower case strings.",
                 params())
    , INT_MEMBER(crunch_long_repetitions, 3, "Crunch words with long repetitions.", params())
    , INT_MEMBER(crunch_debug, 0, "Print debug info for word and character crunch.", params())
    , INT_MEMBER(fixsp_non_noise_limit, 1, "How many non-noise blobs either side?", params())
    , DOUBLE_MEMBER(fixsp_small_outlines_size, 0.28, "Small if lt xht x this.", params())
    , BOOL_MEMBER(tessedit_prefer_joined_punct, false, "Reward punctuation joins.", params())
    , INT_MEMBER(fixsp_done_mode, 1, "What constitutes done for spacing.", params())
    , INT_MEMBER(debug_fix_space_level, 0, "Contextual fixspace debug (0..3).", params())
    , STRING_MEMBER(numeric_punctuation, ".,", "Punct. chs expected WITHIN numbers.", params())
    , INT_MEMBER(x_ht_acceptance_tolerance, 8,
                 "Max allowed deviation of blob top outside of font data.", params())
    , INT_MEMBER(x_ht_min_change, 8, "Min change in xht before actually trying it.", params())
    , INT_MEMBER(superscript_debug, 0, "Debug level for sub & superscript fixer.", params())
    , DOUBLE_MEMBER(superscript_worse_certainty, 2.0,
                    "How many times worse "
                    "certainty does a superscript position glyph need to be for "
                    "us to try classifying it as a char with a different "
                    "baseline?",
                    params())
    , DOUBLE_MEMBER(superscript_bettered_certainty, 0.97,
                    "What reduction in "
                    "badness do we think sufficient to choose a superscript "
                    "over what we'd thought.  For example, a value of 0.6 means "
                    "we want to reduce badness of certainty by at least 40%.",
                    params())
    , DOUBLE_MEMBER(superscript_scaledown_ratio, 0.4,
                    "A superscript scaled down more than this is unbelievably "
                    "small.  For example, 0.3 means we expect the font size to "
                    "be no smaller than 30% of the text line font size.",
                    params())
    , DOUBLE_MEMBER(subscript_max_y_top, 0.5,
                    "Maximum top of a character measured as a multiple of "
                    "x-height above the baseline for us to reconsider whether "
                    "it's a subscript.",
                    params())
    , DOUBLE_MEMBER(superscript_min_y_bottom, 0.3,
                    "Minimum bottom of a character measured as a multiple of "
                    "x-height above the baseline for us to reconsider whether "
                    "it's a superscript.",
                    params())
    , BOOL_MEMBER(tessedit_write_block_separators, false, "Write block separators in output.", params())
    , BOOL_MEMBER(tessedit_write_rep_codes, false, "Write repetition char code.", params())
    , BOOL_MEMBER(tessedit_write_unlv, false, "Write .unlv output file.", params())
    , BOOL_MEMBER(tessedit_create_txt, false, "Write .txt output file.", params())
    , BOOL_MEMBER(tessedit_create_hocr, false, "Write .html hOCR output file.", params())
    , BOOL_MEMBER(tessedit_create_alto, false, "Write .xml ALTO file.", params())
    , BOOL_MEMBER(tessedit_create_page_xml, false, "Write .page.xml PAGE file", params())
    , BOOL_MEMBER(page_xml_polygon, true, "Create the PAGE file with polygons instead of box values", params())
    , INT_MEMBER(page_xml_level, 0, "Create the PAGE file on 0=line or 1=word level.", params())
    , BOOL_MEMBER(tessedit_create_lstmbox, false, "Write .box file for LSTM training.", params())
    , BOOL_MEMBER(tessedit_create_tsv, false, "Write .tsv output file.", params())
    , BOOL_MEMBER(tessedit_create_wordstrbox, false, "Write WordStr format .box output file.", params())
    , BOOL_MEMBER(tessedit_create_pdf, false, "Write .pdf output file.", params())
    , BOOL_MEMBER(textonly_pdf, false, "Create PDF with only one invisible text layer.", params())
    , INT_MEMBER(jpg_quality, 85, "Set JPEG quality level.", params())
    , INT_MEMBER(user_defined_dpi, 0, "Specify DPI for input image.", params())
    , INT_MEMBER(min_characters_to_try, 50, "Specify minimum characters to try during OSD.", params())
    , STRING_MEMBER(unrecognised_char, "|", "Output char for unidentified blobs.", params())
    , INT_MEMBER(suspect_level, 99, "Suspect marker level (0..4)", params())
    , INT_MEMBER(suspect_short_words, 2, "Don't suspect dict wds longer than this.", params())
    , BOOL_MEMBER(suspect_constrain_1Il, false, "UNLV keep 1Il chars rejected.", params())
    , DOUBLE_MEMBER(suspect_rating_per_ch, 999.9, "Don't touch bad rating limit.", params())
    , DOUBLE_MEMBER(suspect_accept_rating, -999.9, "Accept good rating limit.", params())
    , BOOL_MEMBER(tessedit_minimal_rejection, false, "Only reject tess failures.", params())
    , BOOL_MEMBER(tessedit_zero_rejection, false, "Don't reject ANYTHING.", params())
    , BOOL_MEMBER(tessedit_word_for_word, false, "Make output have exactly one word per WERD.", params())
    , BOOL_MEMBER(tessedit_zero_kelvin_rejection, false, "Don't reject ANYTHING AT ALL.", params())
    , INT_MEMBER(tessedit_reject_mode, 0, "Rejection algorithm.", params())
    , BOOL_MEMBER(tessedit_rejection_debug, false, "Debug adaption/rejection.", params())
    , BOOL_MEMBER(tessedit_flip_0O, true, "Contextual 0O O0 flips.", params())
    , DOUBLE_MEMBER(tessedit_lower_flip_hyphen, 1.5, "Aspect ratio dot/hyphen test.", params())
    , DOUBLE_MEMBER(tessedit_upper_flip_hyphen, 1.8, "Aspect ratio dot/hyphen test.", params())
    , BOOL_MEMBER(rej_trust_doc_dawg, false, "Use DOC dawg in 11l conf. detector.", params())
    , BOOL_MEMBER(rej_1Il_use_dict_word, false, "Use dictword test.", params())
    , BOOL_MEMBER(rej_1Il_trust_permuter_type, true, "Don't double check.", params())
    , BOOL_MEMBER(rej_use_tess_accepted, true, "Individual rejection control.", params())
    , BOOL_MEMBER(rej_use_tess_blanks, true, "Individual rejection control.", params())
    , BOOL_MEMBER(rej_use_good_perm, true, "Individual rejection control.", params())
    , BOOL_MEMBER(rej_use_sensible_wd, false, "Extend permuter check.", params())
    , BOOL_MEMBER(rej_alphas_in_number_perm, false, "Extend permuter check.", params())
    , DOUBLE_MEMBER(rej_whole_of_mostly_reject_word_fract, 0.85, "reject whole of word if > this fract.", params())
    , INT_MEMBER(tessedit_image_border, 2, "Rej blbs near image edge limit.", params())
    , STRING_MEMBER(ok_repeated_ch_non_alphanum_wds, "-?*\075", "Allow NN to unrej.", params())
    , STRING_MEMBER(conflict_set_I_l_1, "Il1[]", "Il1 conflict set.", params())
    , INT_MEMBER(min_sane_x_ht_pixels, 8, "Reject any x-ht lt or eq than this.", params())
    , BOOL_MEMBER(tessedit_create_boxfile, false, "Output text with boxes.", params())
    , INT_MEMBER(tessedit_page_number, -1, "-1 -> All pages, else specific page to process.", params())
    , BOOL_MEMBER(tessedit_write_images, false, "Capture the image from the internal processing engine at various stages of progress (the generated image filenames will reflect this).", params())
    , BOOL_MEMBER(interactive_display_mode, false, "Run interactively? Turn OFF (false) to NOT use the external ScrollView process. Instead, where available, image data is appended to debug_pixa.", params()),
      STRING_MEMBER(file_type, ".tif", "Filename extension.", params())
    , BOOL_MEMBER(tessedit_override_permuter, true, "According to dict_word.", params())
    , STRING_MEMBER(tessedit_load_sublangs, "", "List of languages to load with this one.", params())
    , BOOL_MEMBER(tessedit_use_primary_params_model, false,
                  "In multilingual mode use params model of the "
                  "primary language.",
                  params())
    , DOUBLE_MEMBER(min_orientation_margin, 7.0, "Min acceptable orientation margin.",
                    params())
    // , BOOL_MEMBER(textord_tabfind_show_vlines, false, "Debug line finding.", params())      --> debug_line_finding
    , BOOL_MEMBER(textord_use_cjk_fp_model, false, "Use CJK fixed pitch model.", params())
    , BOOL_MEMBER(tsv_lang_info, false, "Include language info in the  .tsv output file", this->params())
    , BOOL_MEMBER(poly_allow_detailed_fx, false,
                  "Allow feature extractors to see the original outline.", params())
    , BOOL_INIT_MEMBER(tessedit_init_config_only, false,
                       "Only initialize with the config file. Useful if the "
                       "instance is not going to be used for OCR but say only "
                       "for layout analysis.",
                       params())
#if !DISABLED_LEGACY_ENGINE
    , BOOL_MEMBER(textord_equation_detect, false, "Turn on equation detector.", params())
#endif // !DISABLED_LEGACY_ENGINE
    , BOOL_MEMBER(textord_tabfind_vertical_text, true, "Enable vertical detection.", params())
    , BOOL_MEMBER(textord_tabfind_force_vertical_text, false, "Force using vertical text page mode.",
                  params())
    , DOUBLE_MEMBER(textord_tabfind_vertical_text_ratio, 0.5,
                    "Fraction of textlines deemed vertical to use vertical page "
                    "mode",
                    params())
    , DOUBLE_MEMBER(textord_tabfind_aligned_gap_fraction, 0.75,
                    "Fraction of height used as a minimum gap for aligned blobs.", params())
    , INT_MEMBER(tessedit_parallelize, 0, "Run in parallel where possible.", params())
    , BOOL_MEMBER(preserve_interword_spaces, false, "When `true`: preserve multiple inter-word spaces as-is, or when `false`: compress multiple inter-word spaces to a single space character.",
                  params())
    , STRING_MEMBER(page_separator, "\f", "Page separator (default is form feed control character)",
                    params())
    , INT_MEMBER(lstm_choice_mode, 0,
                 "Allows to include alternative symbols choices in the hOCR output. "
                 "Valid input values are 0, 1, 2 and 3. 0 is the default value. "
                 "With 1 the alternative symbol choices per timestep are included. "
                 "With 2 alternative symbol choices are extracted from the CTC "
                 "process instead of the lattice. The choices are mapped per "
                 "character."
                 "With 3 both choice mode 1 and mode 2 outputs are included in the "
		         "hOCR output.",
                 params())
    , INT_MEMBER(lstm_choice_iterations, 5,
                 "Sets the number of cascading iterations for the Beamsearch in "
                 "lstm_choice_mode. Note that lstm_choice_mode must be set to a "
                 "value greater than 0 to produce results.",
                 params())
    , DOUBLE_MEMBER(lstm_rating_coefficient, 5,
                    "Sets the rating coefficient for the lstm choices. The smaller the "
                    "coefficient, the better are the ratings for each choice and less "
                    "information is lost due to the cut off at 0. The standard value is "
                    "5.",
                    params())
    , BOOL_MEMBER(pageseg_apply_music_mask, false,
                  "Detect music staff and remove intersecting components.", params())
    , DOUBLE_MEMBER(max_page_gradient_recognize, 100,
                  "Exit early (without running recognition) if page gradient is above this amount.", params())
    , BOOL_MEMBER(debug_write_unlv, false, "Saves page segmentation intermediate and output box set as UZN file for diagnostics.", params())
    , INT_MEMBER(debug_baseline_fit, 0, "Baseline fit debug level 0..3.", params())
    , INT_MEMBER(debug_baseline_y_coord, -2000, "Output baseline fit debug diagnostics for given Y coord, even when debug_baseline_fit is NOT set. Specify a negative value to disable this debug feature.", params())
    , BOOL_MEMBER(debug_line_finding, false, "Debug the line finding process.", params())
    , BOOL_MEMBER(debug_image_normalization, false, "Debug the image normalization process (which precedes the thresholder).", params())
    , BOOL_MEMBER(debug_display_page, false, "Display preliminary OCR results in debug_pixa.", params())
    , BOOL_MEMBER(debug_display_page_blocks, false, "Display preliminary OCR results in debug_pixa: show the blocks.", params())
    , BOOL_MEMBER(debug_display_page_baselines, false, "Display preliminary OCR results in debug_pixa: show the baselines.", params()) 
    , BOOL_MEMBER(dump_segmented_word_images, false, "Display intermediate individual bbox/word images about to be fed into the OCR engine in debug_pixa.", params()) 
    , BOOL_MEMBER(dump_osdetect_process_images, false, "Display intermediate OS (Orientation & Skew) image stages in debug_pixa.", params()) 

    , pixa_debug_(this)
    , splitter_(this)
    , image_finder_(this)
    , line_finder_(this)
    , backup_config_file_(nullptr)
    , pix_binary_(nullptr)
    , pix_grey_(nullptr)
    , pix_original_(nullptr)
    , pix_thresholds_(nullptr)
    , source_resolution_(0)
    , textord_(this, this)
    , right_to_left_(false)
    , scaled_color_(nullptr)
    , scaled_factor_(-1)
    , deskew_(1.0f, 0.0f)
    , reskew_(1.0f, 0.0f)
    , gradient_(0.0f)
    , most_recently_used_(this)
    , font_table_size_(0)
#if !DISABLED_LEGACY_ENGINE
    , equ_detect_(nullptr)
#endif // !DISABLED_LEGACY_ENGINE
    , lstm_recognizer_(nullptr)
    , train_line_page_num_(0)
    , instance_has_been_initialized_(false)
{
  ScrollViewManager::AddActiveTesseractInstance(this);
}

Tesseract::~Tesseract() {
  ScrollViewManager::RemoveActiveTesseractInstance(this);

  WipeSqueakyCleanForReUse(true);
}

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
void Tesseract::WipeSqueakyCleanForReUse(bool invoked_by_destructor) {
  if (lstm_recognizer_ != nullptr) {
    lstm_recognizer_->Clean();
  }

  Clear(invoked_by_destructor);
  end_tesseract();
  {
      std::vector<Tesseract *> langs = std::move(sub_langs_);
 
      // delete the sublangs IN REVERSE ORDER!
      // 
      // Otherwise you MAY run into races and crashes re
      // `fz_set_error_callback()` calls in the related DebugPixa destructor!
      for (int i = langs.size() - 1; i >= 0; i--) {
        delete langs[i];
      }
  }

#if !DISABLED_LEGACY_ENGINE
  delete equ_detect_;
  equ_detect_ = nullptr;
#endif // !DISABLED_LEGACY_ENGINE
  delete lstm_recognizer_;
  lstm_recognizer_ = nullptr;

  instance_has_been_initialized_ = false;
}

bool Tesseract::RequiresWipeBeforeIndependentReUse() const {
  return instance_has_been_initialized_;
}

Dict &Tesseract::getDict() {
  if (0 == Classify::getDict().NumDawgs() && AnyLSTMLang()) {
    if (lstm_recognizer_ && lstm_recognizer_->GetDict()) {
      return *lstm_recognizer_->GetDict();
    }
  }
  return Classify::getDict();
}

void Tesseract::Clear(bool invoked_by_destructor) {
  for (auto &sub_lang : sub_langs_) {
    sub_lang->Clear(invoked_by_destructor);
  }

  ReportDebugInfo();

  if (invoked_by_destructor) {
    pixa_debug_.Clear(invoked_by_destructor);
    ClearPixForDebugView();
  }

  pix_original_.destroy();
  pix_binary_.destroy();
  pix_grey_.destroy();
  pix_thresholds_.destroy();
  pix_for_debug_view_.destroy();
  scaled_color_.destroy();
  deskew_ = FCOORD(1.0f, 0.0f);
  reskew_ = FCOORD(1.0f, 0.0f);
  gradient_ = 0.0f;
  splitter_.Clear();
  scaled_factor_ = -1;
}

#if !DISABLED_LEGACY_ENGINE

void Tesseract::SetEquationDetect(EquationDetect *detector) {
  equ_detect_ = detector;
  equ_detect_->SetLangTesseract(this);
}

// Clear all memory of adaption for this and all subclassifiers.
void Tesseract::ResetAdaptiveClassifier() {
  ResetAdaptiveClassifierInternal();
  for (auto &sub_lang : sub_langs_) {
    sub_lang->ResetAdaptiveClassifierInternal();
  }
}

#endif // !DISABLED_LEGACY_ENGINE

// Clear the document dictionary for this and all subclassifiers.
void Tesseract::ResetDocumentDictionary() {
  getDict().ResetDocumentDictionary();
  for (auto &sub_lang : sub_langs_) {
    sub_lang->getDict().ResetDocumentDictionary();
  }
}

void Tesseract::SetBlackAndWhitelist() {
  // Set the white and blacklists (if any)
  unicharset.set_black_and_whitelist(tessedit_char_blacklist.c_str(),
                                     tessedit_char_whitelist.c_str(),
                                     tessedit_char_unblacklist.c_str());
  if (lstm_recognizer_) {
    UNICHARSET &lstm_unicharset = lstm_recognizer_->GetUnicharset();
    lstm_unicharset.set_black_and_whitelist(tessedit_char_blacklist.c_str(),
                                            tessedit_char_whitelist.c_str(),
                                            tessedit_char_unblacklist.c_str());
  }
  // Black and white lists should apply to all loaded classifiers.
  for (auto &sub_lang : sub_langs_) {
    sub_lang->unicharset.set_black_and_whitelist(tessedit_char_blacklist.c_str(),
                                                 tessedit_char_whitelist.c_str(),
                                                 tessedit_char_unblacklist.c_str());
    if (sub_lang->lstm_recognizer_) {
      UNICHARSET &lstm_unicharset = sub_lang->lstm_recognizer_->GetUnicharset();
      lstm_unicharset.set_black_and_whitelist(tessedit_char_blacklist.c_str(),
                                              tessedit_char_whitelist.c_str(),
                                              tessedit_char_unblacklist.c_str());
    }
  }
}

// Perform steps to prepare underlying binary image/other data structures for
// page segmentation.
void Tesseract::PrepareForPageseg() {
  if (tessedit_dump_pageseg_images) {
    AddPixDebugPage(pix_binary(), "Binarized Source Image");
  }

  textord_.set_use_cjk_fp_model(textord_use_cjk_fp_model);
  // Find the max splitter strategy over all langs.
  auto max_pageseg_strategy = static_cast<ShiroRekhaSplitter::SplitStrategy>(static_cast<int32_t>(pageseg_devanagari_split_strategy));
  for (auto &sub_lang : sub_langs_) {
    auto pageseg_strategy = static_cast<ShiroRekhaSplitter::SplitStrategy>(static_cast<int32_t>(sub_lang->pageseg_devanagari_split_strategy));
    if (pageseg_strategy > max_pageseg_strategy) {
      max_pageseg_strategy = pageseg_strategy;
    }
    sub_lang->set_pix_binary(pix_binary().clone());
  }
  // Perform shiro-rekha (top-line) splitting and replace the current image by
  // the newly split image.
  splitter_.set_orig_pix(pix_binary());
  splitter_.set_pageseg_split_strategy(max_pageseg_strategy);
  if (splitter_.Split(true)) {
    Image image = splitter_.splitted_image();
    ASSERT_HOST_MSG(!!image, "splitted_image() must never fail.\n");
    set_pix_binary(image.clone());

    if (tessedit_dump_pageseg_images) {
      ASSERT0(max_pageseg_strategy >= 0);
      ASSERT0(max_pageseg_strategy < 3);
      static const char *strategies[] = {"NO_SPLIT", "MINIMAL_SPLIT", "MAXIMAL_SPLIT"};
      AddPixDebugPage(pix_binary(), fmt::format("Source Image as replaced by Splitter mode {}", strategies[max_pageseg_strategy]));
    }
  }
}

// Perform steps to prepare underlying binary image/other data structures for
// OCR. The current segmentation is required by this method.
// Note that this method resets pix_binary_ to the original binarized image,
// which may be different from the image actually used for OCR depending on the
// value of devanagari_ocr_split_strategy.
void Tesseract::PrepareForTessOCR(BLOCK_LIST *block_list, OSResults *osr) {
  // Find the max splitter strategy over all langs.
  auto max_ocr_strategy = static_cast<ShiroRekhaSplitter::SplitStrategy>(static_cast<int32_t>(ocr_devanagari_split_strategy));
  for (auto &sub_lang : sub_langs_) {
    auto ocr_strategy = static_cast<ShiroRekhaSplitter::SplitStrategy>(static_cast<int32_t>(sub_lang->ocr_devanagari_split_strategy));
    if (ocr_strategy > max_ocr_strategy) {
      max_ocr_strategy = ocr_strategy;
    }
  }
  // Utilize the segmentation information available.
  splitter_.set_segmentation_block_list(block_list);
  splitter_.set_ocr_split_strategy(max_ocr_strategy);
  // Run the splitter for OCR
  bool split_for_ocr = splitter_.Split(false);
  // Restore pix_binary to the binarized original pix for future reference.
  Image orig_source_image = splitter_.orig_pix();
  ASSERT_HOST_MSG(orig_source_image, "orig_pix() should never fail to deliver a valid Image pix.\n");
  set_pix_binary(orig_source_image.clone());
  // If the pageseg and ocr strategies are different, refresh the block list
  // (from the last SegmentImage call) with blobs from the real image to be used
  // for OCR.
  if (splitter_.HasDifferentSplitStrategies()) {
    BLOCK block("", true, 0, 0, 0, 0, pixGetWidth(pix_binary_), pixGetHeight(pix_binary_));
    Image pix_for_ocr = split_for_ocr ? splitter_.splitted_image() : splitter_.orig_pix();
    extract_edges(pix_for_ocr, &block);
    splitter_.RefreshSegmentationWithNewBlobs(block.blob_list());
  }
  // The splitter isn't needed any more after this, so save memory by clearing.
  splitter_.Clear();
}

// Return a memory capacity cost estimate for the given image / current original image.
//
// uses the current original image for the estimate, i.e. tells you the cost estimate of this run:
ImageCostEstimate Tesseract::EstimateImageMemoryCost(const Pix* pix) const {
  // default: use pix_original() data 
  if (pix == nullptr) {
    pix = pix_original();
  }

  return TessBaseAPI::EstimateImageMemoryCost(pix, allowed_image_memory_capacity);
}

// Helper, which may be invoked after SetInputImage() or equivalent has been called:
// reports the cost estimate for the current instance/image via `tprintDebug()` and returns
// `true` when the cost is expected to be too high.
bool Tesseract::CheckAndReportIfImageTooLarge(const Pix* pix) const {
  // default: use pix_original() data 
  if (pix == nullptr) {
    pix = pix_original();
  }

  auto w = pixGetWidth(pix);
  auto h = pixGetHeight(pix);
  return CheckAndReportIfImageTooLarge(w, h);
}

bool Tesseract::CheckAndReportIfImageTooLarge(int width, int height) const {
  auto cost = TessBaseAPI::EstimateImageMemoryCost(width, height, allowed_image_memory_capacity);

  if (debug_misc) {
    tprintDebug("Image size & memory cost estimate: {} x {} px, estimated cost {} vs. {} allowed capacity.\n",
      width, height, cost.to_string(), ImageCostEstimate::capacity_to_string(allowed_image_memory_capacity));
  }

  if (width >= TDIMENSION_MAX) {
    tprintError("Image is too large: ({} x {} px, {}) (maximum accepted width: {} px)\n", width, height, cost.to_string(), TDIMENSION_MAX - 1);
    return true;
  }
  if (height >= TDIMENSION_MAX) {
    tprintError("Image is too large: ({} x {} px, {}) (maximum accepted height: {} px)\n", width, height, cost.to_string(), TDIMENSION_MAX - 1);
    return true;
  }
  if (cost.is_too_large()) {
    tprintError("Image is too large: ({} x {} px, {}) (maximum allowed memory cost: {} vs. estimated cost: {})\n", width, height, cost.to_string(), ImageCostEstimate::capacity_to_string(allowed_image_memory_capacity), cost.to_string());
    return true;
  }
  return false;
}

void Tesseract::AddPixCompedOverOrigDebugPage(const Image& pix, const TBOX& bbox, const char* title) {
  // extract part from the source image pix and fade the surroundings,
  // so a human can easily spot which bbox is the current focus but also
  // quickly spot where the extracted part originated within the large source image.
  int iw = pixGetWidth(pix);
  int ih = pixGetHeight(pix);
  int pw = bbox.width();
  int ph = bbox.height();
  ASSERT0(pw > 0);
  ASSERT0(ph > 0);
  ASSERT0(iw >= pw);
  ASSERT0(ih >= ph);
  int border = std::max(std::max(iw / 50 /* 2% of the original size -> 1% + 1% padding */, ih / 50 /* 2% of the original size -> 1% + 1% padding */), std::max(50, std::max(pw / 2, ph / 2)));
  BOX *b = boxCreateValid(bbox.left(), bbox.bottom(), pw, ph);
  l_int32 x, y, w, h;
  boxGetGeometry(b, &x, &y, &w, &h);
  l_int32 x1 = x - border;
  l_int32 y1 = y - border;
  l_int32 w1 = w + 2 * border;
  l_int32  h1 = h + 2 * border;
  if (x1 < 0)
    x1 = 0;
  if (y1 < 0)
    y1 = 0;
  if (w1 > iw)
    w1 = iw;
  if (h1 > ih)
    h1 = ih;
  BOX *b1 = boxCreateValid(x1, y1, w1, h1);
  BOX *b2 = nullptr;
  PIX *ppix = pixClipRectangle(pix, b1, &b2);
  PIX *ppix32 = pixConvertTo32(ppix);
  // generate boxes surrounding the focus bbox, covering the surrounding area in ppix32:
  BOXA *blist = boxaCreate(1);
  // box(x1, y1, x - x1, h1) - (x1, y1) ==>
  int w_edge = x - x1;
  BOX *edgebox = boxCreateValid(0, 0, w_edge, h1);
  l_int32 valid;
  boxIsValid(edgebox, &valid);
  if (valid)
    boxaAddBox(blist, edgebox, L_INSERT);
  // box(x1 + w_edge, y1, w, y - y1) - (x1, y1) ==>
  int h_edge = y - y1;
  edgebox = boxCreate(w_edge, 0, w, h_edge);
  boxIsValid(edgebox, &valid);
  if (valid)
    boxaAddBox(blist, edgebox, L_INSERT);
  // box(x1 + w_edge, y + h, w, h1 - (y + h)) - (x1, y1) ==>
  edgebox = boxCreate(w_edge, h_edge + h, w, h1 - (h_edge + h));
  boxIsValid(edgebox, &valid);
  if (valid)
    boxaAddBox(blist, edgebox, L_INSERT);
  // box(x1 + w_edge + w, y1, w1 - (x + w), h1) - (x1, y1) ==>
  edgebox = boxCreate(w_edge + w, 0, w1 - (w_edge + w), h1);
  boxIsValid(edgebox, &valid);
  if (valid)
    boxaAddBox(blist, edgebox, L_INSERT);
  pixRenderHashBoxaBlend(ppix32, blist, 2, 1, L_POS_SLOPE_LINE, true, 255, 0, 0, 0.5f);
  boxaDestroy(&blist);
  boxDestroy(&b2);
  boxDestroy(&b1);
  boxDestroy(&b);
  pixDestroy(&ppix);
  ASSERT0(bbox.area() > 0);
  pixa_debug_.AddPixWithBBox(ppix32, bbox, title);

  pixDestroy(&ppix32);
}

void Tesseract::AddPixCompedOverOrigDebugPage(const Image &pix, const char *title) {
  pixa_debug_.AddPixWithBBox(pix, title);
}

// Destroy any existing pix and return a pointer to the pointer.
void Tesseract::set_pix_binary(Image pix) {
  pix_binary_.destroy();
  pix_binary_ = pix;
  // Clone to sublangs as well.
  for (auto &lang_ref : sub_langs_) {
    lang_ref->set_pix_binary(pix ? pix.clone() : nullptr);
  }
}

void Tesseract::set_pix_grey(Image grey_pix) {
  pix_grey_.destroy();
  pix_grey_ = grey_pix;
  // Clone to sublangs as well.
  for (auto &lang_ref : sub_langs_) {
    lang_ref->set_pix_grey(grey_pix ? grey_pix.clone() : nullptr);
  }
}

// Takes ownership of the given original_pix.
void Tesseract::set_pix_original(Image original_pix) {
  pix_original_.destroy();
  pix_original_ = original_pix;
  // Clone to sublangs as well.
  for (auto &lang_ref : sub_langs_) {
    lang_ref->set_pix_original(original_pix ? original_pix.clone() : nullptr);
  }
}

  Image Tesseract::pix_binary() const {
    return pix_binary_;
  }
  Image Tesseract::pix_grey() const {
    return pix_grey_;
  }
  Image Tesseract::pix_original() const {
    return pix_original_;
  }

Image Tesseract::GetPixForDebugView() {
  if (pix_for_debug_view_ != nullptr) {
    return pix_for_debug_view_;
  }

  Image pix;
  if (pix_grey_ != nullptr) {
    pix = pix_grey_;
  } else {
    pix = pix_binary_;
  }
  pix_for_debug_view_ = pixConvertTo32(pix);
  return pix_for_debug_view_;
}

void Tesseract::ClearPixForDebugView() {
  if (pix_for_debug_view_ != nullptr) {
    pix_for_debug_view_.destroy();
    pix_for_debug_view_ = nullptr;
  }
}

// Returns a pointer to a Pix representing the best available resolution image
// of the page, with best available bit depth as second priority. Result can
// be of any bit depth, but never color-mapped, as that has always been
// removed. Note that in grey and color, 0 is black and 255 is
// white. If the input was binary, then black is 1 and white is 0.
// To tell the difference pixGetDepth() will return 32, 8 or 1.
// In any case, the return value is a borrowed Pix, and should not be
// deleted or pixDestroyed.
Image Tesseract::BestPix() const {
  if (pix_original_ != nullptr && pixGetWidth(pix_original_) == ImageWidth()) {
    return pix_original_;
  } else if (pix_grey_ != nullptr) {
    return pix_grey_;
  } else {
    return pix_binary_;
  }
}

void Tesseract::set_pix_thresholds(Image thresholds) {
  pix_thresholds_.destroy();
  pix_thresholds_ = thresholds;
}

void Tesseract::set_source_resolution(int ppi) {
  source_resolution_ = ppi;
}

int Tesseract::ImageWidth() const {
  return pixGetWidth(pix_binary_);
}

int Tesseract::ImageHeight() const {
  return pixGetHeight(pix_binary_);
}

void Tesseract::SetScaledColor(int factor, Image color) {
  scaled_factor_ = factor;
  scaled_color_ = color;
}

Tesseract * Tesseract::get_sub_lang(int index) const {
  return sub_langs_[index];
}

  Image Tesseract::pix_thresholds() {
	  return pix_thresholds_;
  }

  int Tesseract::source_resolution() const {
    return source_resolution_;
  }

  Image Tesseract::scaled_color() const {
    return scaled_color_;
  }

  int Tesseract::scaled_factor() const {
    return scaled_factor_;
  }

  const Textord &Tesseract::textord() const {
    return textord_;
  }

  Textord *Tesseract::mutable_textord() {
    return &textord_;
  }

  bool Tesseract::right_to_left() const {
    return right_to_left_;
  }

  int Tesseract::num_sub_langs() const {
    return sub_langs_.size();
  }

// Returns true if any language uses Tesseract (as opposed to LSTM).
bool Tesseract::AnyTessLang() const {
  if (tessedit_ocr_engine_mode != OEM_LSTM_ONLY) {
    return true;
  }
  for (auto &lang_ref : sub_langs_) {
    if (lang_ref->tessedit_ocr_engine_mode != OEM_LSTM_ONLY) {
      return true;
    }
  }
  return false;
}

// Returns true if any language uses the LSTM.
bool Tesseract::AnyLSTMLang() const {
  if (tessedit_ocr_engine_mode != OEM_TESSERACT_ONLY) {
    return true;
  }
  for (auto &lang_ref : sub_langs_) {
    if (lang_ref->tessedit_ocr_engine_mode != OEM_TESSERACT_ONLY) {
      return true;
    }
  }
  return false;
}

// debug PDF output helper methods:
void Tesseract::AddPixDebugPage(const Image &pix, const char *title) {
  if (pix == nullptr)
    return;

  pixa_debug_.AddPix(pix, title);
}

void Tesseract::AddPixDebugPage(const Image &pix, const std::string &title) {
  AddPixDebugPage(pix, title.c_str());
}

void Tesseract::AddPixCompedOverOrigDebugPage(const Image &pix, const TBOX &bbox, const std::string &title) {
  AddPixCompedOverOrigDebugPage(pix, bbox, title.c_str());
}

void Tesseract::AddPixCompedOverOrigDebugPage(const Image &pix, const std::string &title) {
  AddPixCompedOverOrigDebugPage(pix, title.c_str());
}

int Tesseract::PushNextPixDebugSection(const std::string &title) { // sibling
  return pixa_debug_.PushNextSection(title);
}

int Tesseract::PushSubordinatePixDebugSection(const std::string &title) { // child
  return pixa_debug_.PushSubordinateSection(title);
}

void Tesseract::PopPixDebugSection(int handle) { // pop active; return focus to parent
  pixa_debug_.WriteSectionParamsUsageReport();

  pixa_debug_.PopSection(handle);
}

int Tesseract::GetPixDebugSectionLevel() const {
  return pixa_debug_.GetCurrentSectionLevel();
}

void Tesseract::ResyncVariablesInternally() {
    if (lstm_recognizer_ != nullptr) {
        lstm_recognizer_->SetDataPathPrefix(language_data_path_prefix);
        lstm_recognizer_->CopyDebugParameters(this, &Classify::getDict());
        lstm_recognizer_->SetDebug(tess_debug_lstm);
    }

    if (language_model_ != nullptr) {
        int lvl = language_model_->language_model_debug_level;

#if 0
        language_model_->CopyDebugParameters(this, &Classify::getDict());

        INT_VAR_H(language_model_debug_level);
        BOOL_VAR_H(language_model_ngram_on);
        INT_VAR_H(language_model_ngram_order);
        INT_VAR_H(language_model_viterbi_list_max_num_prunable);
        INT_VAR_H(language_model_viterbi_list_max_size);
        DOUBLE_VAR_H(language_model_ngram_small_prob);
        DOUBLE_VAR_H(language_model_ngram_nonmatch_score);
        BOOL_VAR_H(language_model_ngram_use_only_first_uft8_step);
        DOUBLE_VAR_H(language_model_ngram_scale_factor);
        DOUBLE_VAR_H(language_model_ngram_rating_factor);
        BOOL_VAR_H(language_model_ngram_space_delimited_language);
        INT_VAR_H(language_model_min_compound_length);
        // Penalties used for adjusting path costs and final word rating.
        DOUBLE_VAR_H(language_model_penalty_non_freq_dict_word);
        DOUBLE_VAR_H(language_model_penalty_non_dict_word);
        DOUBLE_VAR_H(language_model_penalty_punc);
        DOUBLE_VAR_H(language_model_penalty_case);
        DOUBLE_VAR_H(language_model_penalty_script);
        DOUBLE_VAR_H(language_model_penalty_chartype);
        DOUBLE_VAR_H(language_model_penalty_font);
        DOUBLE_VAR_H(language_model_penalty_spacing);
        DOUBLE_VAR_H(language_model_penalty_increment);
        INT_VAR_H(wordrec_display_segmentations);
        BOOL_VAR_H(language_model_use_sigmoidal_certainty);
#endif
    }

    // init sub-languages:
     for (auto &sub_tess : sub_langs_) {
        if (sub_tess != nullptr) {
            auto lvl = (bool)sub_tess->debug_display_page;
        }
    }
}

void Tesseract::ReportDebugInfo() {
    if (!debug_output_path.empty() && pixa_debug_.HasContent()) {
        AddPixDebugPage(GetPixForDebugView(), "this page's scan/image");

        std::string file_path = mkUniqueOutputFilePath(debug_output_path.value().c_str() /* imagebasename */, 1 + tessedit_page_number, lang.c_str(), "html");
        pixa_debug_.WriteHTML(file_path.c_str());

        ClearPixForDebugView();
        pixa_debug_.Clear();
    }
}

} // namespace tesseract
