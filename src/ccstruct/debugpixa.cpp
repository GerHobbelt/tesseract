// UTF-8: ö,ä,💩,⏱️

#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/image.h>

#include "debugpixa.h"
#include <tesseract/tprintf.h>
#include "tesseractclass.h"
#include "global_params.h"
#include "pixProcessing.h" 
#include "rect.h" 

#include <leptonica/allheaders.h>

#include <string>
#include <vector>
#include <chrono>  // chrono::system_clock
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip> // put_time

#include "out/HtmlResourceFileManager.h"
#include "out/diag-report.h"
#include "out/normalize.h"
#include "out/modern-normalize.h"

#if defined(HAVE_MUPDF)
#include "mupdf/fitz.h"
#endif

#ifndef TESSERACT_DISABLE_DEBUG_FONTS 
#define TESSERACT_DISABLE_DEBUG_FONTS 1
#endif

// Set to 0 for PNG, 1 for WEBP output as part of the HTML report.
#define GENERATE_WEBP_IMAGES  1

namespace tesseract {

  static const char *IMAGE_EXTENSION =
#if GENERATE_WEBP_IMAGES
    ".webp";
#else
    ".png";
#endif

  // enforce the use of our own basic char checks; MSVC RTL ones barf with
  //    minkernel\crts\ucrt\src\appcrt\convert\isctype.cpp(36) : Assertion failed: c >= -1 && c <= 255
  // thanks to char being signed and incoming UTF8 bytes. Plus I don't want to be Unicode/codepage sensitive
  // in here by using iswalpha() et al.

  static inline bool isalpha(int c) {
    c &= ~0x20;
    return c >= 'A' && c <= 'Z';
  }

  static inline bool isdigit(int c) {
    return c >= '0' && c <= '9';
  }

  static inline bool isalnum(int c) {
    return isalpha(c) || isdigit(c);
  }

  static inline bool isspace(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0;
  }

  static inline bool ispunct(int c) {
    return strchr("!():;,.?", c) != nullptr;
  }

  // ... because fmt::format("{:.3f}") does not remove trailing insignificant zero digits

  static inline std::string nanoseconds_to_seconds_6(double nsecs) {
    char buf[60];
    snprintf(buf, sizeof(buf), "%0.6lf", nsecs / 1E9);
    // trim off the insignificant tail
    char *d = buf + strlen(buf) - 1;
    while (d > buf) {
      if (*d == '0') {
        *d-- = 0;
        continue;
      }
      if (*d == '.') {
        *d-- = 0;
      }
      break;
    }
    return buf;
  }

  #undef N        // just in case...
  #define N(v) nanoseconds_to_seconds_6(v)

  // aligned output: all are 10 wide. For tables.
  static inline std::string nanoseconds_to_seconds_10_6(double nsecs) {
    char buf[60];
    snprintf(buf, sizeof(buf), "%10.6lf", nsecs / 1E9);
    // trim off the insignificant tail
    char *d = buf + strlen(buf) - 1;
    while (d > buf) {
      if (*d == '0') {
        *d-- = ' ';
        continue;
      }
      if (*d == '.') {
        *d-- = ' ';
      }
      break;
    }
    return buf;
  }

#undef N10 // just in case...
#define N10(v) nanoseconds_to_seconds_10_6(v)

