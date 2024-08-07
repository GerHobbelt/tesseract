 ///////////////////////////////////////////////////////////////////////
// File:        scrollview.h
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
// ScrollView is designed as an UI which can be run remotely. This is the
// client code for it, the server part is written in java. The client consists
// mainly of 2 parts:
// The "core" ScrollView which sets up the remote connection,
// takes care of event handling etc.
// The other part of ScrollView consists of predefined API calls through LUA,
// which can basically be used to get a zoomable canvas in which it is possible
// to draw lines, text etc.
// Technically, thanks to LUA, its even possible to bypass the here defined LUA
// API calls at all and generate a java user interface from scratch (or
// basically generate any kind of java program, possibly even dangerous ones).

#ifndef TESSERACT_VIEWER_SCROLLVIEW_H_
#define TESSERACT_VIEWER_SCROLLVIEW_H_

#include "image.h"

#include <fmt/format.h>
#include <tesseract/export.h>

#include <leptonica/environ.h>  /* leptonica */

#include <cstdio>
#include <memory>
#include <mutex>
#include <list>
#include <vector>

namespace tesseract {

class ScrollView;
class SVNetwork;
class SVSemaphore;
struct SVPolyLineBuffer;

class TESS_API Tesseract;

#define TESSERACT_NULLPTR     nullptr

#if !GRAPHICS_DISABLED

class TESS_API ScrollViewReference {
public:
  ScrollViewReference();
  ScrollViewReference(ScrollView *view);
  ~ScrollViewReference();
 
  ScrollViewReference(const ScrollViewReference &o) /* = delete */ ;
  ScrollViewReference(ScrollViewReference &&o);

  ScrollViewReference &operator=(const ScrollViewReference &other) /* = delete */ ;
  ScrollViewReference &operator=(ScrollViewReference &&other);

  void clear();

  ScrollView *GetRef() const {
    return view_;
  }

  ScrollView *operator->() const {
    return view_;
  }

  operator bool() const {
    return view_ != nullptr;
  }

  ScrollViewReference &operator=(ScrollView *new_view);

protected:
  void cleanup_before_delete();

  ScrollView *view_; // reference
  int *counter_;

public:
  int id;
};

#endif // !GRAPHICS_DISABLED

#if !GRAPHICS_DISABLED

enum SVEventType {
  SVET_DESTROY,   // Window has been destroyed by user.
  SVET_EXIT,      // User has destroyed the last window by clicking on the 'X'.
  SVET_CLICK,     // Left button pressed.
  SVET_SELECTION, // Left button selection.
  SVET_INPUT,     // There is some input (single key or a whole string).
  SVET_MOUSE,     // The mouse has moved with a button pressed.
  SVET_MOTION,    // The mouse has moved with no button pressed.
  SVET_HOVER,     // The mouse has stayed still for a second.
  SVET_POPUP,     // A command selected through a popup menu.
  SVET_MENU,      // A command selected through the menubar.
  SVET_ANY,       // Any of the above.

  SVET_COUNT // Array sizing.
};

struct SVEvent {
  ~SVEvent() {
    delete[] parameter;
  }
  std::unique_ptr<SVEvent> copy() const;
  SVEventType type = SVET_DESTROY; // What kind of event.
  ScrollViewReference window;      // Window event relates to.
  char *parameter = nullptr;       // Any string that might have been passed as argument.
  int x = 0;                       // Coords of click or selection.
  int y = 0;
  int x_size = 0; // Size of selection.
  int y_size = 0;
  int command_id = 0; // The ID of the possibly associated event (e.g. MENU)
  int counter = 0;    // Used to detect which kind of event to process next.

  SVEvent() = default;
  SVEvent(const SVEvent &);
  SVEvent &operator=(const SVEvent &);
};

// The SVEventHandler class is used for Event handling: If you register your
// class as SVEventHandler to a ScrollView Window, the SVEventHandler will be
// called whenever an appropriate event occurs.
class TESS_API SVEventHandler {
public:
  virtual ~SVEventHandler();

  // Gets called by the SV Window. Does nothing on default, overwrite this
  // to implement the desired behaviour
  virtual void Notify(const SVEvent *sve) {
    (void)sve;
  }
};

#endif

namespace Diagnostics {

