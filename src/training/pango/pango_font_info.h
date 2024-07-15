/**********************************************************************
 * File:        pango_font_info.h
 * Description: Font-related objects and helper functions
 * Author:      Ranjith Unnikrishnan
 * Created:     Mon Nov 18 2013
 *
 * (C) Copyright 2013, Google Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **********************************************************************/

#ifndef TESSERACT_TRAINING_PANGO_FONT_INFO_H_
#define TESSERACT_TRAINING_PANGO_FONT_INFO_H_

#include "export.h"
#include <tesseract/export.h>

#include "../common/commandlineflags.h"

#if defined(PANGO_ENABLE_ENGINE)

//#include "pango/pango-font.h"
//#include "pango/pango.h"
//#include "pango/pangocairo.h"

#include "hb.h"

typedef enum {
	PANGO_UNDERLINE_NONE,
	PANGO_UNDERLINE_SINGLE,
	PANGO_UNDERLINE_DOUBLE,
	PANGO_UNDERLINE_LOW,
	PANGO_UNDERLINE_ERROR,
	PANGO_UNDERLINE_SINGLE_LINE,
	PANGO_UNDERLINE_DOUBLE_LINE,
	PANGO_UNDERLINE_ERROR_LINE
} PangoUnderline;

enum PangoStyle : int;
enum PangoVariant : int;
enum PangoWeight  : int;
enum PangoStretch  : int;

typedef enum {
	PANGO_GRAVITY_SOUTH,
	PANGO_GRAVITY_EAST,
	PANGO_GRAVITY_NORTH,
	PANGO_GRAVITY_WEST,
	PANGO_GRAVITY_AUTO
} PangoGravity;

typedef enum {
	PANGO_GRAVITY_HINT_NATURAL,
	PANGO_GRAVITY_HINT_STRONG,
	PANGO_GRAVITY_HINT_LINE
} PangoGravityHint;

typedef enum {
	PANGO_WRAP_WORD,
	PANGO_WRAP_CHAR,
	PANGO_WRAP_WORD_CHAR
} PangoWrapMode;


struct _PangoFontDescription
{
	char *family_name;

	enum PangoStyle style;
	enum PangoVariant variant;
	enum PangoWeight weight;
	enum PangoStretch stretch;
	PangoGravity gravity;

	char *variations;

	unsigned short  mask;
	unsigned static_family : 1;
	unsigned static_variations : 1;
	unsigned size_is_absolute : 1;

	int size;
};

/**
* PANGO_SCALE:
*
* The scale between dimensions used for Pango distances and device units.
*
* The definition of device units is dependent on the output device; it will
* typically be pixels for a screen, and points for a printer. %PANGO_SCALE is
* currently 1024, but this may be changed in the future.
*
* When setting font sizes, device units are always considered to be
* points (as in "12 point font"), rather than pixels.
*/
/**
* PANGO_PIXELS:
* @d: a dimension in Pango units.
*
* Converts a dimension to device units by rounding.
*
* Return value: rounded dimension in device units.
*/
/**
* PANGO_PIXELS_FLOOR:
* @d: a dimension in Pango units.
*
* Converts a dimension to device units by flooring.
*
* Return value: floored dimension in device units.
* Since: 1.14
*/
/**
* PANGO_PIXELS_CEIL:
* @d: a dimension in Pango units.
*
* Converts a dimension to device units by ceiling.
*
* Return value: ceiled dimension in device units.
* Since: 1.14
*/
#define PANGO_SCALE 1024
#define PANGO_PIXELS(d) (((int)(d) + 512) >> 10)
#define PANGO_PIXELS_FLOOR(d) (((int)(d)) >> 10)
#define PANGO_PIXELS_CEIL(d) (((int)(d) + 1023) >> 10)
/* The above expressions are just slightly wrong for floating point d;
* For example we'd expect PANGO_PIXELS(-512.5) => -1 but instead we get 0.
* That's unlikely to matter for practical use and the expression is much
* more compact and faster than alternatives that work exactly for both
* integers and floating point.
*
* PANGO_PIXELS also behaves differently for +512 and -512.
*/

/**
* PANGO_UNITS_FLOOR:
* @d: a dimension in Pango units.
*
* Rounds a dimension down to whole device units, but does not
* convert it to device units.
*
* Return value: rounded down dimension in Pango units.
* Since: 1.50
*/
#define PANGO_UNITS_FLOOR(d)                \
  ((d) & ~(PANGO_SCALE - 1))

