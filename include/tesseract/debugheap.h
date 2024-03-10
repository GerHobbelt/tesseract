#ifndef __TESSERACT_HELPER_DEBUGHEAP_H__
#define __TESSERACT_HELPER_DEBUGHEAP_H__

#if defined(HAVE_MUPDF)

#include "mupdf/helpers/debugheap.h"

#else

#if defined(_MSC_VER)
#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#endif

#ifndef _CRTDBG_MAP_ALLOC_NEW
#define _CRTDBG_MAP_ALLOC_NEW
#endif

#include <crtdbg.h>

#if defined(_DEBUG) && defined(_CRTDBG_REPORT_FLAG)

#ifndef NEW_CBDBG // new operator: debug clientblock:
#define NEW_CBDBG new (_CLIENT_BLOCK, __FILE__, __LINE__)
#define new NEW_CBDBG
#endif
#endif
#endif


#define FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(prefix)  /**/
#define FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(prefix)    /**/

#endif

#endif