  // Color enum for pens and brushes.
  //
  // Note: the type formerly known as ScrollView::Color.
  enum Color {
    NONE,
    BLACK,
    WHITE,
    RED,
    YELLOW,
    GREEN,
    CYAN,
    BLUE,
    MAGENTA,
    AQUAMARINE,
    DARK_SLATE_BLUE,
    LIGHT_BLUE,
    MEDIUM_BLUE,
    MIDNIGHT_BLUE,
    NAVY_BLUE,
    SKY_BLUE,
    SLATE_BLUE,
    STEEL_BLUE,
    CORAL,
    BROWN,
    SANDY_BROWN,
    GOLD,
    GOLDENROD,
    DARK_GREEN,
    DARK_OLIVE_GREEN,
    FOREST_GREEN,
    LIME_GREEN,
    PALE_GREEN,
    YELLOW_GREEN,
    LIGHT_GREY,
    DARK_SLATE_GREY,
    DIM_GREY,
    GREY,
    KHAKI,
    MAROON,
    ORANGE,
    ORCHID,
    PINK,
    PLUM,
    INDIAN_RED,
    ORANGE_RED,
    VIOLET_RED,
    SALMON,
    TAN,
    TURQUOISE,
    DARK_TURQUOISE,
    VIOLET,
    WHEAT,
    GREEN_YELLOW // Make sure this one is last.
  };

}

#if !GRAPHICS_DISABLED

// The ScrollView class provides the external API to the scrollviewer process.
// The scrollviewer process manages windows and displays images, graphics and
// text while allowing the user to zoom and scroll the windows arbitrarily.
// Each ScrollView class instance represents one window, and stuff is drawn in
// the window through method calls on the class. The constructor is used to
// create the class instance (and the window).
class TESS_API ScrollView {
  using Color = Diagnostics::Color;

public:
  virtual ~ScrollView();

  // Create a window. The pixel size of the window may be 0,0, in which case
  // a default size is selected based on the size of your canvas.
  // The canvas may not be 0,0 in size!
  // With a flag whether the x axis is reversed.
  // Connect to a server other than localhost.
  ScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos, int x_size, int y_size, int x_canvas_size,
             int y_canvas_size, bool y_axis_reversed, const char *server_name);

protected:
  Tesseract *tesseract_; // reference to the driving tesseract instance

public:
  // Add a handler to help with the exit process, i.e. nuking the global
  // reference to this ScrollView, IFF there's any.
  // 
  // `ref2ref` must point to an instance that has a near-infinite scope/lifetime or Bad Things Will Happen(tm)!
  void RegisterGlobalRefToMe(ScrollViewReference *ref_of_ref);

private:
  ScrollViewReference *ref_of_ref_;

public:
  /*******************************************************************************
   * Event handling
   * To register as listener, the class has to derive from the SVEventHandler
   * class, which consists of a notifyMe(SVEvent*) function that should be
   * overwritten to process the event the way you want.
   *******************************************************************************/

  virtual bool HasInteractiveFeature() const {
    return false;
  }

  // Add an Event Listener to this ScrollView Window.
  virtual void AddEventHandler(SVEventHandler *listener) = 0;

  // Block until an event of the given type is received.
  virtual std::unique_ptr<SVEvent> AwaitEvent(SVEventType type) = 0;

  /*******************************************************************************
   * Getters and Setters
   *******************************************************************************/

  // Returns the title of the window.
  const char *GetName() {
    return window_name_;
  }

  // Returns the unique ID of the window.
  int GetId() {
    return window_id_;
  }

  bool is_y_axis_reversed() {
    return y_axis_is_reversed_;
  }

  /*******************************************************************************
   * API functions for LUA calls
   * the implementations for these can be found in svapi.cc
   * (keep in mind that the window is actually created through the ScrollView
   * constructor, so this is not listed here)
   *******************************************************************************/

  // Add comment
  virtual void Comment(std::string msg) = 0;

  // Draw an image on (x,y).
  virtual void Draw(Image image, int x_pos, int y_pos, const char *title) = 0;

  // Flush buffers and update display.
  static void Update();

  // Exit the program.
  static void Exit();

  // Helper function to exit the program.
  virtual void ExitHelper();

  // Update the contents of a specific window.
  virtual void UpdateWindow() = 0;

  // Erase all content from the window, but do not destroy it.
  virtual void Clear() = 0;

  // Set pen color with an enum.
  virtual void Pen(Color color) = 0;

  // Set pen color to RGB (0-255).
  virtual void Pen(int red, int green, int blue) = 0;

  // Set pen color to RGBA (0-255).
  virtual void Pen(int red, int green, int blue, int alpha) = 0;

  // Set brush color with an enum.
  virtual void Brush(Color color) = 0;

  // Set brush color to RGB (0-255).
  virtual void Brush(int red, int green, int blue) = 0;

  // Set brush color to RGBA (0-255).
  virtual void Brush(int red, int green, int blue, int alpha) = 0;

  // Set attributes for future text, like font name (e.g.
  // "Times New Roman"), font size etc..
  // Note: The underlined flag is currently not supported
  virtual void TextAttributes(const char *font, int pixel_size, bool bold, bool italic, bool underlined) = 0;

  // Set up a X/Y offset for the subsequent drawing primitives.
  virtual void SetXYOffset(int x, int y) = 0;

  // Draw line from (x1,y1) to (x2,y2) with the current pencolor.
  virtual void Line(int x1, int y1, int x2, int y2) = 0;

  // Set the stroke width of the pen.
  virtual void Stroke(float width) = 0;

  // Draw a rectangle given upper left corner and lower right corner.
  // The current pencolor is used as outline, the brushcolor to fill the shape.
  virtual void Rectangle(int x1, int y1, int x2, int y2) = 0;