/**
* PANGO_UNITS_CEIL:
* @d: a dimension in Pango units.
*
* Rounds a dimension up to whole device units, but does not
* convert it to device units.
*
* Return value: rounded up dimension in Pango units.
* Since: 1.50
*/
#define PANGO_UNITS_CEIL(d)                 \
  (((d) + (PANGO_SCALE - 1)) & ~(PANGO_SCALE - 1))

/**
* PANGO_UNITS_ROUND:
* @d: a dimension in Pango units.
*
* Rounds a dimension to whole device units, but does not
* convert it to device units.
*
* Return value: rounded dimension in Pango units.
* Since: 1.18
*/
#define PANGO_UNITS_ROUND(d)				\
  (((d) + (PANGO_SCALE >> 1)) & ~(PANGO_SCALE - 1))


typedef struct _PangoFontDescription PangoFontDescription;

struct _PangoFont    ;
typedef struct _PangoFont    PangoFont;

struct _PangoFontMap;
typedef struct _PangoFontMap PangoFontMap;

struct _PangoRectangle
{
	int x, y, w, h;
	int width, height;
};
typedef struct _PangoRectangle PangoRectangle;

struct _PangoContext;
typedef struct _PangoContext PangoContext;

struct _PangoLanguage ;
typedef struct _PangoLanguage PangoLanguage;

struct PangoAttrList ;
struct PangoAttribute
{
	int start_index ;
	int end_index ;
};

typedef unsigned int  guint;

struct cairo_surface_t ;
struct _cairo_t ;

struct _PangoLayout;
typedef struct _PangoLayout PangoLayout;

struct _PangoFontDescription;
typedef struct _PangoFontDescription PangoFontDescription;

struct _PangoFontFamily;
typedef struct _PangoFontFamily PangoFontFamily;

struct _PangoFontFace;
typedef struct _PangoFontFace  PangoFontFace;

typedef hb_codepoint_t PangoGlyph ;
typedef wchar_t gunichar;

struct _PangoCoverage;
typedef struct _PangoCoverage PangoCoverage;

struct _PangoLayoutIter;
typedef struct _PangoLayoutIter PangoLayoutIter ;

#define PANGO_GLYPH_EMPTY           ((PangoGlyph)0x0FFFFFFF)
#define PANGO_GLYPH_INVALID_INPUT   ((PangoGlyph)0xFFFFFFFF)
#define PANGO_GLYPH_UNKNOWN_FLAG    ((PangoGlyph)0x10000000)
#define PANGO_GET_UNKNOWN_GLYPH(wc) ((PangoGlyph)(wc)|PANGO_GLYPH_UNKNOWN_FLAG)

struct _PangoGlyphItemIter;
typedef struct _PangoGlyphItemIter  PangoGlyphItemIter ;
struct _PangoGlyphItemIter
{
	struct {
		PangoFont *font;
	} analysis;

	int start_index;
	int end_index;
	int start_glyph;
	int end_glyph;

	PangoGlyphItemIter *glyph_item;
	
	PangoGlyphItemIter *glyphs;
	
	PangoGlyph glyph;
};

struct _PangoLayoutRun
{
	PangoGlyphItemIter *item;
};
typedef _PangoLayoutRun PangoLayoutRun ;

typedef int     gboolean ;

struct _PangoCairoFontMap;
typedef struct _PangoCairoFontMap  PangoCairoFontMap;

struct _PangoLayoutLine
{
	int start_index;
	int length;
};
typedef struct _PangoLayoutLine  PangoLayoutLine;




static inline const char *pango_version_string() { return "X.X"; }

static inline void
pango_font_description_free (PangoFontDescription *desc)
{
	if (desc == NULL)
		return;
	return;
}

/**
* pango_font_description_to_filename:
* @desc: a `PangoFontDescription`
*
* Creates a filename representation of a font description.
*
* The filename is identical to the result from calling
* [method@Pango.FontDescription.to_string], but with underscores
* instead of characters that are untypical in filenames, and in
* lower case only.
*
* Returns: (transfer full) (nullable): a new string that must be freed with g_free().
*/
static inline const char *
pango_font_description_to_string (const PangoFontDescription *desc)
{
	return "Bogus";
}

