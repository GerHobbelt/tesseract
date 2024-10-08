///////////////////////////////////////////////////////////////////////
// File:        paramsd.h
// Description: Tesseract parameter editor
// Author:      Joern Wanke
//
// (C) Copyright 2007, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////
//
// Tesseract parameter editor is used to edit all the parameters used
// within tesseract from the ui.
#ifndef TESSERACT_CCMAIN_PARAMSD_H_
#define TESSERACT_CCMAIN_PARAMSD_H_

#if !GRAPHICS_DISABLED

#  include "elst.h"       // for ELIST_ITERATOR, ELISTIZEH, ELIST_LINK
#  include "scrollview.h" // for ScrollView (ptr only), SVEvent (ptr only)
#  include <tesseract/params.h>     // for ParamType

namespace tesseract {

class SVMenuNode;
class Tesseract;

// A rather hackish helper structure which can take any kind of parameter input
// (defined by ParamType) and do a couple of common operations on them, like
// comparison or getting its value. It is used in the context of the
// ParamsEditor as a bridge from the internal tesseract parameters to the
// ones displayed by the ScrollView server.
class ParamContent : public ELIST_LINK {
public:
  // Compare two VC objects by their name.
  static int Compare(const void *v1, const void *v2);

  // Gets a VC object identified by its ID.
  static ParamContent *GetParamContentById(int id);

  // Constructors for the various ParamTypes.
  ParamContent() = default;
  ParamContent(tesseract::Param *it);

  // Getters and Setters.
  void SetValue(const char *val);
  std::string GetValue() const;
  const char *GetName() const;
  const char *GetDescription() const;

  int GetId() const {
    return my_id_;
  }
  bool HasChanged() const {
    return changed_;
  }

private:
  // The unique ID of this VC object.
  int my_id_;
  // Whether the parameter was changed_ and thus needs to be rewritten.
  bool changed_ = false;

  tesseract::Param *it_;
};

ELISTIZEH(ParamContent);

// The parameters editor enables the user to edit all the parameters used within
// tesseract. It can be invoked on its own, but is supposed to be invoked by
// the program editor.
class ParamsEditor : public SVEventHandler {
public:
  // Integrate the parameters editor as popupmenu into the existing scrollview
  // window (usually the pg editor). If sv == null, create a new empty
  // empty window and attach the parameter editor to that window (ugly).
  explicit ParamsEditor(tesseract::Tesseract *tess, ScrollViewReference &sv);

  // Event listener. Waits for SVET_POPUP events and processes them.
  void Notify(const SVEvent *sve) override;

private:
  // Find all editable parameters used within tesseract and create a
  // SVMenuNode tree from it.
  SVMenuNode *BuildListOfAllLeaves(tesseract::Tesseract *tess);

  // Write all (changed_) parameters to a config file.
  void WriteParams(char *filename, bool changes_only);

  ScrollViewReference sv_window_;
};

} // namespace tesseract

#endif // !GRAPHICS_DISABLED
#endif // TESSERACT_CCMAIN_PARAMSD_H_