  // Draw an ellipse centered on (x,y).
  // The current pencolor is used as outline, the brushcolor to fill the shape.
  virtual void Ellipse(int x, int y, int width, int height) = 0;

  // Draw text with the current pencolor
  virtual void Text(int x, int y, const char *mystring) = 0;

  // Draw an image from a local filename. This should be faster than
  // createImage. WARNING: This only works on a local machine. This also only
  // works image types supported by java (like bmp,jpeg,gif,png) since the image
  // is opened by the server.
  virtual void Draw(const char *image, int x_pos, int y_pos) = 0;

  // Set the current position to draw from (x,y). In conjunction with...
  virtual void SetCursor(int x, int y) = 0;

  // ...this function, which draws a line from the current to (x,y) and then
  // sets the new position to the new (x,y), this can be used to easily draw
  // polygons using vertices
  virtual void DrawTo(int x, int y) = 0;

  // Set the SVWindow visible/invisible.
  virtual void SetVisible(bool visible) = 0;

  // Set the SVWindow always on top or not always on top.
  virtual void AlwaysOnTop(bool b) = 0;

  // Shows a modal dialog with "msg" as question and returns 'y' or 'n'.
  virtual int ShowYesNoDialog(const char *msg) = 0;

  // Shows a modal dialog with "msg" as question and returns a char* string.
  // Constraint: As return, only words (e.g. no whitespaces etc.) are allowed.
  virtual char *ShowInputDialog(const char *msg) = 0;

  // Adds a messagebox to the SVWindow. This way, it can show the messages...
  virtual void AddMessageBox() = 0;

  virtual void vAddMessage(fmt::string_view format, fmt::format_args args) = 0;

  // ...which can be added by this command.
  // This is intended as an "debug" output window.
  template <typename S, typename... Args>
  void AddMessage(const S &format, Args &&...args) {
    vAddMessage(format, fmt::make_format_args(args...));
  }

  // Zoom the window to the rectangle given upper left corner and
  // lower right corner.
  virtual void ZoomToRectangle(int x1, int y1, int x2, int y2) = 0;

  // Custom messages (manipulating java code directly) can be send through this.
  // Send a message to the server and attach the Id of the corresponding window.
  // Note: This should only be called if you are know what you are doing, since
  // you are fiddling with the Java objects on the server directly. Calling
  // this just for fun will likely break your application!
  // It is public so you can actually take use of the LUA functionalities, but
  // be careful!
  virtual void vSendMsg(fmt::string_view format, fmt::format_args args) = 0;

  template <typename S, typename... Args>
  void SendMsg(const S &format, Args&&... args) {
    vSendMsg(format, fmt::make_format_args(args...));
  }

  /*******************************************************************************
   * Add new menu entries to parent. If parent is "", the entry gets added to
   *the main menubar (toplevel).
   *******************************************************************************/
  // This adds a new submenu to the menubar.
  virtual void MenuItem(const char *parent, const char *name) = 0;

  // This adds a new (normal) menu entry with an associated eventID, which
  // should be unique among menubar eventIDs.
  virtual void MenuItem(const char *parent, const char *name, int cmdEvent) = 0;

  // This adds a new checkbox entry, which might initially be flagged.
  virtual void MenuItem(const char *parent, const char *name, int cmdEvent,
                        bool flagged) = 0;

  // This adds a new popup submenu to the popup menu. If parent is "", the entry
  // gets added at "toplevel" popupmenu.
  virtual void PopupItem(const char *parent, const char *name) = 0;

  // This adds a new popup entry with the associated eventID, which should be
  // unique among popup eventIDs.
  // If value and desc are given, on a click the server will ask you to modify
  // the value and return the new value.
  virtual void PopupItem(const char *parent, const char *name, int cmdEvent, const char *value, const char *desc) = 0;

  // Returns the correct Y coordinate for a window, depending on whether it
  // might have to be flipped (by ySize).
  virtual int TranslateYCoordinate(int y) = 0;

  virtual char Wait() = 0;

protected:
  // Sets up ScrollView, depending on the variables from the constructor.
  virtual void Initialize(Tesseract* tess, const char *name, int x_pos, int y_pos, int x_size, int y_size, int x_canvas_size,
                  int y_canvas_size, bool y_axis_reversed, const char *server_name);

  // Send the current buffered polygon (if any) and clear it.
  virtual void SendPolygon() = 0;

public:
  // Place an event into the event_table (synchronized).
  virtual void SetEvent(const SVEvent *svevent) = 0;

  // Wake up the semaphore.
  virtual void Signal() = 0;

  // Starts a new event handler.
  // Called asynchronously whenever a new window is created.
  virtual void StartEventHandler() = 0;

protected:
  // Escapes each of the given characters with a \, so it can be processed by LUA.
  static std::string AddEscapeChars(const char *input, const char *chars_to_escape);
  static std::string AddEscapeChars(const std::string &input, const char *chars_to_escape) {
    // warning C5263 : calling 'std::move' on a temporary object prevents copy elision
    return AddEscapeChars(input.c_str(), chars_to_escape);
  }

protected:
  // The name of the window.
  const char *window_name_;
  // The id of the window.
  int window_id_;
  // The points of the currently under-construction polyline.
  SVPolyLineBuffer *points_;
  // Whether the axis is reversed.
  bool y_axis_is_reversed_;
  // If the y axis is reversed, flip all y values by ySize.
  int y_size_;
  // # of created windows (used to assign an id to each ScrollView* for svmap).
  static int nr_created_windows_;
  // Serial number of sent images to ensure that the viewer knows they
  // are distinct.
  static int image_index_;
};

