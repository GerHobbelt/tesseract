///////////////////////////////////////////////////////////////////////
// File:        scrollview.cpp
// Description: ScrollView
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

// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.

#include "scrollview.h"
#include "bbgrid.h"
#include "tesseractclass.h"
#include "global_params.h"

#include "svutil.h" // for SVNetwork

#include <leptonica/allheaders.h>

#include "drawtord.h"

#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <map>
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <string>
#include <thread> // for std::thread
#include <utility>
#include <vector>


namespace tesseract {

FZ_HEAPDBG_TRACKER_SECTION_START_MARKER(_)

const int kSvPort = 8461;
const int kMaxMsgSize = 4096;
const int kMaxIntPairSize = 45; // Holds %d,%d, for up to 64 bit.

struct SVPolyLineBuffer {
  bool empty; // Independent indicator to allow SendMsg to call SendPolygon.
  std::vector<int> xcoords;
  std::vector<int> ycoords;
};

// A map between the window IDs and their corresponding pointers.
static std::vector<ScrollViewReference> svmap;
static std::mutex *svmap_mu = nullptr;       // lock managed by the ScrollViewReference class instances + ScrollViewManager factory

// A map of all semaphores waiting for a specific event on a specific window.
static std::map<std::pair<ScrollViewReference, SVEventType>,
                std::pair<SVSemaphore *, std::unique_ptr<SVEvent>>>
    waiting_for_events;
static std::mutex *waiting_for_events_mu;

FZ_HEAPDBG_TRACKER_SECTION_END_MARKER(_)

std::unique_ptr<SVEvent> SVEvent::copy() const {
  auto any = std::unique_ptr<SVEvent>(new SVEvent);
  any->command_id = command_id;
  any->counter = counter;
  any->parameter = new char[strlen(parameter) + 1];
  strcpy(any->parameter, parameter);
  any->type = type;
  any->x = x;
  any->y = y;
  any->x_size = x_size;
  any->y_size = y_size;
  any->window = window;
  return any;
}

// Destructor.
// It is defined here, so the compiler can create a single vtable
// instead of weak vtables in every compilation unit.
SVEventHandler::~SVEventHandler() = default;

#if !GRAPHICS_DISABLED

/// This is the main loop which handles the ScrollView-logic from the server
/// to the client. It basically loops through messages, parses them to events
/// and distributes it to the waiting handlers.
/// It is run from a different thread and synchronizes via SVSync.
void InteractiveScrollView::MessageReceiver() {
  int counter_event_id = 0; // ongoing counter
  char *message = nullptr;
  // Wait until a new message appears in the input stream_.
  do {
    message = InteractiveScrollView::GetStream()->Receive();
  } while (message == nullptr);

  // This is the main loop which iterates until the server is dead (strlen =
  // -1). It basically parses for 3 different messagetypes and then distributes
  // the events accordingly.
  while (true) {
    // The new event we create.
    std::unique_ptr<SVEvent> cur(new SVEvent);
    // The ID of the corresponding window.
    int window_id;

    int ev_type;

    int n;
    // Fill the new SVEvent properly.
    sscanf(message, "%d,%d,%d,%d,%d,%d,%d,%n", &window_id, &ev_type, &cur->x,
           &cur->y, &cur->x_size, &cur->y_size, &cur->command_id, &n);
    char *p = (message + n);

    {
      std::lock_guard<std::mutex> guard(*svmap_mu);
      cur->window = svmap[window_id];
    }

    if (cur->window) {
      auto length = strlen(p);
      cur->parameter = new char[length + 1];
      strcpy(cur->parameter, p);
      if (length > 0) { // remove the last \n
        cur->parameter[length - 1] = '\0';
      }
      cur->type = static_cast<SVEventType>(ev_type);
      // Correct selection coordinates so x,y is the min pt and size is +ve.
      if (cur->x_size > 0) {
        cur->x -= cur->x_size;
      } else {
        cur->x_size = -cur->x_size;
      }
      if (cur->y_size > 0) {
        cur->y -= cur->y_size;
      } else {
        cur->y_size = -cur->y_size;
      }
      // Returned y will be the bottom-left if y is reversed.
      if (cur->window->is_y_axis_reversed()) {
        cur->y = cur->window->TranslateYCoordinate(cur->y + cur->y_size);
      }
      cur->counter = counter_event_id;
      // Increase by 2 since we will also create an SVET_ANY event from cur,
      // which will have a counter_id of cur + 1 (and thus gets processed
      // after cur).
      counter_event_id += 2;

      // In case of an SVET_EXIT event, quit the whole application.
      if (ev_type == SVET_EXIT) {
        SendRawMessage("svmain:exit()");
        break;
      }

      // Place two copies of it in the table for the window.
      cur->window->SetEvent(cur.get());

      // Check if any of the threads currently waiting want it.
      std::pair<ScrollViewReference, SVEventType> awaiting_list(cur->window,
                                                                cur->type);
      std::pair<ScrollViewReference, SVEventType> awaiting_list_any(cur->window,
                                                                    SVET_ANY);
      std::pair<ScrollViewReference, SVEventType> awaiting_list_any_window(
          (ScrollViewReference) nullptr, SVET_ANY);
      waiting_for_events_mu->lock();
      if (waiting_for_events.count(awaiting_list) > 0) {
        waiting_for_events[awaiting_list].second = std::move(cur);
        waiting_for_events[awaiting_list].first->Signal();
      } else if (waiting_for_events.count(awaiting_list_any) > 0) {
        waiting_for_events[awaiting_list_any].second = std::move(cur);
        waiting_for_events[awaiting_list_any].first->Signal();
      } else if (waiting_for_events.count(awaiting_list_any_window) > 0) {
        waiting_for_events[awaiting_list_any_window].second = std::move(cur);
        waiting_for_events[awaiting_list_any_window].first->Signal();
      }
      waiting_for_events_mu->unlock();
      // Signal the corresponding semaphore twice (for both copies).
      cur->window->Signal();
      cur->window->Signal();
    }

    // Wait until a new message appears in the input stream_.
    do {
      message = InteractiveScrollView::GetStream()->Receive();
    } while (message == nullptr);
  }
}

// Table to implement the color index values in the old system.
static const uint8_t table_colors[Diagnostics::GREEN_YELLOW + 1][4] = {
    {0, 0, 0, 0},         // NONE (transparent)
    {0, 0, 0, 255},       // BLACK.
    {255, 255, 255, 255}, // WHITE.
    {255, 0, 0, 255},     // RED.
    {255, 255, 0, 255},   // YELLOW.
    {0, 255, 0, 255},     // GREEN.
    {0, 255, 255, 255},   // CYAN.
    {0, 0, 255, 255},     // BLUE.
    {255, 0, 255, 255},   // MAGENTA.
    {0, 128, 255, 255},   // AQUAMARINE.
    {0, 0, 64, 255},      // DARK_SLATE_BLUE.
    {128, 128, 255, 255}, // LIGHT_BLUE.
    {64, 64, 255, 255},   // MEDIUM_BLUE.
    {0, 0, 32, 255},      // MIDNIGHT_BLUE.
    {0, 0, 128, 255},     // NAVY_BLUE.
    {192, 192, 255, 255}, // SKY_BLUE.
    {64, 64, 128, 255},   // SLATE_BLUE.
    {32, 32, 64, 255},    // STEEL_BLUE.
    {255, 128, 128, 255}, // CORAL.
    {128, 64, 0, 255},    // BROWN.
    {128, 128, 0, 255},   // SANDY_BROWN.
    {192, 192, 0, 255},   // GOLD.
    {192, 192, 128, 255}, // GOLDENROD.
    {0, 64, 0, 255},      // DARK_GREEN.
    {32, 64, 0, 255},     // DARK_OLIVE_GREEN.
    {64, 128, 0, 255},    // FOREST_GREEN.
    {128, 255, 0, 255},   // LIME_GREEN.
    {192, 255, 192, 255}, // PALE_GREEN.
    {192, 255, 0, 255},   // YELLOW_GREEN.
    {192, 192, 192, 255}, // LIGHT_GREY.
    {64, 64, 128, 255},   // DARK_SLATE_GREY.
    {64, 64, 64, 255},    // DIM_GREY.
    {128, 128, 128, 255}, // GREY.
    {64, 192, 0, 255},    // KHAKI.
    {255, 0, 192, 255},   // MAROON.
    {255, 128, 0, 255},   // ORANGE.
    {255, 128, 64, 255},  // ORCHID.
    {255, 192, 192, 255}, // PINK.
    {128, 0, 128, 255},   // PLUM.
    {255, 0, 64, 255},    // INDIAN_RED.
    {255, 64, 0, 255},    // ORANGE_RED.
    {255, 0, 192, 255},   // VIOLET_RED.
    {255, 192, 128, 255}, // SALMON.
    {128, 128, 0, 255},   // TAN.
    {0, 255, 255, 255},   // TURQUOISE.
    {0, 128, 128, 255},   // DARK_TURQUOISE.
    {192, 0, 255, 255},   // VIOLET.
    {128, 128, 0, 255},   // WHEAT.
    {128, 255, 0, 255}    // GREEN_YELLOW
};

/*******************************************************************************
 * Scrollview implementation.
 *******************************************************************************/

SVNetwork *InteractiveScrollView::stream_ = nullptr;
int ScrollView::nr_created_windows_ = 0;
int ScrollView::image_index_ = 0;

/// Calls Initialize with all arguments given.
ScrollView::ScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos,
                       int x_size, int y_size, int x_canvas_size,
                       int y_canvas_size, bool y_axis_reversed,
                       const char *server_name) {
  Initialize(tess, name, x_pos, y_pos, x_size, y_size, x_canvas_size,
             y_canvas_size, y_axis_reversed, server_name);
}

