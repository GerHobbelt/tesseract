
#ifndef TESSERACT_PIX_PROCESSING_SUPPLEMENT_H_
#define TESSERACT_PIX_PROCESSING_SUPPLEMENT_H_

#include <leptonica/allheaders.h>

#include <tesseract/image.h>

struct Pix;

namespace tesseract {

//----------------------------------------------------------
//
// leptonica additions
//
//----------------------------------------------------------

extern "C" {

// Return non-linear normalized grayscale
PIX *pixNLNorm2(PIX *pixs, int *pthresh);

// Return non-linear normalized grayscale
PIX *pixNLNorm1(PIX *pixs, int *pthresh, int *pfgval, int *pbgval);

// Return non-linear normalized thresholded image
PIX *pixNLBin(PIX *pixs, bool adaptive);

PIX *pixEmphasizeImageNoise(PIX *pixs);
PIX *pixEmphasizeImageNoise2(PIX *pixs);

PIX *pixMaxDynamicRange2(PIX *pixs, l_int32 type);

} // extern "C"

class TBOX; // bounding box

Image pixMixWithTintedBackground(const Image &src, const Image &background,
                                float r_factor, float g_factor, float b_factor,
                                float src_factor, float background_factor,
                                const TBOX *cliprect);

} // namespace tesseract.

#endif 
