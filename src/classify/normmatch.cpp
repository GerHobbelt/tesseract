/******************************************************************************
 ** Filename:    normmatch.c
 ** Purpose:     Simple matcher based on character normalization features.
 ** Author:      Dan Johnson
 **
 ** (c) Copyright Hewlett-Packard Company, 1988.
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

#include <tesseract/preparation.h> // compiler config, etc.

#if !DISABLED_LEGACY_ENGINE

#include "normmatch.h"

#include "classify.h"
#include "clusttool.h"
#include "helpers.h"
#include "normfeat.h"
#include <tesseract/params.h>
#include "unicharset.h"

#include <cmath>
#include <cstdio>
#include <sstream> // for std::istringstream

namespace tesseract {

struct NORM_PROTOS {
  NORM_PROTOS(size_t n) : NumProtos(n), Protos(n) {
  }
  int NumParams = 0;
  int NumProtos;
  PARAM_DESC *ParamDesc = nullptr;
  std::vector<LIST> Protos;
};

/*----------------------------------------------------------------------------
              Private Code
----------------------------------------------------------------------------*/

/**
 * @name NormEvidenceOf
 *
 * Return the new type of evidence number corresponding to this
 * normalization adjustment.  The equation that represents the transform is:
 *       1 / (1 + (NormAdj / midpoint) ^ curl)
 */
static float NormEvidenceOf(float NormAdj) {
  NormAdj /= classify_norm_adj_midpoint.value();

  NormAdj = std::pow(NormAdj, classify_norm_adj_curl.value());

  return (1 / (1 + NormAdj));
}

/*----------------------------------------------------------------------------
        Variables
----------------------------------------------------------------------------*/

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

/** control knobs used to control the normalization adjustment process */
DOUBLE_VAR(classify_norm_adj_midpoint, 32.0, "Norm adjust midpoint ...");
DOUBLE_VAR(classify_norm_adj_curl, 2.0, "Norm adjust curl ...");
/** Weight of width variance against height and vertical position. */
const float kWidthErrorWeighting = 0.125f;

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

/*----------------------------------------------------------------------------
              Public Code
----------------------------------------------------------------------------*/
/**
 * This routine compares Features against each character
 * normalization proto for ClassId and returns the match
 * rating of the best match.
 * @param ClassId id of class to match against
 * @param feature character normalization feature
 * @param DebugMatch controls dump of debug info
 *
 * Globals:
 * #NormProtos character normalization prototypes
 *
 * @return Best match rating for Feature against protos of ClassId.
 */
