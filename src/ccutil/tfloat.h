#ifndef TESSERACT_LSTM_TFLOAT_H
#define TESSERACT_LSTM_TFLOAT_H

namespace tesseract {

#if defined(FAST_FLOAT) || 01
typedef float TFloat;
#else
typedef double TFloat;
#endif

}

#endif