/// Sets up a ScrollView window, depending on the constructor variables.
void ScrollView::Initialize(Tesseract *tess, const char *name, int x_pos,
                            int y_pos, int x_size, int y_size,
                            int x_canvas_size, int y_canvas_size,
                            bool y_axis_reversed, const char *server_name) {
  tesseract_ = tess;
  ref_of_ref_ = nullptr;

  ASSERT0(svmap_mu != nullptr);
#if 0
  if (!svmap_mu) {
    svmap_mu = new std::mutex();
  }
#endif

  // Set up the variables on the clientside.
  y_axis_is_reversed_ = y_axis_reversed;
  y_size_ = y_canvas_size;
  window_name_ = name;

  window_id_ = nr_created_windows_;
  nr_created_windows_++;

  // Set up polygon buffering.
  points_ = new SVPolyLineBuffer;
  points_->empty = true;

#if 0
  svmap_mu->lock();
  svmap.insert_or_assign(GetId(), this);
  svmap_mu->unlock();
#endif
}


void ScrollView::ExitHelper() {
  tprintDebug("Nuking ScrollView #{}.\n", GetId());

  if (ref_of_ref_) {
    *ref_of_ref_ = nullptr;
  }
}

void ScrollView::RegisterGlobalRefToMe(ScrollViewReference* ref2ref) {
  ref_of_ref_ = ref2ref;
}

/// Calls Initialize with all arguments given.
InteractiveScrollView::InteractiveScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos,
                       int x_size, int y_size, int x_canvas_size,
                       int y_canvas_size, bool y_axis_reversed,
                       const char *server_name) :
    ScrollView(tess, name, x_pos, y_pos, x_size, y_size, x_canvas_size, y_canvas_size, y_axis_reversed, server_name) 
{
  Initialize(tess, name, x_pos, y_pos, x_size, y_size, x_canvas_size,
             y_canvas_size, y_axis_reversed, server_name);
}

/// Sets up a ScrollView window, depending on the constructor variables.
void InteractiveScrollView::Initialize(Tesseract *tess, const char *name,
                                       int x_pos, int y_pos, int x_size,
                                       int y_size, int x_canvas_size,
                                       int y_canvas_size, bool y_axis_reversed,
                                       const char *server_name) {
  waiting_for_events_mu = new std::mutex();

  event_handler_ = nullptr;
  event_handler_ended_ = false;

  for (auto &i : event_table_) {
    i = nullptr;
  }

  semaphore_ = new SVSemaphore();

  // If this is the first ScrollView Window which gets created, there is no
  // network connection yet and we have to set it up in a different thread.
  if (stream_ == nullptr) {
    //nr_created_windows_ = 0;
    stream_ = new SVNetwork(server_name, kSvPort);
    SendRawMessage(
        "svmain = luajava.bindClass('com.google.scrollview.ScrollView')\n");
    std::thread t(&InteractiveScrollView::MessageReceiver);
    t.detach();
  }

  // Set up the variables on the clientside.

  // Set up an actual Window on the client side.
  char message[kMaxMsgSize];
  snprintf(message, sizeof(message),
           "w%u = luajava.newInstance('com.google.scrollview.ui"
           ".SVWindow','%s',%u,%u,%u,%u,%u,%u,%u)\n",
           window_id_, window_name_, window_id_, x_pos, y_pos, x_size, y_size,
           x_canvas_size, y_canvas_size);
  SendRawMessage(message);

  std::thread t(&InteractiveScrollView::StartEventHandler, this);
  t.detach();
}

/// Sits and waits for events on this window.
void InteractiveScrollView::StartEventHandler() {
  for (;;) {
    stream_->Flush();
    semaphore_->Wait();
    int serial = -1;
    int k = -1;
    mutex_.lock();
    // Check every table entry if it is valid and not already processed.

    for (int i = 0; i < SVET_COUNT; i++) {
      if (event_table_[i] != nullptr &&
          (serial < 0 || event_table_[i]->counter < serial)) {
        serial = event_table_[i]->counter;
        k = i;
      }
    }
    // If we didn't find anything we had an old alarm and just sleep again.
    if (k != -1) {
      auto new_event = std::move(event_table_[k]);
      mutex_.unlock();
      if (event_handler_ != nullptr) {
        event_handler_->Notify(new_event.get());
      }
      if (new_event->type == SVET_DESTROY) {
        // Signal the destructor that it is safe to terminate.
        event_handler_ended_ = true;
        return;
      }
    } else {
      mutex_.unlock();
    }
    // The thread should run as long as its associated window is alive.
  }
}

#endif // !GRAPHICS_DISABLED

ScrollView::~ScrollView() {
#if !GRAPHICS_DISABLED
#if 0    // Cannot invoke virtual method any more as the derived instane has already finished delete-ing by now: we're delete-ing the base class right now! This is taken care of instead by calling Update() from the ScrollViewReference BEFORE invoking the destructor of the derived class!
  Update();
#endif

#if !defined(NO_ASSERTIONS)
  auto &ref = svmap[GetId()];
  ASSERT0(ref.GetRef() == nullptr);
#endif

  delete points_;
#endif // !GRAPHICS_DISABLED
}

InteractiveScrollView::~InteractiveScrollView() {
#if !GRAPHICS_DISABLED
  //svmap_mu->lock();
  if (semaphore_ /* !exit_handler_has_been_invoked */) {
    //svmap_mu->unlock();

    // So the event handling thread can quit.
    SendMsg("destroy()");

    AwaitEvent(SVET_DESTROY);

    // The event handler thread for this window *must* receive the
    // destroy event and set its pointer to this to nullptr before we allow
    // the destructor to exit.
    while (!event_handler_ended_) {
      Update();
    }
  }

  delete semaphore_;
  semaphore_ = nullptr;
#endif // !GRAPHICS_DISABLED
}

#if !GRAPHICS_DISABLED
/// Send a message to the server, attaching the window id.
void InteractiveScrollView::vSendMsg(fmt::string_view format,
                                     fmt::format_args args) {
  auto message = fmt::vformat(format, args);

  if (!points_->empty) {
    SendPolygon();
  }

  char winidstr[kMaxIntPairSize];
  snprintf(winidstr, kMaxIntPairSize, "w%u:", window_id_);
  std::string form(winidstr);
  form += message;
  stream_->Send(form.c_str());
}

/// Send a message to the server without a
/// window id. Used for global events like exit().
void InteractiveScrollView::SendRawMessage(const char *msg) {
  stream_->Send(msg);
}

/// Add an Event Listener to this ScrollView Window
void InteractiveScrollView::AddEventHandler(SVEventHandler *listener) {
  event_handler_ = listener;
}

void InteractiveScrollView::Signal() {
  semaphore_->Signal();
}

void InteractiveScrollView::SetEvent(const SVEvent *svevent) {
  // Copy event
  auto any = svevent->copy();
  auto specific = svevent->copy();
  any->counter = specific->counter + 1;

  // Place both events into the queue.
  std::lock_guard<std::mutex> guard(mutex_);

  event_table_[specific->type] = std::move(specific);
  event_table_[SVET_ANY] = std::move(any);
}

