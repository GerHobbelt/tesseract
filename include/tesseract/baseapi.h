// SPDX-License-Identifier: Apache-2.0
// File:        baseapi.h
// Description: Simple API for calling tesseract.
// Author:      Ray Smith
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

#ifndef TESSERACT_API_BASEAPI_H_
#define TESSERACT_API_BASEAPI_H_

#include <tesseract/preparation.h> // compiler config, etc.

#include "export.h"
#include "pageiterator.h"
#include "publictypes.h"
#include "resultiterator.h"
#include "unichar.h"

#include <tesseract/version.h>
#include <tesseract/memcost_estimate.h>  // for ImageCostEstimate
#include <tesseract/params.h>
#include <tesseract/filepath.h>

#include <cstdio>
#include <tuple>  // for std::tuple
#include <vector> // for std::vector

struct Pix;
struct Pixa;
struct Boxa;

namespace tesseract {

using namespace parameters;

class PAGE_RES;
class ParagraphModel;
class BLOCK_LIST;
class ETEXT_DESC;
struct OSResults;
class UNICHARSET;

class Dawg;
class Dict;
class EquationDetect;
class PageIterator;
class ImageThresholder;
class LTRResultIterator;
class ResultIterator;
class MutableIterator;
class TessResultRenderer;
class Tesseract;

// Function to read a std::vector<char> from a whole file.
// Returns false on failure.
using FileReader = bool (*)(const char *filename, std::vector<char> *data);

enum PermuterType : int;
// function prototype:
//   PermuterType Dict::letter_is_okay(void *void_dawg_args, const UNICHARSET &unicharset, UNICHAR_ID unichar_id, bool word_end)
using DictFunc = PermuterType (Dict::*)(void *, const UNICHARSET &, UNICHAR_ID,
                               bool) const;

using ProbabilityInContextFunc = double (Dict::*)(const char *, const char *,
                                                  int, const char *, int);

/**
 * Defines the trinity of page images to be used by tesseract for each page to be OCR'ed.
 */
struct ImagePageFileSpec {
  std::string page_image_path;          // the page image which must be OCR'ed.
  std::string segment_mask_image_path;  // an optional segmentation assistant page image. When not RGB, anything non-white is considered content.
  std::string visible_page_image_path;  // an optional image, specifically for PDF page overlay usage.
};

/**
 * Base class for all tesseract APIs.
 * Specific classes can add ability to work on different inputs or produce
 * different outputs.
 * This class is mostly an interface layer on top of the Tesseract instance
 * class to hide the data types so that users of this class don't have to
 * include any other Tesseract headers.
 */
class TESS_API TessBaseAPI {
public:
  TessBaseAPI();
  virtual ~TessBaseAPI();
  // Copy constructor and assignment operator are currently unsupported.
  TessBaseAPI(TessBaseAPI const &) = delete;
  TessBaseAPI &operator=(TessBaseAPI const &) = delete;

  /**
   * Returns the version identifier as a static string. Do not delete.
   */
  static const char *Version();

  /**
   * Set the name of the input file. Needed for training and
   * reading a UNLV zone file, and for searchable PDF output.
   */
  void SetInputName(const char *name);

  /**
   * These functions are required for searchable PDF output.
   * We need our hands on the input file so that we can include
   * it in the PDF without transcoding. If that is not possible,
   * we need the original image. Finally, resolution metadata
   * is stored in the PDF so we need that as well.
   *
   * @{
   */

  const char *GetInputName();

  // Takes ownership of the input pix.
  void SetInputImage(Pix *pix);

  Pix *GetInputImage();

  int GetSourceYResolution();

  const char *GetDatapath();

  void SetVisibleImageFilename(const char *name);

  const char *GetVisibleImageFilename();

  void SetVisibleImage(Pix *pix);

  Pix* GetVisibleImage();

  /**
   * @}
   */

  /**
  * Return a memory capacity cost estimate for the given image dimensions and
  * some heuristics re tesseract behaviour, e.g. input images will be normalized/greyscaled,
  * then thresholded, all of which will be kept in memory while the session runs.
  *
  * Also uses the Tesseract Variable `allowed_image_memory_capacity` to indicate
  * whether the estimated cost is oversized --> `cost.is_too_large()`
  *
  * For user convenience, static functions are provided:
  * the static functions MAY be used by userland code *before* the high cost of
  * instantiating a Tesseract instance is incurred.
  */
  static ImageCostEstimate EstimateImageMemoryCost(int image_width, int image_height, float allowance = 1.0e30f /* a.k.a.dont_care, use system limit and be done */ );
  static ImageCostEstimate EstimateImageMemoryCost(const Pix* pix, float allowance = 1.0e30f /* a.k.a. dont_care, use system limit and be done */ );

  /**
  * Ditto, but this API may be invoked after SetInputImage() or equivalent has been called
  * and reports the cost estimate for the current instance/image.
  */
  ImageCostEstimate EstimateImageMemoryCost() const;

