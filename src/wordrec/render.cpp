/******************************************************************************
 *
 * File:         render.cpp  (Formerly render.c)
 * Description:  Convert the various data type into line lists
 * Author:       Mark Seaman, OCR Technology
 *
 * (c) Copyright 1989, Hewlett-Packard Company.
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 *
 *****************************************************************************/

// Include automatically generated configuration file if running autoconf.
#include <tesseract/preparation.h> // compiler config, etc.

#include "render.h"

#include "blobs.h"

#include <cmath>

namespace tesseract {

/*----------------------------------------------------------------------
              V a r i a b l e s
----------------------------------------------------------------------*/
ScrollViewReference blob_window;

Diagnostics::Color color_list[] = {Diagnostics::RED,  Diagnostics::CYAN,  Diagnostics::YELLOW,
                                  Diagnostics::BLUE, Diagnostics::GREEN, Diagnostics::WHITE};

BOOL_VAR(wordrec_display_all_blobs, 0, "Display Blobs");

BOOL_VAR(wordrec_blob_pause, 0, "Blob pause");

/*----------------------------------------------------------------------
              F u n c t i o n s
----------------------------------------------------------------------*/
#if !GRAPHICS_DISABLED
/**********************************************************************
 * display_blob
 *
 * Macro to display blob in a window.
 **********************************************************************/
void display_blob(TBLOB *blob, Diagnostics::Color color) {
  /* Size of drawable */
  if (!blob_window) {
    blob_window = ScrollViewManager::MakeScrollView(TESSERACT_NULLPTR, "Blobs", 520, 10, 500, 256, 2000, 256, true);
    blob_window->RegisterGlobalRefToMe(&blob_window);
  } else {
    blob_window->Clear();
  }

  render_blob(blob_window, blob, color);
}

/**********************************************************************
 * render_blob
 *
 * Create a list of line segments that represent the expanded outline
 * that was supplied as input.
 **********************************************************************/
void render_blob(ScrollViewReference &window, TBLOB *blob, Diagnostics::Color color) {
  /* No outline */
  if (!blob) {
    return;
  }

  render_outline(window, blob->outlines, color);
}

/**********************************************************************
 * render_edgepts
 *
 * Create a list of line segments that represent the expanded outline
 * that was supplied as input.
 **********************************************************************/
void render_edgepts(ScrollViewReference &window, EDGEPT *edgept, Diagnostics::Color color) {
  if (!edgept) {
    return;
  }

  float x = edgept->pos.x;
  float y = edgept->pos.y;
  EDGEPT *this_edge = edgept;

  window->Pen(color);
  window->SetCursor(x, y);
  do {
    this_edge = this_edge->next;
    x = this_edge->pos.x;
    y = this_edge->pos.y;
    window->DrawTo(x, y);
  } while (edgept != this_edge);
}

/**********************************************************************
 * render_outline
 *
 * Create a list of line segments that represent the expanded outline
 * that was supplied as input.
 **********************************************************************/
void render_outline(ScrollViewReference &window, TESSLINE *outline, Diagnostics::Color color) {
  /* No outline */
  if (!outline) {
    return;
  }
  /* Draw Compact outline */
  if (outline->loop) {
    render_edgepts(window, outline->loop, color);
  }
  /* Add on next outlines */
  render_outline(window, outline->next, color);
}

#endif // !GRAPHICS_DISABLED

} // namespace tesseract
