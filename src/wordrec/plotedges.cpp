/******************************************************************************
 *
 * File:         plotedges.cpp  (Formerly plotedges.c)
 * Description:  Graphics routines for "Edges" and "Outlines" windows
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

#include "plotedges.h"

#include "render.h"
#include "split.h"

#if !GRAPHICS_DISABLED

namespace tesseract {

/*----------------------------------------------------------------------
              V a r i a b l e s
----------------------------------------------------------------------*/
ScrollViewReference edge_window;

/*----------------------------------------------------------------------
              F u n c t i o n s
----------------------------------------------------------------------*/
/**********************************************************************
 * display_edgepts
 *
 * Macro to display edge points in a window.
 **********************************************************************/
void display_edgepts(LIST outlines) {
  /* Set up window */
  if (!edge_window) {
    edge_window = ScrollViewManager::MakeScrollView(TESSERACT_NULLPTR, "Edges", 750, 150, 400, 128, 800, 256, true);
    edge_window->RegisterGlobalRefToMe(&edge_window);
  } else {
    edge_window->Clear();
  }
  /* Render the outlines */
  ScrollViewReference &window = edge_window;
  /* Reclaim old memory */
  iterate(outlines) {
    render_edgepts(window, reinterpret_cast<EDGEPT *>(outlines->first_node()), Diagnostics::WHITE);
  }
}

/**********************************************************************
 * draw_blob_edges
 *
 * Display the edges of this blob in the edges window.
 **********************************************************************/
void draw_blob_edges(TBLOB *blob) {
  if (wordrec_display_splits) {
    LIST edge_list = NIL_LIST;
    for (TESSLINE *ol = blob->outlines; ol != nullptr; ol = ol->next) {
      edge_list = push(edge_list, ol->loop);
    }
    display_edgepts(edge_list);
    destroy(edge_list);
  }
}

/**********************************************************************
 * mark_outline
 *
 * Make a mark on the edges window at a particular location.
 **********************************************************************/
void mark_outline(EDGEPT *edgept) { /* Start of point list */
  ScrollViewReference &window = edge_window;
  float x = edgept->pos.x;
  float y = edgept->pos.y;

  window->Pen(Diagnostics::RED);
  window->SetCursor(x, y);

  x -= 4;
  y -= 12;
  window->DrawTo(x, y);

  x -= 2;
  y += 4;
  window->DrawTo(x, y);

  x -= 4;
  y += 2;
  window->DrawTo(x, y);

  x += 10;
  y += 6;
  window->DrawTo(x, y);

  window->UpdateWindow();
}

} // namespace tesseract

#endif // !GRAPHICS_DISABLED