static inline hb_font_t *pango_font_get_hb_font(PangoFont *font) {
	return NULL;
}

static inline void g_free(void *desc_str) {
}

static inline void 
pango_cairo_font_map_set_default(void *ptr) {}

static inline 	PangoFontMap * pango_cairo_font_map_get_default(void) {
	return NULL; 
}
static inline void	pango_font_map_list_families(PangoFontMap *font_map, PangoFontFamily ***families, int *n_families)
{
	*families = NULL;
	*n_families = 0;
}

static inline 	const char * pango_font_description_get_family(const PangoFontDescription *desc) { return "XXX"; }
static inline 	PangoFontDescription * pango_font_description_copy(const PangoFontDescription *desc) { return NULL; }

static inline int pango_font_description_get_size(const PangoFontDescription *desc) { return 0; }
static inline int pango_font_description_get_size_is_absolute(const PangoFontDescription *desc) { return 1; }

static inline PangoFontDescription * pango_font_description_from_string(const char *name) { return NULL; }

static inline 	PangoContext * pango_context_new() { return NULL; }
static inline 	void pango_cairo_context_set_resolution(PangoContext *context, int resolution_) {}
static inline 	void pango_context_set_font_map(PangoContext *context, PangoFontMap * font_map) {}
static inline 	PangoFont *pango_font_map_load_font(PangoFontMap * font_map, PangoContext *context, const PangoFontDescription *desc_) { return NULL; }
static inline void	g_object_unref(void *context) {}

static inline 	PangoCoverage * pango_font_get_coverage(PangoFont *font, void *ptr) { return NULL; }
static inline  int pango_is_zero_width(int it) { return 0 ; }
static inline  int pango_coverage_get(PangoCoverage *coverage, int it) { return 0; }

#define PANGO_COVERAGE_EXACT  1

static inline  void  pango_coverage_unref(PangoCoverage *coverage) { return; }

static inline  void  pango_font_get_glyph_extents(PangoFont *font, PangoGlyph glyph_index, PangoRectangle  *ink_rect, PangoRectangle  *logical_rect) {
}

#define PANGO_LBEARING(ink_rect)		1
#define PANGO_RBEARING(logical_rect)	1

static inline  void  			pango_layout_set_font_description(PangoLayout *layout, const PangoFontDescription *desc_) {}
static inline  void  	pango_layout_set_text(PangoLayout *layout, const char *utf8_word, int len) {}
static inline  PangoLayoutIter *pango_layout_get_iter(PangoLayout *layout) { return NULL; }
static inline  		PangoLayoutRun *pango_layout_iter_get_run_readonly(PangoLayoutIter *run_iter) { return NULL; }
static inline  		PangoFontDescription *pango_font_describe(PangoFont *font) { return NULL; }
static inline  		int pango_glyph_item_iter_init_start(PangoGlyphItemIter *cluster_iter, PangoLayoutRun *run, const char *utf8_word) { return 0; }
static inline  		int  pango_glyph_item_iter_next_cluster(PangoGlyphItemIter  *cluster_iter) { return 0; }

static inline  void  		pango_layout_iter_free(PangoLayoutIter *run_iter) {}

static inline  	bool pango_font_description_equal(const PangoFontDescription *desc, const PangoFontDescription *selected_desc) { return 0; }
static inline  	int pango_font_description_get_weight(const PangoFontDescription *desc) { return 0; }

static inline  		const char * pango_font_family_get_name(PangoFontFamily *famili) { return NULL; }
static inline   void	pango_font_family_list_faces(PangoFontFamily *famili, PangoFontFace ***faces, int *n_faces) {
	*faces = NULL;
	*n_faces = 0;
}
static inline  	PangoFontDescription * pango_font_face_describe(const PangoFontFace *face) { return NULL; }
static inline  	int pango_font_face_is_synthesized(const PangoFontFace *face) { return 1; }

static inline PangoLayout * pango_layout_new(PangoContext *context) { return NULL; }

static inline bool pango_layout_iter_next_run(PangoLayoutIter *run_iter) { return false; }






typedef enum _cairo_font_type {
	CAIRO_FONT_TYPE_TOY,
	CAIRO_FONT_TYPE_FT,
	CAIRO_FONT_TYPE_WIN32,
	CAIRO_FONT_TYPE_QUARTZ,
	CAIRO_FONT_TYPE_USER,
	CAIRO_FONT_TYPE_DWRITE
} cairo_font_type_t;

