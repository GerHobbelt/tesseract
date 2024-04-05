/******************************************************************************
 **  Filename:  cntraining.cpp
 **  Purpose:  Generates a normproto and pffmtable.
 **  Author:    Dan Johnson
 **  Revisment:  Christy Russon
 **
 **  (c) Copyright Hewlett-Packard Company, 1988.
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 ******************************************************************************/

/*----------------------------------------------------------------------------
          Include Files and Type Defines
----------------------------------------------------------------------------*/
#include <tesseract/unichar.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "cluster.h"
#include "clusttool.h"
#include "common/commontraining.h"
#include "featdefs.h"
#include "ocrfeatures.h"
#include "oldlist.h"
#include "mupdf/fitz/string-util.h"

#include "tesseract/capi_training_tools.h"


#define PROGRAM_FEATURE_TYPE "cn"

#if !DISABLED_LEGACY_ENGINE

using namespace tesseract;

/*----------------------------------------------------------------------------
          Private Function Prototypes
----------------------------------------------------------------------------*/

static int WriteNormProtos(const char *Directory, LIST LabeledProtoList,
                            const FEATURE_DESC_STRUCT *feature_desc);

static void WriteProtos(FILE *File, uint16_t N, LIST ProtoList, bool WriteSigProtos,
                        bool WriteInsigProtos);

/*----------------------------------------------------------------------------
          Global Data Definitions and Declarations
----------------------------------------------------------------------------*/
/* global variable to hold configuration parameters to control clustering */
//-M 0.025   -B 0.05   -I 0.8   -C 1e-3
static const CLUSTERCONFIG CNConfig = {elliptical, 0.025, 0.05, 0.8, 1e-3, 0};

/*----------------------------------------------------------------------------
              Public Code
----------------------------------------------------------------------------*/

/**
* This program reads in a text file consisting of feature
* samples from a training page in the following format:
* @verbatim
   FontName CharName NumberOfFeatureTypes(N)
      FeatureTypeName1 NumberOfFeatures(M)
         Feature1
         ...
         FeatureM
      FeatureTypeName2 NumberOfFeatures(M)
         Feature1
         ...
         FeatureM
      ...
      FeatureTypeNameN NumberOfFeatures(M)
         Feature1
         ...
         FeatureM
   FontName CharName ...
@endverbatim
* It then appends these samples into a separate file for each
* character.  The name of the file is
*
*   DirectoryName/FontName/CharName.FeatureTypeName
*
* The DirectoryName can be specified via a command
* line argument.  If not specified, it defaults to the
* current directory.  The format of the resulting files is:
* @verbatim
   NumberOfFeatures(M)
      Feature1
      ...
      FeatureM
   NumberOfFeatures(M)
   ...
@endverbatim
* The output files each have a header which describes the
* type of feature which the file contains.  This header is
* in the format required by the clusterer.  A command line
* argument can also be used to specify that only the first
* N samples of each class should be used.
* @param argc  number of command line arguments
* @param argv  array of command line arguments
* @return 0 on success
*/
#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_cn_training_main(int argc, const char** argv)
#endif
{
  tesseract::CheckSharedLibraryVersion();
  (void)tesseract::SetConsoleModeToUTF8();

  // Set the global Config parameters before parsing the command line.
  Config = CNConfig;

  int rv;
  LIST CharList = NIL_LIST;
  CLUSTERER* Clusterer = nullptr;
  LIST ProtoList = NIL_LIST;
  LIST NormProtoList = NIL_LIST;
  LIST pCharList;
  LABELEDLIST CharSample;
  FEATURE_DEFS_STRUCT FeatureDefs;
  InitFeatureDefs(&FeatureDefs);

  rv = ParseArguments(&argc, &argv);
  if (rv >= 0) {
    return rv;
  }
  rv = EXIT_SUCCESS;

  // int num_fonts = 0;
  for (const char* PageName = *++argv; PageName != nullptr; PageName = *++argv) {
    tprintInfo("Reading {} ...\n", PageName);
    FILE* TrainingPage = fopen(PageName, "rb");
    ASSERT_HOST(TrainingPage);
    if (TrainingPage) {
      ReadTrainingSamples(FeatureDefs, PROGRAM_FEATURE_TYPE, 100, nullptr, TrainingPage, &CharList);
      fclose(TrainingPage);
      // ++num_fonts;
    }
  }
  tprintInfo("Clustering ...\n");
  // To allow an individual font to form a separate cluster,
  // reduce the min samples:
  // Config.MinSamples = 0.5 / num_fonts;
  pCharList = CharList;
  // The norm protos will count the source protos, so we keep them here in
  // freeable_protos, so they can be freed later.
  std::vector<LIST> freeable_protos;
  iterate(pCharList) {
    // Cluster
    CharSample = reinterpret_cast<LABELEDLIST>(pCharList->first_node());
    Clusterer = SetUpForClustering(FeatureDefs, CharSample, PROGRAM_FEATURE_TYPE);
    if (Clusterer == nullptr) { // To avoid a SIGSEGV
      tprintError("nullptr clusterer! SetUpForClustering failed!\n");
      rv = EXIT_FAILURE;
      break;
    }
    else {
      float SavedMinSamples = Config.MinSamples;
      // To disable the tendency to produce a single cluster for all fonts,
      // make MagicSamples an impossible to achieve number:
      // Config.MagicSamples = CharSample->SampleCount * 10;
      Config.MagicSamples = CharSample->SampleCount;
      while (Config.MinSamples > 0.001) {
        ProtoList = ClusterSamples(Clusterer, &Config);
        if (NumberOfProtos(ProtoList, true, false) > 0) {
          break;
        }
        else {
          Config.MinSamples *= 0.95;
          tprintInfo(
              "0 significant protos for {}"
              " Retrying clustering with MinSamples = {}%\n",
              CharSample->Label, Config.MinSamples);
        }
      }
      Config.MinSamples = SavedMinSamples;
      AddToNormProtosList(&NormProtoList, ProtoList, CharSample->Label);
      freeable_protos.push_back(ProtoList);
      FreeClusterer(Clusterer);
    }
  }
  FreeTrainingSamples(CharList);
  int desc_index = ShortNameToFeatureType(FeatureDefs, PROGRAM_FEATURE_TYPE);
  if (WriteNormProtos(FLAGS_D.c_str(), NormProtoList, FeatureDefs.FeatureDesc[desc_index]))
    rv = EXIT_FAILURE;
  FreeNormProtoList(NormProtoList);
  for (auto& freeable_proto : freeable_protos) {
    FreeProtoList(&freeable_proto);
  }
  tprintInfo("\n");
  return rv;
} // main

