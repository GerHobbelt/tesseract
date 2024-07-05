// UTF-8: ö,ä,💩

#include "debugpixa.h"
#include "image.h"
#include "tprintf.h"
#include "tesseractclass.h"

#include <leptonica/allheaders.h>

#include <string>
#include <vector>
#include <chrono>  // chrono::system_clock
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip> // put_time


#if defined(HAVE_MUPDF)
#include "mupdf/fitz.h"
#endif

#ifndef TESSERACT_DISABLE_DEBUG_FONTS 
#define TESSERACT_DISABLE_DEBUG_FONTS 1
#endif

namespace tesseract {

#if defined(HAVE_MUPDF)

  static inline bool findPos(const std::vector<unsigned int> &arr, unsigned int value) {
    for (const auto &elem : arr) {
      if (elem == value) {
        return true;
      }
    }
    return false;
  }

  static void add_inline_particle_as_html(std::ostringstream &dst, const char *message, size_t length) {
    if (length == 0)
      return;
    const char *start = message;
    // sneaky: we 'know' we will only need this for modifiable content that was
    // sent to us, i.e. we're trampling through a std::string internal buffer or similar. 
    // (Like with the stuff that's coming out of do_transmit_logline().)
    // 
    // Ultimately we don't change anything, so we're legally safe, but it is assuming some things 
    // (such as message not pointing at protected/ROM/non-modifiable data space) and this is
    // therefor a nasty bit of code, that Just Works(tm) with minimal overhead and easier to read
    // internals than when we were to do this 'clean' with a lot of 'are we there yet' checks vs.
    // the NUL sentinel check we now permit ourselves to get away with!
    char sentinel_backup = 0;
    if (length != UINT_MAX)
      sentinel_backup = start[length];
    // hence, we only *temporarily* violate the higher expectations of `const char *message` when we 
    // actually need to do the temporary 'plug in a NUL sentinel for now' work.
    if (sentinel_backup) {
      const_cast<char *>(start)[length] = 0;
    }
    // now we can act as if the incoming message string is always neatly NUL-sentinelled like any good ol' C string.
    while (*message) {
      auto pos = strcspn(message, "\n<>&`*\t");
      if (pos > 0) {
        std::string_view particle(message, pos);
        dst << particle;
        message += pos;
      }
      switch (*message) {
        case 0:
          break;

        case '<':
          dst << "&lt;";
          message++;
          continue;

        case '>':
          dst << "&gt;";
          message++;
          continue;

        case '&':
          dst << "&amp;";
          message++;
          continue;

        case '\n':
          dst << "\n<br>";
          message++;
          continue;

        case '`':
          // markdown-ish in-line code phrase...
          // which, in our case, cannot break across lines as we only use this for
          // simple stuff.
          //
          // furthermore, we require it's preceded by whitespace or punctuation and trailed by whitespace or punctuation, such as in `demo-with-colon-at-end`:...
          //
          // Re the condition used below:
          //     (isalnum(message[-1]) || message[+1] == message[-1])
          // is to also catch when we're dealing with 'quoted' backquotes, such as in: '`', as then the quotes of any kind will both follow AND precede the backtick
          if (message > start && !(isalnum(message[-1]) || message[+1] == message[-1])) {
            pos = 0;
            do {
              pos++;
              auto pos2 = strcspn(message + pos, "\n`");
              pos += pos2;
            } while (message[pos] == '`' && message[pos + 1] != 0 &&
                     (isalnum(message[pos + 1]) || message[pos + 1] == message[pos - 1]));

            if (message[pos] == '`') {
              dst << "<code>";
              std::string_view particle(message + 1, pos - 1);
              dst << particle;
              dst << "</code>";
              message += pos + 1;
              continue;
            }
          }
          dst << '`';
          message++;
          continue;

        case '*':
          // markdown-ish in-line emphasis phrase...
          // which, in our case, cannot break across lines as we only use this for
          // simple stuff.
          //
          // furthermore, we require it's preceded (and trailed) by whitespace:
          if (message > start && isspace(message[-1]) && !isspace(message[1])) {
            pos = 0;
            do {
              pos++;
              auto pos2 = strcspn(message + pos, "\n*");
              pos += pos2;
            } while (message[pos] == '*' && !(message[pos + 1] == 0 || isspace(message[pos + 1])));

            if (message[pos] == '*') {
              dst << "<em>";
              std::string_view particle(message + 1, pos - 1);
              dst << particle;
              dst << "</em>";
              message += pos + 1;
              continue;
            }
          }
          dst << '*';
          message++;
          continue;

        case '\t':
          dst << "<code>TAB</code>";
          message++;
          continue;
      }
    }
    // and now that we are done, we must undo our violation of `const char *message`:
    if (sentinel_backup) {
      const_cast<char *>(start)[length] = sentinel_backup;
    }
  }

  // expected line format is for paramters report:
  //
  //    bidi_debug.................................................. (Global) .R [Integer] = 0
  //
  struct dots_marker {
    const char *start;
    const char *end;
  };

