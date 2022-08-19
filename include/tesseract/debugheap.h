#ifndef __TESSERACT_HELPER_DEBUGHEAP_H__
#define __TESSERACT_HELPER_DEBUGHEAP_H__

#if defined(HAVE_MUPDF)

#include "mupdf/helpers/debugheap.h"

#else

#define FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(prefix)  /**/
#define FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(prefix)    /**/

#endif

#endif
