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

#include <tesseract/preparation.h> // compiler config, etc.

#include "common/commontraining.h"
#include "common/mastertrainer.h"
#include <tesseract/params.h>

#include "tesseract/capi_training_tools.h"


#if !DISABLED_LEGACY_ENGINE

using namespace tesseract;

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

INT_VAR(shapclustering_display_cloud_font, -1, "Display cloud of this font, canonical_class1");
INT_VAR(shapclustering_display_canonical_font, -1, "Display canonical sample of this font, canonical_class2");
STRING_VAR(shapclustering_canonical_class1, "", "Class to show ambigs for");
STRING_VAR(shapclustering_canonical_class2, "", "Class to show ambigs for");

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

// Loads training data, if requested displays debug information, otherwise
// creates the master shape table by shape clustering and writes it to a file.
// If shapclustering_display_cloud_font is set, then the cloud features of
// shapclustering_canonical_class1/shapclustering_display_cloud_font are shown in green ON TOP
// OF the red canonical features of shapclustering_canonical_class2/
// shapclustering_display_canonical_font, so as to show which canonical features are
// NOT in the cloud.
// Otherwise, if shapclustering_canonical_class1 is set, prints a table of font-wise
// cluster distances between shapclustering_canonical_class1 and shapclustering_canonical_class2.
#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_shape_clustering_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();

  ParseArguments(&argc, &argv);

  std::string file_prefix;
  auto trainer = tesseract::LoadTrainingData(argv + 1, false, nullptr, file_prefix);

  if (!trainer) {
    return EXIT_FAILURE;
  }

  if (shapclustering_display_cloud_font >= 0) {
#if !GRAPHICS_DISABLED
    trainer->DisplaySamples(shapclustering_canonical_class1.c_str(), shapclustering_display_cloud_font,
                            shapclustering_canonical_class2.c_str(), shapclustering_display_canonical_font);
#endif // !GRAPHICS_DISABLED
    return EXIT_SUCCESS;
  } else if (!shapclustering_canonical_class1.empty()) {
    trainer->DebugCanonical(shapclustering_canonical_class1.c_str(), shapclustering_canonical_class2.c_str());
    return EXIT_SUCCESS;
  }
  trainer->SetupMasterShapes();
  WriteShapeTable(file_prefix, trainer->master_shapes());

  return EXIT_SUCCESS;
} /* main */

#else

TESS_API int tesseract_shape_clustering_main(int argc, const char** argv)
{
	tesseract::tprintError("the {} tool is not supported in this build.\n", argv[0]);
	return 1;
}

#endif