  static dots_marker find_start_of_leaderdots(const char *line) {
    dots_marker nil{nullptr};
    if (isalnum(*line) || *line == '_') {
      const char *som;
      line++;
      while (isalnum(*line) || *line == '_')
        line++;
      const char *start = line;
      if (*line++ != '.')
        return nil;
      if (*line++ != '.')
        return nil;
      if (*line++ != '.')
        return nil;
      while (*line == '.')
        line++;
      if (*line++ != ' ')
        return nil;
      if (*line != '(')
        return nil;
      return {.start = start, .end = line};
    }
    return nil;
  }

    
  static void add_encoded_as_html(std::ostringstream &dst, const char *style, const char *message) {
    if (*message == 0)
      return;

    // when messages carry embedded TABs, they may well be tables and we should help the user a little by rendering them to HTML as such.
    if (strchr(message, '\t')) {
      // scan to see if this could serve as a (multiline?) table.
      std::vector<unsigned int> LF_positions;
      std::vector<std::vector<unsigned int>> TAB_positions(1);
      std::vector<unsigned int> *active_TABs = &TAB_positions[0];
      unsigned int tab_count_per_line_min = UINT_MAX;
      unsigned int tab_count_per_line_max = 0;
      unsigned int tab_count_avg = 0;
      unsigned int tab_count = 0;
      unsigned int line_count = 0;
      size_t pos = 0;
      size_t last_LF = 0;
      for (;;) {
        auto pos2 = strcspn(message + pos, "\t\n");
        pos += pos2;
        switch (message[pos]) {
          case '\t':
            active_TABs->push_back(pos);

            tab_count++;
            if (tab_count > tab_count_per_line_max)
              tab_count_per_line_max = tab_count;
            pos++;
            continue;

          case '\n':
            LF_positions.push_back(pos);
            
            active_TABs->push_back(pos);   // end of line marker
            TAB_positions.push_back({});
            active_TABs = &TAB_positions[TAB_positions.size() - 1];

            last_LF = pos;
            // don't count empty lines when we check for the minimum # of tabs per line:
            if (pos2 > 0) {
              if (tab_count_per_line_min > tab_count)
                tab_count_per_line_min = tab_count;
              line_count++;
              tab_count_avg += tab_count;
              tab_count = 0;
            }
            pos++;
            continue;

          case 0:
            if (pos > last_LF + 1) {
              // end of line marker: SENTINEL if last part was non-empty, 
              // i.e. when we have something left to print after the 
              // last '\n' LF.
              active_TABs->push_back(pos); 
              LF_positions.push_back(pos); // ditto
            }
            TAB_positions.push_back({});  // end of entire chunk: EOF
            LF_positions.push_back(pos); // ditto

            // don't count empty lines when we check for the minimum # of tabs per line:
            if (pos2 > 0) {
              if (tab_count_per_line_min > tab_count)
                tab_count_per_line_min = tab_count;
              line_count++;
              tab_count_avg += tab_count;
            }
            break;
        }
        break;
      }

      // heuristic: tolerate a very few TABs as if it were regular content?
      //
      // We tolerate about 50% lines without any TAB as a rough heuristic.
      tab_count_avg *= 128;
      tab_count_avg /= line_count;
      if (tab_count_avg >= 64) {
        tab_count_per_line_max++; // turns the max TAB count in a max column count.

        // Do it as a table; total column count equals tab_count_per_line_max
        int line = 0;

        active_TABs = &TAB_positions[line];
        unsigned int line_spos = 0;
        unsigned int line_epos = line_epos = LF_positions[line];
        while (active_TABs->size() > 0) {
          const char *end_plug = nullptr;

          while (active_TABs->size() == 1) {
            // special treatment for leader lines: treat these as NOT part of the table.
            if (!end_plug) {
              if (style)
                dst << "<p class=\"" << style << "\">";
              else
                dst << "<p>";
            } else {
              dst << end_plug;
            }
            add_inline_particle_as_html(dst, message + line_spos, line_epos - line_spos);
            line++;
            line_spos = line_epos + 1;
            line_epos = LF_positions[line];
            active_TABs = &TAB_positions[line];
            
            end_plug = "\n<br>\n";
          }

          if (end_plug)
            dst << "</p>\n";

          // hit the end sentinel? if so, end it all.
          if (active_TABs->size() == 0)
            break;

          // first row is a header row(? --> nope, guess not...)
          const char *td_elem = "td";

          dst << "<table>\n";

          int tab = 0;
          while (active_TABs->size() > 1) {
            dst << "<tr>\n";

            tab = 0;
            unsigned int tab_spos = line_spos;
            unsigned int tab_epos = (*active_TABs)[tab];
            unsigned int colcount = active_TABs->size();
            while (tab < colcount - 1) {
              dst << "<" << td_elem << ">";
              add_inline_particle_as_html(dst, message + tab_spos, tab_epos - tab_spos);
              dst << "</" << td_elem << ">";
              tab++;
              tab_spos = tab_epos + 1;
              tab_epos = (*active_TABs)[tab];
            }

            {
              dst << "<" << td_elem << " colspan=\""
                  << (tab_count_per_line_max - tab) << "\">";
              add_inline_particle_as_html(dst, message + tab_spos, tab_epos - tab_spos);
              dst << "</" << td_elem << "></tr>\n";
            }

            line++;
            line_spos = line_epos + 1;
            line_epos = LF_positions[line];
            active_TABs = &TAB_positions[line];
          }

          dst << "</table>\n";
        }
        return;
      }
      // otherwise treat it as regular content. The TAB will be encoded then...
    }

    if (0 == strncmp(message, "PROCESS: ", 9)) {
      if (style)
        dst << "<blockquote class=\"" << style << "\">\n";
      else
        dst << "<blockquote>\n";
      message += 9;

      add_encoded_as_html(dst, style, message);

      dst << "</blockquote>\n\n";
      return;
    } 

    if (0 == strncmp(message, "ERROR: ", 7)) {
      if (style)
        dst << "<p class=\"error-message " << style << "\">";
      else
        dst << "<p class=\"error-message\">";

      const char *pbreak = strstr(message, "\n\n");
      if (pbreak) {
        add_inline_particle_as_html(dst, message, pbreak - message);
        dst << "</p>\n\n";
        message = pbreak;
        while (message[0] == '\n')
          message++;
        // deal with the remainder in this tail recursion call.
        add_encoded_as_html(dst, style, message);
      } else {
        add_inline_particle_as_html(dst, message, UINT_MAX);
        dst << "</p>\n\n";
      }
      return;
    } 
    if (0 == strncmp(message, "WARNING: ", 9)) {
      if (style)
        dst << "<p class=\"warning-message " << style << "\">";
      else
        dst << "<p class=\"warning-message\">";

      const char *pbreak = strstr(message, "\n\n");
      if (pbreak) {
        add_inline_particle_as_html(dst, message, pbreak - message);
        dst << "</p>\n\n";
        message = pbreak;
        while (message[0] == '\n')
          message++;
        // deal with the remainder in this tail recursion call.
        add_encoded_as_html(dst, style, message);
      } else {
        add_inline_particle_as_html(dst, message, UINT_MAX);
        dst << "</p>\n\n";
      }
      return;
    } 

    // skip leading newlines before we check a few more options for alternative content coming through here:
    while (message[0] == '\n')
      message++;

    if (0 == strncmp(message, "* ", 2) && isalpha(message[2])) {
      // see if this is a LIST instead of a paragraph:
      const char *next = strchr(message + 3, '\n');
      if (next && 0 == strncmp(next + 1, "* ", 2) && isalpha(next[3])) {
        dots_marker dots = find_start_of_leaderdots(message + 3);
        dots_marker dots_next = find_start_of_leaderdots(next + 4);
        if (dots.start && dots.end && dots_next.start && dots_next.end) {
          // Yes, we're looking at a list of at least two elements: deal with it now!
          if (style)
            dst << "<ul class=\"leaders " << style << "\">\n";
          else
            dst << "<ul class=\"leaders\">\n";
          for (;;) {
            message += 2;
            dst << "<li class=\"paramreport_line\">\n<span class=\"paramreport_itemname\">";
            add_inline_particle_as_html(dst, message, dots.start - message);
            dst << "</span><span class=\"paramreport_itemspec\">";
            add_inline_particle_as_html(dst, dots.end, next - dots.end);
            dst << "</li\">\n";
            message = next + 1;
            if (0 != strncmp(message, "* ", 2) || !isalpha(message[2])) {
              break;
            }
            dots = find_start_of_leaderdots(message + 3);
            if (!dots.start) {
              break;
            }
            next = strchr(dots.end, '\n');
            if (!next) {
              break;
            }
          }
          dst << "</ul>\n\n";
          while (message[0] == '\n')
            message++;
          // deal with the remainder in this tail recursion call.
          add_encoded_as_html(dst, style, message);
          return;
        }
      }
    }
    if (message[0] == '#') {
      int pos = 1;
      while (message[pos] == '#')
        pos++;
      // we tolerate a few more than the 6 Heading levels available in HTML.
      if (pos <= 8 && message[pos] == ' ') {
        int state = pos;
        if (style)
          dst << "\n\n<h" << "12345666"[state] << " class=\"" << style << "\">";
        else
          dst << "\n\n<h" << "12345666"[state] << ">";
        pos++;
        while (message[pos] == ' ')
          pos++;
        message += pos;

        // heading is a single line, always, so we must check if there's stuff following!
        const char *nlp = strchr(message, '\n');
        if (nlp) {
          add_inline_particle_as_html(dst, message, nlp - message);
          dst << "</h" << "12345666"[state] << ">\n\n";
          message = nlp;
          while (message[0] == '\n')
            message++;
          // deal with the remainder in this tail recursion call.
          add_encoded_as_html(dst, style, message);
        } else {
          add_inline_particle_as_html(dst, message, UINT_MAX);
          dst << "</h" << "12345666"[state] << ">\n\n";
        }
        return;
      }

      // more than 8 consecutive '#' is ... content?
    } 

    if (message[0] == '-') {
      int pos = 1;
      while (message[pos] == '-')
        pos++;
      if (pos > 8 && message[pos] == '\n') {
        if (style)
          dst << "\n\n<hr class=\"" << style << "\">\n\n";
        else
          dst << "\n\n<hr>\n\n";
        pos++;
        message += pos;
        while (message[0] == '\n')
          message++;
        // deal with the remainder in this tail recursion call.
        add_encoded_as_html(dst, style, message);
        return;
      }

      // more than 8 consecutive '#' is ... content?
    } 


    // next: if content is split by double/triple newline somewhere in the middle, than we're talking multiple paragraphs of content.
    const char *pbreak = strstr(message, "\n\n");
    if (pbreak) {
      if (style)
        dst << "<p class=\"" << style << "\">";
      else
        dst << "<p>";

      add_inline_particle_as_html(dst, message, pbreak - message);

      dst << "</p>\n\n";
      message = pbreak;
      while (message[0] == '\n')
        message++;
      // deal with the remainder in this tail recursion call.
      add_encoded_as_html(dst, style, message);
      return;
    }

    // nothing particular jumped at us, so now deal with this as a single block of content: 
    // one paragraph with perhaps a couple of line breaks, but nothing more complex than that.
    if (style)
      dst << "<p class=\"" << style << "\">";
    else
      dst << "<p>";

    add_inline_particle_as_html(dst, message, UINT_MAX);

    dst << "</p>\n\n";
  }