/// Block until an event of the given type is received.
/// Note: The calling function is responsible for deleting the returned
/// SVEvent afterwards!
std::unique_ptr<SVEvent> InteractiveScrollView::AwaitEvent(SVEventType type) {
  // Initialize the waiting semaphore.
  auto *sem = new SVSemaphore();
  std::pair<ScrollViewReference, SVEventType> ea(this, type);
  waiting_for_events_mu->lock();
  waiting_for_events[ea] = {sem, nullptr};
  waiting_for_events_mu->unlock();
  // Wait on it, but first flush.
  stream_->Flush();
  sem->Wait();
  // Process the event we got woken up for (it's in waiting_for_events pair).
  waiting_for_events_mu->lock();
  auto ret = std::move(waiting_for_events[ea].second);
  waiting_for_events.erase(ea);
  delete sem;
  waiting_for_events_mu->unlock();
  return ret;
}

// Send the current buffered polygon (if any) and clear it.
void InteractiveScrollView::SendPolygon() {
  if (!points_->empty) {
    points_->empty = true; // Allows us to use SendMsg.
    int length = points_->xcoords.size();
    // length == 1 corresponds to 2 SetCursors in a row and only the
    // last setCursor has any effect.
    if (length == 2) {
      // An isolated line!
      SendMsg("drawLine({},{},{},{})", points_->xcoords[0], points_->ycoords[0], points_->xcoords[1], points_->ycoords[1]);
    } else if (length > 2) {
      // A polyline.
      SendMsg("createPolyline({})", length);
      char coordpair[kMaxIntPairSize];
      std::string decimal_coords;
      for (int i = 0; i < length; ++i) {
        snprintf(coordpair, kMaxIntPairSize, "%d,%d,", points_->xcoords[i],
                 points_->ycoords[i]);
        decimal_coords += coordpair;
      }
      decimal_coords += '\n';
      SendRawMessage(decimal_coords.c_str());
      SendMsg("drawPolyline()");
    }
    points_->xcoords.clear();
    points_->ycoords.clear();
  }
}

/*******************************************************************************
 * LUA "API" functions.
 *******************************************************************************/

void InteractiveScrollView::Comment(std::string text) {
  SendPolygon();
  // NO-OP for ScrollView JAVA app
}

// Sets the position from which to draw to (x,y).
void InteractiveScrollView::SetCursor(int x, int y) {
  SendPolygon();
  DrawTo(x, y);
}

// Draws from the current position to (x,y) and sets the new position to it.
void InteractiveScrollView::DrawTo(int x, int y) {
  points_->xcoords.push_back(x);
  points_->ycoords.push_back(TranslateYCoordinate(y));
  points_->empty = false;
}

// Draw a line using the current pen color.
void InteractiveScrollView::Line(int x1, int y1, int x2, int y2) {
  if (!points_->xcoords.empty() && x1 == points_->xcoords.back() &&
      TranslateYCoordinate(y1) == points_->ycoords.back()) {
    // We are already at x1, y1, so just draw to x2, y2.
    DrawTo(x2, y2);
  } else if (!points_->xcoords.empty() && x2 == points_->xcoords.back() &&
             TranslateYCoordinate(y2) == points_->ycoords.back()) {
    // We are already at x2, y2, so just draw to x1, y1.
    DrawTo(x1, y1);
  } else {
    // This is a new line.
    SetCursor(x1, y1);
    DrawTo(x2, y2);
  }
}

// Set the visibility of the window.
void InteractiveScrollView::SetVisible(bool visible) {
  if (visible) {
    SendMsg("setVisible(true)");
  } else {
    SendMsg("setVisible(false)");
  }
}

// Set the alwaysOnTop flag.
void InteractiveScrollView::AlwaysOnTop(bool b) {
  if (b) {
    SendMsg("setAlwaysOnTop(true)");
  } else {
    SendMsg("setAlwaysOnTop(false)");
  }
}

// Adds a message entry to the message box.
void InteractiveScrollView::vAddMessage(fmt::string_view format,
                                        fmt::format_args args) {
  auto message = fmt::vformat(format, args);

  char winidstr[kMaxIntPairSize];
  snprintf(winidstr, kMaxIntPairSize, "w%u:", window_id_);
  std::string form(winidstr);
  form += message;

  auto s = AddEscapeChars(form, "'");
  SendMsg("addMessage(\"{}\")", s);
}

// Set a messagebox.
void InteractiveScrollView::AddMessageBox() {
  SendMsg("addMessageBox()");
}

// Exit the client completely (and notify the server of it).
void InteractiveScrollView::ExitHelper() {
  SendRawMessage("svmain:exit()");
}

// Clear the canvas.
void InteractiveScrollView::Clear() {
  SendMsg("clear()");
}

// Set the stroke width.
void InteractiveScrollView::Stroke(float width) {
  SendMsg("setStrokeWidth({})", width);
}

// Draw a rectangle using the current pen color.
// The rectangle is filled with the current brush color.
void InteractiveScrollView::Rectangle(int x1, int y1, int x2, int y2) {
  if (x1 == x2 && y1 == y2) {
    return; // Scrollviewer locks up.
  }
  SendMsg("drawRectangle({},{},{},{})", x1, TranslateYCoordinate(y1), x2,
          TranslateYCoordinate(y2));
}

// Draw an ellipse using the current pen color.
// The ellipse is filled with the current brush color.
void InteractiveScrollView::Ellipse(int x1, int y1, int width, int height) {
  SendMsg("drawEllipse({},{},{},{})", x1, TranslateYCoordinate(y1), width,
          height);
}

// Set the pen color to the given RGB values.
void InteractiveScrollView::Pen(int red, int green, int blue) {
  SendMsg("pen({},{},{})", red, green, blue);
}

// Set the pen color to the given RGB values.
void InteractiveScrollView::Pen(int red, int green, int blue, int alpha) {
  SendMsg("pen({},{},{},{})", red, green, blue, alpha);
}

// Set the brush color to the given RGB values.
void InteractiveScrollView::Brush(int red, int green, int blue) {
  SendMsg("brush({},{},{})", red, green, blue);
}

// Set the brush color to the given RGB values.
void InteractiveScrollView::Brush(int red, int green, int blue, int alpha) {
  SendMsg("brush({},{},{},{})", red, green, blue, alpha);
}

// Set the attributes for future Text(..) calls.
void InteractiveScrollView::TextAttributes(const char *font, int pixel_size,
                                           bool bold, bool italic,
                                           bool underlined) {
  const char *b;
  const char *i;
  const char *u;

  if (bold) {
    b = "true";
  } else {
    b = "false";
  }
  if (italic) {
    i = "true";
  } else {
    i = "false";
  }
  if (underlined) {
    u = "true";
  } else {
    u = "false";
  }
  SendMsg("textAttributes('{}',{},{},{},{})", font, pixel_size, b, i, u);
}

// Set up a X/Y offset for the subsequent drawing primitives.
void InteractiveScrollView::SetXYOffset(int x, int y) {
  // no-op
}

// Draw text at the given coordinates.
void InteractiveScrollView::Text(int x, int y, const char *mystring) {
  SendMsg("drawText({},{},'{}')", x, TranslateYCoordinate(y), mystring);
}

// Open and draw an image given a name at (x,y).
void InteractiveScrollView::Draw(const char *image, int x_pos, int y_pos) {
  SendMsg("openImage('{}')", image);
  SendMsg("drawImage('{}',{},{})", image, x_pos, TranslateYCoordinate(y_pos));
}

// Add new checkboxmenuentry to menubar.
void InteractiveScrollView::MenuItem(const char *parent, const char *name,
                                     int cmdEvent, bool flag) {
  if (parent == nullptr) {
    parent = "";
  }
  if (flag) {
    SendMsg("addMenuBarItem('{}','{}',{},true)", parent, name, cmdEvent);
  } else {
    SendMsg("addMenuBarItem('{}','{}',{},false)", parent, name, cmdEvent);
  }
}

// Add new menuentry to menubar.
void InteractiveScrollView::MenuItem(const char *parent, const char *name,
                                     int cmdEvent) {
  if (parent == nullptr) {
    parent = "";
  }
  SendMsg("addMenuBarItem('{}','{}',{})", parent, name, cmdEvent);
}

// Add new submenu to menubar.
void InteractiveScrollView::MenuItem(const char *parent, const char *name) {
  if (parent == nullptr) {
    parent = "";
  }
  SendMsg("addMenuBarItem('{}','{}')", parent, name);
}

// Add new submenu to popupmenu.
void InteractiveScrollView::PopupItem(const char *parent, const char *name) {
  if (parent == nullptr) {
    parent = "";
  }
  SendMsg("addPopupMenuItem('{}','{}')", parent, name);
}