/*----------------------------------------------------------------------------
              Private Code
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
 * This routine writes the specified samples into files which
 * are organized according to the font name and character name
 * of the samples.
 * @param Directory  directory to place sample files into
 * @param LabeledProtoList List of labeled protos
 * @param feature_desc Description of the features
 */
static int WriteNormProtos(const char *Directory, LIST LabeledProtoList,
                            const FEATURE_DESC_STRUCT *feature_desc) {
  FILE *File;
  LABELEDLIST LabeledProto;
  int N;

  std::string Filename = "";
  if (Directory != nullptr && Directory[0] != '\0') {
    Filename += Directory;
    Filename += "/";
  }
  Filename += "normproto";
  tprintInfo("\nWriting {} ...\n", Filename);
  File = fopen(Filename.c_str(), "wb");
  ASSERT_HOST(File);
  fprintf(File, "%0d\n", feature_desc->NumParams);
  WriteParamDesc(File, feature_desc->NumParams, feature_desc->ParamDesc);
  iterate(LabeledProtoList) {
    LabeledProto = reinterpret_cast<LABELEDLIST>(LabeledProtoList->first_node());
    N = NumberOfProtos(LabeledProto->List, true, false);
    if (N < 1) {
      tprintError("Not enough protos for {}: {} protos"
          " ({} significant protos"
          ", {} insignificant protos)\n",
          LabeledProto->Label, N, NumberOfProtos(LabeledProto->List, true, false),
          NumberOfProtos(LabeledProto->List, false, true));

	  tprintError("\nWriting {} aborted.\n", Filename);
	  fclose(File);
	  return 1;
    }
    fprintf(File, "\n%s %d\n", LabeledProto->Label.c_str(), N);
    WriteProtos(File, feature_desc->NumParams, LabeledProto->List, true, false);
  }
  fprintf(File, "\n");
  fclose(File);
  tprintInfo("\nWriting {} completed.\n", Filename);
  return 0;
} // WriteNormProtos

/*-------------------------------------------------------------------------*/

static void WriteProtos(FILE *File, uint16_t N, LIST ProtoList, bool WriteSigProtos,
                        bool WriteInsigProtos) {
  PROTOTYPE *Proto;

  // write prototypes
  iterate(ProtoList) {
    Proto = reinterpret_cast<PROTOTYPE *>(ProtoList->first_node());
    if ((Proto->Significant && WriteSigProtos) || (!Proto->Significant && WriteInsigProtos)) {
      WritePrototype(File, N, Proto);
    }
  }
} // WriteProtos

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" TESS_API int tesseract_cn_training_main(int argc, const char** argv)
#endif
{
	tesseract::tprintError("the {} tool is not supported in this build.\n", fz_basename(argv[0]));
	return EXIT_FAILURE;
}

#endif
