#ifndef TESSERACT_CCSTRUCT_DEBUGPIXA_H_
#define TESSERACT_CCSTRUCT_DEBUGPIXA_H_

#include <tesseract/image.h>

#include <leptonica/allheaders.h>

#include <string>
#include <vector>
#include <sstream>

#include <plf_nanotimer.hpp>

#if defined(HAVE_MUPDF)
#include "mupdf/fitz.h"
#endif

namespace tesseract {

  class TESS_API Tesseract;
  class TESS_API TBOX;

  enum Image4WebOutputType : int {
    IMG4W_PNG = 0,
    IMG4W_JPEG,
    IMG4W_WEBP,
    IMG4W_WEBP_LOSSLESS,
    IMG4W_TIFF,
    IMG4W_BMP,
  };

  // Class to hold a Pixa collection of debug images with captions and save them
  // to a PDF file.
  // The class MAY also store additional diagnostic information, that's interspersed
  // between the images, thus building a "360 degrees" diagnostics report view.
  class DebugPixa {
  public:
    // TODO(rays) add another constructor with size control.
    DebugPixa(Tesseract& tess);

    // If the filename_ has been set and there are any debug images, they are
    // written to the set filename_.
    ~DebugPixa();

    // Adds the given pix to the set of pages in the PDF file, with the given
    // caption added to the top.
    void AddPix(const Image& pix, const char* caption);
    void AddPixWithBBox(const Image &pix, const TBOX &bbox, const char *caption);
    void AddPixWithBBox(const Image &pix, const char *caption);

    /// Note the given command (argv[] set as vector) for later reporting
    /// in the diagnostics output as part of the HTML log heading.
    void DebugAddCommandline(const std::vector<std::string> &argv);

    // Return reference to info stream, where you can collect the diagnostic information gathered.
    //
    // NOTE: we expect HTML formatted context to be fed into the stream.
    std::ostringstream& GetInfoStream()
    {
      return info_chunks[info_chunks.size() - 1].information;
    }

    int PushNextSection(const std::string &title);        // sibling; return handle for pop()
    int PushSubordinateSection(const std::string &title); // child; return handle for pop()
    void PopSection(int handle = -1);                     // pop active; return focus to parent; pop(0) pops all the way back up to the root.
    int GetCurrentSectionLevel() const;

  protected:
    void AddPixInternal(const Image &pix, const TBOX &bbox, const char *caption);

    int PrepNextSection(int level, const std::string &title); // internal use

    void WriteImageToHTML(int &counter, const std::string &partname, FILE *html, int idx);
    int WriteInfoSectionToHTML(int &counter, int &next_image_index, const std::string &partname, FILE *html, int current_section_index);

  public:

    // Return true when one or more images have been collected and/or one or more lines of text have been collected.
    bool HasContent() const;

    // Sets the destination filename and outputs the collective of images and textual info as a HTML file (+ PNGs)
    // on destruction.
    void WriteHTML(const char* filename);

    void WriteSectionParamsUsageReport();

    void Clear(bool final_cleanup = false);

  protected:
    double gather_cummulative_elapsed_times();

    struct DebugProcessInfoChunk {
      std::ostringstream information;    // collects the diagnostic information gathered while this section/chunk is the active one.

      int appended_image_index { -1 };      // index into the DebugPixa image list
    };

    struct DebugProcessStep {
      std::string title;              // title of the process step

      // The info organization is as follows:
      //
      // When a section is started, it immediately gets one (empty) info_chunk for the diagnostic methods to file the data they receive into.
      // When the run-time pushes a sub-level step, it is added to the sublevel_items[] set and is made active, until 'popped'.
      // Once the sublevel step has been "popped" (and before the next one (sub-level sibling) is pushed), the next info_chunk is readied
      // and made the active one.
      // Thus, when this step itself is popped (inactivated), the size of the info_chunks[] array should be one longer than the sublevel_items[]
      // array, as the latter *interleaves* the former.

      int level = 0;                      // hierarchy depth. 0: root

      plf::nanotimer clock;           // performance timer, per section
      double elapsed_ns = 0.0;
      double elapsed_ns_cummulative = 0.0;

      int first_info_chunk { -1 }; // index into info_chunks[] array
      int last_info_chunk { -1 };     // index into info_chunks[] array; necessary as we allow return-to-parent process steps hierarchy layout.
    };

  private:
    Tesseract& tesseract_;   // reference to the driving tesseract instance

  private:
    // The collection of images to put in the PDF.
    Pixa* pixa_;
    // The fonts used to draw text captions.
    L_Bmf* fonts_;
    // the captions for each image
    std::vector<std::string> captions;
    std::vector<TBOX> cliprects;

    // non-image additional diagnostics information, collected and stored as hierarchy:
    std::vector<DebugProcessStep> steps;
    std::vector<DebugProcessInfoChunk> info_chunks;
    int active_step_index;
    bool content_has_been_written_to_file;

    std::vector<double> image_series_elapsed_ns;
    double total_images_production_cost;

#if defined(HAVE_MUPDF)
    fz_context *fz_ctx; 
    fz_error_print_callback *fz_cbs[3];
    void *fz_cb_userptr[3];

    static void fz_error_cb_tess_tprintf(fz_context *ctx, void *user, const char *message);
    static void fz_warn_cb_tess_tprintf(fz_context *ctx, void *user, const char *message);
    static void fz_info_cb_tess_tprintf(fz_context *ctx, void *user, const char *message);
#endif
  };

  Image MixWithLightRedTintedBackground(const Image &pix, const Image &original_image, const TBOX *cliprect);

  typedef int AutoExecOnScopeExitFunction_f(void);

  class AutoExecOnScopeExit {
  public:
	  AutoExecOnScopeExit() = delete;
	  AutoExecOnScopeExit(auto&& callback) {
		  handler_ = callback;
	  }

	  // auto-pop via end-of-scope i.e. object destructor:
	  ~AutoExecOnScopeExit() {
		  pop();
	  }

	  // forced (early) pop by explicit pop() call:
	  void pop() {
		  --depth_;
		  if (depth_ == 0 && handler_ != nullptr) {
			  (*handler_)();
		  }
	  }

  protected:
	  int depth_ {1};
	  const AutoExecOnScopeExitFunction_f *handler_ {nullptr};
  };

  class AutoPopDebugSectionLevel {
  public:
    AutoPopDebugSectionLevel(Tesseract &tess, int section_handle)
        : section_handle_(section_handle), tesseract_(tess) {}

    // auto-pop via end-of-scope i.e. object destructor:
    ~AutoPopDebugSectionLevel();

    // forced (early) pop by explicit pop() call:
    void pop();

  protected:
    Tesseract &tesseract_;
    int section_handle_;
  };

  } // namespace tesseract

#endif // TESSERACT_CCSTRUCT_DEBUGPIXA_H_