// Add new submenuentry to popupmenu.
void InteractiveScrollView::PopupItem(const char *parent, const char *name,
                                      int cmdEvent, const char *value,
                                      const char *desc) {
  if (parent == nullptr) {
    parent = "";
  }
  auto esc = AddEscapeChars(value, "'");
  auto esc2 = AddEscapeChars(desc, "'");
  SendMsg("addPopupMenuItem('{}','{}',{},'{}','{}')", parent, name, cmdEvent, esc, esc2);
}

// Send an update message for a single window.
void InteractiveScrollView::UpdateWindow() {
  SendMsg("update()");
}

// Note: this is an update to all windows
void ScrollView::Update() {
  ASSERT0(svmap_mu != nullptr);
  std::vector<ScrollViewReference> worklist;
  // limit scope of lock
  {
      std::lock_guard<std::mutex> guard(*svmap_mu);
      for (auto &iter : svmap) {
        if (iter) {
          worklist.push_back(iter);
        }
      }
  }
  for (auto &iter : worklist) {
      iter->UpdateWindow();
  }
}

// 
void ScrollView::Exit() {
  ASSERT0(svmap_mu != nullptr);
  std::vector<ScrollViewReference> worklist;
  // limit scope of lock
  {
    std::lock_guard<std::mutex> guard(*svmap_mu);
    for (auto &iter : svmap) {
        if (iter) {
          worklist.push_back(iter);
        }
    }
  }
  for (auto &iter : worklist) {
    iter->ExitHelper();
  }

  exit(667);
}

// Set the pen color, using an enum value (e.g. Diagnostics::ORANGE)
void InteractiveScrollView::Pen(Color color) {
  Pen(table_colors[color][0], table_colors[color][1], table_colors[color][2],
      table_colors[color][3]);
}

// Set the brush color, using an enum value (e.g. Diagnostics::ORANGE)
void InteractiveScrollView::Brush(Color color) {
  Brush(table_colors[color][0], table_colors[color][1], table_colors[color][2],
        table_colors[color][3]);
}

// Shows a modal Input Dialog which can return any kind of String
char *InteractiveScrollView::ShowInputDialog(const char *msg) {
  SendMsg("showInputDialog(\"{}\")", msg);
  // wait till an input event (all others are thrown away)
  auto ev = AwaitEvent(SVET_INPUT);
  char *p = new char[strlen(ev->parameter) + 1];
  strcpy(p, ev->parameter);
  return p;
}

// Shows a modal Yes/No Dialog which will return 'y' or 'n'
int InteractiveScrollView::ShowYesNoDialog(const char *msg) {
  SendMsg("showYesNoDialog(\"{}\")", msg);
  // Wait till an input event (all others are thrown away)
  auto ev = AwaitEvent(SVET_INPUT);
  int a = ev->parameter[0];
  return a;
}

// Zoom the window to the rectangle given upper left corner and
// lower right corner.
void InteractiveScrollView::ZoomToRectangle(int x1, int y1, int x2, int y2) {
  y1 = TranslateYCoordinate(y1);
  y2 = TranslateYCoordinate(y2);
  SendMsg("zoomRectangle({},{},{},{})", std::min(x1, x2), std::min(y1, y2),
          std::max(x1, x2), std::max(y1, y2));
}

// Send an image of type Pix.
void InteractiveScrollView::Draw(Image image, int x_pos, int y_pos, const char *title) {
  l_uint8 *data;
  size_t size;
  pixWriteMem(&data, &size, image, IFF_PNG);
  int base64_len = (size + 2) / 3 * 4;
  y_pos = TranslateYCoordinate(y_pos);
  SendMsg("readImage({},{},{})", x_pos, y_pos, base64_len);
  // Base64 encode the data.
  const char kBase64Table[64] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
  };
  char *base64 = new char[base64_len + 1];
  memset(base64, '=', base64_len);
  base64[base64_len] = '\0';
  int remainder = 0;
  int bits_left = 0;
  int code_len = 0;
  for (size_t i = 0; i < size; ++i) {
    int code = (data[i] >> (bits_left + 2)) | remainder;
    base64[code_len++] = kBase64Table[code & 63];
    bits_left += 2;
    remainder = data[i] << (6 - bits_left);
    if (bits_left == 6) {
      base64[code_len++] = kBase64Table[remainder & 63];
      bits_left = 0;
      remainder = 0;
    }
  }
  if (bits_left > 0) {
    base64[code_len++] = kBase64Table[remainder & 63];
  }
  SendRawMessage(base64);
  delete[] base64;
  lept_free(data);
}

// Escapes the ' character with a \, so it can be processed by LUA.
// Note: The caller will have to make sure it deletes the newly allocated item.
std::string ScrollView::AddEscapeChars(const char *input, const char *chars_to_escape) {
  size_t len = strlen(input);
  char *message = new char[len * 2 + 1];
  char *d = message;
  const char *s = input;
  while (*s) {
    size_t pos = strcspn(s, chars_to_escape);
    if (pos) {
      memcpy(d, s, pos);
      d += pos;
      s += pos;
    }
    *d++ = '\\';
    *d++ = *s++;
  }
  *d = 0;
  std::string rv(message);
  delete[] message;
  return std::move(rv);
}

// Inverse the Y axis if the coordinates are actually inversed.
int InteractiveScrollView::TranslateYCoordinate(int y) {
  if (!y_axis_is_reversed_) {
    return y;
  } else {
    return y_size_ - y;
  }
}

char InteractiveScrollView::Wait() {
  // Wait till an input or click event (all others are thrown away)
  char ret = '\0';
  SVEventType ev_type = SVET_ANY;
  do {
    std::unique_ptr<SVEvent> ev(AwaitEvent(SVET_ANY));
    ev_type = ev->type;
    if (ev_type == SVET_INPUT) {
      ret = ev->parameter[0];
    }
  } while (ev_type != SVET_INPUT && ev_type != SVET_CLICK);
  return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Calls Initialize with all arguments given.
BackgroundScrollView::BackgroundScrollView(Tesseract *tess, const char *name,
                                             int x_pos, int y_pos, int x_size,
                                             int y_size, int x_canvas_size,
                                             int y_canvas_size,
                                             bool y_axis_reversed,
                                             const char *server_name)
    : ScrollView(tess, name, x_pos, y_pos, x_size, y_size, x_canvas_size,
                 y_canvas_size, y_axis_reversed, server_name)
    , dirty(false), x_offset(0), y_offset(0) {
  Initialize(tess, name, x_pos, y_pos, x_size, y_size, x_canvas_size,
             y_canvas_size, y_axis_reversed, server_name);
}

void BackgroundScrollView::PrepCanvas(void) {
  auto width = tesseract_->ImageWidth();
  auto height = tesseract_->ImageHeight();

  Image wh_pix = pixCreate(width, height, 32 /* RGBA */);
  pixSetAll(wh_pix);
  pix = MixWithLightRedTintedBackground(wh_pix, tesseract_->pix_binary(), nullptr);
  ASSERT0(pix.pix_ != wh_pix.pix_);
  wh_pix.destroy();
}

/// Sets up a ScrollView window, depending on the constructor variables.
void BackgroundScrollView::Initialize(Tesseract *tess, const char *name,
                                       int x_pos, int y_pos, int x_size,
                                       int y_size, int x_canvas_size,
                                       int y_canvas_size, bool y_axis_reversed,
                                       const char *server_name) {
  composeRGBPixel(255, 50, 255, &pen_color);
  composeRGBPixel(50, 255, 255, &brush_color);

  PrepCanvas();
}

/// Sits and waits for events on this window.
void BackgroundScrollView::StartEventHandler() {
  ASSERT_HOST_MSG(false, "Should never get here!");
}
#endif // !GRAPHICS_DISABLED

BackgroundScrollView::~BackgroundScrollView() {
  // we ASSUME the gathered content has been pushed off before we get here!
  ASSERT0(!dirty);
  pix.destroy();
}

#if !GRAPHICS_DISABLED
/// Send a message to the server, attaching the window id.
void BackgroundScrollView::vSendMsg(fmt::string_view format,
                                    fmt::format_args args) {
  auto message = fmt::vformat(format, args);
  //tprintDebug("DEBUG-DRAW: {}\n", message);
}

/// Add an Event Listener to this ScrollView Window
void BackgroundScrollView::AddEventHandler(SVEventHandler *listener) {
  ASSERT_HOST_MSG(false, "Should never get here!");
}

void BackgroundScrollView::Signal() {
    ASSERT_HOST_MSG(false, "Should never get here!");
}

void BackgroundScrollView::SetEvent(const SVEvent *svevent) {
    ASSERT_HOST_MSG(false, "Should never get here!");
}

/// Block until an event of the given type is received.
/// Note: The calling function is responsible for deleting the returned
/// SVEvent afterwards!
std::unique_ptr<SVEvent> BackgroundScrollView::AwaitEvent(SVEventType type) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  return std::make_unique<SVEvent>();
}