  void DebugPixa::fz_error_cb_tess_tprintf(fz_context *ctx, void *user, const char *message)
  {
    DebugPixa *self = (DebugPixa *)user;
    if (self->fz_cbs[0]) {
      (self->fz_cbs[0])(self->fz_ctx, self->fz_cb_userptr[0], message);
    }
    auto& f = self->GetInfoStream();
    add_encoded_as_html(f, "error", message);
  }

  void DebugPixa::fz_warn_cb_tess_tprintf(fz_context *ctx, void *user, const char *message)
  {
    DebugPixa *self = (DebugPixa *)user;
    if (self->fz_cbs[1]) {
      (self->fz_cbs[1])(self->fz_ctx, self->fz_cb_userptr[1], message);
    }
    auto& f = self->GetInfoStream();
    add_encoded_as_html(f, "warning", message);
  }

  void DebugPixa::fz_info_cb_tess_tprintf(fz_context *ctx, void *user, const char *message)
  {
    DebugPixa *self = (DebugPixa *)user;
    if (self->fz_cbs[2]) {
      (self->fz_cbs[2])(self->fz_ctx, self->fz_cb_userptr[2], message);
    }
    auto& f = self->GetInfoStream();
    add_encoded_as_html(f, nullptr, message);
  }
#endif

