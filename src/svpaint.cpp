// Copyright 2007 Google Inc. All Rights Reserved.
//
// Author: Joern Wanke
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
// Simple drawing program to illustrate ScrollView capabilities.
//
// Functionality:
// - The menubar is used to select from different sample styles of input.
// - With the RMB it is possible to change the RGB values in different
//   popup menus.
// - A LMB click either draws point-to-point, point or text.
// - A LMB dragging either draws a line, a rectangle or ellipse.

// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.

#  include <cstdlib>
#  include <iostream>
#include "mupdf/fitz/string-util.h"

#if !GRAPHICS_DISABLED
#  include "scrollview.h"
#  include "svmnode.h"

namespace tesseract {

// The current color values we use, initially white (== Diagnostics::WHITE).
static int rgb[3] = {255, 255, 255};

class SVPaint : public SVEventHandler {
public:
  explicit SVPaint(Tesseract *tess, const char *server_name);
  // This is the main event handling function that we need to overwrite, defined
  // in SVEventHandler.
  void Notify(const SVEvent *sv_event) override;

private:
  // The Handler take care of the SVET_POPUP, SVET_MENU, SVET_CLICK and
  // SVET_SELECTION events.
  void PopupHandler(const SVEvent *sv_event);
  void MenuBarHandler(const SVEvent *sv_event);
  void ClickHandler(const SVEvent *sv_event);
  void SelectionHandler(const SVEvent *sv_event);

  // Convenience functions to build little menus.
  SVMenuNode *BuildPopupMenu();
  SVMenuNode *BuildMenuBar();

  // Our window.
  ScrollViewReference window_;

  // The mode we are in when an SVET_CLICK or an SVET_SELECTION event occurs.
  int click_mode_;
  int drag_mode_;