const float kMixFactor = 0.65;
const float kBlendPaintLayerFactor = 0.65;

    // Send the current buffered polygon (if any) and clear it.
void BackgroundScrollView::SendPolygon() {
  if (!points_->empty) {
    dirty = true;

    points_->empty = true; // Allows us to use SendMsg.
    int length = points_->xcoords.size();
    // length == 1 corresponds to 2 SetCursors in a row and only the
    // last setCursor has any effect.
    if (length == 2) {
      // An isolated line!

      //SendMsg("drawLine({},{},{},{})", points_->xcoords[0], points_->ycoords[0], points_->xcoords[1], points_->ycoords[1]);

      PTA *ptas = ptaCreate(2);
      ptaAddPt(ptas, points_->xcoords[0], points_->ycoords[0]);
      ptaAddPt(ptas, points_->xcoords[1], points_->ycoords[1]);

      const int width = 1;
      l_int32 r, g, b, a;
      extractRGBAValues(pen_color, &r, &g, &b, &a);
      pixRenderPolylineBlend(pix, ptas, width, r, g, b, kMixFactor, 0, 1 /* removedups */);
      ptaDestroy(&ptas);
    } else if (length > 2) {
      // A polyline.
      bool done = false;
      if (length == 5) {
        // A rectangle?
        if (points_->xcoords[0] == points_->xcoords[4] &&
            points_->ycoords[0] == points_->ycoords[4]) {
          if (points_->xcoords[0] == points_->xcoords[1] &&
              points_->xcoords[2] == points_->xcoords[3] &&
              points_->ycoords[0] == points_->ycoords[3] &&
              points_->ycoords[1] == points_->ycoords[2]) {
            //SendMsg("drawRectangle({},{},{},{})", points_->xcoords[0], points_->ycoords[0], points_->xcoords[2], points_->ycoords[2]);

            PTA *ptas = ptaCreate(4);
            ptaAddPt(ptas, points_->xcoords[0], points_->ycoords[0]);
            ptaAddPt(ptas, points_->xcoords[1], points_->ycoords[1]);
            ptaAddPt(ptas, points_->xcoords[2], points_->ycoords[2]);
            ptaAddPt(ptas, points_->xcoords[3], points_->ycoords[3]);

            const int width = 1;
            l_int32 r, g, b, a;
            extractRGBAValues(pen_color, &r, &g, &b, &a);
            pixRenderPolylineBlend(pix, ptas, width, r, g, b, kMixFactor, 1 /* closed */, 1 /* removedups */);
            ptaDestroy(&ptas);

            done = true;
          }
        }
      }
      if (!done) {
        char coordpair[kMaxIntPairSize];
        std::string decimal_coords;
        for (int i = 0; i < length; ++i) {
          snprintf(coordpair, kMaxIntPairSize, "%d,%d,", points_->xcoords[i], points_->ycoords[i]);
          decimal_coords += coordpair;
        }
        // erase trailing ',':
        decimal_coords.pop_back();
        //SendMsg("drawPolyline({})", decimal_coords.c_str());

        PTA *ptas = ptaCreate(length);
        ASSERT0(ptas != NULL);
        for (int i = 0; i < length; ++i) {
          ptaAddPt(ptas, points_->xcoords[i], points_->ycoords[i]);
        }

        const int width = 1;
        l_int32 r, g, b, a;
        extractRGBAValues(pen_color, &r, &g, &b, &a);
        pixRenderPolylineBlend(pix, ptas, width, r, g, b, kMixFactor, 0 /* closed */, 1 /* removedups */);
        ptaDestroy(&ptas);
      }
    }
    points_->xcoords.clear();
    points_->ycoords.clear();
  }
}

/*******************************************************************************
 * LUA "API" functions.
 *******************************************************************************/

void BackgroundScrollView::Comment(std::string text) {
  SendPolygon();
  SendMsg("comment(\"{}\")", text.c_str());
}

// Sets the position from which to draw to (x,y).
void BackgroundScrollView::SetCursor(int x, int y) {
  SendPolygon();
  DrawTo(x, y);
}

// Draws from the current position to (x,y) and sets the new position to it.
void BackgroundScrollView::DrawTo(int x, int y) {
  x += x_offset;
  y += y_offset;
  points_->xcoords.push_back(x);
  points_->ycoords.push_back(TranslateYCoordinate(y));
  points_->empty = false;
}

// Draw a line using the current pen color.
void BackgroundScrollView::Line(int x1, int y1, int x2, int y2) {
  x1 += x_offset;
  y1 += y_offset;
  x2 += x_offset;
  y2 += y_offset;
  if (!points_->xcoords.empty() && x1 == points_->xcoords.back() &&
      TranslateYCoordinate(y1) == points_->ycoords.back()) {
    // We are already at x1, y1, so just draw to x2, y2.
    DrawTo(x2, y2);
  } else if (!points_->xcoords.empty() && x2 == points_->xcoords.back() &&
             TranslateYCoordinate(y2) == points_->ycoords.back()) {
    // We are already at x2, y2, so just draw to x1, y1.
    DrawTo(x1, y1);
  } else {
    // This is a new line.
    SetCursor(x1, y1);
    DrawTo(x2, y2);
  }
}

// Set the visibility of the window.
void BackgroundScrollView::SetVisible(bool visible) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  if (visible) {
    SendMsg("setVisible(true)");
  } else {
    SendMsg("setVisible(false)");
  }
}

// Set the alwaysOnTop flag.
void BackgroundScrollView::AlwaysOnTop(bool b) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  if (b) {
    SendMsg("setAlwaysOnTop(true)");
  } else {
    SendMsg("setAlwaysOnTop(false)");
  }
}

// Adds a message entry to the message box.
void BackgroundScrollView::vAddMessage(fmt::string_view format,
                                        fmt::format_args args) {
  auto message = fmt::vformat(format, args);

  char winidstr[kMaxIntPairSize];
  snprintf(winidstr, kMaxIntPairSize, "w%u:", window_id_);
  std::string form(winidstr);
  form += message;

  auto esc = AddEscapeChars(form, "'\"");
  SendMsg("addMessage(\"{}\")", esc);
}

// Set a messagebox.
void BackgroundScrollView::AddMessageBox() {
  ASSERT_HOST_MSG(false, "Should never get here!");
  SendMsg("addMessageBox()");
}

// Exit the client completely (and notify the server of it).
void BackgroundScrollView::ExitHelper() {
  SendMsg("svmain:exit()");

  // invoke super::ExitHelper():
  ScrollView::ExitHelper();
}

// Clear the canvas.
void BackgroundScrollView::Clear() {
  SendMsg("clear()");
  SendPolygon();
  if (dirty) {
    tesseract_->AddPixCompedOverOrigDebugPage(pix, GetName());
    PrepCanvas();

    dirty = false;
    x_offset = 0;
    y_offset = 0;
  }
}

// Set the stroke width.
void BackgroundScrollView::Stroke(float width) {
  SendPolygon();
  SendMsg("setStrokeWidth({})", width);
}

// Draw a rectangle using the current pen color.
// The rectangle is filled with the current brush color.
void BackgroundScrollView::Rectangle(int x1, int y1, int x2, int y2) {
  SendPolygon();
  x1 += x_offset;
  y1 += y_offset;
  x2 += x_offset;
  y2 += y_offset;
  
  //SendMsg("drawRectangle({},{},{},{})", x1, TranslateYCoordinate(y1), x2, TranslateYCoordinate(y2));

  PTA *ptas = ptaCreate(5);
  ASSERT0(ptas != NULL);

  ptaAddPt(ptas, x1, TranslateYCoordinate(y1));
  ptaAddPt(ptas, x2, TranslateYCoordinate(y1));
  ptaAddPt(ptas, x2, TranslateYCoordinate(y2));
  ptaAddPt(ptas, x1, TranslateYCoordinate(y2));
  ptaAddPt(ptas, x1, TranslateYCoordinate(y1));

  const int width = 1;
  l_int32 r, g, b, a;
  extractRGBAValues(pen_color, &r, &g, &b, &a);
  pixRenderPolylineBlend(pix, ptas, width, r, g, b, kMixFactor, 0, 1 /* removedups */);
  ptaDestroy(&ptas);
}