  /**
  * Helper, which may be invoked after SetInputImage() or equivalent has been called:
  * reports the cost estimate for the current instance/image via `tprintf()` and returns
  * `true` when the cost is expected to be too high.
  *
  * You can use this as a fast pre-flight check. Many major tesseract APIs perform
  * this same check as part of their startup routine.
  */
  bool CheckAndReportIfImageTooLarge(const Pix* pix = nullptr /* default: use GetInputImage() data */ ) const;

  /** Set the name of the bonus output files. Needed only for debugging. */
  void SetOutputName(const char *name);

  /** Get the name of the bonus output files, which are used for debugging. */
  const std::string &GetOutputName();

  /**
   * Set the value of an internal "parameter."
   *
   * Supply the name of the parameter and the value as a string, just as
   * you would in a config file.
   * E.g. `SetVariable("tessedit_char_blacklist", "xyz");` to ignore 'x', 'y' and 'z'.
   * Or `SetVariable("classify_bln_numeric_mode", "1");` to set numeric-only mode.
   *
   * Returns false if the name lookup failed (or the set-value attempt is rejected
   * for any reason).
   *
   * SetVariable() may be used before Init(), but settings will revert to
   * defaults on End().
   *
   * Note: critical variables are "locked" during the Init() phase and any attempt
   * to set them to a different value before the End() call will be ignored/rejected
   * while an error message about the attempt is logged.
   *
   * @{
   */
  bool SetVariable(const char *name, const char *value);
  bool SetVariable(const char *name, int value);
  bool SetVariable(const char *name, bool value);
  bool SetVariable(const char *name, double value);
  bool SetVariable(const char *name, const std::string &value);
  /**
   * @}
   */

  /**
   * Returns true if the parameter was found among Tesseract parameters.
   * Fills in `value` with the value of the parameter.
   *
   * @{
   */
  bool GetIntVariable(const char *name, int *value) const;
  bool GetBoolVariable(const char *name, bool *value) const;
  bool GetDoubleVariable(const char *name, double *value) const;
  /**
   * @}
   */

  /**
   * Returns the pointer to the string that represents the value of the
   * parameter if it was found among Tesseract parameters.
   */
  const char *GetStringVariable(const char *name) const;

  // Set up the Tesseract parameters listed in `vars_vec[]` to the specified values
  // in `vars_values[]`.
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

#if !DISABLED_LEGACY_ENGINE

  /**
   * Print Tesseract fonts table to the given file (stdout by default).
   */
  void PrintFontsTable(FILE *fp = nullptr) const;

#endif

  /** 
   * Print Tesseract parameters to the given file (stdout by default).
   * Cannot be used as Tesseract configuration file due to descriptions 
   * (use DumpVariables instead to create config files).
   */
  void PrintVariables(FILE *fp = nullptr) const;

  /*
  * Report parameters' usage statistics, i.e. report which params have been
  * set, modified and read/checked until now during this run-time's lifetime.
  *
  * Use this method for run-time 'discovery' about which tesseract parameters
  * are actually *used* during your particular usage of the library, ergo
  * answering the question:
  * "Which of all those parameters are actually *relevant* to my use case today?"
  */
  void ReportParamsUsageStatistics() const;

  /** 
   * Print Tesseract parameters to the given file without descriptions. 
   * Can be used as Tesseract configuration file.
  */
  void DumpVariables(FILE *fp) const;

  // Functions added by Tesseract.js-core to save and restore parameters
  void SaveParameters();
  void RestoreParameters();

  /**
   * Get value of named variable as a string, if it exists.
   */
  bool GetVariableAsString(const char *name, std::string *val) const;

  /**
   * Take all the internally gathered diagnostics data (including the
   * tprintError/Warn/Info/Debug/Trace messages issued thus far, plus all
   * collected image snapshots representing the intermediate state of the
   * tesseract process at that time) and produce a HTML report from it
   * for human consumption.
   */
  void FinalizeAndWriteDiagnosticsReport(); //  --> ReportDebugInfo()

  /**
   * Inspect the path_params list (which lists images and image-list files)
   * and recognize the latter (vs. older text-based image file formats) and
   * expand these into a set of image file paths.
   *
   * This process applies a little heuristic for performance reasons:
   * so as not to have to load every listed image file entirely, we merely
   * fetch the initial couple of kilobytes and check if that part is
   * a (aborted) list of file paths, rather than (text-based) image data.
   * If it is, then it is treated as an image list file and expanded in line,
   * while otherwise it is flagged as an image file.
   *
   * Returns a non-empty set of page image specs on success. The returned set is empty on error.
   */
  std::vector<ImagePageFileSpec> ExpandImagelistFilesInSet(const std::vector<std::string> &path_params);

