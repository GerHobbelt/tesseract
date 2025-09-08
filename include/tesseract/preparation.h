
#if defined(_MSC_VER) && !defined(_USE_MATH_DEFINES)
#error "tesseract needs you to define _USE_MATH_DEFINES when compiling with MSVC to get access to M_PI et al on the Win32 platform."
#endif

// Include automatically generated configuration file if running autoconf.
#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>

#if 0
#include <winsock2.h>
#include <windows.h>
#endif

#include <float.h> // for __control87
// #pragma fenv_access (on)
#endif

#include <tesseract/debugheap.h>