static inline cairo_font_type_t pango_cairo_font_map_get_font_type(PangoCairoFontMap *font_map) { return CAIRO_FONT_TYPE_TOY; }







typedef enum _cairo_format {
	CAIRO_FORMAT_INVALID   = -1,
	CAIRO_FORMAT_ARGB32    = 0,
	CAIRO_FORMAT_RGB24     = 1,
	CAIRO_FORMAT_A8        = 2,
	CAIRO_FORMAT_A1        = 3,
	CAIRO_FORMAT_RGB16_565 = 4,
	CAIRO_FORMAT_RGB30     = 5,
	CAIRO_FORMAT_RGB96F    = 6,
	CAIRO_FORMAT_RGBA128F  = 7
} cairo_format_t;

struct _cairo_t;
typedef struct _cairo_t  cairo_t ;


static inline cairo_format_t cairo_image_surface_get_format(cairo_surface_t *surface) {
	return CAIRO_FORMAT_ARGB32;
}
static inline int cairo_image_surface_get_width(cairo_surface_t *surface) {
	return 0; 
}
static inline int cairo_image_surface_get_height(cairo_surface_t *surface) {
	return 0;
}
static inline int cairo_image_surface_get_stride(cairo_surface_t *surface) {
	return 0; 
}
static inline uint8_t *cairo_image_surface_get_data(cairo_surface_t *surface) {
	return NULL;
}

static inline cairo_surface_t *cairo_image_surface_create(cairo_format_t fmt, int page_width_, int page_height_) { return NULL; }

static inline cairo_t *cairo_create(cairo_surface_t *surface_) { return NULL; }
static inline PangoLayout *pango_cairo_create_layout(cairo_t *cr_) { return NULL; }

static inline  PangoContext *pango_layout_get_context(PangoLayout *layout_) { return NULL; }


static inline void pango_context_set_base_gravity(PangoContext *context, PangoGravity grav) { return ; }
static inline void pango_context_set_gravity_hint(PangoContext *context, PangoGravityHint hint ) { return ; }
static inline void 		pango_layout_context_changed(PangoLayout *layout_) {}

static inline void 	pango_layout_set_width(PangoLayout *layout_, double max_width ) { return ; }
static inline void 	pango_layout_set_wrap(PangoLayout *layout_, PangoWrapMode mode) { return ; }

static inline 	PangoAttrList *pango_attr_list_new(void) { return NULL; }
static inline PangoAttribute * pango_attr_letter_spacing_new(double char_spacing_ ) { return NULL; }
static inline void 		pango_attr_list_change(PangoAttrList *attr_list, PangoAttribute *spacing_attr) { return ; }

static inline PangoAttribute * pango_attr_font_features_new(const char *str) { return NULL; }
static inline void 	pango_layout_set_attributes(PangoLayout *layout_, PangoAttrList *attr_list) { return ; }
static inline void 	pango_attr_list_unref(PangoAttrList *attr_list) { return ; }
static inline void 		pango_layout_set_spacing(PangoLayout *layout_, double leading_ ) { return ; }

static inline 	const char *pango_layout_get_text(PangoLayout *layout_) { return NULL; }

static inline int pango_layout_iter_get_index(PangoLayoutIter *cluster_iter) { return 0; }

static inline bool pango_layout_iter_next_cluster(PangoLayoutIter *cluster_iter) { return false; }


static inline 	PangoLayoutLine *pango_layout_iter_get_line(PangoLayoutIter *line_iter) { return NULL; }

static inline void pango_layout_iter_get_line_extents(PangoLayoutIter *line_iter, PangoRectangle  *ink_rect, PangoRectangle *logical_rect) { return ; }

static inline int pango_layout_iter_get_baseline(PangoLayoutIter *line_iter) { return 0; }
		
static inline bool pango_layout_iter_next_line(PangoLayoutIter *line_iter) { return false; }

static inline void pango_layout_iter_get_cluster_extents(PangoLayoutIter *cluster_iter, PangoRectangle  *cluster_rect, PangoRectangle  *ext) { return ; }
static inline void pango_extents_to_pixels(PangoRectangle  *cluster_rect, PangoRectangle  *ext) { return ; }