  DebugPixa::DebugPixa(Tesseract* tess)
      : tesseract_(tess), content_has_been_written_to_file(false)
  {
    pixa_ = pixaCreate(100);

#if defined(HAVE_MUPDF)
    fz_ctx = fz_get_global_context();
    fz_get_error_callback(fz_ctx, &fz_cbs[0], &fz_cb_userptr[0]);
    fz_get_warning_callback(fz_ctx, &fz_cbs[1], &fz_cb_userptr[1]);
    fz_get_info_callback(fz_ctx, &fz_cbs[2], &fz_cb_userptr[2]);

    fz_set_error_callback(fz_ctx, fz_error_cb_tess_tprintf, this);
    fz_set_warning_callback(fz_ctx, fz_warn_cb_tess_tprintf, this);
    fz_set_info_callback(fz_ctx, fz_info_cb_tess_tprintf, this);
#endif

    // set up the root info section:
    active_step_index = -1;
    PushNextSection("Start a tesseract run");

#ifdef TESSERACT_DISABLE_DEBUG_FONTS
    fonts_ = NULL;
#else
    fonts_ = bmfCreate(nullptr, 10);
#endif
  }

  // If the filename_ has been set and there are any debug images, they are
  // written to the set filename_.
  DebugPixa::~DebugPixa() {
    Clear(true);

#if defined(HAVE_MUPDF)
    fz_set_error_callback(fz_ctx, fz_cbs[0], fz_cb_userptr[0]);
    fz_set_warning_callback(fz_ctx, fz_cbs[1], fz_cb_userptr[1]);
    fz_set_info_callback(fz_ctx, fz_cbs[2], fz_cb_userptr[2]);
    fz_ctx = nullptr;
    memset(fz_cbs, 0, sizeof(fz_cbs));
    memset(fz_cb_userptr, 0, sizeof(fz_cb_userptr));
#endif

    pixaDestroy(&pixa_);
    bmfDestroy(&fonts_);
  }

  // Adds the given pix to the set of pages in the PDF file, with the given
  // caption added to the top.
  void DebugPixa::AddPix(const Image &pix, const char *caption) {
    TBOX dummy;
    AddPixInternal(pix, dummy, caption);
  }

  void DebugPixa::AddPixInternal(const Image &pix, const TBOX &bbox, const char *caption) {
    int depth = pixGetDepth(pix);
    ASSERT0(depth >= 1 && depth <= 32);
    {
      int depth = pixGetDepth(pix);
      ASSERT0(depth == 1 || depth == 8 || depth == 24 || depth == 32);
    }
#ifdef TESSERACT_DISABLE_DEBUG_FONTS
    pixaAddPix(pixa_, pix, L_COPY);
#else
    int color = depth < 8 ? 1 : (depth > 8 ? 0x00ff0000 : 0x80);
    Image pix_debug = pixAddSingleTextblock(pix, fonts_, caption, color, L_ADD_BELOW, nullptr);

    pixaAddPix(pixa_, pix_debug, L_INSERT);
#endif

    captions.push_back(caption);
    cliprects.push_back(bbox);

    // make sure follow-up log messages end up AFTER the imge in the output by dumping them in a subsequent info_chunk:
    auto &info_ref = info_chunks.emplace_back();
    info_ref.appended_image_index = captions.size(); // neat way to get the number of images: every image comes with its own caption
  }

  // Adds the given pix to the set of pages in the PDF file, with the given
  // caption added to the top.
  void DebugPixa::AddPixWithBBox(const Image &pix, const TBOX &bbox, const char *caption) {
    AddPixInternal(pix, bbox, caption);
  }

  void DebugPixa::AddPixWithBBox(const Image &pix, const char *caption) {
    TBOX bbox(pix);
    AddPixInternal(pix, bbox, caption);
  }

  // Return true when one or more images have been collected.
  bool DebugPixa::HasContent() const {
    return (pixaGetCount(pixa_) > 0 /* || steps.size() > 0    <-- see also notes at Clear() method; the logic here is that we'll only have something *useful* to report once we've collected one or more images along the way... */ );
  }

  int DebugPixa::PushNextSection(const std::string &title)
  {
    // sibling
    if (active_step_index < 0)
    {
      return PushSubordinateSection(title);
    }
    ASSERT0(steps.size() >= 1);
    ASSERT0(active_step_index < steps.size());
    auto& prev_step = steps[active_step_index];
    int prev_level = prev_step.level;
    if (prev_level == 0)
    {
      return PushSubordinateSection(title);
    }

    return PrepNextSection(prev_level, title);
  }

  int DebugPixa::PushSubordinateSection(const std::string &title)
  {
    // child (or root!)
    int prev_level = -1;
    if (active_step_index >= 0)
    {
      auto& prev_step = steps[active_step_index];
      prev_level = prev_step.level;
    }

    // child
    return PrepNextSection(prev_level + 1, title);
  }

  int DebugPixa::PrepNextSection(int level, const std::string &title)
  {
    auto& step_ref = steps.emplace_back();
    // sibling
    step_ref.level = level;
    step_ref.title = title;
    step_ref.first_info_chunk = info_chunks.size();
    //ASSERT0(!title.empty());

    int rv = active_step_index;
    if (rv < 0)
      rv = 0;

    active_step_index = steps.size() - 1;
    ASSERT0(active_step_index >= 0);

    auto& info_ref = info_chunks.emplace_back();
    info_ref.appended_image_index = captions.size();     // neat way to get the number of images: every image comes with its own caption

    return rv;
  }

