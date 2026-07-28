/*!
 * @file Mw/LowLevel/Cairo.h
 * @brief Cairo Backend
 * @warning This is used MEGA internally.
 */
#ifndef __MW_LOWLEVEL_CAIRO_H__
#define __MW_LOWLEVEL_CAIRO_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/LowLevel.h>
#include <Mw/Abstract/Dynamic.h>

#include <cairo/cairo.h>

#ifdef __cplusplus
extern "C" {
#endif

MWDECL int MwLLCairoCallInit(void);

#ifdef __cplusplus
}
#endif

#define CSD_BORDER_FRAME_LEFT 5
#define CSD_BORDER_FRAME_RIGHT 5
#define CSD_BORDER_FRAME_TOP 23
#define CSD_BORDER_FRAME_BOTTOM 4

typedef struct cairo_call_table {
	void* cairo_lib;

	void (*cairo_set_line_width)(cairo_t* cr,
				     double   width);
	void (*cairo_scale)(cairo_t* cr,
			    double   sx,
			    double   sy);
	cairo_t* (*cairo_create)(cairo_surface_t* target);
	void (*cairo_move_to)(cairo_t* cr,
			      double   x,
			      double   y);

	void (*cairo_rectangle)(cairo_t* cr,
				double	 x,
				double	 y,
				double	 width,
				double	 height);
	void (*cairo_new_path)(cairo_t* cr);
	void (*cairo_set_line_cap)(cairo_t*	    cr,
				   cairo_line_cap_t line_cap);
	void (*cairo_set_source_rgba)(cairo_t* cr,
				      double   red,
				      double   green,
				      double   blue,
				      double   alpha);
	void (*cairo_set_source_rgb)(cairo_t* cr,
				     double   red,
				     double   green,
				     double   blue);
	void (*cairo_reset_clip)(cairo_t* cr);
	unsigned char* (*cairo_image_surface_get_data)(cairo_surface_t* surface);
	void (*cairo_line_to)(cairo_t* cr,
			      double   x,
			      double   y);
	cairo_status_t (*cairo_surface_write_to_png)(cairo_surface_t* surface, const char* filename);
	void (*cairo_surface_destroy)(cairo_surface_t* surface);
	cairo_surface_t* (*cairo_image_surface_create_for_data)(unsigned char* data,
								cairo_format_t format,
								int	       width,
								int	       height,
								int	       stride);
	cairo_surface_t* (*cairo_image_surface_create)(cairo_format_t format,
						       int	      width,
						       int	      height);
	void (*cairo_destroy)(cairo_t* cr);
	cairo_pattern_t* (*cairo_get_source)(cairo_t* cr);
	void (*cairo_close_path)(cairo_t* cr);
	void (*cairo_paint)(cairo_t* cr);
	void (*cairo_surface_flush)(cairo_surface_t* surface);
	void (*cairo_clip)(cairo_t* cr);
	void (*cairo_save)(cairo_t* cr);
	void (*cairo_restore)(cairo_t* cr);
	void (*cairo_surface_mark_dirty)(cairo_surface_t* surface);
	void (*cairo_stroke)(cairo_t* cr);
	void (*cairo_set_source_surface)(cairo_t*	  cr,
					 cairo_surface_t* surface,
					 double		  x,
					 double		  y);
	void (*cairo_fill)(cairo_t* cr);
	void (*cairo_pattern_set_filter)(cairo_pattern_t* pattern,
					 cairo_filter_t	  filter);
	void (*cairo_set_antialias)(cairo_t*, cairo_antialias_t);
	void (*cairo_set_operator)(cairo_t* cr, cairo_operator_t op);

} cairo_call_table_t;

extern cairo_call_table_t cairo_call_tbl;