  // In the point-to-point drawing mode, we need to set a start-point the first
  // time we call it (e.g. call SetCursor).
  bool has_start_point_;
};

// Build a sample popup menu.
SVMenuNode *SVPaint::BuildPopupMenu() {
  auto *root = new SVMenuNode(); // Empty root node
  // Initial color is white, so we  all values to 255.
  root->AddChild("R",                 // Shown caption.
                 1,                   // assoc. command_id.
                 "255",               // initial value.
                 "Red Color Value?"); // Shown description.
  root->AddChild("G", 2, "255", "Green Color Value?");
  root->AddChild("B", 3, "255", "Blue Color Value?");
  return root;
}

// Build a sample menu bar.
SVMenuNode *SVPaint::BuildMenuBar() {
  auto *root = new SVMenuNode(); // Empty root node

  // Create some submenus and add them to the root.
  SVMenuNode *click = root->AddChild("Clicking");
  SVMenuNode *drag = root->AddChild("Dragging");

  // Put some nodes into the submenus.
  click->AddChild("Point to Point Drawing", // Caption.
                  1);                       // command_id.
  click->AddChild("Point Drawing", 2);
  click->AddChild("Text Drawing", 3);
  drag->AddChild("Line Drawing", 4);
  drag->AddChild("Rectangle Drawing", 5);
  drag->AddChild("Ellipse Drawing", 6);
  return root;
}

// Takes care of the SVET_POPUP events.
// In our case, SVET_POPUP is used to set RGB values.
void SVPaint::PopupHandler(const SVEvent *sv_event) {
  // Since we only have the RGB values as popup items,
  // we take a shortcut to not bloat up code:
  rgb[sv_event->command_id - 1] = atoi(sv_event->parameter);
  window_->Pen(rgb[0], rgb[1], rgb[2]);
}

// Takes care of the SVET_MENU events.
// In our case, we change either the click_mode_ (commands 1-3)
// or the drag_mode_ (commands 4-6).
void SVPaint::MenuBarHandler(const SVEvent *sv_event) {
  if ((sv_event->command_id > 0) && (sv_event->command_id < 4)) {
    click_mode_ = sv_event->command_id;
    has_start_point_ = false;
  } else {
    drag_mode_ = sv_event->command_id;
  }
}

// Takes care of the SVET_CLICK events.
// Depending on the click_mode_ we are in, either do Point-to-Point drawing,
// point drawing, or draw text.
void SVPaint::ClickHandler(const SVEvent *sv_event) {
  switch (click_mode_) {
    case 1: // Point to Point
      if (has_start_point_) {
        window_->DrawTo(sv_event->x, sv_event->y);
      } else {
        has_start_point_ = true;
        window_->SetCursor(sv_event->x, sv_event->y);
      }
      break;
    case 2: // Point Drawing..simulated by drawing a 1 pixel line.
      window_->Line(sv_event->x, sv_event->y, sv_event->x, sv_event->y);
      break;
    case 3: // Text
      // We show a modal input dialog on our window, then draw the input and
      // finally delete the input pointer.
      char *p = window_->ShowInputDialog("Text:");
      window_->Text(sv_event->x, sv_event->y, p);
      delete[] p;
      break;
  }
}

// Takes care of the SVET_SELECTION events.
// Depending on the drag_mode_ we are in, either draw a line, a rectangle or
// an ellipse.
void SVPaint::SelectionHandler(const SVEvent *sv_event) {
  switch (drag_mode_) {
      // FIXME inversed x_size, y_size
    case 4: // Line
      window_->Line(sv_event->x, sv_event->y, sv_event->x - sv_event->x_size,
                    sv_event->y - sv_event->y_size);
      break;
    case 5: // Rectangle
      window_->Rectangle(sv_event->x, sv_event->y, sv_event->x - sv_event->x_size,
                         sv_event->y - sv_event->y_size);
      break;
    case 6: // Ellipse
      window_->Ellipse(sv_event->x - sv_event->x_size, sv_event->y - sv_event->y_size,
                       sv_event->x_size, sv_event->y_size);
      break;
  }
}

// The event handling function from ScrollView which we have to overwrite.
// We handle CLICK, SELECTION, MENU and POPUP and throw away all other events.
void SVPaint::Notify(const SVEvent *sv_event) {
  if (sv_event->type == SVET_CLICK) {
    ClickHandler(sv_event);
  } else if (sv_event->type == SVET_SELECTION) {
    SelectionHandler(sv_event);
  } else if (sv_event->type == SVET_MENU) {
    MenuBarHandler(sv_event);
  } else if (sv_event->type == SVET_POPUP) {
    PopupHandler(sv_event);
  }
  // throw other events away
}

// Builds a new window, initializes the variables and event handler and builds
// the menu.
SVPaint::SVPaint(Tesseract *tess, const char *server_name) {
  window_ = ScrollViewManager::MakeScrollView(tess,
                           "ScrollView Paint Example", // window caption
                           0, 0,                       // x,y window position
                           500, 500,                   // window size
                           500, 500,                   // canvas size
                           false,                      // whether the Y axis is inversed.
                                                       // this is included due to legacy
                                                       // reasons for tesseract and enables
                                                       // us to have (0,0) as the LOWER left
                                                       // of the coordinate system.
                           server_name);               // the server address.

  // Set the start modes to point-to-point and line drawing.
  click_mode_ = 1;
  drag_mode_ = 4;
  has_start_point_ = false;

  if (window_->HasInteractiveFeature()) {
    // Build our menus and add them to the window. The flag illustrates whether
    // this is a menu bar.
    SVMenuNode *popup_menu = BuildPopupMenu();
    popup_menu->BuildMenu(window_, false);

    SVMenuNode *bar_menu = BuildMenuBar();
    bar_menu->BuildMenu(window_, true);
  }

  // Set the initial color values to White (could also be done by
  // passing (rgb[0], rgb[1], rgb[2]).
  window_->Pen(Diagnostics::WHITE);
  window_->Brush(Diagnostics::WHITE);

  // Adds the event handler to the window. This actually ensures that Notify
  // gets called when events occur.
  window_->AddEventHandler(this);

  // Set the window visible (calling this is important to actually render
  // everything. Without this call, the window would also be drawn, but the
  // menu bars would be missing.
  window_->SetVisible(true);

  // Rest this thread until its window is destroyed.
  // Note that a special event handling thread was created when constructing
  // the window. Due to this, the application will not deadlock here.
  window_->AwaitEvent(SVET_DESTROY);
  // We now have 3 Threads running:
  // (1) The MessageReceiver thread which fetches messages and distributes them
  // (2) The EventHandler thread which handles all events for window_
  // (3) The main thread which waits on window_ for a DESTROY event (blocked)
}

} // namespace tesseract

// If a parameter is given, we try to connect to the given server.
// This enables us to test the remote capabilities of ScrollView.


#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char **argv)
#else
extern "C" int tesseract_svpaint_main(int argc, const char **argv)
#endif
{
  const char *server_name;
  if (argc > 1) {
    server_name = argv[1];
  } else {
    server_name = "localhost";
  }
  tesseract::SVPaint svp(nullptr, server_name);
  return EXIT_SUCCESS;
}

#else

#if defined(TESSERACT_STANDALONE) && !defined(BUILD_MONOLITHIC)
extern "C" int main(int argc, const char** argv)
#else
extern "C" int tesseract_svpaint_main(int argc, const char** argv)
#endif
{
  fprintf(stderr, "%s: this tool is not supported in this build.\n", fz_basename(argv[0]));
  return EXIT_FAILURE;
}

#endif // !GRAPHICS_DISABLED