  // Note: pop(0) pops all the way back up to the root.
  void DebugPixa::PopSection(int handle)
  {
    int idx = active_step_index;
    ASSERT0(steps.size() >= 1);
    ASSERT0(active_step_index >= 0);
    ASSERT0(active_step_index < steps.size());

    // return to parent
    auto& step = steps[idx];
    step.last_info_chunk = info_chunks.size() - 1;
    auto level = step.level - 1; // level we seek
    if (handle >= 0) {
      ASSERT0(handle < steps.size());
      auto &parent = steps[handle];
      //ASSERT0(parent.level <= std::max(0, level));

      // bingo!
      active_step_index = handle;

      // now all we need is a fresh info_chunk:
      auto &info_ref = info_chunks.emplace_back();
      info_ref.appended_image_index = captions.size(); // neat way to get the number of images: every image comes with its own caption
      return;
    }

    for (idx--; idx >= 0; idx--)
    {
      auto& prev_step = steps[idx];
      if (prev_step.level == level)
      {
        // bingo!
        active_step_index = idx;

        // now all we need is a fresh info_chunk:
        auto& info_ref = info_chunks.emplace_back();
        info_ref.appended_image_index = captions.size();     // neat way to get the number of images: every image comes with its own caption
        return;
      }
    }

    // when we get here, we're aiming below root, so we reset to last root-entry level:
    idx = steps.size();
    for (idx--; idx >= 0; idx--) {
      auto &prev_step = steps[idx];
      if (prev_step.level == 0) {
        // bingo!
        active_step_index = idx;

        // now all we need is a fresh info_chunk:
        auto &info_ref = info_chunks.emplace_back();
        info_ref.appended_image_index = captions.size(); // neat way to get the number of images: every image comes with its own caption
        return;
      }
    }
    ASSERT0(!"Should never get here!");
    return;
  }

  int DebugPixa::GetCurrentSectionLevel() const {
    int idx = active_step_index;
    ASSERT0(steps.size() >= 1);
    ASSERT0(active_step_index >= 0);
    ASSERT0(active_step_index < steps.size());

    auto &step = steps[idx];
    return step.level;
  }

  static char* strnrpbrk(char* base, const char* breakset, size_t len)
  {
    for (size_t i = len; i > 0; ) {
      if (strchr(breakset, base[--i]))
        return base + i;
    }
    return nullptr;
  }

  static std::string check_unknown(const std::string& s, const char* default_value = "(unknown / nil)") {
    if (s.empty())
      return default_value;
    return s;
  }

  static void SanitizeCaptionForFilenamePart(std::string& str) {
    auto len = str.size();
    char* s = str.data();
    char* d = s;
    char* base = s;

    for (int i = 0; i < len; i++, s++) {
      char c = *s;

      if (isalnum(c) || c == '_' || c == '-') {
        *d++ = *s;
      } else {
        if (d > base && d[-1] != '.')
          *d++ = '.';
      }
    }

    if (d > base && d[-1] == '.')
      d--;

    len = d - base;

    // heuristics: shorten the part to a sensible length
    while (len > 40) {
      char* pnt = strnrpbrk(base, ".-_", len);
      if (!pnt)
        break;

      len = pnt - base;
    }

    // hard clip if heuristic didn't deliver:
    if (len > 40) {
      len = 40;
    }

    str.resize(len);
  }

  static inline int FADE(int val, const int factor) {
    return (val * factor + 255 * (256 - factor)) >> 8 /* div 256 */;
  }

  static inline int MIX(int val1, int val2, const int factor) {
    return (val2 * factor + val1 * (256 - factor)) >> 8 /* div 256 */;
  }