  /**
   * Instances are now mostly thread-safe and totally independent,
   * but some global parameters remain. Basically it is safe to use multiple
   * TessBaseAPIs in different threads in parallel, UNLESS:
   * you use SetVariable on some of the Params in `classify` and `textord`.
   * If you do, then the effect will be to change it for all your instances.
   *
   * Starts tesseract. Returns zero on success and -1 on failure.
   * NOTE that the only members that may be called before Init are those
   * listed above here in the class definition.
   *
   * The datapath must be the name of the tessdata directory.
   * The language is (usually) an ISO 639-3 string or, when empty or nullptr, will default to
   * "eng". It is entirely safe (and eventually will be efficient too) to call
   * Init() multiple times on the same instance to change language, or just
   * to reset the classifier.
   * 
   * The language may be a string of the form [~]<lang>[+[~]<lang>]* indicating
   * that multiple languages are to be loaded. E.g. "hin+eng" will load Hindi and
   * English. Languages may specify internally that they want to be loaded
   * with one or more other languages, so the `~` sign is available to override
   * that. E.g. if "hin" were set to load "eng" by default, then "hin+~eng" would force
   * loading only "hin". The number of loaded languages is limited only by
   * memory, with the caveat that loading additional languages will impact
   * both speed and accuracy, as there is more work to do to decide on the
   * applicable language, and there is more chance of hallucinating incorrect
   * words.
   * 
   * WARNING: On changing languages, all Tesseract parameters are reset
   * back to their default values. (Which may vary between languages.)
   * If you have a rare need to set a Variable that controls
   * initialization for a second call to Init you should explicitly
   * call End() and then use SetVariable before Init. This is only a very
   * rare use case, since there are very few uses that require any parameters
   * to be set before Init.
   *
   * @{
   */
  int Init(const char *datapath,
           ParamsVectorSet &vars);

  int Init(const char *datapath,
           ParamsVectorSet &vars,
           const std::vector<std::string> &configs);

  int Init(ParamsVectorSet &vars);

  int Init(ParamsVectorSet &vars,
           const std::vector<std::string> &configs);

  int Init(const char *datapath,
           ParamsVectorSet &vars,
           FileReader reader);

  int Init(const char *datapath,
           ParamsVectorSet &vars,
           const std::vector<std::string> &configs,
           FileReader reader);

  int Init(const char *datapath, 
           const std::vector<std::string> &vars_vec,
           const std::vector<std::string> &vars_values);

  int Init(const char *datapath,
           const std::vector<std::string> &vars_vec,
           const std::vector<std::string> &vars_values,
           const std::vector<std::string> &configs);

  int Init(const char *datapath, const char *language, OcrEngineMode oem);

  int Init(const char *datapath, const char *language, OcrEngineMode oem,
           const std::vector<std::string> &configs);

  int Init(const char *datapath, const char *language);

  int Init(const char *datapath, const char *language,
           const std::vector<std::string> &configs);

  int Init(const char *language, OcrEngineMode oem);

  int Init(const char *language, OcrEngineMode oem,
           const std::vector<std::string> &configs);

  int Init(const char *language);

  int Init(const char *language,
           const std::vector<std::string> &configs);

  /// Reads the traineddata via a FileReader from path `datapath`.
  int Init(const char *datapath, 
           const std::vector<std::string> &vars_vec,
           const std::vector<std::string> &vars_values,
           FileReader reader);

  int Init(const char *datapath,
           const std::vector<std::string> &vars_vec,
           const std::vector<std::string> &vars_values,
           const std::vector<std::string> &configs,
           FileReader reader);

  int Init(const char *datapath,
           const char *language, OcrEngineMode oem,
           FileReader reader);

  // In-memory version reads the traineddata directly from the given
  // data[data_size] array.
  int InitFromMemory(const char *data, size_t data_size, 
           const std::vector<std::string> &vars_vec,
           const std::vector<std::string> &vars_values);

  int InitFromMemory(const char *data, size_t data_size, 
          const std::vector<std::string> &vars_vec,
          const std::vector<std::string> &vars_values,
          const std::vector<std::string> &configs);

protected:
  int Init_Internal(const char *datapath,
                    ParamsVectorSet &vars,
                    const std::vector<std::string> &configs,
                    FileReader reader,
                    const char *data = nullptr, size_t data_size = 0);

/** @} */

public:
  /**
   * Returns the languages string used in the last valid initialization.
   * If the last initialization specified "deu+hin" then that will be
   * returned. If "hin" loaded "eng" automatically as well, then that will
   * not be included in this list. To find the languages actually
   * loaded use GetLoadedLanguagesAsVector.
   * The returned string should NOT be deleted.
   */
  const char *GetInitLanguagesAsString() const;

  /**
   * Returns the loaded languages in the vector of std::string.
   * Includes all languages loaded by the last Init, including those loaded
   * as dependencies of other loaded languages.
   */
  void GetLoadedLanguagesAsVector(std::vector<std::string> *langs) const;

  /**
   * Returns the available languages in the sorted vector of std::string.
   */
  void GetAvailableLanguagesAsVector(std::vector<std::string> *langs) const;

  /**
   * Init only for page layout analysis. Use only for calls to SetImage and
   * AnalysePage. Calls that attempt recognition will generate an error.
   */
  void InitForAnalysePage();

  /**
   * Read a "config" file containing a set of param, value pairs.
   * Searches the standard places: tessdata/configs, tessdata/tessconfigs
   * and also accepts a relative or absolute path name.
   * Note: only non-init params will be set (init params are set by Init()).
   */
  void ReadConfigFile(const char *filename);
  
  /**
   * Set the current page segmentation mode. Defaults to PSM_SINGLE_BLOCK.
   * The mode is stored as an IntParam so it can also be modified by
   * ReadConfigFile or SetVariable("tessedit_pageseg_mode").
   */
  void SetPageSegMode(PageSegMode mode);