/* defined inline right here so that it doesn't conflict with the other macros */
MwInline int cairo_load_funcs() {
#ifdef __APPLE__
	cairo_call_tbl.cairo_lib = MwDynamicOpen("libcairo.dylib");
#else
	cairo_call_tbl.cairo_lib = MwDynamicOpen("libcairo.so");
#endif
	if(!cairo_call_tbl.cairo_lib) {
		printf("(libcairo not found, cannot use Wayland or Cairo backend.)\n");
		return 1;
	}

#define CAIRO_FUNC(x) \
	cairo_call_tbl.x = MwDynamicSymbol(cairo_call_tbl.cairo_lib, #x); \
	if(!cairo_call_tbl.x) { \
		printf("WARNING: Could use Wayland/Cairo backend if " #x " was not found. Do you have libcairo?\n"); \
		return 1; \
	};

	CAIRO_FUNC(cairo_set_line_width);
	CAIRO_FUNC(cairo_scale);
	CAIRO_FUNC(cairo_create);
	CAIRO_FUNC(cairo_move_to);
	CAIRO_FUNC(cairo_rectangle);
	CAIRO_FUNC(cairo_new_path);
	CAIRO_FUNC(cairo_set_line_cap);
	CAIRO_FUNC(cairo_set_antialias);
	CAIRO_FUNC(cairo_set_source_rgba);
	CAIRO_FUNC(cairo_set_source_rgb);
	CAIRO_FUNC(cairo_reset_clip);
	CAIRO_FUNC(cairo_image_surface_get_data);
	CAIRO_FUNC(cairo_line_to);
	CAIRO_FUNC(cairo_surface_write_to_png);
	CAIRO_FUNC(cairo_surface_destroy);
	CAIRO_FUNC(cairo_image_surface_create_for_data);
	CAIRO_FUNC(cairo_image_surface_create);
	CAIRO_FUNC(cairo_destroy);
	CAIRO_FUNC(cairo_get_source);
	CAIRO_FUNC(cairo_close_path);
	CAIRO_FUNC(cairo_paint);
	CAIRO_FUNC(cairo_surface_flush);
	CAIRO_FUNC(cairo_clip);
	CAIRO_FUNC(cairo_save);
	CAIRO_FUNC(cairo_restore);
	CAIRO_FUNC(cairo_surface_mark_dirty);
	CAIRO_FUNC(cairo_stroke);
	CAIRO_FUNC(cairo_set_source_surface);
	CAIRO_FUNC(cairo_fill);
	CAIRO_FUNC(cairo_set_operator);
	CAIRO_FUNC(cairo_pattern_set_filter);
#undef CAIRO_FUNC

	return 0;
}

#define cairo_set_line_width cairo_call_tbl.cairo_set_line_width
#define cairo_scale cairo_call_tbl.cairo_scale
#define cairo_create cairo_call_tbl.cairo_create
#define cairo_move_to cairo_call_tbl.cairo_move_to
#define cairo_rectangle cairo_call_tbl.cairo_rectangle
#define cairo_new_path cairo_call_tbl.cairo_new_path
#define cairo_set_line_cap cairo_call_tbl.cairo_set_line_cap
#define cairo_set_antialias cairo_call_tbl.cairo_set_antialias
#define cairo_set_source_rgba cairo_call_tbl.cairo_set_source_rgba
#define cairo_set_source_rgb cairo_call_tbl.cairo_set_source_rgb
#define cairo_reset_clip cairo_call_tbl.cairo_reset_clip
#define cairo_image_surface_get_data cairo_call_tbl.cairo_image_surface_get_data
#define cairo_line_to cairo_call_tbl.cairo_line_to
#define cairo_surface_write_to_png cairo_call_tbl.cairo_surface_write_to_png
#define cairo_surface_destroy cairo_call_tbl.cairo_surface_destroy
#define cairo_image_surface_create_for_data cairo_call_tbl.cairo_image_surface_create_for_data
#define cairo_image_surface_create cairo_call_tbl.cairo_image_surface_create
#define cairo_destroy cairo_call_tbl.cairo_destroy
#define cairo_get_source cairo_call_tbl.cairo_get_source
#define cairo_close_path cairo_call_tbl.cairo_close_path
#define cairo_paint cairo_call_tbl.cairo_paint
#define cairo_surface_flush cairo_call_tbl.cairo_surface_flush
#define cairo_clip cairo_call_tbl.cairo_clip
#define cairo_save cairo_call_tbl.cairo_save
#define cairo_restore cairo_call_tbl.cairo_restore
#define cairo_surface_mark_dirty cairo_call_tbl.cairo_surface_mark_dirty
#define cairo_stroke cairo_call_tbl.cairo_stroke
#define cairo_set_source_surface cairo_call_tbl.cairo_set_source_surface
#define cairo_fill cairo_call_tbl.cairo_fill
#define cairo_set_operator cairo_call_tbl.cairo_set_operator
#define cairo_pattern_set_filter cairo_call_tbl.cairo_pattern_set_filter

struct _MwLLCairo {
	struct _MwLLCommon common;

	cairo_surface_t* front_cs;
	cairo_surface_t* back_cs;
	cairo_t*	 front_cairo;
	cairo_t*	 back_cairo;
	/* The cairo to actually use for draw operations. Typically is front_cairo, but wayland's MwLLBeginDraw can change this to the back_cairo so it can be used to draw window decorations. */
	cairo_t* selected_cairo;

	int x;
	int y;
	int width;
	int height;

	MwBool toplevel;
	MwLL   parent;
};

struct _MwLLCairoColor {
	struct _MwLLCommonColor common;
};

struct _MwLLCairoPixmap {
	struct _MwLLCommonPixmap common;

	cairo_surface_t* cs;
};

void	   MwLLCairoFrontSetup(struct _MwLLCairo* cairo, MwU8* data, MwU32 width, MwU32 height);
void	   MwLLCairoBackSetup(struct _MwLLCairo* cairo, MwU8* data, MwU32 width, MwU32 height);
void	   MwLLCairoFrontDestroy(struct _MwLLCairo* cairo);
void	   MwLLCairoBackDestroy(struct _MwLLCairo* cairo);
void	   MwLLCairoPolygon(struct _MwLLCairo handle, MwPoint* points, int points_count, MwLLColor color);
void	   MwLLCairoLine(struct _MwLLCairo handle, MwPoint* points, MwLLColor color);
MwLLPixmap MwLLCairoCreatePixmap(struct _MwLLCairo handle, unsigned char* data, int width, int height);
void	   MwLLCairoPixmapUpdate(MwLLPixmap handle);
void	   MwLLCairoDestroyPixmap(MwLLPixmap pixmap);
void	   MwLLCairoDrawPixmap(struct _MwLLCairo handle, MwRect* rect, MwLLPixmap pixmap);

void MwLLCairoDestroy(struct _MwLLCairo handle);

#endif