  static bool is_nonnegligible_difference(double t1, double t2) {
    auto d = t1; // do NOT use std::max(t1, t2) as we're focusing on T1 as the leading number in our caller!
    auto delta = fabs(t2 - t1);
    if (delta <= 1E-6)
      return false;
    auto diffperunage = delta / d;
    return diffperunage > 1E-4;
  }

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
          if (message == start || !(isalnum(message[-1]) || (message[+1] == message[-1] && !isspace(message[+1])))) {
            pos = 0;
            do {
              pos++;
              auto pos2 = strcspn(message + pos, "\n`");
              pos += pos2;
              // before we go, there's the scenario of "`.. ` ..`" to consider, i.e. message[pos] is a solitary backtick.
              //
              // The answer to that is usually relatively simple: when the number of backticks in both directions is equal, then
              // it is. ('equal' as in either both odd or even count.) This is only a problem if the message[pos] backtick
              // looks like a solitary one:
              while (message[pos] == '`' && isspace(message[pos + 1]) && isspace(message[pos - 1])) {
                int c1 = 0;
                for (size_t i = pos + 1; message[i]; i++) {
                  if (message[i] == '`')
                    c1++;
                }
                int c2 = 1;
                for (size_t i = 1; i < pos; i++) {
                  if (message[i] == '`')
                    c2++;
                }
                if (c1 % 2 == c2 % 2) {
                  // solitary. move forward to skip over this one now.
                  pos++;
                  pos2 = strcspn(message + pos, "\n`");
                  pos += pos2;
                  continue;
                }
                break;
              }
            } while (message[pos] == '`' && message[pos + 1] != 0 && message[pos + 1] != '\n' &&
                     (isalnum(message[pos + 1]) ||
                       (message[pos + 1] == message[pos - 1] && !isspace(message[pos + 1]))));

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
          if (message[1] == '*') {
            if (message > start && !isalnum(message[-1]) && isalnum(message[2])) {
              pos = 1;
              do {
                pos++;
                auto pos2 = strcspn(message + pos, "\n*");
                pos += pos2;
              } while (message[pos] == '*' && message[pos + 1] != '*');

              if (message[pos] == '*' && message[pos + 1] == '*') {
                dst << "<strong>";
                add_inline_particle_as_html(dst, message + 2, pos - 2);
                dst << "</strong>";
                message += pos + 1;
                continue;
              }
            }
          }
          if (message > start && !isalnum(message[-1]) && isalnum(message[1])) {
            pos = 0;
            do {
              pos++;
              auto pos2 = strcspn(message + pos, "\n*");
              pos += pos2;
            } while (message[pos] == '*' && message[pos + 1] != 0 && isalnum(message[pos + 1]));

            if (message[pos] == '*') {
              dst << "<em>";
              add_inline_particle_as_html(dst, message + 1, pos - 1);
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
    const char *eq_sep;
  };

  static dots_marker find_start_of_leaderdots(const char *line) {
    dots_marker nil{nullptr};
    if (isalnum(*line) || *line == '_') {
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
      const char *end = line;
      if (*line++ != '(')
        return nil;
      line = strchr(line, ']');
      if (!line)
        return nil;
      line++;
      while (*line == ' ')
        line++;
      const char *eq = line;
      if (*line++ != '=')
        return nil;
      if (*line != ' ')
        return nil;
      return {.start = start, .end = end, .eq_sep = eq};
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
      if (tab_count_avg >= 64 && line_count >= 2) {
        tab_count_per_line_max++; // turns the max TAB count in a max column count.

        // Do it as a table; total column count equals tab_count_per_line_max
        int line = 0;

        active_TABs = &TAB_positions[line];
        unsigned int line_spos = 0;
        unsigned int line_epos = LF_positions[line];
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
        if (dots.start && dots_next.start) {
          // Yes, we're looking at a list of at least two elements: deal with it now!
          if (style)
            dst << "<table class=\"leaders paramreport " << style << "\">\n";
          else
            dst << "<table class=\"leaders paramreport\">\n";
          for (;;) {
            message += 2;
            dst << "<tr class=\"paramreport_line\">\n<td class=\"paramreport_itemname\">";
            add_inline_particle_as_html(dst, message, dots.start - message);
            dst << "</td><td class=\"paramreport_itemspec\">";
            add_inline_particle_as_html(dst, dots.end, dots.eq_sep - 1 - dots.end);
            dst << "</td><td class=\"paramreport_itemvalue\">";
            add_inline_particle_as_html(dst, dots.eq_sep + 2, next - 2 - dots.eq_sep);
            dst << "</td></tr>\n";
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
          dst << "</table>\n\n";
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
    : tesseract_(tess)
    , content_has_been_written_to_file(false)
    , active_step_index(-1)
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
    PushNextSection("Start a tesseract run");

// warning C4574: 'TESSERACT_DISABLE_DEBUG_FONTS' is defined to be '0': did you mean to use '#if TESSERACT_DISABLE_DEBUG_FONTS'?
#if defined(TESSERACT_DISABLE_DEBUG_FONTS) && TESSERACT_DISABLE_DEBUG_FONTS
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
    ASSERT0(pixGetRefCount(pix) >= 1);
    int depth = pixGetDepth(pix);
    ASSERT0(depth >= 1 && depth <= 32);
    {
      int depth = pixGetDepth(pix);
      ASSERT0(depth == 1 || depth == 8 || depth == 24 || depth == 32);
    }
    {
      int width = -1, height = -1, depth = -1;
      int ok = pixGetDimensions(pix, &width, &height, &depth);
      ASSERT0(ok == 0 && width >= 1 && width < 16000 && height >= 1 && height < 16000 && depth >= 1 && depth <= 32);
    }

    // warning C4574: 'TESSERACT_DISABLE_DEBUG_FONTS' is defined to be '0': did you mean to use '#if TESSERACT_DISABLE_DEBUG_FONTS'?
#if defined(TESSERACT_DISABLE_DEBUG_FONTS) && TESSERACT_DISABLE_DEBUG_FONTS
    pixaAddPix(pixa_, const_cast<PIX *>(pix.ptr()), L_COPY);
#else
    {
      int color = depth < 8 ? 1 : (depth > 8 ? 0x00ff0000 : 0x80);
      Image pix_debug = pixAddSingleTextblock(const_cast<PIX *>(pix.ptr()), fonts_, caption, color, L_ADD_BELOW, nullptr);

#if 01
      pixaAddPix(pixa_, pix_debug.relinquish(), L_INSERT);
#else
      // or use L_CLONE:
      pixaAddPix(pixa_, pix_debug, L_CLONE);
#endif
  }
#endif

    captions.push_back(caption);
    cliprects.push_back(bbox);

    // make sure follow-up log messages end up AFTER the image in the output by dumping them in a subsequent info_chunk:
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
    return (!content_has_been_written_to_file && pixaGetCount(pixa_) > 0 /* || steps.size() > 0    <-- see also notes at Clear() method; the logic here is that we'll only have something *useful* to report once we've collected one or more images along the way... */ );
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
    step_ref.elapsed_ns = 0.0;
    step_ref.clock.start();
    step_ref.first_info_chunk = info_chunks.size();
    //ASSERT0(!title.empty());

    if (active_step_index >= 0) {
      auto &prev_step = steps[active_step_index];
      prev_step.elapsed_ns += prev_step.clock.get_elapsed_ns();
      prev_step.clock.stop();
      if (prev_step.level >= level)
        prev_step.last_info_chunk = info_chunks.size() - 1;
    }

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
    step.elapsed_ns += step.clock.get_elapsed_ns();
    step.clock.stop();

    if (handle >= 0) {
      ASSERT0(handle < steps.size());
      auto &parent = steps[handle];
      //ASSERT0(parent.level <= std::max(0, level));

      // bingo!
      active_step_index = handle;
      parent.clock.start();

      // now all we need is a fresh info_chunk:
      auto &info_ref = info_chunks.emplace_back();
      info_ref.appended_image_index = captions.size(); // neat way to get the number of images: every image comes with its own caption
      return;
    }

    auto level = step.level - 1; // level we seek
    if (handle < -1 || level < 0) {
      // when we get here, we're aiming below root, so we reset to last
      // root-entry level.
      level = 0;
      // Note: we also accept when the very last entry is the active entry and
      // happens to be root: can't pop a root like that!  :-)
      idx++;
    }
    for (idx--; idx >= 0; idx--)
    {
      auto& prev_step = steps[idx];
      if (prev_step.level == level)
      {
        // bingo!
        active_step_index = idx;
        prev_step.clock.start();

        // now all we need is a fresh info_chunk:
        auto& info_ref = info_chunks.emplace_back();
        info_ref.appended_image_index = captions.size();     // neat way to get the number of images: every image comes with its own caption
        return;
      }
    }

    ASSERT_HOST_MSG(false, "Should never get here!\n");
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

  static std::string argv_particle_as_html(const std::string &arg) {
    const char *message = arg.c_str();
    bool needs_quotes = (nullptr != strchr(message, ' '));
    std::string rv;
    if (needs_quotes) {
      rv += '"';
    }

    bool zwnbs_pending = false;
    while (*message) {
      if (zwnbs_pending) {
        rv += "&#8288;";
      }
      auto pos = strcspn(message, "<>&-");
      if (pos > 0) {
        std::string_view particle(message, pos);
        rv += particle;
        message += pos;
        zwnbs_pending = false;
      }
      switch (*message) {
        case 0:
          break;

        case '<':
          rv += "&lt;";
          message++;
          break;

        case '>':
          rv += "&gt;";
          message++;
          break;

        case '&':
          rv += "&amp;";
          message++;
          break;

        case '-':
          if (!zwnbs_pending)
            rv += "&#8288;-"; // inject Zero width no-break space to prevent PRE wrapping at the dash
          else
            rv += "-";
          message++;
          zwnbs_pending = true;
          continue;
      }
      zwnbs_pending = false;
    }

    if (needs_quotes) {
      rv += '"';
    }

    return rv;
  }

  void DebugPixa::DebugAddCommandline(const std::vector<std::string>& argv) {
    ASSERT0(steps.size() >= 1);
    ASSERT0(active_step_index >= 0);
    ASSERT0(active_step_index < steps.size());
    ASSERT0(info_chunks.size() >= 1);

    auto &f = this->GetInfoStream();
    f << "\n\
<blockquote class=\"command-line\">\n\
<h6>Tesseract Excutes Command</h6>\n\
\n\
<pre class=\"command-line\">";
    bool first = true;
    for (const std::string &arg : argv) {
      if (!first)
        f << ' ';
      first = false;
      f << argv_particle_as_html(arg);
    }
    f << "</pre>\n\
</blockquote>\n\
\n";
  }

  static std::string encode_as_html(const std::string &str) {
    std::string dst;
    const char *start = str.c_str();
    const char *message = start;
    while (*message) {
      auto pos = strcspn(message, "\n<>&");
      if (pos > 0) {
        std::string_view particle(message, pos);
        dst += particle;
        message += pos;
      }
      switch (*message) {
        case 0:
          break;

        case '<':
          dst += "&lt;";
          message++;
          continue;

        case '>':
          dst += "&gt;";
          message++;
          continue;

        case '&':
          dst += "&amp;";
          message++;
          continue;

        case '\n':
          dst += "\n<br>";
          message++;
          continue;
      }
    }
    return dst;
  }

  static std::string check_unknown_and_encode(const std::string& s, const char* default_value = "<em>(unknown / nil)</em>") {
    if (s.empty())
      return default_value;
    return "<code>" + encode_as_html(s) + "</code>";
  }

  static std::string SanitizeFilenamePart(const std::string& srcstr) {
    std::string str = srcstr;
    auto len = str.size();
    char* s = str.data();
    char* d = s;
    char* base = s;

    for (int i = 0; i < len; i++, s++) {
      char c = *s;

      if (isalnum(c)) {
        *d++ = c;
      } else if (c == '_' || c == '-' || c == '.') {
        if (d > base) {
          if (d[-1] != '.')
            *d++ = c;
        } else {
          *d++ = c;
        }
      } else {
        if (d > base) {
          if (strchr("-_.", d[-1])) {
            d[-1] = '.';
          } else {
            *d++ = '.';
          }
        } else {
          *d++ = '.';
        }
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
    return str;
  }
    
  static std::string html_styling(const std::string &datadir, const std::string &filename) {
    // first search if the HTML / CSS resource is available on your filesystem.
    //
    // When we cannot find it anywhere there, we take the built-in version as a fallback.
    //
    // Note that we ripped this chained-check approach from our Tesseract::read_config_file()
    // so there's a refactor/dedup TODO lurking in here somewhere!   ;-)
    std::string path = datadir;
    path += "html-styling/";
    path += filename;
    tprintDebug("Read HTML Styling Partial: test if '{}' is a readable file: ", path);
    FILE *fp;
    if ((fp = fopen(path.c_str(), "rb")) != nullptr) {
    } else {
      path = datadir;
      path += "tessconfigs/html-styling/";
      path += filename;
      tprintDebug(
          "NO.\n"
          "Read HTML Styling Partial: test if '{}' is a readable file: ",
          path);
      if ((fp = fopen(path.c_str(), "rb")) != nullptr) {
      } else {
        path = filename;
        tprintDebug(
            "NO.\n"
            "Read HTML Styling Partial: test if '{}' is a readable file: ",
            path);
        if ((fp = fopen(path.c_str(), "rb")) != nullptr) {
        } else {
          tprintDebug("NO.\n");
          tprintError("Config file '{}' cannot be opened / does not exist anywhere we looked. Picking up the built-in default content instead.\n", filename);

          goto get_built_in;
        }
      }
    }
    tprintDebug("YES\n");

    // shut up error C2362: initialization of 'XYZ' is skipped by 'goto fetch_err'
    {
      if (fseek(fp, 0, SEEK_END))
        goto fetch_err;
      auto size = ftell(fp);
      if (fseek(fp, 0, SEEK_SET))
        goto fetch_err;
      auto data = new uint8_t[size];
      auto rdlen = fread(data, 1, size, fp);
      if (rdlen != size)
        goto fetch_err;
      std::string content(reinterpret_cast<const char *>(data), size);
      fclose(fp);
      return content;
    }

  fetch_err:
    tprintError("An error occurred while fetching the HTML Styling Partial from file `{}`. Error: {}\n", path, errno != 0 ? strerror(errno) : "?unidentified error?");
  get_built_in:
    auto &mgr = bin2cpp::FileManager::getInstance();
    // has this thing been initialized yet?   If not, do it now.
    if (mgr.getFileCount() == 0) {
      mgr.registerFile(bin2cpp::getNormalizeCssFile);
      mgr.registerFile(bin2cpp::getModernnormalizeCssFile);
      mgr.registerFile(bin2cpp::getDiagreportCssFile);
    }
    // warning C4296: '>=': expression is always true
    for (int i = mgr.getFileCount() - 1; i >= 0; i--) {
      const auto built_in = mgr.getFile(i);
      if (filename == built_in->getFileName())
        return built_in->getBuffer();
    }
    ASSERT_HOST_MSG(false, "should never get here\n");
    return "b0rked!";
  }

  Image MixWithLightRedTintedBackground(const Image &pix, const Image &original_image, const TBOX *cliprect) {
    return pixMixWithTintedBackground(pix, original_image, 0.1, 0.5, 0.5, 0.90, 0.085, cliprect);
  }

  static std::string TruncatedForTitle(const std::string &str) {
    if (str.size() < 70)
      return str;
    const char *s = str.c_str();
    auto len = str.size();
    for (;;) {
      const char *p = strnrpbrk(const_cast<char *>(s), ".[(: ", len);
      if (!p)
        break;
      len = p - s;
      if (len < 70)
        break;
    }
    // trim the tail, after the clipping above.
    len--;
    // warning C4296: '>=': expression is always true
    while (int(len) >= 0 && strchr(".[(: ", s[len]))
      len--;
    len++;
    std::string rv(s, len);
    return rv + "\u2026";  // append ellipsis
  }

  static void write_one_pix_for_html(FILE *html, int counter, const std::string &img_filename, const Image &pix, const Image &original_image, const std::string &title, const std::string &description, const TBOX *cliprect = nullptr) {
    if (!!pix) {
      const char *pixfname = fz_basename(img_filename.c_str());
      int w, h, depth;
      pixGetDimensions(pix, &w, &h, &depth);
      const char *depth_str = ([depth]() {
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
#if !GENERATE_WEBP_IMAGES
      /* With best zlib compression (9), get between 1 and 10% improvement
       * over default (6), but the compression is 3 to 10 times slower.
       * Use the zlib default (6) as our default compression unless
       * pix->special falls in the range [10 ... 19]; then subtract 10
       * to get the compression value.  */
      pixSetSpecial(img, 10 + 1);
      pixWrite(img_filename.c_str(), img, IFF_PNG);
#else
      FILE *fp = fopen(img_filename.c_str(), "wb+");
      if (!fp) {
        tprintError("Failed to open file '{}' for writing one of the debug/diagnostics log impages.\n", img_filename);
      } else {
        auto rv = pixWriteStreamWebP(fp, img, 10, TRUE);
        fclose(fp);
        if (rv) {
          tprintError("Did not succeeed writing the image data to file '{}' while generating the HTML diagnostic/log report.\n", img_filename);
        }
      }
#endif
      fputs(
        fmt::format("<section class=\"image-display\">\n\
  <h6>image #{:02d}: {}</h6>\n\
  <figure>\n\
    <img src=\"{}\" >\n\
    <figcaption>size: {} x {} px; {}</figcaption>\n\
  </figure>\n\
  <p>{}</p>\n\
</section>\n",
            counter, encode_as_html(title),
            pixfname,
            w, h, depth_str,
            encode_as_html(description)
        ).c_str(),
        html);
    }
  }

  void DebugPixa::WriteImageToHTML(int &counter, const std::string &partname, FILE *html, int idx) {
    plf::nanotimer image_clock;
    image_clock.start();

    counter++;
    const std::string caption = captions[idx];
    std::string fn(partname + SanitizeFilenamePart(fmt::format(".img{:04d}.", counter) + caption) + IMAGE_EXTENSION);

    Image pixs = pixaGetPix(pixa_, idx, L_CLONE);    // takes ownership, correct
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
    Image bgimg;
    if (clip_area > 0) {
      bgimg = tesseract_->pix_original();       // clones ownership
    }

    write_one_pix_for_html(html, counter, fn, pixs, bgimg, TruncatedForTitle(caption), caption);

    if (clip_area > 0 && false) {
      counter++;
      fn = partname + SanitizeFilenamePart(fmt::format(".img{:04d}.", counter) + caption) + IMAGE_EXTENSION;

      write_one_pix_for_html(html, counter, fn, pixs, bgimg, TruncatedForTitle(caption), caption, &cliprect);
    }

    //pixs.destroy();

    double t = image_clock.get_elapsed_ns();
    image_series_elapsed_ns.push_back(t);
    total_images_production_cost += t;
  }

  int DebugPixa::WriteInfoSectionToHTML(int &counter, int &next_image_index, const std::string &partname, FILE *html, int current_section_index) {
    const DebugProcessStep &section_info = steps[current_section_index];

    auto title = section_info.title.c_str();
    if (!*title)
      title = "(null)";
    auto h_level = section_info.level + 1;
    ASSERT0(h_level >= 1);
    if (h_level > 5)
      h_level = 5;
    fputs(fmt::format("\n\n<section>\n<h{0}>{1}</h{0}>\n\n", h_level, title).c_str(), html);

      std::string section_timings_intro_msg;
    if (is_nonnegligible_difference(section_info.elapsed_ns, section_info.elapsed_ns_cummulative)) {
        section_timings_intro_msg = fmt::format("<p class=\"timing-info\">This section of the tesseract run took {} sec (cummulative, i.e. including all subsections: {} sec.)</p>\n\n", N(section_info.elapsed_ns), N(section_info.elapsed_ns_cummulative));
    } else {
      section_timings_intro_msg = fmt::format("<p class=\"timing-info\">This section of the tesseract run took {} sec</p>\n\n", N(section_info.elapsed_ns));
    }
    fputs(section_timings_intro_msg.c_str(), html);

    int next_section_index = current_section_index + 1;
    // special tweak for when we report (meta!) on our own report production:
    // that's when `current_section_index + 1` will produce an out-of-bounds access!
    if (next_section_index >= steps.size())
      next_section_index = 0;
    const DebugProcessStep *next_section_info = &steps[next_section_index];

    int start_info_chunk_index = section_info.first_info_chunk;
    int last_info_chunk_index = section_info.last_info_chunk;
    for (int chunk_idx = start_info_chunk_index; chunk_idx <= last_info_chunk_index; chunk_idx++) {
      // make sure we don't dump info chunks which belong to sub-sections:
      if (chunk_idx == next_section_info->first_info_chunk) {
        next_section_index = WriteInfoSectionToHTML(counter, next_image_index, partname, html, next_section_index);
        chunk_idx = next_section_info->last_info_chunk;
        next_section_info = &steps[next_section_index];
        continue;
      }
      const DebugProcessInfoChunk &info_chunk = info_chunks[chunk_idx];
      auto v = info_chunk.information.str();
      auto content = v.c_str();
      if (*content) {
        fputs(content, html);
        fputs("\n\n", html);
      }

      // does this chunk end with an image?
      int next_chunk_idx = chunk_idx + 1;
      // same hack as for next_section_index above: prevent out-of-bounds access.  >:-/
      if (next_chunk_idx >= info_chunks.size()) {
        break;
      }
      const DebugProcessInfoChunk &next_info_chunk = info_chunks[next_chunk_idx];
      if (info_chunk.appended_image_index != next_info_chunk.appended_image_index) {
        WriteImageToHTML(counter, partname, html, info_chunk.appended_image_index);
        if (next_image_index <= info_chunk.appended_image_index)
          next_image_index = info_chunk.appended_image_index + 1;
      }
    }

    fputs("\n</section>\n\n", html);

    return next_section_index;
  }

  class GrandTotalPerf {
  public:
    GrandTotalPerf() : clock() {
      clock.start();
    }
    plf::nanotimer clock;
  };
  static GrandTotalPerf grand_clock;


  double DebugPixa::gather_cummulative_elapsed_times() {
    int step_count = steps.size();

    // we can safely assume that parents are always *older* and thus *earlier*
    // in the steps[] array, so we can easily traverse from end to start and
    // update every parent as we travel along.
    //
    // First we clear all previously gathered values as we must do that job from
    // scratch to be safe & sure.
    for (int i = 0; i < step_count; i++) {
      auto &step = steps[i];
      step.elapsed_ns_cummulative = 0.0;
    }
    // now traverse backwards, bottom-to-top, gathering the cummulative numbers
    // along the way..
    double grand_total_time_ns = 0.0;
    for (int i = step_count - 1; i >= 0; i--) {
      auto &step = steps[i];
      // add self to accumulus.
      step.elapsed_ns_cummulative += step.elapsed_ns;
      // now update the first parent with our time: as we travel back in time,
      // we only need to update our *parent* in order to end up with the
      // grand total time at the end of the traversal of the steps[] tree.
      //
      // There's one caveat though: the way we coded steps[] and deal with it,
      // there's no TRUE ROOT element in that tree; that would be a fictional
      // step[-1], so the code deals with this by detecting all the 'root nodes'
      // and accumulating them into our `grand_total_time_ns` value.
      int level = step.level - 1;
      int j = i - 1;
      for (; j >= 0; j--) {
        auto &parent = steps[j];
        if (parent.level == level) {
          // actual parent found.
          parent.elapsed_ns_cummulative += step.elapsed_ns_cummulative;
          break;
        }
      }
      if (j < 0) {
        // we are a 'root node'; deal with us accordingly.
        grand_total_time_ns += step.elapsed_ns_cummulative;
      }
    }
    return grand_total_time_ns;
  }

  
  void DebugPixa::WriteHTML(const char* filename) {
    ASSERT0(tesseract_ != nullptr);
    if (HasContent()) {
      double time_elapsed_until_report = grand_clock.clock.get_elapsed_ns();
      plf::nanotimer report_clock;
      double source_image_elapsed_ns;
      total_images_production_cost = 0.0;

      // pop all levels and push a couple of *sentinels* so our tree traversal
      // logic can be made simpler with far fewer boundary checks as we'll have
      // valid slots at size+1:
      PopSection(-2);
#if 0
      // enforce a new root
      active_step_index = -1;
#endif
      PushNextSection("diagnostics/log reporting (HTML)");
      ASSERT_HOST(GetCurrentSectionLevel() == 1);

      report_clock.start();

      const char *ext = strrchr(filename, '.');
      if (!ext)
        ext = filename + strlen(filename);
      std::string partname(filename, ext - filename);
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
        tprintError("cannot open diagnostics HTML output file {}: {}\n", filename, strerror(errno));
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
        int i;
        for (i = 0; i < num_subs - 1; ++i) {
          languages << tesseract_->get_sub_lang(i)->lang_ << " + ";
        }
        languages << tesseract_->get_sub_lang(i)->lang_;
      }

      // CSS styles for the generated HTML
      // 
      // - CSS UL list with classic leader dots: based on https://www.w3.org/Style/Examples/007/leaders.en.html
      //
      //   <link rel="stylesheet" href="https://unpkg.com/normalize.css@8.0.1/normalize.css" >
      //   <link rel="stylesheet" href="https://unpkg.com/modern-normalize@1.1.0/modern-normalize.css" >
      //   <link rel="stylesheet" href="diag-report.css" >
      fputs(fmt::format("<html>\n\
<head>\n\
	<meta charset=\"UTF-8\">\n\
  <title>Tesseract diagnostic image set</title>\n\
  <style>\n\
  {}\n\
  </style>\n\
  <style>\n\
  {}\n\
  </style>\n\
  <style>\n\
  {}\n\
  </style>\n\
</head>\n\
<body>\n\
<article>\n\
<h1>Tesseract diagnostic image set</h1>\n\
<table>\n\
<tr><td>tesseract version</td><td>{}, run @ {}</td></tr>\n\
<tr><td>Input image file path</td><td>{}</td></tr>\n\
<tr><td>Output base</td><td>{}</td></tr>\n\
<tr><td>Input image path</td><td>{}</td></tr>\n\
<tr><td>Primary Language</td><td>{}</td></tr>\n\
<tr><td>Scondary Languages</td><td>{}</td></tr>\n\
<tr><td>Language Data Path Prefix</td><td>{}</td></tr>\n\
<tr><td>Data directory</td><td>{}</td></tr>\n\
<tr><td>Main directory</td><td>{}</td></tr>\n\
</table>\n\
",
        html_styling(tesseract_->datadir_, "normalize.css").c_str(),
        html_styling(tesseract_->datadir_, "modern-normalize.css").c_str(),
        html_styling(tesseract_->datadir_, "diag-report.css").c_str(),
        TESSERACT_VERSION_STR, 
        now_str.c_str(),
        check_unknown_and_encode(tesseract_->input_file_path_).c_str(),
        check_unknown_and_encode(tesseract_->imagebasename_).c_str(),
        check_unknown_and_encode(tesseract_->imagefile_).c_str(),
        tesseract_->lang_.c_str(),
        languages.str().c_str(),
        check_unknown_and_encode(tesseract_->language_data_path_prefix_).c_str(),
        check_unknown_and_encode(tesseract_->datadir_).c_str(),
        check_unknown_and_encode(tesseract_->directory_).c_str()
      ).c_str(), html);

      plf::nanotimer image_clock;
      image_clock.start();
      {
        std::string fn(partname + SanitizeFilenamePart(".img-original.") + IMAGE_EXTENSION);

        write_one_pix_for_html(html, 0, fn, tesseract_->pix_original(), tesseract_->pix_original(), "original image", "The original image as registered with the Tesseract instance.");
      }
      source_image_elapsed_ns = image_clock.get_elapsed_ns();

      int section_count = steps.size() - 1;          // adjust size due to sentinel which was pushed at the end just now.
      int pics_count = pixaGetCount(pixa_);

      int next_image_index = 0;

      // calculate the cummulative elapsed time by traversing the section tree bottom-to-top
      double total_time_elapsed_ns = gather_cummulative_elapsed_times();

      int current_section_index = 0; 
      while (current_section_index < section_count) {
        current_section_index = WriteInfoSectionToHTML(counter, next_image_index, partname, html, current_section_index);
      }

      for (int i = next_image_index; i < pics_count; i++) {
        WriteImageToHTML(counter, partname, html, i);
      }
      //pixaClear(pixa_);

      // now jiggle the report creation section, while we're in it. Yeah. It's crazy like that,
      // but we want to report this part too, through regular channels as much as possible.
      // So we do most of the work before this point in time, then pop the section and have its
      // time two ways: via our own measurement an via the section tree mechanism.
      //
      // It's a bit convoluted, but this should give us some decent numbers to analyze.
      // 
      // Pop() but don't Pop(), that's what local scope below attempts to achieve. Just for
      // gathering and a last WriteInfoSectionToHTML() just down below.
      {
        int idx = active_step_index;
        auto &step = steps[idx];
        step.last_info_chunk = info_chunks.size() - 1;
        step.elapsed_ns += step.clock.get_elapsed_ns();
        // !step.clock.stop();
        // ^^^^^^^^^^^^^^^^^
        // keep it running! we'll nuke the `elapsed_ns`
        // after we're done with gathering it below!

        // gather the time data again, now that we're near the end of our reporting section
        // and wish to report on that one too!      Gosh darn, so *meta*!  8-P  
        total_time_elapsed_ns = gather_cummulative_elapsed_times();

      current_section_index = section_count;
      (void)WriteInfoSectionToHTML(counter, next_image_index, partname, html, current_section_index);

      double report_span_time = report_clock.get_elapsed_ns();
      double grand_total_time = grand_clock.clock.get_elapsed_ns();

      std::string section_timings_intro_msg;
      if (is_nonnegligible_difference(total_time_elapsed_ns, grand_total_time)) {
        section_timings_intro_msg = fmt::format("\n<hr>\n\n<p class=\"timing-info\">The entire tesseract run took {} sec (including all application preparation / overhead: {} sec).</p>\n\n", N(total_time_elapsed_ns), N(grand_total_time));
      } else {
        section_timings_intro_msg = fmt::format("\n<hr>\n\n<p class=\"timing-info\">The entire tesseract run took {} sec.</p>\n\n", N(total_time_elapsed_ns));
      }
      fputs(section_timings_intro_msg.c_str(), html);

      std::string img_timings_msg;
      for (int i = 0; i < image_series_elapsed_ns.size(); i++) {
        img_timings_msg += fmt::format("<li>Image #{}: {} sec</li>\n", i, N(image_series_elapsed_ns[i]));
      }

      std::string timing_report_msg = fmt::format(
          "<p>\n"
        "Re overhead costs: the above total time ({} sec) includes at least this HTML report production as one of the overhead components (which together clock in at {} sec, alas). It took {} sec before tesseract was ready to report.\n"
          "</p><p>\n"
          "While producing this HTML diagnotics log report took {} sec, this can be further subdivided into a few more numbers, where producing and saving the <em>lossless</em> WEBP images included in this report are a significant cost:\n"
          "</p><ul>\n"
          "<li> saving a <em>lossless</em> WEBP copy of the source image: {} sec </li>\n"
              "<li> plus the other images @ {} sec total:\n"
          "<br>\n"
          "<ul>\n{}\n</ul></li>\n"
          "\n",
          N(grand_total_time), 
        N(grand_total_time - total_time_elapsed_ns),
          N(time_elapsed_until_report),
        N(report_span_time),
          N(source_image_elapsed_ns),
          N(total_images_production_cost),
          img_timings_msg);
      
      std::string sectiontiming_summary_msg = 
          "<p>You can always reduce these overhead numbers by <em>turning off</em> the tesseract debug parameters which help produce these support images, e.g.:</p>"
          "<ul>\n"
        "<li><code>tessedit_dump_pageseg_images</code></li>\n"
          "<li><code>debug_image_normalization</code></li>\n"
          "<li><code>tessedit_write_images</code></li>\n"
          "<li><code>verbose_process</code></li>\n"
          "<li><code>dump_osdetect_process_images</code></li>\n"
          "</ul>\n"
          "<p>Tip: you also often can gain extra speed by <em>turning off</em> any other debug/diagnostics parameters that are currently active and in use. The latter can be easily observed by the per-section parameter usage reports that are part of this HTML diagnostics/log report: see the designated sections &amp; tables to see which sections took a major chunk of the total time and which parameters may habe been involved.</p>\n"
          "<p>Thank you for using tesseract. Enjoy!</p>\n"
        ;

      fputs(timing_report_msg.c_str(), html);

      std::string section_timings_msg;
      for (int i = 0; i < steps.size(); i++) {
        auto &step = steps[i];
        std::string indent;
        for (int j = 0; j < step.level; j++) {
          indent += "&ensp;";
        }
        section_timings_msg += fmt::format(
            "<tr><td>{}</td><td>{}{}</td><td class=\"timing-value\">{}</td><td class=\"timing-value\">{}</td></tr>\n",
            step.level, indent, step.title,
            N(step.elapsed_ns), N(step.elapsed_ns_cummulative));
      }

      section_timings_msg = fmt::format(
              "\n\n\n<div class=\"timing-details\">\n"
          "<p>Timings per section, cumulatives for the entire work tree. All times in unit: <em>seconds</em>:</p>\n"
          "<table><tbody>\n"
          "<tr><th>Level</th><th>Section Name</th><th>Self</th><th>Self + Children</th></tr>\n"
          "{}\n"
          "</table>\n\n",
          section_timings_msg);

      fputs(section_timings_msg.c_str(), html);

      // get the cummulative for "Process Pages" subsection:
      double proc_pages_time = time_elapsed_until_report;
      for (const auto &step : steps) {
        if (step.title == "Process pages") {
          proc_pages_time = step.elapsed_ns_cummulative;
          break;
        }
      }

      std::string timing_summary_msg = fmt::format(
              "\n\n\n<div class=\"timing-summary\">\n"
          "<h6>Timing summary</h6>\n"
        "<pre>\n"
        "Wall clock duration:...................... {} sec.\n"
        "Time until report:........................ {} sec.\n"
        "  - actual work:.......................... {} sec.\n"
        "  - prep & misc. overhead:................ {} sec\n"
        "  - gap:unaccounted for (~ unidentified overhead):\n"
        "    ...................................... {} sec / {} sec\n"
        "Report production:........................ {} sec.\n"
        "\n"
        "Cummulative work effort:.................. {} sec\n"
        "Overhead:................................. {} sec\n"
        "  - reported images production:........... {} sec\n"
        "  - report text production + I/O:......... {} sec\n"
        "  - misc + I/O:........................... {} sec\n"
          "</pre>\n"
        "{}"
        "</div>\n",
      N10(grand_total_time), 
        N10(time_elapsed_until_report), 
        N10(proc_pages_time), 
        N10(time_elapsed_until_report - proc_pages_time),
          N10(grand_total_time - total_time_elapsed_ns),
          N(grand_total_time - time_elapsed_until_report - report_span_time),
          N10(report_span_time),

        N10(proc_pages_time), 
       N10(grand_total_time - proc_pages_time),
          N10(source_image_elapsed_ns + total_images_production_cost),
          N10(report_span_time - (source_image_elapsed_ns + total_images_production_cost)),
          N10(grand_total_time - proc_pages_time - report_span_time),

        sectiontiming_summary_msg
        );

      fputs(timing_summary_msg.c_str(), html);

      fputs("\n<hr>\n<h2>Tesseract parameters usage report</h2>\n\n", html);

        // reset the elapsed_ns value so the regular way this is dealt with will
        // produce correct numbers afterwards.
        step.elapsed_ns = 0;
      }
      
      tesseract::ParamsVectors *vec = tesseract_->params();

      // produce a HTML-formatted parameter usage report by using the regular way to get such a report,
      // then feed it through the NDtext-to-HTML transformer and only then write the final result in one fell swoop to file.
      // 
      // Takes a bit of regrettable extra string copying this way, but alas, we'll take one for code re-use.
      ParamsReportStringWriter writer;
      ParamUtils::ReportParamsUsageStatistics(writer, vec, -1, nullptr);
      std::ostringstream html_report_dst;
      add_encoded_as_html(html_report_dst, "params-report", writer.to_string().c_str());
      fputs(html_report_dst.str().c_str(), html);

      fputs("\n</body>\n</html>\n", html);

      fclose(html);
    }
  }


  void DebugPixa::WriteSectionParamsUsageReport()
  {
    const DebugProcessStep &section_info = steps[active_step_index];

    auto title = section_info.title.c_str();
    if (!*title)
      title = "(null)";
    auto level = section_info.level;

    if (level == 3 && verbose_process) {
      tesseract::ParamsVectors *vec = tesseract_->params();
      ParamUtils::ReportParamsUsageStatistics(nullptr, vec, level, title);
    }
  }


  void DebugPixa::Clear(bool final_cleanup) {
    //final_cleanup |= +content_has_been_written_to_file;
    pixaClear(pixa_);
    captions.clear();
    cliprects.clear();

    steps.clear();
    info_chunks.clear();
    image_series_elapsed_ns.clear();
    active_step_index = -1;
    total_images_production_cost = 0.0;

    // make sure the logging can commence if we aren't really at the very end yet, i.e. when we aren't invoked by the PixaDebug destructor itself:
    if (!final_cleanup) {
      // set up the root info section:
      PushNextSection("After final cleanup...");
      content_has_been_written_to_file = false;
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
      section_handle_ = INT_MIN;
    }
  }

  } // namespace tesseract