  PIX *pixMixWithTintedBackground(PIX *src, const PIX *background,
                                  float r_factor, float g_factor, float b_factor,
                                  float src_factor, float background_factor, 
                                  const TBOX *cliprect) 
  {
    int w, h, depth;
    ASSERT0(src != nullptr);
    pixGetDimensions(src, &w, &h, &depth);

    if (background == nullptr) {
      return pixConvertTo32(src);
    } else {
      int ow, oh, od;
      pixGetDimensions(background, &ow, &oh, &od);

      Image toplayer = pixConvertTo32(src);
      Image botlayer = pixConvertTo32(const_cast<PIX*>(background));  // quick hack

      if (w != ow || h != oh) {
        if (cliprect != nullptr) {
          // when a TBOX is provided, you can bet your bottom dollar `src` is an 'extract' of `background`
          // and we therefore should paint it back onto there at the right spot:
          // the cliprectx/y coordinates will tell us!
          int cx, cy, cw, ch;
          cx = cliprect->left();
          cy = cliprect->top();
          cw = cliprect->width();
          ch = cliprect->height();

          // when the clipping rectangle indicates another area than we got in `src`, we need to scale `src` first:
          // 
          // smaller images are generally masks, etc. and we DO NOT want to be
          // confused by the smoothness introduced by regular scaling, so we
          // apply brutal sampled scale then:
          if (w != cw || h != ch) {
            if (w < cw && h < ch) {
              toplayer = pixScaleBySamplingWithShift(toplayer, cw * 1.0f / w, ch * 1.0f / h, 0.0f, 0.0f);
            } else if (w > cw && h > ch) {
              // the new image has been either scaled up vs. the original OR a
              // border was added (TODO)
              //
              // for now, we simply apply regular smooth scaling
              toplayer = pixScale(toplayer, cw * 1.0f / w, ch * 1.0f / h);
            } else {
              // scale a clipped partial to about match the size of the
              // original/base image, so the generated HTML + image sequence is
              // more, äh, uniform/readable.
#if 0
              ASSERT0(!"Should never get here! Non-uniform scaling of images collected in DebugPixa!");
#endif
              toplayer = pixScale(toplayer, cw * 1.0f / w, ch * 1.0f / h);
            }

            pixGetDimensions(toplayer, &w, &h, &depth);
          }
            // now composite over 30% grey: this is done by simply resizing to background using 30% grey as a 'border':
          int bl = cx;
          int br = ow - cx - cw;
          int bt = cy;
          int bb = oh - cy - ch;
          
          if (bl || br || bt || bb) {
            l_uint32 grey;
            const int g = int(0.7 * 256);
            (void)composeRGBAPixel(g, g, g, 256, &grey);
            toplayer = pixAddBorderGeneral(toplayer, bl, br, bt, bb, grey);
          }
        } else {
          // no cliprect specified, so `src` must be scaled version of
          // `background`.
          //
          // smaller images are generally masks, etc. and we DO NOT want to be
          // confused by the smoothness introduced by regular scaling, so we
          // apply brutal sampled scale then:
          if (w < ow && h < oh) {
            toplayer = pixScaleBySamplingWithShift(toplayer, ow * 1.0f / w, oh * 1.0f / h, 0.0f, 0.0f);
          } else if (w > ow && h > oh) {
            // the new image has been either scaled up vs. the original OR a
            // border was added (TODO)
            //
            // for now, we simply apply regular smooth scaling
            toplayer = pixScale(toplayer, ow * 1.0f / w, oh * 1.0f / h);
          } else {
            // scale a clipped partial to about match the size of the
            // original/base image, so the generated HTML + image sequence is
            // more, äh, uniform/readable.
#if 0
            ASSERT0(!"Should never get here! Non-uniform scaling of images collected in DebugPixa!");
#endif
            toplayer = pixScale(toplayer, ow * 1.0f / w, oh * 1.0f / h);
          }
        }
      }

      auto datas = pixGetData(toplayer);
      auto datad = pixGetData(botlayer);
      auto wpls = pixGetWpl(toplayer);
      auto wpld = pixGetWpl(botlayer);
      int i, j;
      for (i = 0; i < oh; i++) {
        auto lines = (datas + i * wpls);
        auto lined = (datad + i * wpld);
        for (j = 0; j < ow; j++) {
          // if top(SRC) is black, use that.
          // if top(SRC) is white, and bot(DST) isn't, color bot(DST) red and use
          // that. if top(SRC) is white, and bot(DST) is white, use white.

          // constant fade factors:
          const int red_factor = r_factor * 256;
          const int green_factor = g_factor * 256;
          const int blue_factor = g_factor * 256;
          const int base_mix_factor = src_factor * 256;
          const int bottom_mix_factor = background_factor * 256;

          int rvals, gvals, bvals;
          extractRGBValues(lines[j], &rvals, &gvals, &bvals);

          int rvald, gvald, bvald;
          extractRGBValues(lined[j], &rvald, &gvald, &bvald);

          // R
          int rval = FADE(rvald, red_factor);
          if (rvals < rval)
            rval = MIX(rvals, rval, bottom_mix_factor);
          else
            rval = MIX(rvals, rval, base_mix_factor);

          // G
          int gval = FADE(gvald, green_factor);
          if (gvals < gval)
            gval = MIX(gvals, gval, bottom_mix_factor);
          else
            gval = MIX(gvals, gval, base_mix_factor);

          // B
          int bval = FADE(bvald, blue_factor);
          if (bvals < bval)
            bval = MIX(bvals, bval, bottom_mix_factor);
          else
            bval = MIX(bvals, bval, base_mix_factor);

          // A
          // avald = 0;

          composeRGBPixel(rval, gval, bval, lined + j);
        }
      }
      // pixCopyResolution(pixd, pixs);
      // pixCopyInputFormat(pixd, pixs);

      // botlayer.destroy();
      toplayer.destroy();

      return botlayer;
    }
  }

  Image MixWithLightRedTintedBackground(const Image &pix, const PIX *original_image, const TBOX *cliprect) {
    return pixMixWithTintedBackground(pix, original_image, 0.1, 0.5, 0.5, 0.90, 0.085, cliprect);
  }

  static void write_one_pix_for_html(FILE* html, int counter, const char* img_filename, const Image& pix, const char* title, const char* description, const TBOX *cliprect = nullptr, const Pix* original_image = nullptr)
  {
    if (!!pix) {
    const char* pixfname = fz_basename(img_filename);
    int w, h, depth;
    pixGetDimensions(pix, &w, &h, &depth);
    const char* depth_str = ([depth]() {
      switch (depth) {
      default:
        ASSERT0(!"Should never get here!");
        return "unidentified color depth (probably color paletted)";
      case 1:
        return "monochrome (binary)";
      case 32:
        return "full color + alpha";
      case 24:
        return "full color";
      case 8:
        return "color palette (256 colors)";
      case 4:
        return "color palette (16 colors)";
      }
    })();

    Image img = MixWithLightRedTintedBackground(pix, original_image, cliprect);
    pixWrite(img_filename, img, IFF_PNG);
    img.destroy();

    fprintf(html, "<section>\n\
  <h6>image #%02d: %s</h6>\n\
  <figure>\n\
    <img src=\"%s\" >\n\
    <figcaption>size: %d x %d px; %s</figcaption>\n\
  </figure>\n\
  <p>%s</p>\n\
</section>\n",
      counter, title,
      pixfname,
      (int) w, (int) h, depth_str,
      description
    );
    }
  }

  void DebugPixa::WriteImageToHTML(int &counter, const std::string &partname, FILE *html, int idx) {
    counter++;
    char in[40];
    snprintf(in, 40, ".img%04d", counter);
    std::string caption = captions[idx];
    SanitizeCaptionForFilenamePart(caption);
    const char *cprefix = (caption.empty() ? "" : ".");
    std::string fn(partname + in + cprefix + caption + /* ext */ ".png");

    Image pixs = pixaGetPix(pixa_, idx, L_CLONE);
    if (pixs == nullptr) {
      tprintError("{}: pixs[{}] not retrieved.\n", __func__, idx);
      return;
    }
    {
      int depth = pixGetDepth(pixs);
      ASSERT0(depth == 1 || depth == 8 || depth == 24 || depth == 32);
    }
    TBOX cliprect = cliprects[idx];
    auto clip_area = cliprect.area();
    PIX *bgimg = nullptr;
    if (clip_area > 0) {
      bgimg = tesseract_->pix_original();
    }

    write_one_pix_for_html(html, counter, fn.c_str(), pixs, caption.c_str(), captions[idx].c_str());

    if (clip_area > 0 && false) {
      counter++;
      snprintf(in, 40, ".img%04d", counter);
      fn = partname + in + cprefix + caption + /* ext */ ".png";

      write_one_pix_for_html(html, counter, fn.c_str(), pixs, caption.c_str(),
                           captions[idx].c_str(), 
                           &cliprect,
                           bgimg);
    }

    pixs.destroy();
  }