  /** Return the current page segmentation mode. */
  PageSegMode GetPageSegMode() const;

  /**
   * Recognize a rectangle from an image and return the result as a string.
   * May be called many times for a single Init.
   * Currently has no error checking.
   * Greyscale of 8 and color of 24 or 32 bits per pixel may be given (in RGB/RGBA byte layout).
   * Palette color images will not work properly and must be converted to
   * 24 bit.
   * Binary images of 1 bit per pixel may also be given but they must be
   * byte packed with the MSB of the first byte being the first pixel, and a
   * 1 represents WHITE. For binary images set bytes_per_pixel=0.
   * The recognized text is returned as a char* which is coded
   * as UTF8 and must be freed with the delete [] operator.
   *
   * Note that TesseractRect is the simplified convenience interface.
   * For advanced uses, use SetImage, (optionally) SetRectangle, Recognize,
   * and one or more of the Get*Text functions below.
   */
  char *TesseractRect(const unsigned char *imagedata, int bytes_per_pixel,
                      int bytes_per_line, int left, int top, int width,
                      int height);

  /**
   * Call between pages or documents, etc., to free up memory and forget
   * adaptive data.
   */
  void ClearAdaptiveClassifier();

  /**
   * @defgroup AdvancedAPI Advanced API
   * The following methods break TesseractRect into pieces, so you can
   * get hold of the thresholded image, get the text in different formats,
   * get bounding boxes, confidences etc.
   */
  /* @{ */

  /**
   * Provide an image for Tesseract to recognize. Format is as
   * TesseractRect above. Copies the image buffer and converts to Pix.
   * SetImage clears all recognition results, and sets the rectangle to the
   * full image, so it may be followed immediately by a GetUTF8Text, and it
   * will automatically perform recognition.
   */
  void SetImage(const unsigned char *imagedata, int width, int height,
                int bytes_per_pixel, int bytes_per_line, int exif = 1, 
                const float angle = 0, bool upscale = false);

  /**
   * Provide an image for Tesseract to recognize. As with SetImage above,
   * Tesseract takes its own copy of the image, so it need not persist until
   * after Recognize.
   * Pix vs raw, which to use?
   * Use Pix where possible. Tesseract uses Pix as its internal representation
   * and it is therefore more efficient to provide a Pix directly.
   */
  void SetImage(Pix *pix, int exif = 1, const float angle = 0, bool upscale = false);

  int SetImageFile(int exif = 1, const float angle = 0, bool upscale = false);

  /**
   * Preprocessing the InputImage 
   * Grayscale normalization based on nlbin (Thomas Breuel)
   * Current modes: 
   *  - 0 = No normalization
   *  - 1 = Thresholding+Recognition
   *  - 2 = Thresholding
   *  - 3 = Recognition
   */
  bool NormalizeImage(int mode);

  /**
   * Set the resolution of the source image in pixels per inch so font size
   * information can be calculated in results.  Call this after SetImage().
   */
  void SetSourceResolution(int ppi);

  /**
   * Restrict recognition to a sub-rectangle of the image. Call after SetImage.
   * Each SetRectangle clears the recognition results so multiple rectangles
   * can be recognized with the same image.
   */
  void SetRectangle(int left, int top, int width, int height);

  /**
   * Stores lstmf based on in-memory data for one line with pix and text
  */
  bool WriteLSTMFLineData(const char *name, const char *path, Pix *pix, const char *truth_text, bool vertical);

  /**
   * Get a copy of the internal thresholded image from Tesseract.
   * Caller takes ownership of the Pix and must pixDestroy it.
   * May be called any time after SetImage, or after TesseractRect.
   */
  Pix *GetThresholdedImage();

  /**
   * Saves a .png image of the type specified by `type` to the file `filename`.
   *
   * Type 0 is the original image, type 1 is the greyscale (derivative) image
   * and type 2 is the binary (thresholded) derivative image.
   */
  void WriteImage(const int type);

  /**
   * Return average gradient of lines on page.
   */
  float GetGradient();

  /**
   * Get the result of page layout analysis as a leptonica-style
   * Boxa, Pixa pair, in reading order.
   * Can be called before or after Recognize.
   */
  Boxa *GetRegions(Pixa **pixa);

  /**
   * Get the textlines as a leptonica-style
   * Boxa, Pixa pair, in reading order.
   * Can be called before or after Recognize.
   * If raw_image is true, then extract from the original image instead of the
   * thresholded image and pad by raw_padding pixels.
   * If blockids is not nullptr, the block-id of each line is also returned as
   * an array of one element per line. delete [] after use. If paraids is not
   * nullptr, the paragraph-id of each line within its block is also returned as
   * an array of one element per line. delete [] after use.
   */
  Boxa *GetTextlines(bool raw_image, int raw_padding, Pixa **pixa,
                     int **blockids, int **paraids);
  /*
   Helper method to extract from the thresholded image. (most common usage)
*/
  Boxa *GetTextlines(Pixa **pixa, int **blockids) {
    return GetTextlines(false, 0, pixa, blockids, nullptr);
  }

