#ifndef TESSERACT_CCSTRUCT_DEBUGPIXA_H_
#define TESSERACT_CCSTRUCT_DEBUGPIXA_H_

#include "image.h"

#include <allheaders.h>

#include <string>
#include <vector>
#include <sstream>

namespace tesseract {

  class TESS_API Tesseract;

  // Class to hold a Pixa collection of debug images with captions and save them
  // to a PDF file.
  // The class MAY also store additional diagnostic information, that's interspersed
  // between the images, thus building a "360 degrees" diagnostics report view.
  class DebugPixa {
  public:
    // TODO(rays) add another constructor with size control.
    DebugPixa(Tesseract* tesseract_ref);

    // If the filename_ has been set and there are any debug images, they are
    // written to the set filename_.
    ~DebugPixa();

    // Adds the given pix to the set of pages in the PDF file, with the given
    // caption added to the top.
    void AddPix(const Image& pix, const char* caption);
    void AddPix(Image& pix, const char* caption, bool keep_a_copy);

    // Return true when one or more images have been collected.
    bool HasPix() const;

    // Sets the destination filename and enables images to be written to a PDF
    // on destruction.
    void WritePDF(const char* filename);

    void WritePNGs(const char* filename);

    void WriteHTML(const char* filename);

    void Clear();

  protected:

    struct DebugProcessInfoChunk {
      std::ostringstream information;    // collects the diagnostic information gathered while this section/chunk is the active one.

      int first_image_index;             // index into the DebugPixa image list
      int image_count;                   // number of images in DebugPixa that were filed while this section/chunk was active.
    };

    struct DebugProcessStep {
      std::string title;              // title of the process step

      // The info organization is as follows:
      //
      // When a section is started, it immediately gets one (empty) info_chunk for the diagnostic methods to file the data they receive into.
      // When the run-time pushes a sub-level step, it is added to the sublevel_items[] set and is made active, until 'popped'.
      // Once the sublevel step has been "popped" (and before the next one (sub-level sibling) is pushed), thee next info_chunk is readied
      // and made the active one.
      // Thus, when this step itself is popped (inactivated), the size of the info_chunks[] array should be one longer than the sublevel_items[]
      // array, as the latter *interleaves* the former.
      //
      // Note that we expect a *single* "root" step to carry this hierarchy of diagnostic registered process steps: DebugPixa::info_root

      std::vector<DebugProcessInfoChunk> info_chunks;   // carries the information that was filed while this section/chunk was active.

      std::vector<DebugProcessStep> sublevel_items;     // the process sublevel steps that were filed when this section/chunk was active/parent.
    };

  private:
    Tesseract* tesseract_;   // reference to the driving tesseract instance

  private:
    // The collection of images to put in the PDF.
    Pixa* pixa_;
    // The fonts used to draw text captions.
    L_Bmf* fonts_;
    // the captions for each image
    std::vector<std::string> captions;

    // non-image additional diagnostics information, collected and stored as hierarchy:
    DebugProcessStep info_root;
  };

} // namespace tesseract

#endif // TESSERACT_CCSTRUCT_DEBUGPIXA_H_