  int DebugPixa::WriteInfoSectionToHTML(int &counter, int &next_image_index, const std::string &partname, FILE *html, int current_section_index) {
    DebugProcessStep &section_info = steps[current_section_index];

    auto title = section_info.title.c_str();
    if (!title || !*title)
      title = "(null)";
    auto h_level = section_info.level + 1;
    ASSERT0(h_level >= 1);
    if (h_level > 5)
      h_level = 5;
    fprintf(html, "\n\n<section>\n<h%d>%s</h%d>\n\n", h_level, title, h_level);

    int next_section_index = current_section_index + 1;
    DebugProcessStep &next_section_info = steps[next_section_index];

    int start_info_chunk_index = section_info.first_info_chunk;
    int last_info_chunk_index = section_info.last_info_chunk;
    for (int chunk_idx = start_info_chunk_index; chunk_idx <= last_info_chunk_index; chunk_idx++) {
      // make sure we don't dump info chunks which belong to sub-sections:
      if (chunk_idx == next_section_info.first_info_chunk) {
        next_section_index = WriteInfoSectionToHTML(counter, next_image_index, partname, html, next_section_index);
        chunk_idx = next_section_info.last_info_chunk;
        next_section_info = steps[next_section_index];
        continue;
      }
      DebugProcessInfoChunk &info_chunk = info_chunks[chunk_idx];
      auto v = info_chunk.information.str();
      auto content = v.c_str();
      if (content && *content) {
        fputs(content, html);
        fputs("\n\n", html);
      }

      // does this chunk end with an image?
      DebugProcessInfoChunk &next_info_chunk = info_chunks[chunk_idx + 1];
      if (info_chunk.appended_image_index != next_info_chunk.appended_image_index) {
        WriteImageToHTML(counter, partname, html, info_chunk.appended_image_index);
        if (next_image_index <= info_chunk.appended_image_index)
          next_image_index = info_chunk.appended_image_index + 1;
      }
    }

    fputs("\n</section>\n\n", html);

    return next_section_index;
  }

  
  void DebugPixa::WriteHTML(const char* filename) {
    ASSERT0(tesseract_ != nullptr);
    if (HasContent()) {
      const char *ext = strrchr(filename, '.');
      std::string partname(filename);
      partname = partname.substr(0, ext - filename);
      int counter = 0;
      const char *label = NULL;

      content_has_been_written_to_file = true;

      FILE *html;

#if defined(HAVE_MUPDF)
      fz_mkdir_for_file(fz_get_global_context(), filename);
      html = fz_fopen_utf8(fz_get_global_context(), filename, "w");
#else
      html = fopen(filename, "w");
#endif
      if (!html) {
        tprintError("cannot open diagnostics HTML output file %s: %s\n", filename, strerror(errno));
        return;
      }

      auto now = std::chrono::system_clock::now();
      auto in_time_t = std::chrono::system_clock::to_time_t(now);

      std::stringstream ss;
      ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
      std::string now_str = ss.str();

      std::ostringstream languages;
      int num_subs = tesseract_->num_sub_langs();
      if (num_subs > 0) {
        languages << "<p>Language";
        if (num_subs > 1)
          languages << "s";
        languages << ": ";
        int i;
        for (i = 0; i < num_subs - 1; ++i) {
          languages << tesseract_->get_sub_lang(i)->lang << " + ";
        }
        languages << tesseract_->get_sub_lang(i)->lang << "</p>";
      }

      // CSS styles for the generated HTML
      // 
      // - CSS UL list with classic leader dots: based on https://www.w3.org/Style/Examples/007/leaders.en.html
      //
      fprintf(html, "<html>\n\
<head>\n\
	<meta charset=\"UTF-8\">\n\
  <title>Tesseract diagnostic image set</title>\n\
  <link rel=\"stylesheet\" href=\"https://unpkg.com/normalize.css@8.0.1/normalize.css\" >\n\
  <link rel=\"stylesheet\" href=\"https://unpkg.com/modern-normalize@1.1.0/modern-normalize.css\" >\n\
  <style>\n\
    html {\n\
      margin: 1em 2em;\n\
    }\n\
    h1 {\n\
      font-size: 2.5em;\n\
    }\n\
    h2 {\n\
      font-size: 2em;\n\
    }\n\
    h3 {\n\
      font-size: 1.75em;\n\
    }\n\
    h4 {\n\
      font-size: 1.5em;\n\
    }\n\
    h5 {\n\
      font-size: 1.35em;\n\
    }\n\
    h6 {\n\
      font-size: 1.25em;\n\
    }\n\
    h2 {\n\
      margin-top: 4em;\n\
      border-top: 1px solid grey;\n\
      padding-top: 1em;\n\
    }\n\
    img {\n\
      border: solid #b0cfff .5em;\n\
      max-width: 70em;\n\
      margin-left: auto;\n\
      margin-right: auto;\n\
      display: block;\n\
    }\n\
    figcaption {\n\
      background-color: #325180;\n\
      color: #fff;\n\
      font-style: italic;\n\
      padding: .2em;\n\
      text-align: center;\n\
    }\n\
    figure {\n\
      max-width: 70em;\n\
      margin-left: 0;\n\
      background-color: #c5d5ed;\n\
    }\n\
    blockquote {\n\
      margin-left: 2em;\n\
      margin-right: 2em;\n\
      padding: 0.25em 4em;\n\
      border: solid 4px #b0cfff;\n\
      background-color: #ebf3ff;\n\
      max-width: 66em;\n\
    }\n\
    em {\n\
      background-color: #f7ffeb;\n\
    }\n\
    table {\n\
      border: solid 2px black;\n\
      text-align: left;\n\
      border-collapse: collapse;\n\
    }\n\
    th, td {\n\
      text-align: left;\n\
      border: solid 1px grey;\n\
      padding: 0.1em 0.5em;\n\
      border-left: none;\n\
      border-top: none;\n\
    }\n\
    table tr:nth-child(odd) {\n\
      background: #eeeff0;\n\
    }\n\
    table th:last-child, table td:last-child {\n\
      border-right: none;\n\
    }\n\
    table tr:last-child th, table tr:last-child td {\n\
      border-bottom: none;\n\
    }\n\
    code {\n\
      padding-left: 0.25em;\n\
      padding-right: 0.25em;\n\
      background-color: #daffdd;\n\
    }\n\
    ul.leaders {\n\
      max-width: 40em;\n\
      padding: 0;\n\
      overflow-x: hidden;\n\
      list-style: none;\n\
    }\n\
    ul.leaders li:before {\n\
      float: left;\n\
      width: 0;\n\
      white-space: nowrap;\n\
    content:\n\
      \". . . . . . . . . . . . . . . . . . . . \"\n\
      \". . . . . . . . . . . . . . . . . . . . \"\n\
      \". . . . . . . . . . . . . . . . . . . . \"\n\
      \". . . . . . . . . . . . . . . . . . . . \"\n\
    }\n\
    ul.leaders span:first-child {\n\
      padding-right: 0.33em;\n\
      background: white;\n\
    }\n\
    ul.leaders span + span {\n\
      float: right;\n\
      padding-left: 0.33em;\n\
      background: white;\n\
    }\n\
  </style>\n\
</head>\n\
<body>\n\
<article>\n\
<h1>Tesseract diagnostic image set</h1>\n\
<p>tesseract (version: %s) run @ %s</p>\n\
<p>Input image file path: %s</p>\n\
<p>Output base: %s</p>\n\
<p>Input image path: %s</p>\n\
<p>Primary Language: %s</p>\n\
%s\
<p>Language Data Path Prefix: %s</p>\n\
<p>Data directory: %s</p>\n\
<p>Main directory: %s</p>\n\
",
        TESSERACT_VERSION_STR, 
        now_str.c_str(),
        check_unknown(tesseract_->input_file_path).c_str(),
        check_unknown(tesseract_->imagebasename).c_str(),
        check_unknown(tesseract_->imagefile).c_str(),
        tesseract_->lang.c_str(),
        languages.str().c_str(),
        check_unknown(tesseract_->language_data_path_prefix).c_str(),
        check_unknown(tesseract_->datadir).c_str(),
        check_unknown(tesseract_->directory).c_str()
      );

      {
        std::string fn(partname + ".img-original.png");

        write_one_pix_for_html(html, 0, fn.c_str(), tesseract_->pix_original(), "original image", "The original image as registered with the Tesseract instance.");
      }

      // pop all levels and push a couple of *sentinels* so our tree traversal logic can be made simpler with far fewer boundary checks
      // as we'll have valid slots at size+1:
      PopSection(-2);
      active_step_index = -1;
      PushNextSection("");

      int section_count = steps.size() - 1;          // adjust size due to sentinel which was pushed at the end just now.
      int pics_count = pixaGetCount(pixa_);

      int next_image_index = 0;

      int current_section_index = 0; 
      while (current_section_index < section_count) {
        current_section_index = WriteInfoSectionToHTML(counter, next_image_index, partname, html, current_section_index);
      }

      for (int i = next_image_index; i < pics_count; i++) {
        WriteImageToHTML(counter, partname, html, i);
      }
      //pixaClear(pixa_);

      fputs("\n<hr>\n<h2>Tesseract parameters usage report</h2>\n\n", html);
      
      tesseract::ParamsVectors *vec = tesseract_->params();

      // produce a HTML-formatted parameter usage report by using the regular way to get such a report,
      // then feed it through the NDtext-to-HTML transformer and only then write the final result in one fell swoop to file.
      // 
      // Takes a bit of regrettable extra string copying this way, but alas, we'll take one for code re-use.
      ParamsReportStringWriter writer;
      ParamUtils::ReportParamsUsageStatistics(writer, vec, -1, nullptr);
      std::ostringstream html_report_dst;
      add_encoded_as_html(html_report_dst, "params-report", writer.to_string().c_str());
      fputs(languages.str().c_str(), html);

      fputs("\n</body>\n</html>\n", html);

      fclose(html);
    }
  }