static inline void cairo_translate(cairo_t *cr_, int page_width_ , int v_margin_) {}

static inline double pango_gravity_to_rotation(double f) { return 0.0; }

static inline double pango_context_get_base_gravity(PangoContext *context) { return 0.0; }

static inline void pango_cairo_update_layout(cairo_t *cr_, PangoLayout *layout_) { return ; }


static inline void 		cairo_set_source_rgb(cairo_t *cr_, double r, double g, double b) { return ; }

static inline void cairo_paint(cairo_t *cr_) { return ; }

static inline void cairo_rotate(cairo_t *cr_, double rotation) { return ; }

static inline PangoAttribute *pango_attr_underline_new(PangoUnderline underline_style_) { return NULL; }



static inline void pango_cairo_show_layout(cairo_t *cr_, PangoLayout *layout_) { return ; }

static inline void cairo_destroy(cairo_t *cr_) { return ; }
static inline void   cairo_surface_destroy(cairo_surface_t *surface_) { return ; }
static inline 	PangoAttrList *pango_layout_get_attributes(PangoLayout *layout_) { return NULL; }

static inline void   pango_attr_list_insert(PangoAttrList *attr_list, PangoAttribute *und_attr){ return ; }

static inline PangoLayoutLine *pango_layout_iter_get_line_readonly(PangoLayoutIter *line_iter) { return NULL; }
























//===================================================================================================================================================================================================

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using char32 = signed int;

namespace tesseract {

// Data holder class for a font, intended to avoid having to work with Pango or
// FontConfig-specific objects directly.
class TESS_PANGO_TRAINING_API PangoFontInfo {
public:
  enum FontTypeEnum {
    UNKNOWN,
    SERIF,
    SANS_SERIF,
    DECORATIVE,
  };
  PangoFontInfo();
  ~PangoFontInfo();
  // Initialize from parsing a font description name, defined as a string of the
  // format:
  //   "FamilyName [FaceName] [PointSize]"
  // where a missing FaceName implies the default regular face.
  // eg. "Arial Italic 12", "Verdana"
  //
  // FaceName is a combination of:
  //   [StyleName] [Variant] [Weight] [Stretch]
  // with (all optional) Pango-defined values of:
  // StyleName: Oblique, Italic
  // Variant  : Small-Caps
  // Weight   : Ultra-Light, Light, Medium, Semi-Bold, Bold, Ultra-Bold, Heavy
  // Stretch  : Ultra-Condensed, Extra-Condensed, Condensed, Semi-Condensed,
  //            Semi-Expanded, Expanded, Extra-Expanded, Ultra-Expanded.
  explicit PangoFontInfo(const std::string &name);
  bool ParseFontDescriptionName(const std::string &name);

  // Returns true if the font have codepoint coverage for the specified text.
  bool CoversUTF8Text(const char *utf8_text, int byte_length) const;
  // Modifies string to remove unicode points that are not covered by the
  // font. Returns the number of characters dropped.
  int DropUncoveredChars(std::string *utf8_text) const;

  // Returns true if the entire string can be rendered by the font with full
  // character coverage and no unknown glyph or dotted-circle glyph
  // substitutions on encountering a badly formed unicode sequence.
  // If true, returns individual graphemes. Any whitespace characters in the
  // original string are also included in the list.
  bool CanRenderString(const char *utf8_word, int len, std::vector<std::string> *graphemes) const;
  bool CanRenderString(const char *utf8_word, int len) const;

  // Retrieves the x_bearing and x_advance for the given utf8 character in the
  // font. Returns false if the glyph for the character could not be found in
  // the font.
  // Ref: http://freetype.sourceforge.net/freetype2/docs/glyphs/glyphs-3.html
  bool GetSpacingProperties(const std::string &utf8_char, int *x_bearing, int *x_advance) const;

  // If not already initialized, initializes FontConfig by setting its
  // environment variable and creating a fonts.conf file that points to the
  // trainer_fonts_dir and the cache to trainer_fontconfig_tmpdir.
  static void SoftInitFontConfig();
  // Re-initializes font config, whether or not already initialized.
  // If already initialized, any existing cache is deleted, just to be sure.
  static void HardInitFontConfig(const char *fonts_dir, const char *cache_dir);