#endif // !GRAPHICS_DISABLED

#if !GRAPHICS_DISABLED

// The InteractiveScrollView class provides the external API to the scrollviewer process.
// The scrollviewer process manages windows and displays images, graphics and
// text while allowing the user to zoom and scroll the windows arbitrarily.
// Each ScrollView class instance represents one window, and stuff is drawn in
// the window through method calls on the class. The constructor is used to
// create the class instance (and the window).
class TESS_API InteractiveScrollView : public ScrollView {
public:
  using Color = Diagnostics::Color;

  virtual ~InteractiveScrollView();

  // Create a window. The pixel size of the window may be 0,0, in which case
  // a default size is selected based on the size of your canvas.
  // The canvas may not be 0,0 in size!
  // With a flag whether the x axis is reversed.
  // Connect to a server other than localhost.
  InteractiveScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos,
             int x_size, int y_size, int x_canvas_size, int y_canvas_size,
             bool y_axis_reversed, const char *server_name);

public:
  /*******************************************************************************
   * Event handling
   * To register as listener, the class has to derive from the SVEventHandler
   * class, which consists of a notifyMe(SVEvent*) function that should be
   * overwritten to process the event the way you want.
   *******************************************************************************/

  virtual bool HasInteractiveFeature() const {
    return true;
  }

  // Add an Event Listener to this ScrollView Window.
  virtual void AddEventHandler(SVEventHandler *listener);

  // Block until an event of the given type is received.
  virtual std::unique_ptr<SVEvent> AwaitEvent(SVEventType type);

  /*******************************************************************************
   * API functions for LUA calls
   * the implementations for these can be found in svapi.cc
   * (keep in mind that the window is actually created through the ScrollView
   * constructor, so this is not listed here)
   *******************************************************************************/

  // Add comment
  virtual void Comment(std::string msg);

  // Draw an image on (x,y).
  virtual void Draw(Image image, int x_pos, int y_pos, const char *title);

  // Helper function to exit the program.
  virtual void ExitHelper();

  // Update the contents of a specific window.
  virtual void UpdateWindow();

  // Erase all content from the window, but do not destroy it.
  virtual void Clear();

  // Set pen color with an enum.
  virtual void Pen(Color color);

  // Set pen color to RGB (0-255).
  virtual void Pen(int red, int green, int blue);

  // Set pen color to RGBA (0-255).
  virtual void Pen(int red, int green, int blue, int alpha);

  // Set brush color with an enum.
  virtual void Brush(Color color);

  // Set brush color to RGB (0-255).
  virtual void Brush(int red, int green, int blue);

  // Set brush color to RGBA (0-255).
  virtual void Brush(int red, int green, int blue, int alpha);

  // Set attributes for future text, like font name (e.g.
  // "Times New Roman"), font size etc..
  // Note: The underlined flag is currently not supported
  virtual void TextAttributes(const char *font, int pixel_size, bool bold,
                              bool italic, bool underlined);

  // Set up a X/Y offset for the subsequent drawing primitives.
  virtual void SetXYOffset(int x, int y);

  // Draw line from (x1,y1) to (x2,y2) with the current pencolor.
  virtual void Line(int x1, int y1, int x2, int y2);

  // Set the stroke width of the pen.
  virtual void Stroke(float width);

  // Draw a rectangle given upper left corner and lower right corner.
  // The current pencolor is used as outline, the brushcolor to fill the shape.
  virtual void Rectangle(int x1, int y1, int x2, int y2);

  // Draw an ellipse centered on (x,y).
  // The current pencolor is used as outline, the brushcolor to fill the shape.
  virtual void Ellipse(int x, int y, int width, int height);

  // Draw text with the current pencolor
  virtual void Text(int x, int y, const char *mystring);

  // Draw an image from a local filename. This should be faster than
  // createImage. WARNING: This only works on a local machine. This also only
  // works image types supported by java (like bmp,jpeg,gif,png) since the image
  // is opened by the server.
  virtual void Draw(const char *image, int x_pos, int y_pos);

  // Set the current position to draw from (x,y). In conjunction with...
  virtual void SetCursor(int x, int y);

  // ...this function, which draws a line from the current to (x,y) and then
  // sets the new position to the new (x,y), this can be used to easily draw
  // polygons using vertices
  virtual void DrawTo(int x, int y);

  // Set the SVWindow visible/invisible.
  virtual void SetVisible(bool visible);

  // Set the SVWindow always on top or not always on top.
  virtual void AlwaysOnTop(bool b);

  // Shows a modal dialog with "msg" as question and returns 'y' or 'n'.
  virtual int ShowYesNoDialog(const char *msg);

  // Shows a modal dialog with "msg" as question and returns a char* string.
  // Constraint: As return, only words (e.g. no whitespaces etc.) are allowed.
  virtual char *ShowInputDialog(const char *msg);

  // Adds a messagebox to the SVWindow. This way, it can show the messages...
  virtual void AddMessageBox();

  virtual void vAddMessage(fmt::string_view format, fmt::format_args args);

  // Zoom the window to the rectangle given upper left corner and
  // lower right corner.
  virtual void ZoomToRectangle(int x1, int y1, int x2, int y2);

  // Custom messages (manipulating java code directly) can be send through this.
  // Send a message to the server and attach the Id of the corresponding window.
  // Note: This should only be called if you are know what you are doing, since
  // you are fiddling with the Java objects on the server directly. Calling
  // this just for fun will likely break your application!
  // It is public so you can actually take use of the LUA functionalities, but
  // be careful!
  virtual void vSendMsg(fmt::string_view format, fmt::format_args args);

  // Custom messages (manipulating java code directly) can be send through this.
  // Send a message to the server without adding the
  // window id. Used for global events like Exit().
  // Note: This should only be called if you know what you are doing, since
  // you are fiddling with the Java objects on the server directly. Calling
  // this just for fun will likely break your application!
  // It is public so you can actually make use of the LUA functionalities, but
  // be careful!
  static void SendRawMessage(const char *msg);

  /*******************************************************************************
   * Add new menu entries to parent. If parent is "", the entry gets added to
   *the main menubar (toplevel).
   *******************************************************************************/

   // This adds a new submenu to the menubar.
  virtual void MenuItem(const char *parent, const char *name);

  // This adds a new (normal) menu entry with an associated eventID, which
  // should be unique among menubar eventIDs.
  virtual void MenuItem(const char *parent, const char *name, int cmdEvent);

  // This adds a new checkbox entry, which might initially be flagged.
  virtual void MenuItem(const char *parent, const char *name, int cmdEvent,
                        bool flagged);

  // This adds a new popup submenu to the popup menu. If parent is "", the entry
  // gets added at "toplevel" popupmenu.
  virtual void PopupItem(const char *parent, const char *name);

  // This adds a new popup entry with the associated eventID, which should be
  // unique among popup eventIDs.
  // If value and desc are given, on a click the server will ask you to modify
  // the value and return the new value.
  virtual void PopupItem(const char *parent, const char *name, int cmdEvent,
                         const char *value, const char *desc);

  // Returns the correct Y coordinate for a window, depending on whether it
  // might have to be flipped (by ySize).
  virtual int TranslateYCoordinate(int y);

  virtual char Wait();