  void DebugPixa::WriteSectionParamsUsageReport()
  {
    DebugProcessStep &section_info = steps[active_step_index];

    auto title = section_info.title.c_str();
    if (!title || !*title)
      title = "(null)";
    auto level = section_info.level;

    if (level == 3 && verbose_process) {
      tesseract::ParamsVectors *vec = tesseract_->params();
      ParamUtils::ReportParamsUsageStatistics(nullptr, vec, level, title);
    }
  }


  void DebugPixa::Clear(bool final_cleanup)
  {
    final_cleanup |= content_has_been_written_to_file;
    pixaClear(pixa_);
    captions.clear();
    // NOTE: we only clean the steps[] logging blocks when we've been ascertained that 
    // this info has been pumped into a logfile (or stdio stream) via our WriteHTML() method....
    if (final_cleanup) {
        steps.clear();
        active_step_index = -1;
    }
  }


  AutoPopDebugSectionLevel::~AutoPopDebugSectionLevel() {
    if (section_handle_ >= 0) {
      tesseract_->PopPixDebugSection(section_handle_);
    }
  }

  void AutoPopDebugSectionLevel::pop() {
    if (section_handle_ >= 0) {
      tesseract_->PopPixDebugSection(section_handle_);
      section_handle_ = INT_MIN / 2;
    }
  }

  } // namespace tesseract