  /**
   * Get textlines and strips of image regions as a leptonica-style Boxa, Pixa
   * pair, in reading order. Enables downstream handling of non-rectangular
   * regions.
   * Can be called before or after Recognize.
   * If blockids is not nullptr, the block-id of each line is also returned as
   * an array of one element per line. delete [] after use.
   */
  Boxa *GetStrips(Pixa **pixa, int **blockids);

  /**
   * Get the words as a leptonica-style
   * Boxa, Pixa pair, in reading order.
   * Can be called before or after Recognize.
   */
  Boxa *GetWords(Pixa **pixa);

  /**
   * Gets the individual connected (text) components (created
   * after pages segmentation step, but before recognition)
   * as a leptonica-style Boxa, Pixa pair, in reading order.
   * Can be called before or after Recognize.
   * Note: the caller is responsible for calling boxaDestroy()
   * on the returned Boxa array and pixaDestroy() on cc array.
   */
  Boxa *GetConnectedComponents(Pixa **cc);

  /**
   * Get the given level kind of components (block, textline, word etc.) as a
   * leptonica-style Boxa, Pixa pair, in reading order.
   * Can be called before or after Recognize.
   * If blockids is not nullptr, the block-id of each component is also returned
   * as an array of one element per component. delete [] after use.
   * If blockids is not nullptr, the paragraph-id of each component with its
   * block is also returned as an array of one element per component. delete []
   * after use. If raw_image is true, then portions of the original image are
   * extracted instead of the thresholded image and padded with raw_padding. If
   * text_only is true, then only text components are returned.
   */
  Boxa *GetComponentImages(PageIteratorLevel level, bool text_only,
                           bool raw_image, int raw_padding, Pixa **pixa,
                           int **blockids, int **paraids);
  // Helper function to get binary images with no padding (most common usage).
  Boxa *GetComponentImages(const PageIteratorLevel level, const bool text_only,
                           Pixa **pixa, int **blockids) {
    return GetComponentImages(level, text_only, false, 0, pixa, blockids,
                              nullptr);
  }

  /**
   * Returns the scale factor of the thresholded image that would be returned by
   * GetThresholdedImage() and the various GetX() methods that call
   * GetComponentImages().
   * Returns 0 if no thresholder has been set.
   */
  int GetThresholdedImageScaleFactor() const;

  /**
   * Runs page layout analysis in the mode set by SetPageSegMode.
   * May optionally be called prior to Recognize to get access to just
   * the page layout results. Returns an iterator to the results.
   * If merge_similar_words is true, words are combined where suitable for use
   * with a line recognizer. Use if you want to use AnalyseLayout to find the
   * textlines, and then want to process textline fragments with an external
   * line recognizer.
   * Returns nullptr on error or an empty page.
   * The returned iterator must be deleted after use.
   * WARNING! This class points to data held within the TessBaseAPI class, and
   * therefore can only be used while the TessBaseAPI class still exists and
   * has not been subjected to a call of Init, SetImage, Recognize, Clear, End
   * DetectOS, or anything else that changes the internal PAGE_RES.
   */
  PageIterator *AnalyseLayout(bool merge_similar_words = false);

  /**
   * Recognize the image from SetAndThresholdImage, generating Tesseract
   * internal structures. Returns 0 on success.
   * Optional. The Get*Text functions below will call Recognize if needed.
   * After Recognize, the output is kept internally until the next SetImage.
   */
  int Recognize(ETEXT_DESC *monitor);

  /**
   * Methods to retrieve information after SetAndThresholdImage(),
   * Recognize() or TesseractRect(). (Recognize is called implicitly if needed.)
   *
   * @{
   */

  /**
   * Turns images into symbolic text.
   *
   * filename can point to a single image, a multi-page TIFF,
   * or a plain text list of image filenames.
   *
   * retry_config is useful for debugging. If not nullptr, you can fall
   * back to an alternate configuration if a page fails for some
   * reason.
   *
   * timeout_millisec terminates processing if any single page
   * takes too long. Set to 0 for unlimited time.
   *
   * renderer is responsible for creating the output. For example,
   * use the TessTextRenderer if you want plaintext output, or
   * the TessPDFRender to produce searchable PDF.
   *
   * If tessedit_page_number is non-negative, will only process that
   * single page. Works for multi-page tiff file, or filelist.
   *
   * Returns true if successful, false on error.
   */
  bool ProcessPages(const char *filename, const char *retry_config,
                    int timeout_millisec, TessResultRenderer *renderer);

protected:
  // Does the real work of ProcessPages.
  bool ProcessPagesInternal(const char *filename, const char *retry_config,
                            int timeout_millisec, TessResultRenderer *renderer);

public:
  /**
   * Turn a single image into symbolic text.
   *
   * The pix is the image processed. filename and page_number are
   * metadata used by side-effect processes, such as reading a box
   * file or formatting as hOCR.
   *
   * See ProcessPages for descriptions of other parameters.
   */
  bool ProcessPage(Pix *pix, const char *filename,
                   const char *retry_config, int timeout_millisec,
                   TessResultRenderer *renderer);