protected:
  // Sets up ScrollView, depending on the variables from the constructor.
  virtual void Initialize(Tesseract *tess, const char *name, int x_pos,
                          int y_pos, int x_size, int y_size, int x_canvas_size,
                          int y_canvas_size, bool y_axis_reversed,
                          const char *server_name);

  // Send the current buffered polygon (if any) and clear it.
  virtual void SendPolygon();

  // Start the message receiving thread.
  static void MessageReceiver();

  // Place an event into the event_table (synchronized).
  virtual void SetEvent(const SVEvent *svevent);

  // Wake up the semaphore.
  virtual void Signal();

  // Returns the unique, shared network stream.
  static SVNetwork *GetStream() {
    return stream_;
  }

  // Starts a new event handler.
  // Called asynchronously whenever a new window is created.
  virtual void StartEventHandler();

protected:
  // The event handler for this window.
  SVEventHandler *event_handler_;
  // Set to true only after the event handler has terminated.
  volatile bool event_handler_ended_;

  // Table of all the currently queued events.
  std::unique_ptr<SVEvent> event_table_[SVET_COUNT];

  // Mutex to access the event_table_ in a synchronized fashion.
  std::mutex mutex_;

  // The stream through which the c++ client is connected to the server.
  static SVNetwork *stream_;

  // Semaphore to the thread belonging to this window.
  SVSemaphore *semaphore_;
};

#endif // !GRAPHICS_DISABLED

/////////////////////////////////////////////////////////////////////////

#if !GRAPHICS_DISABLED

// The BackgroundScrollView class provides the external API to the scrollview-to-DebugPIXA logging path.
class TESS_API BackgroundScrollView : public ScrollView {
public:
  using Color = Diagnostics::Color;

  virtual ~BackgroundScrollView();

  // Create a window. The pixel size of the window may be 0,0, in which case
  // a default size is selected based on the size of your canvas.
  // The canvas may not be 0,0 in size!
  // With a flag whether the x axis is reversed.
  // Connect to a server other than localhost.
  BackgroundScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos,
             int x_size, int y_size, int x_canvas_size, int y_canvas_size,
             bool y_axis_reversed, const char *server_name);

