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

    // Return reference to info stream, where you can collect the diagnostic information gathered.
    //
    // NOTE: we expect HTML formatted context to be fed into the stream.
    std::ostringstream& GetInfoStream()
    {
      return info_chunks[info_chunks.size() - 1].information;
    }

    void PushNextSection(std::string title);          // sibling
    void PushSubordinateSection(std::string title);   // child
    void PopSection();                                // pop active; return focus to parent

  protected:

    void PrepNextSection(int level, std::string title);   // internal use

  public:

    // Return true when one or more images have been collected.
    bool HasPix() const;

#if 0
    // Sets the destination filename and enables images to be written to a PDF
    // on destruction.
    void WritePDF(const char* filename);

    // Sets the destination filename and enables images to be written as a set of PNGs
    // on destruction.
    void WritePNGs(const char* filename);
#endif

    // Sets the destination filename and outputs the collective of images and textual info as a HTML file (+ PNGs)
    // on destruction.
    void WriteHTML(const char* filename);

    void Clear();

  protected:

    struct DebugProcessInfoChunk {
      std::ostringstream information;    // collects the diagnostic information gathered while this section/chunk is the active one.

      int first_image_index;             // index into the DebugPixa image list
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
      //
      // Note that we expect a *single* "root" step to carry this hierarchy of diagnostic registered process steps: DebugPixa::info_root

      int level;                      // hierarchy depth. 0: root

      int first_info_chunk;           // index into info_chunks[] array
      int last_info_chunk { -1 };     // index into info_chunks[] array; necessary as we allow return-to-parent process steps hierarchy layout.
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
    std::vector<DebugProcessStep> steps;
    std::vector<DebugProcessInfoChunk> info_chunks;
    int active_step_index;
  };

} // namespace tesseract

#endif // TESSERACT_CCSTRUCT_DEBUGPIXA_H_
