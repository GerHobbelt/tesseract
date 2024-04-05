// Copyright 2011 Google Inc. All Rights Reserved.
// Author: rays@google.com (Ray Smith)

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//  Filename: shapeclustering.cpp
//  Purpose:  Generates a master shape table to merge similarly-shaped
//            training data of whole, partial or multiple characters.
//  Author:   Ray Smith

#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#include <tesseract/debugheap.h>

#include "common/commontraining.h"
#include "common/mastertrainer.h"
#include "params.h"
#include "mupdf/fitz/string-util.h"

#include "tesseract/capi_training_tools.h"


#if !DISABLED_LEGACY_ENGINE

using namespace tesseract;

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

INT_PARAM_FLAG(display_cloud_font, -1, "Display cloud of this font, canonical_class1");
INT_PARAM_FLAG(display_canonical_font, -1, "Display canonical sample of this font, canonical_class2");
STRING_PARAM_FLAG(canonical_class1, "", "Class to show ambigs for");
STRING_PARAM_FLAG(canonical_class2, "", "Class to show ambigs for");

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

// Loads training data, if requested displays debug information, otherwise
// creates the master shape table by shape clustering and writes it to a file.
// If FLAGS_display_cloud_font is set, then the cloud features of
// FLAGS_canonical_class1/FLAGS_display_cloud_font are shown in green ON TOP
// OF the red canonical features of FLAGS_canonical_class2/
// FLAGS_display_canonical_font, so as to show which canonical features are
// NOT in the cloud.
// Otherwise, if FLAGS_canonical_class1 is set, prints a table of font-wise
// cluster distances between FLAGS_canonical_class1 and FLAGS_canonical_class2.
#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_shape_clustering_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  int rv = ParseArguments(&argc, &argv);
  if (rv >= 0) {
    return EXIT_FAILURE;
  }

  std::string file_prefix;
  auto trainer = tesseract::LoadTrainingData(argv + 1, false, nullptr, file_prefix);

  if (!trainer) {
    return EXIT_FAILURE;
  }

  if (FLAGS_display_cloud_font >= 0) {
#if !GRAPHICS_DISABLED
    trainer->DisplaySamples(FLAGS_canonical_class1.c_str(), FLAGS_display_cloud_font,
                            FLAGS_canonical_class2.c_str(), FLAGS_display_canonical_font);
#endif // !GRAPHICS_DISABLED
    return EXIT_SUCCESS;
  } else if (!FLAGS_canonical_class1.empty()) {
    trainer->DebugCanonical(FLAGS_canonical_class1.c_str(), FLAGS_canonical_class2.c_str());
    return EXIT_SUCCESS;
  }
  trainer->SetupMasterShapes();
  WriteShapeTable(file_prefix, trainer->master_shapes());

  return EXIT_SUCCESS;
} /* main */

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_shape_clustering_main(int argc, const char** argv)
#endif
{
	tesseract::tprintError("the {} tool is not supported in this build.\n", fz_basename(argv[0]));
	return EXIT_FAILURE;
}

#endif