public:
  /*******************************************************************************
   * Event handling
   * To register as listener, the class has to derive from the SVEventHandler
   * class, which consists of a notifyMe(SVEvent*) function that should be
   * overwritten to process the event the way you want.
   *******************************************************************************/

  // Add an Event Listener to this ScrollView Window.
  virtual void AddEventHandler(SVEventHandler *listener);

  // Block until an event of the given type is received.
  virtual std::unique_ptr<SVEvent> AwaitEvent(SVEventType type);

  /*******************************************************************************
   * API functions for LUA calls
   * the implementations for these can be found in svapi.cc
   * (keep in mind that the window is actually created through the ScrollView
   * constructor, so this is not listed here)
   *******************************************************************************/

  // Add comment
  virtual void Comment(std::string msg);

  // Draw an image on (x,y).
  virtual void Draw(Image image, int x_pos, int y_pos, const char *title);

  // Helper function to exit the program.
  virtual void ExitHelper();

  // Update the contents of a specific window.
  virtual void UpdateWindow();

  // Erase all content from the window, but do not destroy it.
  virtual void Clear();

  // Set pen color with an enum.
  virtual void Pen(Color color);

  // Set pen color to RGB (0-255).
  virtual void Pen(int red, int green, int blue);

  // Set pen color to RGBA (0-255).
  virtual void Pen(int red, int green, int blue, int alpha);

  // Set brush color with an enum.
  virtual void Brush(Color color);

  // Set brush color to RGB (0-255).
  virtual void Brush(int red, int green, int blue);

  // Set brush color to RGBA (0-255).
  virtual void Brush(int red, int green, int blue, int alpha);

  // Set attributes for future text, like font name (e.g.
  // "Times New Roman"), font size etc..
  // Note: The underlined flag is currently not supported
  virtual void TextAttributes(const char *font, int pixel_size, bool bold,
                              bool italic, bool underlined);

  // Set up a X/Y offset for the subsequent drawing primitives.
  virtual void SetXYOffset(int x, int y);

  // Draw line from (x1,y1) to (x2,y2) with the current pencolor.
  virtual void Line(int x1, int y1, int x2, int y2);

  // Set the stroke width of the pen.
  virtual void Stroke(float width);

  // Draw a rectangle given upper left corner and lower right corner.
  // The current pencolor is used as outline, the brushcolor to fill the shape.
  virtual void Rectangle(int x1, int y1, int x2, int y2);

  // Draw an ellipse centered on (x,y).
  // The current pencolor is used as outline, the brushcolor to fill the shape.
  virtual void Ellipse(int x, int y, int width, int height);

  // Draw text with the current pencolor
  virtual void Text(int x, int y, const char *mystring);

  // Draw an image from a local filename. This should be faster than
  // createImage. WARNING: This only works on a local machine. This also only
  // works image types supported by java (like bmp,jpeg,gif,png) since the image
  // is opened by the server.
  virtual void Draw(const char *image, int x_pos, int y_pos);

  // Set the current position to draw from (x,y). In conjunction with...
  virtual void SetCursor(int x, int y);

  // ...this function, which draws a line from the current to (x,y) and then
  // sets the new position to the new (x,y), this can be used to easily draw
  // polygons using vertices
  virtual void DrawTo(int x, int y);

  // Set the SVWindow visible/invisible.
  virtual void SetVisible(bool visible);

  // Set the SVWindow always on top or not always on top.
  virtual void AlwaysOnTop(bool b);

  // Shows a modal dialog with "msg" as question and returns 'y' or 'n'.
  virtual int ShowYesNoDialog(const char *msg);

  // Shows a modal dialog with "msg" as question and returns a char* string.
  // Constraint: As return, only words (e.g. no whitespaces etc.) are allowed.
  virtual char *ShowInputDialog(const char *msg);

  // Adds a messagebox to the SVWindow. This way, it can show the messages...
  virtual void AddMessageBox();

  virtual void vAddMessage(fmt::string_view format, fmt::format_args args);

  // Zoom the window to the rectangle given upper left corner and
  // lower right corner.
  virtual void ZoomToRectangle(int x1, int y1, int x2, int y2);

  // Custom messages (manipulating java code directly) can be send through this.
  // Send a message to the server and attach the Id of the corresponding window.
  // Note: This should only be called if you are know what you are doing, since
  // you are fiddling with the Java objects on the server directly. Calling
  // this just for fun will likely break your application!
  // It is public so you can actually take use of the LUA functionalities, but
  // be careful!
  virtual void vSendMsg(fmt::string_view format, fmt::format_args args);

  /*******************************************************************************
   * Add new menu entries to parent. If parent is "", the entry gets added to
   *the main menubar (toplevel).
   *******************************************************************************/

   // This adds a new submenu to the menubar.
  virtual void MenuItem(const char *parent, const char *name);

  // This adds a new (normal) menu entry with an associated eventID, which
  // should be unique among menubar eventIDs.
  virtual void MenuItem(const char *parent, const char *name, int cmdEvent);

  // This adds a new checkbox entry, which might initially be flagged.
  virtual void MenuItem(const char *parent, const char *name, int cmdEvent,
                        bool flagged);

  // This adds a new popup submenu to the popup menu. If parent is "", the entry
  // gets added at "toplevel" popupmenu.
  virtual void PopupItem(const char *parent, const char *name);

  // This adds a new popup entry with the associated eventID, which should be
  // unique among popup eventIDs.
  // If value and desc are given, on a click the server will ask you to modify
  // the value and return the new value.
  virtual void PopupItem(const char *parent, const char *name, int cmdEvent,
                         const char *value, const char *desc);

  // Returns the correct Y coordinate for a window, depending on whether it
  // might have to be flipped (by ySize).
  virtual int TranslateYCoordinate(int y);

  virtual char Wait();

