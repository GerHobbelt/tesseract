
// Include automatically generated configuration file
#include <tesseract/preparation.h> // compiler config, etc.

#include <leptonica/allheaders.h>

#include <tesseract/tprintf.h> // for tprintf

#include "pixProcessing.h" 
#include "rect.h" 

#include <cstdint>
#include <cstring>

namespace tesseract {

// Brewer-derived scale for the noise emphasis (RGB)
static const l_uint32 noiseEmphasisColorMap[] = {
    0x49006a, // [1]
    0x7a0177, // [32]
    0xae017e, // [32+48]
    0xdd3497,  // [128]
    0xf768a1, // [128+48]
    0xfa9fb5, // [128+96 ~ 254-30]
    0xfcc5c0, // [254]
};

static l_uint32 interpolate2NoiseEmphasisColors(l_uint32 a, l_uint32 b, l_uint32 offset256) {
  l_uint32 ax = (a & 0xFF0000) * (256 - offset256);
  l_uint32 bx = (b & 0xFF0000) * offset256;
  l_uint32 ay = (a & 0xFF00) * (256 - offset256);
  l_uint32 by = (b & 0xFF00) * offset256;
  l_uint32 az = (a & 0xFF) * (256 - offset256);
  l_uint32 bz = (b & 0xFF) * offset256;
  l_uint32 rx = ax + bx;
  l_uint32 ry = ay + by;
  l_uint32 rz = az + bz;
  l_uint32 rv = (rx & 0xFF000000) | (ry & 0xFF0000) | (rz & 0xFF00);
  // rv /= 256;   -- not needed as the lower byte is the ALPHA channel and we're fine like this already.
  //rv &= 0xFFFFFF00;   // blow away the alpha channel value
  return rv;
}

// the noise emphasis core function, which will be applied to every pixel of the source image.
static inline l_uint32 mapSourceValueTonoiseEmphasisColor(int value) {
  // mapping: 0 (black) remains BLACK, 255 (white) remains WHITE, the rest get mapped.
  if (value <= 0)
    return 0x00000000;
  if (value >= 255)
    return 0xFFFFFF00;
  if (value < 32) {
    return interpolate2NoiseEmphasisColors(noiseEmphasisColorMap[0], noiseEmphasisColorMap[1], l_uint32(value) * 256 / 32);
  }
  if (value < 32+48) {
    return interpolate2NoiseEmphasisColors(noiseEmphasisColorMap[1], noiseEmphasisColorMap[2], l_uint32(value - 32) * 256 / 48);
  }
  if (value < 128) {
    return interpolate2NoiseEmphasisColors(noiseEmphasisColorMap[2], noiseEmphasisColorMap[3], l_uint32(value - 32 - 48) * 256 / 48);
  }
  if (value < 128+48) {
    return interpolate2NoiseEmphasisColors(noiseEmphasisColorMap[3], noiseEmphasisColorMap[4], l_uint32(value - 128) * 256 / 48);
  }
  if (value < 128 + 96) {
    return interpolate2NoiseEmphasisColors(noiseEmphasisColorMap[4], noiseEmphasisColorMap[5], l_uint32(value - 128 - 48) * 256 / 48);
  }
  return interpolate2NoiseEmphasisColors(noiseEmphasisColorMap[5], noiseEmphasisColorMap[6], l_uint32(value - 128 - 96) * 256 / 32);
}

PIX *pixEmphasizeImageNoise(PIX *pixs) {
  l_int32 d;
  PIX *pixg, *pixd;

  PROCNAME("pixEmphasizeImageNoise");

  if (!pixs || (d = pixGetDepth(pixs)) < 1) {
    return (PIX *)ERROR_PTR("pixs undefined or d < 8 bpp", procName, NULL);
  }
  if (d == 32) {
    // ITU-R 601-2 luma
    pixg = pixConvertRGBToGray(pixs, 0.299, 0.587, 0.114);
    // Legacy converting
    // pixg = pixConvertRGBToGray(pixs, 0.3, 0.4, 0.3);
  } else {
    auto cmap = pixGetColormap(pixs);
    if (d != 8 || cmap != NULL) {
      pixg = pixConvertTo8(pixs, 0);
    } else {
      pixg = pixClone(pixs);
    }
  }

  // because leptonica's pixMaxDynamicRange() does not maximize as it only
  // reckons with the *maximum* pixel value and ignores the *minimum* pixel value.
  // We do reckon with both of 'em, though.
  pixd = pixMaxDynamicRange2(pixg, L_LINEAR_SCALE);
  pixDestroy(&pixg);
  pixg = pixd;

  l_int32 i, j, w, h, wpl;
  l_int32 dstwpl, blrwpl;
  l_uint32 *data, *line;
  l_uint32 *blrdata, *blrline;
  l_uint32 *dstdata, *dstline;

  pixGetDimensions(pixg, &w, &h, &d);
  if (d != 8)
    return (PIX *)ERROR_PTR("pixg not 8 bpp", procName, NULL);

  // blur
  PIX *pixg2 = pixBlockconv(pixg, 2, 2);

  pixd = pixCreate(w, h, 32);
  if (!pixd)
    return NULL;

  data = pixGetData(pixg);
  wpl = pixGetWpl(pixg);

  blrdata = pixGetData(pixg2);
  blrwpl = pixGetWpl(pixg2);

  dstdata = pixGetData(pixd);
  dstwpl = pixGetWpl(pixd);

  for (i = 0; i < h; i++) {
    line = data + i * wpl;
    blrline = blrdata + i * blrwpl;
    dstline = dstdata + i * dstwpl;
    for (j = 0; j < w; j++) {
      l_uint8 val = GET_DATA_BYTE(line, j);
      l_uint8 blrval = GET_DATA_BYTE(blrline, j);
      int delta = int(blrval >= 128 ? 255 : 0) - int(val);
      l_uint32 color;
      if (delta == 0) {
        // grey to RGBA:
        color = val;
        color |= color << 8;
        color |= color << 8;
        color = color << 8;
      } else {
#if 0
        // emphasize the difference
        delta *= 24;
#else
        int a = delta;
        if (a < 0)
          a = -a;
        // emphasize the difference
        if (a < 4)
          delta *= 16;
        else if (a < 8)
          delta *= 8;
        else if (a < 16)
          delta *= 4;
        else if (a < 32)
          delta *= 2;
        else if (a < 64)
          delta *= 1;
        else if (a < 128)
          delta /= 2;
        else
          delta /= 4;
#endif

        int r = int(val >= 128 ? 255 : 0) + delta;
        // fold/mirror value into 256 value range
        if (r < 0) {
          r = -r;
          r &= 0x7F;
          // if (r > 128)
          //   r = 128;
        } else if (r >= 256) {
          r -= 255;
          r &= 0x7F;
          // if (r > 128)
          //   r = 128;
          r = 255 - r;
        }
        color = mapSourceValueTonoiseEmphasisColor(r);
      }
      // color = mapSourceValueTonoiseEmphasisColor(int(blrval));
      SET_DATA_FOUR_BYTES(dstline, j, color);
    }
  }

  pixDestroy(&pixg);
  pixDestroy(&pixg2);

  return pixd;
}


PIX *pixEmphasizeImageNoise2(PIX *pixs) {
  l_int32 d;
  PIX *pixg, *pixd;

  PROCNAME("pixEmphasizeImageNoise");

  if (!pixs || (d = pixGetDepth(pixs)) < 1) {
    return (PIX *)ERROR_PTR("pixs undefined or d < 8 bpp", procName, NULL);
  }
  if (d == 32) {
    // ITU-R 601-2 luma
    pixg = pixConvertRGBToGray(pixs, 0.299, 0.587, 0.114);
    // Legacy converting
    // pixg = pixConvertRGBToGray(pixs, 0.3, 0.4, 0.3);
  } else {
    auto cmap = pixGetColormap(pixs);
    if (d != 8 || cmap != NULL) {
      pixg = pixConvertTo8(pixs, 0);
    } else {
      pixg = pixClone(pixs);
    }
  }

  // because leptonica's pixMaxDynamicRange() does not maximize at it only
  // reckons with the *maximum* pixel value and ignores the *minimum* pixel value.
  // We do reckon with both of 'em, though.
  pixd = pixMaxDynamicRange2(pixg, L_LINEAR_SCALE);
  pixDestroy(&pixg);
  pixg = pixd;

  l_int32 i, j, w, h, wpl;
  l_int32 dstwpl;
  l_uint32 *data, *line;
  l_uint32 *dstdata, *dstline;

  pixGetDimensions(pixg, &w, &h, &d);
  if (d != 8)
    return (PIX *)ERROR_PTR("pixg not 8 bpp", procName, NULL);

  pixd = pixCreate(w, h, 32);
  if (!pixd)
    return NULL;

  data = pixGetData(pixg);
  wpl = pixGetWpl(pixg);

  dstdata = pixGetData(pixd);
  dstwpl = pixGetWpl(pixd);

  for (i = 0; i < h; i++) {
    line = data + i * wpl;
    dstline = dstdata + i * dstwpl;
    for (j = 0; j < w; j++) {
      l_uint8 val = GET_DATA_BYTE(line, j);
      int delta = (val >= 128 ? 255 - int(val) : 0 - int(val));
      l_uint32 color;
      if (delta == 0) {
        // grey to RGBA:
        color = val;
        color |= color << 8;
        color |= color << 8;
        color = color << 8;
      } else {
#if 01
        // emphasize the difference
        delta *= 16;
#else
        int a = delta;
        if (a < 0)
          a = -a;
        // emphasize the difference
        if (a < 4)
          delta *= 16;
        else if (a < 8)
          delta *= 8;
        else if (a < 16)
          delta *= 4;
        else if (a < 32)
          delta *= 2;
        else if (a < 64)
          delta *= 1;
        else if (a < 128)
          delta /= 2;
        else
          delta /= 4;
#endif

        int r = int(val) + delta;
        // fold/mirror value into 256 value range
        if (r < 0) {
          r = -r;
          r &= 0x7F;
          // if (r > 128)
          //  r = 128;
        } else if (r >= 256) {
          r -= 255;
          r &= 0x7F;
          // if (r > 128)
          //  r = 128;
          r = 255 - r;
        }
        color = mapSourceValueTonoiseEmphasisColor(r);
      }
      // color = mapSourceValueTonoiseEmphasisColor(int(blrval));
      SET_DATA_FOUR_BYTES(dstline, j, color);
    }
  }

  pixDestroy(&pixg);

  return pixd;
}





/*-----------------------------------------------------------------------*
 *                    Scale for maximum dynamic range                    *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixMaxDynamicRange2()
 *
 * \param[in]    pixs    8 bpp source
 * \param[in]    type    L_LINEAR_SCALE or L_LOG_SCALE
 * \return  pixd    8 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Scales pixel values to fit maximally within the dest 8 bpp pixd
 *      (2) Assumes the source 'pixels' are a 1-component scalar.
 *      (3) Uses a LUT for log scaling.
 * </pre>
 */
PIX *pixMaxDynamicRange2(PIX *pixs, l_int32 type) {
  l_uint8 min, max;
  l_int32 i, j, w, h, d, wpls, wpld;
  l_uint32 *datas, *datad;
  l_uint32 word;
  l_uint32 *lines, *lined;
  l_float32 factor;
  l_float32 *tab;
  PIX *pixd;

  if (!pixs)
    return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
  pixGetDimensions(pixs, &w, &h, &d);
  if (d != 8)
    return (PIX *)ERROR_PTR("pixs not in {8} bpp", __func__, NULL);
  if (type != L_LINEAR_SCALE && type != L_LOG_SCALE)
    return (PIX *)ERROR_PTR("invalid type", __func__, NULL);

  if ((pixd = pixCreate(w, h, 8)) == NULL)
    return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
  pixCopyResolution(pixd, pixs);
  datas = pixGetData(pixs);
  datad = pixGetData(pixd);
  wpls = pixGetWpl(pixs);
  wpld = pixGetWpl(pixd);

  /* Get min,max */
  max = 0;
  min = 255;
  for (i = 0; i < h; i++) {
    lines = datas + i * wpls;
    for (j = 0; j < wpls; j++) {
      word = *(lines + j);
      max = L_MAX(max, word >> 24);
      max = L_MAX(max, (word >> 16) & 0xff);
      max = L_MAX(max, (word >> 8) & 0xff);
      max = L_MAX(max, word & 0xff);

      min = L_MIN(min, word >> 24);
      min = L_MIN(min, (word >> 16) & 0xff);
      min = L_MIN(min, (word >> 8) & 0xff);
      min = L_MIN(min, word & 0xff);
    }
  }

  /* Map to the full dynamic range */
  if (type == L_LINEAR_SCALE) {
    factor = 255.f / (l_float32)(max - min);
    for (i = 0; i < h; i++) {
      lines = datas + i * wpls;
      lined = datad + i * wpld;
      for (j = 0; j < w; j++) {
        auto sval = GET_DATA_BYTE(lines, j);
        sval -= min;
        auto dval = (l_uint8)(factor * (l_float32)sval + 0.5);
        SET_DATA_BYTE(lined, j, dval);
      }
    }
  } else { /* type == L_LOG_SCALE) */
    tab = makeLogBase2Tab();
    factor = 255.f / getLogBase2(max - min, tab);
    for (i = 0; i < h; i++) {
      lines = datas + i * wpls;
      lined = datad + i * wpld;
      for (j = 0; j < w; j++) {
        auto sval = GET_DATA_BYTE(lines, j);
        sval -= min;
        auto dval = (l_uint8)(factor * getLogBase2(sval, tab) + 0.5);
        SET_DATA_BYTE(lined, j, dval);
      }
    }
    LEPT_FREE(tab);
  }

  return pixd;
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
PIX *pixNLNorm2(PIX *pixs, int *pthresh) {
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


static inline int FADE(int val, const int factor) {
  return (val * factor + 255 * (256 - factor)) >> 8 /* div 256 */;
}

static inline int MIX(int val1, int val2, const int factor) {
  return (val2 * factor + val1 * (256 - factor)) >> 8 /* div 256 */;
}


  Image pixMixWithTintedBackground(const Image &src, const Image &background,
                                float r_factor, float g_factor, float b_factor,
                                float src_factor, float background_factor,
                                const TBOX *cliprect) {
  int w, h, depth;
  ASSERT0(src != nullptr);
  pixGetDimensions(src, &w, &h, &depth);

  if (background == nullptr || background == src) {
    return pixConvertTo32(const_cast<PIX *>(src.ptr()));
  } else {
    int ow, oh, od;
    pixGetDimensions(background, &ow, &oh, &od);

    Image toplayer = pixConvertTo32(const_cast<PIX *>(src.ptr()));
    Image botlayer = pixConvertTo32(const_cast<PIX *>(background.ptr())); // quick hack, safe as `background` gets COPIED anyway in there.

    if (w != ow || h != oh) {
      if (cliprect != nullptr) {
        // when a TBOX is provided, you can bet your bottom dollar `src` is an 'extract' of `background`
        // and we therefore should paint it back onto there at the right spot:
        // the cliprectx/y coordinates will tell us!
        int cx, cy, cw, ch;
        cx = cliprect->left();
        cy = cliprect->top();
        cw = cliprect->width();
        ch = cliprect->height();

        // when the clipping rectangle indicates another area than we got in `src`, we need to scale `src` first:
        //
        // smaller images are generally masks, etc. and we DO NOT want to be
        // confused by the smoothness introduced by regular scaling, so we
        // apply brutal sampled scale then:
        if (w != cw || h != ch) {
          if (w < cw && h < ch) {
            toplayer = pixScaleBySamplingWithShift(toplayer, cw * 1.0f / w, ch * 1.0f / h, 0.0f, 0.0f);
          } else if (w > cw && h > ch) {
            // the new image has been either scaled up vs. the original OR a
            // border was added (TODO)
            //
            // for now, we simply apply regular smooth scaling
            toplayer = pixScale(toplayer, cw * 1.0f / w, ch * 1.0f / h);
          } else {
            // scale a clipped partial to about match the size of the
            // original/base image, so the generated HTML + image sequence is
            // more, äh, uniform/readable.
#if 0
              ASSERT0(!"Should never get here! Non-uniform scaling of images collected in DebugPixa!");
#endif
            toplayer = pixScale(toplayer, cw * 1.0f / w, ch * 1.0f / h);
          }

          pixGetDimensions(toplayer, &w, &h, &depth);
        }
        // now composite over 30% grey: this is done by simply resizing to background using 30% grey as a 'border':
        int bl = cx;
        int br = ow - cx - cw;
        int bt = cy;
        int bb = oh - cy - ch;

        if (bl || br || bt || bb) {
          l_uint32 grey;
          const int g = int(0.7 * 256);
          (void)composeRGBAPixel(g, g, g, 256, &grey);
          toplayer = pixAddBorderGeneral(toplayer, bl, br, bt, bb, grey);
        }
      } else {
        // no cliprect specified, so `src` must be scaled version of
        // `background`.
        //
        // smaller images are generally masks, etc. and we DO NOT want to be
        // confused by the smoothness introduced by regular scaling, so we
        // apply brutal sampled scale then:
        if (w < ow && h < oh) {
          toplayer = pixScaleBySamplingWithShift(toplayer, ow * 1.0f / w, oh * 1.0f / h, 0.0f, 0.0f);
        } else if (w > ow && h > oh) {
          // the new image has been either scaled up vs. the original OR a
          // border was added (TODO)
          //
          // for now, we simply apply regular smooth scaling
          toplayer = pixScale(toplayer, ow * 1.0f / w, oh * 1.0f / h);
        } else {
          // scale a clipped partial to about match the size of the
          // original/base image, so the generated HTML + image sequence is
          // more, äh, uniform/readable.
#if 0
            ASSERT0(!"Should never get here! Non-uniform scaling of images collected in DebugPixa!");
#endif
          toplayer = pixScale(toplayer, ow * 1.0f / w, oh * 1.0f / h);
        }
      }
    }

    // constant fade factors:
    const int red_factor = r_factor * 256;
    const int green_factor = g_factor * 256;
    const int blue_factor = b_factor * 256;
    const int base_mix_factor = src_factor * 256;
    const int bottom_mix_factor = background_factor * 256;

    auto datas = pixGetData(toplayer);
    auto datad = pixGetData(botlayer);
    auto wpls = pixGetWpl(toplayer);
    auto wpld = pixGetWpl(botlayer);
    int i, j;
    for (i = 0; i < oh; i++) {
      auto lines = (datas + i * wpls);
      auto lined = (datad + i * wpld);
      for (j = 0; j < ow; j++) {
        // if top(SRC) is black, use that.
        // if top(SRC) is white, and bot(DST) isn't, color bot(DST) red and use
        // that. if top(SRC) is white, and bot(DST) is white, use white.

        int rvals, gvals, bvals;
        extractRGBValues(lines[j], &rvals, &gvals, &bvals);

        int rvald, gvald, bvald;
        extractRGBValues(lined[j], &rvald, &gvald, &bvald);

        // R
        int rval = FADE(rvald, red_factor);
        if (rvals < rval)
          rval = MIX(rvals, rval, bottom_mix_factor);
        else
          rval = MIX(rvals, rval, base_mix_factor);

        // G
        int gval = FADE(gvald, green_factor);
        if (gvals < gval)
          gval = MIX(gvals, gval, bottom_mix_factor);
        else
          gval = MIX(gvals, gval, base_mix_factor);

        // B
        int bval = FADE(bvald, blue_factor);
        if (bvals < bval)
          bval = MIX(bvals, bval, bottom_mix_factor);
        else
          bval = MIX(bvals, bval, base_mix_factor);

        // A
        // avald = 0;

        composeRGBPixel(rval, gval, bval, lined + j);
      }
    }
    // pixCopyResolution(pixd, pixs);
    // pixCopyInputFormat(pixd, pixs);

    // botlayer.destroy();
    //toplayer.destroy();

#if 0
    return botlayer.clone2pix();
#else
    return botlayer;
#endif
  }
}



} // namespace tesseract