  // Accessors
  std::string DescriptionName() const;
  // Font Family name eg. "Arial"
  const std::string &family_name() const {
    return family_name_;
  }
  // Size in points (1/72"), rounded to the nearest integer.
  int font_size() const {
    return font_size_;
  }
  FontTypeEnum font_type() const {
    return font_type_;
  }

  int resolution() const {
    return resolution_;
  }
  void set_resolution(const int resolution) {
    resolution_ = resolution;
  }

private:
  friend class FontUtils;
  void Clear();
  bool ParseFontDescription(const PangoFontDescription *desc);
  // Returns the PangoFont structure corresponding to the closest available font
  // in the font map.
  PangoFont *ToPangoFont() const;

  // Font properties set automatically from parsing the font description name.
  std::string family_name_;
  int font_size_;
  FontTypeEnum font_type_;
  // The Pango description that was used to initialize the instance.
  PangoFontDescription *desc_;
  // Default output resolution to assume for GetSpacingProperties() and any
  // other methods that returns pixel values.
  int resolution_;
  // Fontconfig operates through an environment variable, so it intrinsically
  // cannot be thread-friendly, but you can serialize multiple independent
  // font configurations by calling HardInitFontConfig(fonts_dir, cache_dir).
  // These hold the last initialized values set by HardInitFontConfig or
  // the first call to SoftInitFontConfig.
  // Directory to be scanned for font files.
  static std::string fonts_dir_;
  // Directory to store the cache of font information. (Can be the same as
  // fonts_dir_)
  static std::string cache_dir_;

private:
  PangoFontInfo(const PangoFontInfo &) = delete;
  void operator=(const PangoFontInfo &) = delete;
};

// Static utility methods for querying font availability and font-selection
// based on codepoint coverage.
class TESS_PANGO_TRAINING_API FontUtils {
public:
  // Returns true if the font of the given description name is available in the
  // target directory specified by --fonts_dir
  static bool IsAvailableFont(const char *font_desc) {
    return IsAvailableFont(font_desc, nullptr);
  }
  // Returns true if the font of the given description name is available in the
  // target directory specified by --fonts_dir. If false is returned, and
  // best_match is not nullptr, the closest matching font is returned there.
  static bool IsAvailableFont(const char *font_desc, std::string *best_match);
  // Outputs description names of available fonts.
  static const std::vector<std::string> &ListAvailableFonts();

  // Picks font among available fonts that covers and can render the given word,
  // and returns the font description name and the decomposition of the word to
  // graphemes. Returns false if no suitable font was found.
  static bool SelectFont(const char *utf8_word, const int utf8_len, std::string *font_name,
                         std::vector<std::string> *graphemes);

  // Picks font among all_fonts that covers and can render the given word,
  // and returns the font description name and the decomposition of the word to
  // graphemes. Returns false if no suitable font was found.
  static bool SelectFont(const char *utf8_word, const int utf8_len,
                         const std::vector<std::string> &all_fonts, std::string *font_name,
                         std::vector<std::string> *graphemes);

  // NOTE: The following utilities were written to be backward compatible with
  // StringRender.

  // BestFonts returns a font name and a bit vector of the characters it
  // can render for the fonts that score within some fraction of the best
  // font on the characters in the given hash map.
  // In the flags vector, each flag is set according to whether the
  // corresponding character (in order of iterating ch_map) can be rendered.
  // The return string is a list of the acceptable fonts that were used.
  static std::string BestFonts(const std::unordered_map<char32, int64_t> &ch_map,
                               std::vector<std::pair<const char *, std::vector<bool>>> *font_flag);

  // FontScore returns the weighted renderability score of the given
  // hash map character table in the given font. The unweighted score
  // is also returned in raw_score.
  // The values in the bool vector ch_flags correspond to whether the
  // corresponding character (in order of iterating ch_map) can be rendered.
  static int FontScore(const std::unordered_map<char32, int64_t> &ch_map,
                       const std::string &fontname, int *raw_score, std::vector<bool> *ch_flags);

  // PangoFontInfo is reinitialized, so clear the static list of fonts.
  static void ReInit();
  static void PangoFontTypeInfo();

private:
  static std::vector<std::string> available_fonts_; // cache list
};

} // namespace tesseract

#endif

#endif  // TESSERACT_TRAINING_PANGO_FONT_INFO_H_
