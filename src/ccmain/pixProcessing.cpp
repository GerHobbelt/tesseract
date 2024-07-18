
// Include automatically generated configuration file
#include <tesseract/preparation.h> // compiler config, etc.

#include <leptonica/allheaders.h>

#include <tesseract/tprintf.h> // for tprintf

#include <cstdint>
#include <cstring>

namespace tesseract {

// Brewer-derived scale for the noise emphasis (RGB)
static const l_uint32 noiseEmphasisColorMap[] = {
    0xfcc5c0,
    0xfa9fb5,
    0xf768a1,
    0xdd3497,
    0xae017e,
    0x7a0177,
    0x49006a,
};

PIX *pixEmphasizeImageNoise(PIX *pixs) {
  return pixClone(pixs);
}



/*----------------------------------------------------------------------*
 *                  Non-linear contrast normalization                   *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixNLNorm2()
 *
 * \param[in]    pixs          8 or 32 bpp
 * \param[out]   ptresh        l_int32 global threshold value
 * \return       pixd          8 bpp grayscale, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This composite operation is good for adaptively removing
 *          dark background. Adaption of Thomas Breuel's nlbin version
 *          from ocropus.
 *      (2) A good thresholder together with NLNorm is WAN
 * </pre>
 */
Pix *pixNLNorm2(Pix *pixs, int *pthresh) {
  l_int32 d, thresh, w1, h1, w2, h2, fgval, bgval;
  // l_uint32 black_val, white_val;
  l_float32 factor, threshpos, avefg, avebg, numfg, numbg;
  PIX *pixg, *pixd, *pixd2;
  BOX *pixbox;
  NUMA *na;

  PROCNAME("pixNLNorm");

  if (!pixs || (d = pixGetDepth(pixs)) < 8) {
    return (PIX *)ERROR_PTR("pixs undefined or d < 8 bpp", procName, NULL);
  }
  if (d == 32) {
    // ITU-R 601-2 luma
    pixg = pixConvertRGBToGray(pixs, 0.299, 0.587, 0.114);
    // Legacy converting
    // pixg = pixConvertRGBToGray(pixs, 0.3, 0.4, 0.3);
  } else {
    pixg = pixConvertTo8(pixs, 0);
  }

  /// Normalize contrast
  //  pixGetBlackOrWhiteVal(pixg, L_GET_BLACK_VAL, &black_val);
  //  if (black_val>0) pixAddConstantGray(pixg, -1 * black_val);
  //  pixGetBlackOrWhiteVal(pixg, L_GET_WHITE_VAL, &white_val);
  //  if (white_val<255) pixMultConstantGray(pixg, (255. / white_val));
  pixd = pixMaxDynamicRange(pixg, L_LINEAR_SCALE);
  pixDestroy(&pixg);
  pixg = pixCopy(nullptr, pixd);
  pixDestroy(&pixd);

  /// Calculate flat version
  pixGetDimensions(pixg, &w1, &h1, NULL);
  pixd = pixScaleGeneral(pixg, 0.5, 0.5, 0.0, 0);
  pixd2 = pixRankFilter(pixd, 20, 2, 0.8);
  pixDestroy(&pixd);
  pixd = pixRankFilter(pixd2, 2, 20, 0.8);
  pixDestroy(&pixd2);
  pixGetDimensions(pixd, &w2, &h2, NULL);
  pixd2 = pixScaleGrayLI(pixd, (l_float32)w1 / (l_float32)w2,
                         (l_float32)h1 / (l_float32)h2);
  pixDestroy(&pixd);
  pixInvert(pixd2, pixd2);
  pixAddGray(pixg, pixg, pixd2);
  pixDestroy(&pixd2);

  /// Local contrast enhancement
  //  Ignore a border of 10% and get a mean threshold,
  //  background and foreground value
  pixbox = boxCreate(w1 * 0.1, h1 * 0.1, w1 * 0.9, h1 * 0.9);
  na = pixGetGrayHistogramInRect(pixg, pixbox, 1);
  numaSplitDistribution(na, 0.1, &thresh, &avefg, &avebg, &numfg, &numbg, NULL);
  boxDestroy(&pixbox);
  numaDestroy(&na);

  if (numfg > numbg) {
    // white = fg --> swap the values produced by numaSplitDistribution()
    l_float32 tmp = avefg;
    avefg = avebg;
    avebg = tmp;

    tmp = numfg;
    numfg = numbg;
    numbg = tmp;
  }

  /// Subtract by a foreground value and multiply by factor to
  //  set a background value to 255
  fgval = (l_int32)(avefg + 0.5);
  bgval = (l_int32)(avebg + 0.5);
  threshpos = (l_float32)(thresh - fgval) / (bgval - fgval);
  // Todo: fgval or fgval + slightly offset
  fgval = fgval; // + (l_int32) ((thresh - fgval)*.25);
  bgval = bgval +
          (l_int32)std::min((l_int32)((bgval - thresh) * .5), (255 - bgval));
  factor = 255. / (bgval - fgval);
  if (pthresh) {
    *pthresh = (l_int32)threshpos * factor - threshpos * .1;
  }
  pixAddConstantGray(pixg, -1 * fgval);
  pixMultConstantGray(pixg, factor);

  return pixg;
}

/*----------------------------------------------------------------------*
 *                  Non-linear contrast normalization                   *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixNLNorm1()
 *
 * \param[in]    pixs          8 or 32 bpp
 * \param[out]   ptresh        l_int32 global threshold value
 * \param[out]   pfgval        l_int32 global foreground value
 * \param[out]   pbgval        l_int32 global background value
 * \return  pixd    8 bpp grayscale, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This composite operation is good for adaptively removing
 *          dark background. Adaption of Thomas Breuel's nlbin version from ocropus.
 * </pre>
 */
PIX *pixNLNorm1(PIX *pixs, int *pthresh, int *pfgval, int *pbgval) {
  l_int32 d, fgval, bgval, thresh, w1, h1, w2, h2;
  l_float32 factor;
  PIX *pixg, *pixd;

  PROCNAME("pixNLNorm");

  if (!pixs || (d = pixGetDepth(pixs)) < 8)
    return (PIX *)ERROR_PTR("pixs undefined or d < 8 bpp", procName, NULL);
  if (d == 32)
    pixg = pixConvertRGBToGray(pixs, 0.3, 0.4, 0.3);
  else
    pixg = pixConvertTo8(pixs, 0);

  /* Normalize contrast */
  pixd = pixMaxDynamicRange(pixg, L_LINEAR_SCALE);

  /* Calculate flat version */
  pixGetDimensions(pixd, &w1, &h1, NULL);
  pixd = pixScaleSmooth(pixd, 0.5, 0.5);
  pixd = pixRankFilter(pixd, 2, 20, 0.8);
  pixd = pixRankFilter(pixd, 20, 2, 0.8);
  pixGetDimensions(pixd, &w2, &h2, NULL);
  pixd = pixScaleGrayLI(pixd, (l_float32)w1 / (l_float32)w2, (l_float32)h1 / (l_float32)h2);
  pixInvert(pixd, pixd);
  pixg = pixAddGray(NULL, pixg, pixd);
  pixDestroy(&pixd);

  /* Local contrast enhancement */
  pixSplitDistributionFgBg(pixg, 0.1, 2, &thresh, &fgval, &bgval, NULL);
  if (pthresh)
    *pthresh = thresh;
  if (pfgval)
    *pfgval = fgval;
  if (pbgval)
    *pbgval = bgval;
  fgval = fgval + ((thresh - fgval) * 0.25);
  if (fgval < 0)
    fgval = 0;
  pixAddConstantGray(pixg, -1 * fgval);
  factor = 255.0 / l_float32(bgval - fgval);
  pixMultConstantGray(pixg, factor);
  pixd = pixGammaTRC(NULL, pixg, 1.0, 0, bgval - ((bgval - thresh) * 0.5));
  pixDestroy(&pixg);

  return pixd;
}

/*----------------------------------------------------------------------*
 *                  Non-linear contrast normalization                   *
 *                            and thresholding                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixNLBin()
 *
 * \param[in]    pixs          8 or 32 bpp
 * \oaram[in]    adaptive      bool if set to true it uses adaptive thresholding
 *                             recommended for images, which contain dark and light text
 *                             at the same time (it doubles the processing time)
 * \return  pixd    1 bpp thresholded image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This composite operation is good for adaptively removing
 *          dark background. Adaption of Thomas Breuel's nlbin version from ocropus.
 *      (2) The threshold for the binarization uses an
 *          Sauvola adaptive thresholding.
 * </pre>
 */
PIX *pixNLBin(PIX *pixs, bool adaptive) {
  int thresh;
  int fgval, bgval;
  PIX *pixb;

  PROCNAME("pixNLBin");

  pixb = pixNLNorm1(pixs, &thresh, &fgval, &bgval);
  if (!pixb)
    return (PIX *)ERROR_PTR("invalid normalization result", procName, NULL);

  /* Binarize */

  if (adaptive) {
    l_int32 w, h, nx, ny;
    pixGetDimensions(pixb, &w, &h, NULL);
    nx = L_MAX(1, (w + 64) / 128);
    ny = L_MAX(1, (h + 64) / 128);
    /* whsize needs to be this small to use it also for lineimages for tesseract */
    pixSauvolaBinarizeTiled(pixb, 16, 0.5, nx, ny, NULL, &pixb);
  } else {
    pixb = pixDitherToBinarySpec(pixb, bgval - ((bgval - thresh) * 0.75), fgval + ((thresh - fgval) * 0.25));
    // pixb = pixThresholdToBinary(pixb, fgval+((thresh-fgval)*.1));  /* for bg and light fg */
  }

  return pixb;
}





  
  // pixTRCMap(PIX   *pixs, PIX   *pixm, NUMA  *na)  --> can create and use our own dynamic range mapping with this one!
//
// pixAutoPhotoinvert()
//
//     if (edgecrop > 0.0) {
//  box = boxCreate(0.5f * edgecrop * w, 0.5f * edgecrop * h,
//                   (1.0f - edgecrop) * w, (1.0f - edgecrop) * h);
//   pix2 = pixClipRectangle(pix1, box, NULL);
//   boxDestroy(&box);
// }
//   else {
//   pix2 = pixClone(pix1);
// }
//
// pixCleanBackgroundToWhite()
//
//   pixalpha = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);  /* save */
//   if ((nag = numaGammaTRC(gamma, minval, maxval)) == NULL)
//     return (PIX *)ERROR_PTR("nag not made", __func__, pixd);
//   pixTRCMap(pixd, NULL, nag);
//   pixSetRGBComponent(pixd, pixalpha, L_ALPHA_CHANNEL); /* restore */
//   pixSetSpp(pixd, 4);
//   numaDestroy(&nag);
//   pixDestroy(&pixalpha);
//
// l_float32  avefg, avebg;
//   l_float32 numfg, numbg;
//   NUMA *na = pixGetGrayHistogram(pixt, 1);
//   l_float32 mean, median, mode, variance;
//   numaGetHistogramStats(na, 0.0, 1.0, &mean, &median, &mode, &variance);
//
// PIX * pixGetRGBComponent ( PIX *pixs, l_int32 comp );
//
// pixGetRankValue()
// numaHistogramGetValFromRank(na, rank, &val);
//
// numaGetMin(), numaGetMax()
//
// pixThresholdByConnComp()
//
//  numaGetNonzeroRange()
//
// pixMaxDynamicRange





} // namespace tesseract