// Draw an ellipse using the current pen color.
// The ellipse is filled with the current brush color.
void BackgroundScrollView::Ellipse(int x1, int y1, int width, int height) {
  SendPolygon();
  x1 += x_offset;
  y1 += y_offset;
  SendMsg("drawEllipse({},{},{},{})", x1, TranslateYCoordinate(y1), width,
          height);
}

// Set the pen color to the given RGB values.
void BackgroundScrollView::Pen(int red, int green, int blue) {
  SendPolygon();
  composeRGBPixel(red, green, blue, &pen_color);
  //SendMsg("pen({},{},{})", red, green, blue);
}

// Set the pen color to the given RGB values.
void BackgroundScrollView::Pen(int red, int green, int blue, int alpha) {
  SendPolygon();
  composeRGBAPixel(red, green, blue, 255 - alpha, &pen_color);
  //SendMsg("pen({},{},{},{})", red, green, blue, alpha);
}

// Set the brush color to the given RGB values.
void BackgroundScrollView::Brush(int red, int green, int blue) {
  SendPolygon();
  composeRGBPixel(red, green, blue, &brush_color);
  //SendMsg("brush({},{},{})", red, green, blue);
}

// Set the brush color to the given RGB values.
void BackgroundScrollView::Brush(int red, int green, int blue, int alpha) {
  SendPolygon();
  composeRGBAPixel(red, green, blue, 255 - alpha, &brush_color);
  //SendMsg("brush({},{},{},{})", red, green, blue, alpha);
}

// Set the attributes for future Text(..) calls.
void BackgroundScrollView::TextAttributes(const char *font, int pixel_size,
                                           bool bold, bool italic,
                                           bool underlined) {
  SendPolygon();

  const char *b;
  const char *i;
  const char *u;

  if (bold) {
    b = "true";
  } else {
    b = "false";
  }
  if (italic) {
    i = "true";
  } else {
    i = "false";
  }
  if (underlined) {
    u = "true";
  } else {
    u = "false";
  }
  SendMsg("textAttributes('{}',{},{},{},{})", font, pixel_size, b, i, u);
}


// Set up a X/Y offset for the subsequent drawing primitives.
void BackgroundScrollView::SetXYOffset(int x, int y) {
  x_offset = x;
  y_offset = y;
}

// Draw text at the given coordinates.
void BackgroundScrollView::Text(int x, int y, const char *mystring) {
  SendPolygon();
  x += x_offset;
  y += y_offset;

  //SendMsg("drawText({},{},'{}')", x, TranslateYCoordinate(y), mystring);

  BOX *box = boxCreate(x, TranslateYCoordinate(y), 5, 20);
  const int width = 1;
  l_int32 r, g, b, a;
  extractRGBAValues(pen_color, &r, &g, &b, &a);
  pixRenderBoxBlend(pix, box, width, r, g, b, kMixFactor);
  pixBlendInRect(pix, box, pen_color, kBlendPaintLayerFactor);

  const int fontsize = 16;
  L_BMF *bmf = bmfCreate(NULL, fontsize);
  l_int32 width_used = 0;
  l_int32 ovf = 0;
  //pixSetTextline(pix, bmf, mystring, pen_color, x, y, &width_used, &ovf);
  //y += bmf->lineheight + bmf->vertlinesep;

  int x_start = x;
  int h = bmf->lineheight;
  float scale = 13.0 / h;

  int nchar = strlen(mystring);
  for (int i = 0; i < nchar; i++) {
    char chr = mystring[i];
    if (chr == '\n' || chr == '\r')
      continue;
    PIX *tpix = bmfGetPix(bmf, chr);
    l_int32 baseline;
    bmfGetBaseline(bmf, chr, &baseline);
    int w = pixGetWidth(tpix);
    //PIX *tpix2 = pixScaleSmooth(tpix, scale, scale);
    //pixPaintThroughMask(pix, tpix, x, y - baseline * scale, pen_color);
    int d = pixGetDepth(pix);
    PIX *tpix2 = nullptr;
    switch (d) { 
    default: 
        ASSERT_HOST_MSG(false, "Should never get here!");
        break;

    case 32:
        tpix2 = pixConvertTo32(tpix);
        break;
    }
    PIX *tpix3 = pixScaleSmooth(tpix2, scale, scale);
    // pixPaintThroughMask(pix, tpix, x, y - baseline * scale, pen_color);
    l_int32 cw, ch, cd;
    pixGetDimensions(tpix3, &cw, &ch, &cd);
    //pixRasterop(pix, x, y - baseline * scale, cw, ch, PIX_XOR, tpix3, 0, 0);
    pixBlendColorByChannel(pix, pix, tpix3, x, y - baseline * scale, 1.0, 0.0, 1.0, 1, 0xFFFFFF00u);
    x += (w + bmf->kernwidth) * scale;
    pixDestroy(&tpix);
    pixDestroy(&tpix2);
    pixDestroy(&tpix3);
  }

  width_used = x - bmf->kernwidth * scale - x_start;
  ovf = (x > pixGetWidth(pix) - 1);

  bmfDestroy(&bmf);
  boxDestroy(&box);
}

// Open and draw an image given a name at (x,y).
void BackgroundScrollView::Draw(const char *image, int x_pos, int y_pos) {
  SendPolygon();
  
  //SendMsg("openImage('{}')", image);

  x_pos += x_offset;
  y_pos += y_offset;
  
  //SendMsg("drawImage('{}',{},{})", image, x_pos, TranslateYCoordinate(y_pos));

  BOX *box = boxCreate(x_pos, TranslateYCoordinate(y_pos), 5, 20);
  const int width = 1;
  l_int32 r, g, b, a;
  extractRGBAValues(pen_color, &r, &g, &b, &a);
  pixRenderBoxBlend(pix, box, width, r, g, b, kMixFactor);
  pixBlendInRect(pix, box, pen_color, kBlendPaintLayerFactor);
  boxDestroy(&box);
}

// Add new checkboxmenuentry to menubar.
void BackgroundScrollView::MenuItem(const char *parent, const char *name,
                                     int cmdEvent, bool flag) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  if (parent == nullptr) {
    parent = "";
  }
  if (flag) {
    SendMsg("addMenuBarItem('{}','{}',{},true)", parent, name, cmdEvent);
  } else {
    SendMsg("addMenuBarItem('{}','{}',{},false)", parent, name, cmdEvent);
  }
}

// Add new menuentry to menubar.
void BackgroundScrollView::MenuItem(const char *parent, const char *name,
                                     int cmdEvent) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  if (parent == nullptr) {
    parent = "";
  }
  SendMsg("addMenuBarItem('{}','{}',{})", parent, name, cmdEvent);
}

// Add new submenu to menubar.
void BackgroundScrollView::MenuItem(const char *parent, const char *name) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  if (parent == nullptr) {
    parent = "";
  }
  SendMsg("addMenuBarItem('{}','{}')", parent, name);
}

// Add new submenu to popupmenu.
void BackgroundScrollView::PopupItem(const char *parent, const char *name) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  if (parent == nullptr) {
    parent = "";
  }
  SendMsg("addPopupMenuItem('{}','{}')", parent, name);
}

// Add new submenuentry to popupmenu.
void BackgroundScrollView::PopupItem(const char *parent, const char *name,
                                      int cmdEvent, const char *value,
                                      const char *desc) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  if (parent == nullptr) {
    parent = "";
  }
  auto esc = AddEscapeChars(value, "'\"");
  auto esc2 = AddEscapeChars(desc, "'\"");
  SendMsg("addPopupMenuItem('{}','{}',{},'{}','{}')", parent, name, cmdEvent, esc, esc2);
}

// Send an update message for a single window.
void BackgroundScrollView::UpdateWindow() {
  SendMsg("update()");
  SendPolygon();
  if (dirty) {
    tesseract_->AddPixCompedOverOrigDebugPage(pix, fmt::format("{}::update", GetName()));
    // DO NOT clear the canvas!

    dirty = false;
  }
}

// Set the pen color, using an enum value (e.g. Diagnostics::ORANGE)
void BackgroundScrollView::Pen(Color color) {
  Pen(table_colors[color][0], table_colors[color][1], table_colors[color][2],
      table_colors[color][3]);
}

