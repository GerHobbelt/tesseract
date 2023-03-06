#ifndef TESSERACT_CCSTRUCT_DEBUGPIXA_H_
#define TESSERACT_CCSTRUCT_DEBUGPIXA_H_

#include "image.h"

#include <allheaders.h>

#include <string>
#include <vector>

namespace tesseract {

  // Class to hold a Pixa collection of debug images with captions and save them
  // to a PDF file.
  class DebugPixa {
  public:
    // TODO(rays) add another constructor with size control.
    DebugPixa();

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

  private:
    // The collection of images to put in the PDF.
    Pixa* pixa_;
    // The fonts used to draw text captions.
    L_Bmf* fonts_;
    // the captions for each image
    std::vector<std::string> captions;
  };

} // namespace tesseract

#endif // TESSERACT_CCSTRUCT_DEBUGPIXA_H_