protected:
  // Sets up ScrollView, depending on the variables from the constructor.
  virtual void Initialize(Tesseract *tess, const char *name, int x_pos,
                          int y_pos, int x_size, int y_size, int x_canvas_size,
                          int y_canvas_size, bool y_axis_reversed,
                          const char *server_name);

  // Send the current buffered polygon (if any) and clear it.
  virtual void SendPolygon();

  // Start the message receiving thread.
  static void MessageReceiver();

  // Place an event into the event_table (synchronized).
  virtual void SetEvent(const SVEvent *svevent);

  // Wake up the semaphore.
  virtual void Signal();

  // Starts a new event handler.
  // Called asynchronously whenever a new window is created.
  virtual void StartEventHandler();

  void PrepCanvas(void);

protected:
  Image pix;
  l_uint32 pen_color = 0;
  l_uint32 brush_color = 0;
  int x_offset = 0;
  int y_offset = 0;
  bool dirty = false;

};

#endif // !GRAPHICS_DISABLED

/////////////////////////////////////////////////////////////////////////

#if !GRAPHICS_DISABLED

// The DummyScrollView class is a 'null' sink for all things ScrollView.
class TESS_API DummyScrollView : public ScrollView {
public:
	using Color = Diagnostics::Color;

	virtual ~DummyScrollView();

	// Create a window. The pixel size of the window may be 0,0, in which case
	// a default size is selected based on the size of your canvas.
	// The canvas may not be 0,0 in size!
	// With a flag whether the x axis is reversed.
	// Connect to a server other than localhost.
	DummyScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos,
		int x_size, int y_size, int x_canvas_size, int y_canvas_size,
		bool y_axis_reversed, const char *server_name);