// Set the brush color, using an enum value (e.g. Diagnostics::ORANGE)
void BackgroundScrollView::Brush(Color color) {
  Brush(table_colors[color][0], table_colors[color][1], table_colors[color][2],
        table_colors[color][3]);
}

// Shows a modal Input Dialog which can return any kind of String
char *BackgroundScrollView::ShowInputDialog(const char *msg) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  return nullptr;
}

// Shows a modal Yes/No Dialog which will return 'y' or 'n'
int BackgroundScrollView::ShowYesNoDialog(const char *msg) {
  ASSERT_HOST_MSG(false, "Should never get here!");
  return 0;
}

// Zoom the window to the rectangle given upper left corner and
// lower right corner.
void BackgroundScrollView::ZoomToRectangle(int x1, int y1, int x2, int y2) {
  // draw zoom rectangle instead...
  SendMsg("zoomRectangle({},{},{},{})", std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2));
  Pen(255, 128, 0);
  Rectangle(x1, y1, x2, y2);
}

// Send an image of type Pix.
void BackgroundScrollView::Draw(Image image, int x_pos, int y_pos, const char *title) {
  SendPolygon();
  x_pos += x_offset;
  y_pos += y_offset;
  y_pos = TranslateYCoordinate(y_pos);

  //SendMsg("drawImage(x:{},y:{},\"{}\")", x_pos, y_pos, title);

  tesseract_->AddPixCompedOverOrigDebugPage(image, title);

  int w = pixGetWidth(image);
  int h = pixGetHeight(image);
  BOX *box = boxCreate(x_pos, y_pos, w, h);
  const int width = 1;
  l_int32 r, g, b, a;
  extractRGBAValues(pen_color, &r, &g, &b, &a);
  pixRenderBoxBlend(pix, box, width, r, g, b, kMixFactor);
  pixBlendInRect(pix, box, pen_color, kBlendPaintLayerFactor);
  boxDestroy(&box);
}

// Inverse the Y axis if the coordinates are actually inversed.
int BackgroundScrollView::TranslateYCoordinate(int y) {
  if (!y_axis_is_reversed_) {
    return y;
  } else {
    return y_size_ - y;
  }
}