  /**
   * Get a reading-order iterator to the results of LayoutAnalysis and/or
   * Recognize. The returned iterator must be deleted after use.
   * WARNING! This class points to data held within the TessBaseAPI class, and
   * therefore can only be used while the TessBaseAPI class still exists and
   * has not been subjected to a call of Init, SetImage, Recognize, Clear, End
   * DetectOS, or anything else that changes the internal PAGE_RES.
   */
  ResultIterator *GetIterator();

  /**
   * Get a mutable iterator to the results of LayoutAnalysis and/or Recognize.
   * The returned iterator must be deleted after use.
   * WARNING! This class points to data held within the TessBaseAPI class, and
   * therefore can only be used while the TessBaseAPI class still exists and
   * has not been subjected to a call of Init, SetImage, Recognize, Clear, End
   * DetectOS, or anything else that changes the internal PAGE_RES.
   */
  MutableIterator *GetMutableIterator();

  /**
   * The recognized text is returned as a char* which is coded
   * as UTF8 and must be freed with the delete [] operator.
   */
  char *GetUTF8Text();

  size_t GetNumberOfTables() const;

  /// Return the i-th table bounding box coordinates
  ///
  /// Gives the (top_left.x, top_left.y, bottom_right.x, bottom_right.y)
  /// coordinates of the i-th table.
  std::tuple<int, int, int, int> GetTableBoundingBox(
      unsigned i ///< Index of the table, for upper limit \see GetNumberOfTables()
  );

  /// Get bounding boxes of the rows of a table
  /// return values are (top_left.x, top_left.y, bottom_right.x, bottom_right.y)
  std::vector<std::tuple<int, int, int, int> > GetTableRows(
      unsigned i ///< Index of the table, for upper limit \see GetNumberOfTables()
  );

  /// Get bounding boxes of the cols of a table
  /// return values are (top_left.x, top_left.y, bottom_right.x, bottom_right.y)
  std::vector<std::tuple<int, int, int, int> > GetTableCols(
    unsigned i ///< Index of the table, for upper limit \see GetNumberOfTables()
  );

  /**
   * Make a HTML-formatted string with hOCR markup from the internal
   * data structures.
   * page_number is 0-based but will appear in the output as 1-based.
   * monitor can be used to
   *  cancel the recognition
   *  receive progress callbacks
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetHOCRText(ETEXT_DESC *monitor, int page_number);

  /**
   * Make a HTML-formatted string with hOCR markup from the internal
   * data structures.
   * page_number is 0-based but will appear in the output as 1-based.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetHOCRText(int page_number);

  /**
   * Make an XML-formatted string with Alto markup from the internal
   * data structures.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetAltoText(ETEXT_DESC *monitor, int page_number);

  /**
   * Make an XML-formatted string with Alto markup from the internal
   * data structures.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetAltoText(int page_number);

   /**
   * Make an XML-formatted string with PAGE markup from the internal
   * data structures.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetPAGEText(ETEXT_DESC *monitor, int page_number);

  /**
   * Make an XML-formatted string with PAGE markup from the internal
   * data structures.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetPAGEText(int page_number);

  /**
   * Make a TSV-formatted string from the internal data structures.
   * page_number is 0-based but will appear in the output as 1-based.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetTSVText(int page_number, bool lang_info = false);

  /**
   * Make a box file for LSTM training from the internal data structures.
   * Constructs coordinates in the original image - not just the rectangle.
   * page_number is a 0-based page index that will appear in the box file.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetLSTMBoxText(int page_number);

  /**
   * The recognized text is returned as a char* which is coded in the same
   * format as a box file used in training.
   * Constructs coordinates in the original image - not just the rectangle.
   * page_number is a 0-based page index that will appear in the box file.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetBoxText(int page_number);

  /**
   * The recognized text is returned as a char* which is coded in the same
   * format as a WordStr box file used in training.
   * page_number is a 0-based page index that will appear in the box file.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetWordStrBoxText(int page_number);

  /**
   * The recognized text is returned as a char* which is coded
   * as UNLV format Latin-1 with specific reject and suspect codes.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetUNLVText();

  /**
   * Detect the orientation of the input image and apparent script (alphabet).
   * orient_deg is the detected clockwise rotation of the input image in degrees
   * (0, 90, 180, 270)
   * orient_conf is the confidence (15.0 is reasonably confident)
   * script_name is an ASCII string, the name of the script, e.g. "Latin"
   * script_conf is confidence level in the script
   * Returns true on success and writes values to each parameter as an output
   */
  bool DetectOrientationScript(int *orient_deg, float *orient_conf,
                               const char **script_name, float *script_conf);

  /**
   * The recognized text is returned as a char* which is coded
   * as UTF8 and must be freed with the delete [] operator.
   * page_number is a 0-based page index that will appear in the osd file.
   *
   * Returned string must be freed with the delete [] operator.
   */
  char *GetOsdText(int page_number);

  /** Returns the (average) confidence value between 0 and 100. */
  int MeanTextConf();

  /**
   * Returns all word confidences (between 0 and 100) in an array, terminated
   * by -1.
   *
   * The calling function must `delete []` after use.
   *
   * The number of confidences should correspond to the number of space-
   * delimited words in GetUTF8Text.
   */
  int *AllWordConfidences();

