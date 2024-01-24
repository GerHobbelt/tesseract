///////////////////////////////////////////////////////////////////////
// File:        paramsd.cpp
// Description: Tesseract parameter Editor
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
// The parameters editor is used to edit all the parameters used within
// tesseract from the ui.

// Include automatically generated configuration file if running autoconf.
#ifdef HAVE_TESSERACT_CONFIG_H
#  include "config_auto.h"
#endif

#if !GRAPHICS_DISABLED

#include <tesseract/debugheap.h>

#  include "params.h" // for ParamsVectorSet, StringParam, BoolParam
#  include "paramsd.h"
#  include "scrollview.h"     // for SVEvent, ScrollView, SVET_POPUP
#  include "svmnode.h"        // for SVMenuNode
#  include "tesseractclass.h" // for Tesseract

#  include <cstdio>  // for fclose, fopen, fprintf, FILE
#  include <cstdlib> // for atoi
#  include <cstring> // for strcmp, strcspn, strlen, strncpy
#  include <locale>  // for std::locale::classic
#  include <map>     // for map, _Rb_tree_iterator, map<>::iterator
#  include <memory>  // for unique_ptr
#  include <sstream> // for std::stringstream
#  include <utility> // for pair


namespace tesseract {

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

#  define VARDIR "configs/" /*parameters files */
#  define MAX_ITEMS_IN_SUBMENU 30

// The following variables should remain static globals, since they
// are used by debug editor, which uses a single Tesseract instance.
//
// Contains the mappings from unique VC ids to their actual pointers.
static std::map<int, ParamContent *> vcMap;
static int nrParams = 0;
static int writeCommands[2];

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

// Constructors for the various ParamTypes.
ParamContent::ParamContent(tesseract::Param *it) {
  ASSERT0(it != nullptr);
  my_id_ = nrParams;
  nrParams++;
  it_ = it;
  vcMap[my_id_] = this;
}

// Gets a VC object identified by its ID.
ParamContent *ParamContent::GetParamContentById(int id) {
  return vcMap[id];
}

// Copy the first N words from the source string to the target string.
// Words are delimited by "_".
static void GetFirstWords(const char *s, // source string
                          int n,         // number of words
                          std::string &d // target string
) {
  int full_length = strlen(s);
  int reqd_len = 0; // No. of chars required
  const char *next_word = s;

  while ((n > 0) && reqd_len < full_length) {
    reqd_len += strcspn(next_word, "_") + 1;
    next_word = s + reqd_len;
    n--;
  }
  std::string rv(s, reqd_len);  // don't copy beyond s[reqd_len]
  d = std::move(rv);
}

// Getter for the name.
const char *ParamContent::GetName() const {
  return it_->name_str();
}

// Getter for the description.
const char *ParamContent::GetDescription() const {
  return it_->info_str();
}

// Getter for the value.
std::string ParamContent::GetValue() const {
  return it_->raw_value_str();
}

// Setter for the value.
void ParamContent::SetValue(const char *val) {
  // TODO (wanke) Test if the values actually are properly converted.
  // (Quickly visible impacts?)
  changed_ = true;
  it_->set_value(val);
}

// Gets the up to the first 3 prefixes from s (split by _).
// For example, tesseract_foo_bar will be split into tesseract, foo and bar.
static void GetPrefixes(const char *s, std::string &level_one, std::string &level_two,
                        std::string &level_three) {
  GetFirstWords(s, 1, level_one);
  GetFirstWords(s, 2, level_two);
  GetFirstWords(s, 3, level_three);
}

// Compare two VC objects by their name.
int ParamContent::Compare(const void *v1, const void *v2) {
  const ParamContent *one = *static_cast<const ParamContent *const *>(v1);
  const ParamContent *two = *static_cast<const ParamContent *const *>(v2);
  return strcmp(one->GetName(), two->GetName());
}

// Find all editable parameters used within tesseract and create a
// SVMenuNode tree from it.
// 
// TODO (wanke): This is actually sort of hackish.
SVMenuNode *ParamsEditor::BuildListOfAllLeaves(tesseract::Tesseract *tess) {
  auto *mr = new SVMenuNode();
  ParamContent_LIST vclist;
  ParamContent_IT vc_it(&vclist);
  // Amount counts the number of entries for a specific char*.
  // 
  // TODO(rays) get rid of the use of std::map.
  std::map<const char *, int> amount;

  // Add all parameters to a list.
  tesseract::ParamsVectorSet vec({ &GlobalParams(), &tess->params() });
  for (auto &param : vec.as_list()) {
    vc_it.add_after_then_move(new ParamContent(param));
  }

  // Count the # of entries starting with a specific prefix.
  for (vc_it.mark_cycle_pt(); !vc_it.cycled_list(); vc_it.forward()) {
    ParamContent *vc = vc_it.data();
    std::string tag;
    std::string tag2;
    std::string tag3;

    GetPrefixes(vc->GetName(), tag, tag2, tag3);
    amount[tag.c_str()]++;
    amount[tag2.c_str()]++;
    amount[tag3.c_str()]++;
  }

  vclist.sort(ParamContent::Compare); // Sort the list alphabetically.

  SVMenuNode *other = mr->AddChild("OTHER");

  // go through the list again and this time create the menu structure.
  vc_it.move_to_first();
  for (vc_it.mark_cycle_pt(); !vc_it.cycled_list(); vc_it.forward()) {
    ParamContent *vc = vc_it.data();
    std::string tag;
    std::string tag2;
    std::string tag3;
    GetPrefixes(vc->GetName(), tag, tag2, tag3);

    if (amount[tag.c_str()] == 1) {
      other->AddChild(vc->GetName(), vc->GetId(), vc->GetValue().c_str(), vc->GetDescription());
    } else { // More than one would use this submenu -> create submenu.
      SVMenuNode *sv = mr->AddChild(tag.c_str());
      if ((amount[tag.c_str()] <= MAX_ITEMS_IN_SUBMENU) || (amount[tag2.c_str()] <= 1)) {
        sv->AddChild(vc->GetName(), vc->GetId(), vc->GetValue().c_str(), vc->GetDescription());
      } else { // Make subsubmenus.
        SVMenuNode *sv2 = sv->AddChild(tag2.c_str());
        sv2->AddChild(vc->GetName(), vc->GetId(), vc->GetValue().c_str(), vc->GetDescription());
      }
    }
  }
  return mr;
}

// Event listener. Waits for SVET_POPUP events and processes them.
void ParamsEditor::Notify(const SVEvent *sve) {
  if (sve->type == SVET_POPUP) { // only catch SVET_POPUP!
    char *param = sve->parameter;
    if (sve->command_id == writeCommands[0]) {
      WriteParams(param, false);
    } else if (sve->command_id == writeCommands[1]) {
      WriteParams(param, true);
    } else {
      ParamContent *vc = ParamContent::GetParamContentById(sve->command_id);
      vc->SetValue(param);
      sv_window_->AddMessage("Setting {} to {}", vc->GetName(), vc->GetValue());
    }
  }
}

// Integrate the parameters editor as popupmenu into the existing scrollview
// window (usually the pg editor). If sv == null, create a new empty
// empty window and attach the parameters editor to that window (ugly).
ParamsEditor::ParamsEditor(tesseract::Tesseract *tess, ScrollViewReference &sv) {
  if (!sv) {
    sv = ScrollViewManager::MakeScrollView(tess, "ParamEditorMAIN", 1, 1, 200, 200, 300, 200);
  }

  sv_window_ = sv;

  // Only one event handler per window.
  // sv->AddEventHandler((SVEventHandler*) this);

  if (sv->HasInteractiveFeature()) {
    SVMenuNode *svMenuRoot = BuildListOfAllLeaves(tess);

    std::string paramfile;
    paramfile = tess->datadir;
    paramfile += VARDIR;   // parameters dir
    paramfile += "edited"; // actual name

    SVMenuNode *std_menu = svMenuRoot->AddChild("Build Config File");

    writeCommands[0] = nrParams + 1;
    std_menu->AddChild("All Parameters", writeCommands[0], paramfile.c_str(), "Config file name?");

    writeCommands[1] = nrParams + 2;
    std_menu->AddChild("changed_ Parameters Only", writeCommands[1], paramfile.c_str(),
                       "Config file name?");

    svMenuRoot->BuildMenu(sv, false);

    delete svMenuRoot;
  }
}

// Write all (changed_) parameters to a config file.
void ParamsEditor::WriteParams(char *filename, bool changes_only) {
  FILE *fp; // input file
  // if file exists
  if ((fp = fopen(filename, "rb")) != nullptr) {
    fclose(fp);
    std::stringstream msg;
    msg << "Overwrite file " << filename << "? (Y/N)";
    int a = sv_window_->ShowYesNoDialog(msg.str().c_str());
    if (a == 'n') {
      return;
    } // don't write
  }

  fp = fopen(filename, "wb"); // can we write to it?
  if (fp == nullptr) {
    sv_window_->AddMessage("Can't write to file {}", filename);
    return;
  }
  for (auto &iter : vcMap) {
    ParamContent *cur = iter.second;
    if (!changes_only || cur->HasChanged()) {
      fprintf(fp, "%-25s   %-12s   # %s\n", cur->GetName(), cur->GetValue().c_str(),
              cur->GetDescription());
    }
  }
  fclose(fp);
}

} // namespace tesseract

#endif // !GRAPHICS_DISABLED