char BackgroundScrollView::Wait() {
  ASSERT_HOST_MSG(false, "Should never get here!");
  return '\0';
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Calls Initialize with all arguments given.
DummyScrollView::DummyScrollView(Tesseract *tess, const char *name,
	int x_pos, int y_pos, int x_size,
	int y_size, int x_canvas_size,
	int y_canvas_size,
	bool y_axis_reversed,
	const char *server_name)
	: ScrollView(tess, name, x_pos, y_pos, x_size, y_size, x_canvas_size,
		y_canvas_size, y_axis_reversed, server_name)
{
	Initialize(tess, name, x_pos, y_pos, x_size, y_size, x_canvas_size,
		y_canvas_size, y_axis_reversed, server_name);
}

void DummyScrollView::PrepCanvas(void) {
}

/// Sets up a ScrollView window, depending on the constructor variables.
void DummyScrollView::Initialize(Tesseract *tess, const char *name,
	int x_pos, int y_pos, int x_size,
	int y_size, int x_canvas_size,
	int y_canvas_size, bool y_axis_reversed,
	const char *server_name) {
}

/// Sits and waits for events on this window.
void DummyScrollView::StartEventHandler() {
	ASSERT_HOST_MSG(false, "Should never get here!");
}
#endif // !GRAPHICS_DISABLED

DummyScrollView::~DummyScrollView() {
}

#if !GRAPHICS_DISABLED
/// Send a message to the server, attaching the window id.
void DummyScrollView::vSendMsg(fmt::string_view format,	fmt::format_args args) {
}

/// Add an Event Listener to this ScrollView Window
void DummyScrollView::AddEventHandler(SVEventHandler *listener) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

void DummyScrollView::Signal() {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

void DummyScrollView::SetEvent(const SVEvent *svevent) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

/// Block until an event of the given type is received.
/// Note: The calling function is responsible for deleting the returned
/// SVEvent afterwards!
std::unique_ptr<SVEvent> DummyScrollView::AwaitEvent(SVEventType type) {
	ASSERT_HOST_MSG(false, "Should never get here!");
	return std::make_unique<SVEvent>();
}

// Send the current buffered polygon (if any) and clear it.
void DummyScrollView::SendPolygon() {
}

/*******************************************************************************
* LUA "API" functions.
*******************************************************************************/

void DummyScrollView::Comment(std::string text) {
}

// Sets the position from which to draw to (x,y).
void DummyScrollView::SetCursor(int x, int y) {
}

// Draws from the current position to (x,y) and sets the new position to it.
void DummyScrollView::DrawTo(int x, int y) {
}

// Draw a line using the current pen color.
void DummyScrollView::Line(int x1, int y1, int x2, int y2) {
}

// Set the visibility of the window.
void DummyScrollView::SetVisible(bool visible) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

// Set the alwaysOnTop flag.
void DummyScrollView::AlwaysOnTop(bool b) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

// Adds a message entry to the message box.
void DummyScrollView::vAddMessage(fmt::string_view format, fmt::format_args args) {
}

// Set a messagebox.
void DummyScrollView::AddMessageBox() {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

// Exit the client completely (and notify the server of it).
void DummyScrollView::ExitHelper() {
	// invoke super::ExitHelper():
	ScrollView::ExitHelper();
}

// Clear the canvas.
void DummyScrollView::Clear() {
}

// Set the stroke width.
void DummyScrollView::Stroke(float width) {
}

// Draw a rectangle using the current pen color.
// The rectangle is filled with the current brush color.
void DummyScrollView::Rectangle(int x1, int y1, int x2, int y2) {
}

// Draw an ellipse using the current pen color.
// The ellipse is filled with the current brush color.
void DummyScrollView::Ellipse(int x1, int y1, int width, int height) {
}

// Set the pen color to the given RGB values.
void DummyScrollView::Pen(int red, int green, int blue) {
}

// Set the pen color to the given RGB values.
void DummyScrollView::Pen(int red, int green, int blue, int alpha) {
}

// Set the brush color to the given RGB values.
void DummyScrollView::Brush(int red, int green, int blue) {
}

// Set the brush color to the given RGB values.
void DummyScrollView::Brush(int red, int green, int blue, int alpha) {
}

// Set the attributes for future Text(..) calls.
void DummyScrollView::TextAttributes(const char *font, int pixel_size,
	bool bold, bool italic,
	bool underlined) {
}


// Set up a X/Y offset for the subsequent drawing primitives.
void DummyScrollView::SetXYOffset(int x, int y) {
}

// Draw text at the given coordinates.
void DummyScrollView::Text(int x, int y, const char *mystring) {
}

// Open and draw an image given a name at (x,y).
void DummyScrollView::Draw(const char *image, int x_pos, int y_pos) {
}

// Add new checkboxmenuentry to menubar.
void DummyScrollView::MenuItem(const char *parent, const char *name, int cmdEvent, bool flag) {
}

// Add new menuentry to menubar.
void DummyScrollView::MenuItem(const char *parent, const char *name, int cmdEvent) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

// Add new submenu to menubar.
void DummyScrollView::MenuItem(const char *parent, const char *name) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

// Add new submenu to popupmenu.
void DummyScrollView::PopupItem(const char *parent, const char *name) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

// Add new submenuentry to popupmenu.
void DummyScrollView::PopupItem(const char *parent, const char *name,
	int cmdEvent, const char *value,
	const char *desc) {
	ASSERT_HOST_MSG(false, "Should never get here!");
}

// Send an update message for a single window.
void DummyScrollView::UpdateWindow() {
}

// Set the pen color, using an enum value (e.g. Diagnostics::ORANGE)
void DummyScrollView::Pen(Color color) {
}

// Set the brush color, using an enum value (e.g. Diagnostics::ORANGE)
void DummyScrollView::Brush(Color color) {
}

// Shows a modal Input Dialog which can return any kind of String
char *DummyScrollView::ShowInputDialog(const char *msg) {
	ASSERT_HOST_MSG(false, "Should never get here!");
	return nullptr;
}

// Shows a modal Yes/No Dialog which will return 'y' or 'n'
int DummyScrollView::ShowYesNoDialog(const char *msg) {
	ASSERT_HOST_MSG(false, "Should never get here!");
	return 0;
}

// Zoom the window to the rectangle given upper left corner and
// lower right corner.
void DummyScrollView::ZoomToRectangle(int x1, int y1, int x2, int y2) {
}

// Send an image of type Pix.
void DummyScrollView::Draw(Image image, int x_pos, int y_pos, const char *title) {
}

// Inverse the Y axis if the coordinates are actually inversed.
int DummyScrollView::TranslateYCoordinate(int y) {
	return y;
}

char DummyScrollView::Wait() {
	ASSERT_HOST_MSG(false, "Should never get here!");
	return '\0';
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScrollViewReference::ScrollViewReference() : view_(nullptr), counter_(nullptr), id(-1) {
}

ScrollViewReference::ScrollViewReference(ScrollView *view) : view_(view), counter_(nullptr), id(-1) {
  if (view_ != nullptr) {
    counter_ = new int();
    *counter_ = 1;
    id = view_->GetId();
  }
}

void ScrollViewReference::cleanup_before_delete() {
  if (counter_) {
    switch (--*counter_) {
    case 1:
      // NOTE: Cannot invoke virtual method UpdateWindow() et al from the
      // Scrollview destructor any more as the derived instance has already
      // finished delete-ing by then: we're delete-ing the base class by then!
      // Hence we take care of this problem by calling Update() from the
      // ScrollViewReference destructor right here, BEFORE invoking the
      // destructor of the ScrollView and its derived class!
      if (view_ != nullptr) {
        view_->UpdateWindow();

        {
          std::lock_guard<std::mutex> guard(*svmap_mu);
          int id = GetRef()->GetId();
          while (svmap.size() <= id) {
            svmap.push_back(nullptr);
          }
          svmap[id] = nullptr; // --> this will fire another ScrollviewReference's destructor and thus fire `case 0:` below...
          auto &ref = svmap[id];
          ref.clear();
        }

        delete view_;
        ASSERT0(counter_ == nullptr || *counter_ == 0);
        delete counter_;

        view_ = nullptr;
        counter_ = nullptr;
      }
      break;

    case 0:
      //delete view_;
      //delete counter_;

      //view_ = nullptr;
      //counter_ = nullptr;
      break;
    }
  }
}

ScrollViewReference::~ScrollViewReference() {
  cleanup_before_delete();
}

ScrollViewReference &ScrollViewReference::operator=(ScrollView *new_view) {
  if (view_ != new_view) {
    cleanup_before_delete();

    view_ = new_view;
    if (view_ != nullptr) {
      counter_ = new int();
      *counter_ = 1;

      id = new_view->GetId();
    }
    else {
      counter_ = nullptr;
      id = -1;
    }
  }
  return *this;
}

void ScrollViewReference::clear() {
  if (view_ != nullptr) {
    cleanup_before_delete();

    view_ = nullptr;
    counter_ = nullptr;
    id = -1;
  }
}

ScrollViewReference::ScrollViewReference(const ScrollViewReference &o) {
  view_ = o.view_;
  counter_ = o.counter_;
  id = o.id;
  if (counter_ != nullptr) {
    ++*counter_;
  }
}

ScrollViewReference::ScrollViewReference(ScrollViewReference &&o) {
  view_ = o.view_;
  counter_ = o.counter_;
  id = o.id;
  o.view_ = nullptr;
  o.counter_ = nullptr;
  o.id = -1;
}

ScrollViewReference &ScrollViewReference::operator =(const ScrollViewReference &other) {
  if (&other != this) {
    cleanup_before_delete();

    view_ = other.view_;
    counter_ = other.counter_;
    id = other.id;
    if (counter_ != nullptr) {
      ++*counter_;
    }
  }
  return *this;
}

ScrollViewReference &ScrollViewReference::operator =(ScrollViewReference &&other) {
  if (&other != this) {
    cleanup_before_delete();

    view_ = other.view_;
    counter_ = other.counter_;
    id = other.id;
    other.view_ = nullptr;
    other.counter_ = nullptr;
    other.id = -1;
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////

// if (tesseract_->SupportsInteractiveScrollView()) ...

ScrollViewManager::ScrollViewManager() {
  active = nullptr;

  if (!svmap_mu) {
	svmap_mu = new std::mutex();
  }
}

ScrollViewManager &ScrollViewManager::GetScrollViewManager() {
  static ScrollViewManager mgr;

  return mgr;
}

ScrollViewManager::~ScrollViewManager() {
  delete svmap_mu;
  svmap_mu = nullptr;
}

ScrollViewReference ScrollViewManager::MakeScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos, int x_size, int y_size, int x_canvas_size, int y_canvas_size, bool y_axis_reversed, const char *server_name) {
  ScrollViewManager &mgr = GetScrollViewManager();
  mgr.SetActiveTesseractInstance(tess);
  tess = mgr.GetActiveTesseractInstance();    // TODO: only pick up the active one when the current one, i.e. the one we got passed, is NULL.  Though that may(??) be wrong when we add multi-instance running support and don't ditch interactive view --> interactive view cannot go together with multiple tesseract instances running in parallel.

  ScrollViewReference rv; 

  ASSERT_HOST(tess != nullptr);
  if (scrollview_support) {
    if (tess->SupportsInteractiveScrollView()) {
      rv = new InteractiveScrollView(tess, name, x_pos, y_pos, x_size, y_size,
                                       x_canvas_size, y_canvas_size,
                                       y_axis_reversed, server_name);
    } else {
      rv = new BackgroundScrollView(tess, name, x_pos, y_pos, x_size, y_size,
                                       x_canvas_size, y_canvas_size,
                                       y_axis_reversed, server_name);
    }
  } else {
    rv = new DummyScrollView(tess, name, x_pos, y_pos, x_size, y_size,
		  x_canvas_size, y_canvas_size,
		  y_axis_reversed, server_name);
  }

  // Only update the global svmap[] table here, as we want to keep a reference count of the number of references to ScrollView instances via ScrollViewReference
  // and the old code would produce two INDEPENDENT ScrollViewReference instances instead, causing havoc: one used by the application, plus one created implicitly
  // by the `svmap[rv->GetId()] = this;` statement in the `Scrollview::Initialize()` method.
  // 
  // That's why we #if0-ed that chunk of code further above!
  auto wi = rv->GetId();
  rv.id = wi;
  {
    std::lock_guard<std::mutex> guard(*svmap_mu);
    while (svmap.size() <= wi) {
      svmap.push_back(nullptr);
    }
    svmap[wi] = rv;
  }

  return rv;
}

  // set this instance to be the latest active one
void ScrollViewManager::SetActiveTesseractInstance(Tesseract *tess) {
  if (!tess)
    return;
  ScrollViewManager &mgr = GetScrollViewManager();
  if (mgr.active != tess) {
    mgr.AddActiveTesseractInstance(tess);
  }
}

// add this instance to the list of active tesseract instance but don't put it on top yet...
void ScrollViewManager::AddActiveTesseractInstance(Tesseract *tess) {
  if (!tess)
    return;
  ScrollViewManager &mgr = GetScrollViewManager();
  auto it = std::find(mgr.active_set.begin(), mgr.active_set.end(), tess);
  if (it == mgr.active_set.end()) {
    mgr.active_set.push_back(tess);
    mgr.active = mgr.active_set.front();
  }
}

// remove the given instance from the active set as its object is currently
// being destroyed.
void ScrollViewManager::RemoveActiveTesseractInstance(Tesseract *tess) {
  if (!tess)
    return;
  ScrollViewManager &mgr = GetScrollViewManager();
  auto it = std::find(mgr.active_set.begin(), mgr.active_set.end(), tess);
  if (it != mgr.active_set.end()) {
    mgr.active_set.erase(it);
    mgr.active = nullptr;
    if (mgr.active_set.empty()) {
      // flush all debug windows first
      ScrollView::Update();

      // and nuke 'em all, next:
      for (;;) {
      // limit scope of lock
      std::vector<ScrollViewReference> delset;
      {
        std::lock_guard<std::mutex> guard(*svmap_mu);
        for (auto &iter : svmap) {
            if (iter) {
              delset.push_back(iter);
            }
        }
      }

      if (delset.size() == 0)
        break;

      for (int index = delset.size() - 1; index >= 0; index--) {
        ScrollViewReference win_ref = delset[index];
        if (win_ref) {
            win_ref->ExitHelper();
        }
      }
      break;
      }
    }
  }
}

Tesseract *ScrollViewManager::GetActiveTesseractInstance() {
  ScrollViewManager &mgr = GetScrollViewManager();
  if (mgr.active)
    return mgr.active;
  if (mgr.active_set.empty())
    return nullptr;
  mgr.active = mgr.active_set.front();
  return mgr.active;
}

#endif // !GRAPHICS_DISABLED

} // namespace tesseract