float Classify::ComputeNormMatch(CLASS_ID ClassId, const FEATURE_STRUCT &feature, bool DebugMatch) {
  if (ClassId >= NormProtos->NumProtos) {
    ClassId = NO_CLASS;
  }

  /* handle requests for classification as noise */
  if (ClassId == NO_CLASS) {
    /* kludge - clean up constants and make into control knobs later */
    float Match = (feature.Params[CharNormLength] * feature.Params[CharNormLength] * 500.0f +
                   feature.Params[CharNormRx] * feature.Params[CharNormRx] * 8000.0f +
                   feature.Params[CharNormRy] * feature.Params[CharNormRy] * 8000.0f);
    return (1 - NormEvidenceOf(Match));
  }

  if (DebugMatch) {
    tprintDebug("\nChar norm for class {}\n", unicharset_.id_to_unichar(ClassId));
  }

  LIST Protos = NormProtos->Protos[ClassId];
  if (Protos == nullptr) {
     // Avoid FP overflow in NormEvidenceOf.
     return 1.0f;
  }

  float BestMatch = FLT_MAX;
  iterate(Protos) {
    auto Proto = reinterpret_cast<PROTOTYPE *>(Protos->first_node());
    float Delta = feature.Params[CharNormY] - Proto->Mean[CharNormY];
    float Match = Delta * Delta * Proto->Weight.Elliptical[CharNormY];
    if (DebugMatch) {
      tprintDebug("YMiddle: Proto={}, Delta={}, Var={}, Dist={}\n", Proto->Mean[CharNormY], Delta,
              Proto->Weight.Elliptical[CharNormY], Match);
    }
    Delta = feature.Params[CharNormRx] - Proto->Mean[CharNormRx];
    Match += Delta * Delta * Proto->Weight.Elliptical[CharNormRx];
    if (DebugMatch) {
      tprintDebug("Height: Proto={}, Delta={}, Var={}, Dist={}\n", Proto->Mean[CharNormRx], Delta,
              Proto->Weight.Elliptical[CharNormRx], Match);
    }
    // Ry is width! See intfx.cpp.
    Delta = feature.Params[CharNormRy] - Proto->Mean[CharNormRy];
    if (DebugMatch) {
      tprintDebug("Width: Proto={}, Delta={}, Var={}\n", Proto->Mean[CharNormRy], Delta,
              Proto->Weight.Elliptical[CharNormRy]);
    }
    Delta = Delta * Delta * Proto->Weight.Elliptical[CharNormRy];
    Delta *= kWidthErrorWeighting;
    Match += Delta;
    if (DebugMatch) {
      tprintDebug("Total Dist={}, scaled={}, sigmoid={}, penalty={}\n", Match,
              Match / classify_norm_adj_midpoint, NormEvidenceOf(Match),
              256 * (1 - NormEvidenceOf(Match)));
    }

    if (Match < BestMatch) {
      BestMatch = Match;
    }
  }
  return 1 - NormEvidenceOf(BestMatch);
} /* ComputeNormMatch */

void Classify::FreeNormProtos() {
  if (NormProtos != nullptr) {
    for (int i = 0; i < NormProtos->NumProtos; i++) {
      FreeProtoList(&NormProtos->Protos[i]);
    }
    delete[] NormProtos->ParamDesc;
    delete NormProtos;
    NormProtos = nullptr;
  }
}

/**
 * This routine allocates a new data structure to hold
 * a set of character normalization protos.  It then fills in
 * the data structure by reading from the specified File.
 * @param fp open text file to read normalization protos from
 * Globals: none
 * @return Character normalization protos.
 */
NORM_PROTOS *Classify::ReadNormProtos(TFile *fp) {
  char unichar[2 * UNICHAR_LEN + 1];
  UNICHAR_ID unichar_id;
  LIST Protos;
  int NumProtos;

  /* allocate and initialization data structure */
  auto NormProtos = new NORM_PROTOS(unicharset_.size());

  /* read file header and save in data structure */
  NormProtos->NumParams = ReadSampleSize(fp);
  NormProtos->ParamDesc = ReadParamDesc(fp, NormProtos->NumParams);

  /* read protos for each class into a separate list */
  const int kMaxLineSize = 100;
  char line[kMaxLineSize];
  while (fp->FGets(line, kMaxLineSize) != nullptr) {
    std::istringstream stream(line);
    stream.imbue(std::locale::classic());
    stream >> unichar >> NumProtos;
    if (stream.fail()) {
      continue;
    }
    if (unicharset_.contains_unichar(unichar)) {
      unichar_id = unicharset_.unichar_to_id(unichar);
      Protos = NormProtos->Protos[unichar_id];
      for (int i = 0; i < NumProtos; i++) {
        Protos = push_last(Protos, ReadPrototype(fp, NormProtos->NumParams));
      }
      NormProtos->Protos[unichar_id] = Protos;
    } else {
      tprintError("unichar {} in normproto file is not in unichar set.\n", unichar);
      for (int i = 0; i < NumProtos; i++) {
        FreePrototype(ReadPrototype(fp, NormProtos->NumParams));
      }
    }
  }
  return NormProtos;
} /* ReadNormProtos */

} // namespace tesseract

#endif