public:
	/*******************************************************************************
	* Event handling
	* To register as listener, the class has to derive from the SVEventHandler
	* class, which consists of a notifyMe(SVEvent*) function that should be
	* overwritten to process the event the way you want.
	*******************************************************************************/

	// Add an Event Listener to this ScrollView Window.
	virtual void AddEventHandler(SVEventHandler *listener);

	// Block until an event of the given type is received.
	virtual std::unique_ptr<SVEvent> AwaitEvent(SVEventType type);

	/*******************************************************************************
	* API functions for LUA calls
	* the implementations for these can be found in svapi.cc
	* (keep in mind that the window is actually created through the ScrollView
	* constructor, so this is not listed here)
	*******************************************************************************/

	// Add comment
	virtual void Comment(std::string msg);

	// Draw an image on (x,y).
	virtual void Draw(Image image, int x_pos, int y_pos, const char *title);

	// Helper function to exit the program.
	virtual void ExitHelper();

	// Update the contents of a specific window.
	virtual void UpdateWindow();

	// Erase all content from the window, but do not destroy it.
	virtual void Clear();

	// Set pen color with an enum.
	virtual void Pen(Color color);

	// Set pen color to RGB (0-255).
	virtual void Pen(int red, int green, int blue);

	// Set pen color to RGBA (0-255).
	virtual void Pen(int red, int green, int blue, int alpha);

	// Set brush color with an enum.
	virtual void Brush(Color color);

	// Set brush color to RGB (0-255).
	virtual void Brush(int red, int green, int blue);

	// Set brush color to RGBA (0-255).
	virtual void Brush(int red, int green, int blue, int alpha);

	// Set attributes for future text, like font name (e.g.
	// "Times New Roman"), font size etc..
	// Note: The underlined flag is currently not supported
	virtual void TextAttributes(const char *font, int pixel_size, bool bold,
		bool italic, bool underlined);

	// Set up a X/Y offset for the subsequent drawing primitives.
	virtual void SetXYOffset(int x, int y);

	// Draw line from (x1,y1) to (x2,y2) with the current pencolor.
	virtual void Line(int x1, int y1, int x2, int y2);

	// Set the stroke width of the pen.
	virtual void Stroke(float width);

	// Draw a rectangle given upper left corner and lower right corner.
	// The current pencolor is used as outline, the brushcolor to fill the shape.
	virtual void Rectangle(int x1, int y1, int x2, int y2);

	// Draw an ellipse centered on (x,y).
	// The current pencolor is used as outline, the brushcolor to fill the shape.
	virtual void Ellipse(int x, int y, int width, int height);

	// Draw text with the current pencolor
	virtual void Text(int x, int y, const char *mystring);

	// Draw an image from a local filename. This should be faster than
	// createImage. WARNING: This only works on a local machine. This also only
	// works image types supported by java (like bmp,jpeg,gif,png) since the image
	// is opened by the server.
	virtual void Draw(const char *image, int x_pos, int y_pos);

	// Set the current position to draw from (x,y). In conjunction with...
	virtual void SetCursor(int x, int y);

	// ...this function, which draws a line from the current to (x,y) and then
	// sets the new position to the new (x,y), this can be used to easily draw
	// polygons using vertices
	virtual void DrawTo(int x, int y);

	// Set the SVWindow visible/invisible.
	virtual void SetVisible(bool visible);

	// Set the SVWindow always on top or not always on top.
	virtual void AlwaysOnTop(bool b);

	// Shows a modal dialog with "msg" as question and returns 'y' or 'n'.
	virtual int ShowYesNoDialog(const char *msg);

	// Shows a modal dialog with "msg" as question and returns a char* string.
	// Constraint: As return, only words (e.g. no whitespaces etc.) are allowed.
	virtual char *ShowInputDialog(const char *msg);

	// Adds a messagebox to the SVWindow. This way, it can show the messages...
	virtual void AddMessageBox();

	virtual void vAddMessage(fmt::string_view format, fmt::format_args args);

	// Zoom the window to the rectangle given upper left corner and
	// lower right corner.
	virtual void ZoomToRectangle(int x1, int y1, int x2, int y2);

	// Custom messages (manipulating java code directly) can be send through this.
	// Send a message to the server and attach the Id of the corresponding window.
	// Note: This should only be called if you are know what you are doing, since
	// you are fiddling with the Java objects on the server directly. Calling
	// this just for fun will likely break your application!
	// It is public so you can actually take use of the LUA functionalities, but
	// be careful!
	virtual void vSendMsg(fmt::string_view format, fmt::format_args args);

	/*******************************************************************************
	* Add new menu entries to parent. If parent is "", the entry gets added to
	*the main menubar (toplevel).
	*******************************************************************************/

	// This adds a new submenu to the menubar.
	virtual void MenuItem(const char *parent, const char *name);

	// This adds a new (normal) menu entry with an associated eventID, which
	// should be unique among menubar eventIDs.
	virtual void MenuItem(const char *parent, const char *name, int cmdEvent);

	// This adds a new checkbox entry, which might initially be flagged.
	virtual void MenuItem(const char *parent, const char *name, int cmdEvent,
		bool flagged);

	// This adds a new popup submenu to the popup menu. If parent is "", the entry
	// gets added at "toplevel" popupmenu.
	virtual void PopupItem(const char *parent, const char *name);

	// This adds a new popup entry with the associated eventID, which should be
	// unique among popup eventIDs.
	// If value and desc are given, on a click the server will ask you to modify
	// the value and return the new value.
	virtual void PopupItem(const char *parent, const char *name, int cmdEvent,
		const char *value, const char *desc);

	// Returns the correct Y coordinate for a window, depending on whether it
	// might have to be flipped (by ySize).
	virtual int TranslateYCoordinate(int y);

	virtual char Wait();

protected:
	// Sets up ScrollView, depending on the variables from the constructor.
	virtual void Initialize(Tesseract *tess, const char *name, int x_pos,
		int y_pos, int x_size, int y_size, int x_canvas_size,
		int y_canvas_size, bool y_axis_reversed,
		const char *server_name);

	// Send the current buffered polygon (if any) and clear it.
	virtual void SendPolygon();

	// Start the message receiving thread.
	static void MessageReceiver();

	// Place an event into the event_table (synchronized).
	virtual void SetEvent(const SVEvent *svevent);

	// Wake up the semaphore.
	virtual void Signal();

	// Starts a new event handler.
	// Called asynchronously whenever a new window is created.
	virtual void StartEventHandler();

	void PrepCanvas(void);

};

#endif // !GRAPHICS_DISABLED

#if !GRAPHICS_DISABLED

// singleton
class TESS_API ScrollViewManager {
protected:
  ScrollViewManager();
  static ScrollViewManager &GetScrollViewManager();

public:
  ~ScrollViewManager();

  static ScrollViewReference MakeScrollView(Tesseract *tess, const char *name, int x_pos, int y_pos, int x_size, int y_size, int x_canvas_size, int y_canvas_size, bool y_axis_reversed = false, const char *server_name = "localhost");

  // set this instance to be the latest active one
  static void SetActiveTesseractInstance(Tesseract *tess);
  // add this instance to the list of active tesseract instance but don't put it on top yet...
  static void AddActiveTesseractInstance(Tesseract *tess);
  // remove the given instance from the active set as its object is currently being destroyed.
  static void RemoveActiveTesseractInstance(Tesseract *tess);

  static Tesseract *GetActiveTesseractInstance();

private:
  Tesseract *active;    // reference to active instance
  std::list<Tesseract *> active_set;

protected:

};

#else

typedef void* ScrollViewReference;

#endif // !GRAPHICS_DISABLED

} // namespace tesseract

#endif // TESSERACT_VIEWER_SCROLLVIEW_H_