  /** @} */

#if !DISABLED_LEGACY_ENGINE
  /**
   * Applies the given word to the adaptive classifier if possible.
   * The word must be SPACE-DELIMITED UTF-8 - l i k e t h i s , so it can
   * tell the boundaries of the graphemes.
   * Assumes that SetImage/SetRectangle have been used to set the image
   * to the given word. The mode arg should be PSM_SINGLE_WORD or
   * PSM_CIRCLE_WORD, as that will be used to control layout analysis.
   * The currently set PageSegMode is preserved.
   * Returns false if adaption was not possible for some reason.
   */
  bool AdaptToWordStr(PageSegMode mode, const char *wordstr);
#endif // !DISABLED_LEGACY_ENGINE

  /**
   * Free up recognition results and any stored image data, without actually
   * freeing any recognition data that would be time-consuming to reload.
   * Afterwards, you must call SetImage or TesseractRect before doing
   * any Recognize or Get* operation.
   */
  void Clear();

  /**
   * Close down tesseract and free up (almost) all memory.
   * WipeSqueakyCleanForReUse() is near equivalent to destructing and
   * reconstructing your TessBaseAPI or calling End(), with two important
   * distinctions:
   *
   * - WipeSqueakyCleanForReUse() will *not* destroy the internal Tesseract
   *   class instance, but wipe it clean so it'll behave as if destructed and
   *   then reconstructed afresh, with one caveat:
   * - WipeSqueakyCleanForReUse() will not destroy any diagnostics/trace data
   *   cached in the running instance: the goal is to thus be able to produce
   *   diagnostics reports which span multiple rounds of OCR activity, executed 
   *   in the single lifespan of the TesseractAPI instance.
   *
   * Once WipeSqueakyCleanForReUse() has been used, none of the other API
   * functions may be used other than Init and anything declared above it in the
   * class definition: as with after calling End(), the internal state is
   * equivalent to being freshly constructed.
   */
  void WipeSqueakyCleanForReUse();

  /**
   * Close down tesseract and free up all memory. End() is equivalent to
   * destructing and reconstructing your TessBaseAPI.
   * 
   * The 'minor' difference with that delete+new approach is that we will
   * keep stored diagnostics/trace data intact, i.e. we *keep* the debug
   * trace data, so we can produce a series' report at the final end.
   * 
   * Once End() has been used, none of the other API functions may be used
   * other than Init and anything declared above it in the class definition.
   */
  void End();

  /**
   * Equivalent to calling End() but with the added feature of all
   * parameters being reset to factory defaults.
   */
  void ResetToDefaults();

  /**
   * Clear any library-level memory caches.
   * There are a variety of expensive-to-load constant data structures (mostly
   * language dictionaries) that are cached globally -- surviving the `Init()`
   * and `End()` of individual TessBaseAPI's.  This function allows the clearing
   * of these caches.
   **/
  static void ClearPersistentCache();

  /**
   * Check whether a word is valid according to Tesseract's language model
   *
   * @return 0 if the word is invalid, non-zero if valid.
   *
   * @warning temporary! This function will be removed from here and placed
   * in a separate API at some future time.
   */
  int IsValidWord(const char *word) const;

  /// Returns true if utf8_character is defined in the UniCharset.
  bool IsValidCharacter(const char *utf8_character) const;

  bool GetTextDirection(int *out_offset, float *out_slope);

  /** Sets Dict::letter_is_okay_ function to point to the given function. */
  void SetDictFunc(DictFunc f);

  /** Sets Dict::probability_in_context_ function to point to the given
   * function.
   */
  void SetProbabilityInContextFunc(ProbabilityInContextFunc f);

  /**
   * Estimates the Orientation And Script of the image.
   * @return true if the image was processed successfully.
   */
  bool DetectOS(OSResults *results);

  /**
   * Return text orientation of each block as determined by an earlier run
   * of layout analysis.
   */
  void GetBlockTextOrientations(int **block_orientation,
                                bool **vertical_writing);

  /** This method returns the string form of the specified unichar. */
  const char *GetUnichar(int unichar_id) const;

  /** Return the pointer to the i-th dawg loaded into tesseract_ object. */
  const Dawg *GetDawg(int i) const;

  /** Return the number of dawgs loaded into tesseract_ object. */
  int NumDawgs() const;

  /// Returns a reference to the internal instance of the Tesseract class;
  /// the presence of which is guaranteed, i.e. the returned pointer
  /// WILL NOT be `nullptr`.
  Tesseract& tesseract() const;

  OcrEngineMode oem() const {
    return last_oem_requested_;
  }

  void set_min_orientation_margin(double margin);

  void SetupDebugAllPreset();
  void SetupDefaultPreset();

  void ReportDebugInfo();

  /* @} */

protected:
  /** Common code for setting the image. Returns true if Init has been called.
   */
  bool InternalResetImage();

  /**
   * Run the thresholder to make the thresholded image. If pix is not nullptr,
   * the source is thresholded to pix instead of the internal IMAGE.
   */
  virtual bool Threshold(Pix **pix);

  /**
   * Find lines from the image making the BLOCK_LIST.
   * @return 0 on success.
   */
  int FindLines();

  /** Delete the PageRes and block list, readying tesseract for OCRing a new page. */
  void ClearResults();

  /**
   * Return an LTR Result Iterator -- used only for training, as we really want
   * to ignore all BiDi smarts at that point.
   *
   * delete once you're done with it.
   */
  LTRResultIterator *GetLTRIterator();

  /**
   * Return the length of the output text string, as UTF8, assuming
   * one newline per line and one per block, with a terminator,
   * and assuming a single character reject marker for each rejected character.
   * Also return the number of recognized blobs in blob_count.
   */
  int TextLength(int *blob_count) const;

  //// paragraphs.cpp ////////////////////////////////////////////////////
  void DetectParagraphs(bool after_text_recognition);

  const PAGE_RES *GetPageRes() const {
    return page_res_;
  }

protected:
  mutable Tesseract *tesseract_;     ///< The underlying data object.
#if !DISABLED_LEGACY_ENGINE
  Tesseract *osd_tesseract_;         ///< For orientation & script detection.
  EquationDetect *equ_detect_;       ///< The equation detector.
#endif
  FileReader reader_;                ///< Reads files from any filesystem.
  ImageThresholder *thresholder_;    ///< Image thresholding module.
  std::vector<ParagraphModel *> *paragraph_models_;
  BLOCK_LIST *block_list_;           ///< The page layout.
  PAGE_RES *page_res_;               ///< The page-level data.
  std::string visible_image_file_;
  Pix* pix_visible_image_;           ///< Image used in output PDF
  std::string output_file_;          ///< Name used by debug code.
  std::string datapath_;             ///< Current location of tessdata.
  std::string language_;             ///< Last initialized language.
  OcrEngineMode last_oem_requested_; ///< Last ocr language mode requested.
  bool recognition_done_;            ///< page_res_ contains recognition data.

  /**
   * @defgroup ThresholderParams Thresholder Parameters
   * Parameters saved from the Thresholder. Needed to rebuild coordinates.
   */
  /* @{ */
  int rect_left_;
  int rect_top_;
  int rect_width_;
  int rect_height_;
  int image_width_;
  int image_height_;
  /* @} */

private:
  // A list of image filenames gets special consideration
  //
  // If global parameter `tessedit_page_number` is non-negative, will only process that
  // single page. Works for multi-page tiff file, or filelist.
  bool ProcessPagesFileList(FILE *fp, std::string *buf,
                            const char *retry_config, int timeout_millisec,
                            TessResultRenderer *renderer);
  // TIFF supports multipage so gets special consideration.
  //
  // If global parameter `tessedit_page_number` is non-negative, will only process that
  // single page. Works for multi-page tiff file, or filelist.
  bool ProcessPagesMultipageTiff(const unsigned char *data, size_t size,
                                 const char *filename, const char *retry_config,
                                 int timeout_millisec,
                                 TessResultRenderer *renderer);
}; // class TessBaseAPI.

/** Escape a char string - replace &<>"' with HTML codes. */
std::string HOcrEscape(const char *text);

/**
 * Construct a filename(+path) that's unique, i.e. is guaranteed to not yet exist in the filesystem.
 */
std::string mkUniqueOutputFilePath(const char *basepath, int page_number, const char *label, const char *filename_extension);

/**
 * Helper function around leptonica's `pixWrite()` which writes the given `pic` image to file, in the `file_type` format.
 *
 * The `file_type` format is defined in leptonica's `imageio.h`. Here's an (possibly incomplete) extract:
 *
 * - IFF_BMP            = 1 (Windows BMP)
 * - IFF_JFIF_JPEG      = 2 (regular JPEG, default quality 75%)
 * - IFF_PNG            = 3 (PNG, lossless)
 * - IFF_TIFF           = 4 (TIFF)
 * - IFF_TIFF_PACKBITS  = 5 (TIFF, lossless)
 * - IFF_TIFF_RLE       = 6 (TIFF, lossless)
 * - IFF_TIFF_G3        = 7 (TIFF, lossless)
 * - IFF_TIFF_G4        = 8 (TIFF, lossless)
 * - IFF_TIFF_LZW       = 9 (TIFF, lossless)
 * - IFF_TIFF_ZIP       = 10 (TIFF, lossless)
 * - IFF_PNM            = 11 (PNM)
 * - IFF_PS             = 12 (PS: PostScript)
 * - IFF_GIF            = 13 (GIF)
 * - IFF_JP2            = 14 (JP2
 * - IFF_WEBP           = 15 (WebP)
 * - IFF_LPDF           = 16 (LDPF)
 * - IFF_TIFF_JPEG      = 17 (JPEG embedded in TIFF)
 * - IFF_DEFAULT        = 18 (The IFF_DEFAULT flag is used to write the file out in the same (input) file format that the pix was read from.  If the pix was not read from file, the input format field will be IFF_UNKNOWN and the output file format will be chosen to be compressed and lossless; namely: IFF_TIFF_G4 for depth = 1 bit and IFF_PNG for everything else.)
 * - IFF_SPIX           = 19 (SPIX: serialized PIX, a leptonica-specific file format)
 */
void WritePix(const std::string &filepath, Pix *pic, int file_type);

} // namespace tesseract

#endif // TESSERACT_API_BASEAPI_H_
