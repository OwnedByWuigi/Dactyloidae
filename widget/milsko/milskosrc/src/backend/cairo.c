#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

cairo_call_table_t cairo_call_tbl;

void MwLLCairoPolygon(struct _MwLLCairo handle, MwPoint* points, int points_count, MwLLColor color) {
	int i;
	cairo_set_source_rgba(handle.front_cairo, color->common.red / 255.0, color->common.green / 255.0, color->common.blue / 255.0, 1.0);
	cairo_new_path(handle.front_cairo);
	for(i = 0; i < points_count; i++) {
		if(i == 0) {
			cairo_move_to(handle.front_cairo, points[i].x, points[i].y);
		} else {
			cairo_line_to(handle.front_cairo, points[i].x, points[i].y);
		}
	}
	cairo_close_path(handle.front_cairo);

	cairo_set_operator(handle.front_cairo, CAIRO_OPERATOR_SOURCE);
	cairo_fill(handle.front_cairo);
	cairo_set_operator(handle.front_cairo, CAIRO_OPERATOR_OVER);
};

void MwLLCairoLine(struct _MwLLCairo handle, MwPoint* points, MwLLColor color) {
	int i;

	cairo_set_antialias(handle.front_cairo, CAIRO_ANTIALIAS_NONE);
	cairo_set_line_cap(handle.front_cairo, CAIRO_LINE_CAP_SQUARE);
	cairo_set_source_rgba(handle.front_cairo, color->common.red / 255.0, color->common.green / 255.0, color->common.blue / 255.0, 1.0);
	cairo_new_path(handle.front_cairo);
	for(i = 0; i < 2; i++) {
		if(i == 0) {
			cairo_move_to(handle.front_cairo, points[i].x, points[i].y);
		} else {
			cairo_line_to(handle.front_cairo, points[i].x, points[i].y);
		}
	}
	cairo_close_path(handle.front_cairo);
	cairo_stroke(handle.front_cairo);
};

MwLLPixmap MwLLCairoCreatePixmap(struct _MwLLCairo handle, unsigned char* data, int width, int height) {
	MwLLPixmap r = malloc(sizeof(*r));
	(void)handle;

	if(width >= INT16_MAX) width = INT16_MAX;
	if(height >= INT16_MAX) height = INT16_MAX;

	r->common.width	 = width;
	r->common.height = height;
	r->common.raw	 = malloc(4 * width * height);
	memcpy(r->common.raw, data, 4 * width * height);

	r->wayland.cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

	MwLLPixmapUpdate(r);

	return r;
}

void MwLLCairoPixmapUpdate(MwLLPixmap r) {
	int	       i;
	unsigned char* d;

	cairo_surface_flush(r->wayland.cs);
	d = cairo_image_surface_get_data(r->wayland.cs);
	for(i = 0; i < r->common.width * r->common.height * 4; i += 4) {
		MwU32* p = (MwU32*)&d[i];
		*p <<= 8;
		*p |= r->common.raw[i + 3];
		*p <<= 8;
		*p |= r->common.raw[i + 0];
		*p <<= 8;
		*p |= r->common.raw[i + 1];
		*p <<= 8;
		*p |= r->common.raw[i + 2];
	}
	cairo_surface_mark_dirty(r->wayland.cs);
}

void MwLLCairoDestroyPixmap(MwLLPixmap pixmap) {
	cairo_surface_destroy(pixmap->wayland.cs);
	free(pixmap->common.raw);
	free(pixmap);
}

void MwLLCairoDrawPixmap(struct _MwLLCairo handle, MwRect* rect, MwLLPixmap pixmap) {
	cairo_t*	 c;
	cairo_surface_t* cs;
	cairo_t*	 selected_cairo = handle.selected_cairo ? handle.selected_cairo : handle.front_cairo;
	cs				= cairo_image_surface_create(CAIRO_FORMAT_ARGB32, rect->width, rect->height);
	c				= cairo_create(cs);

	if(rect->width <= 0 || rect->height <= 0 || pixmap->common.width <= 0 || pixmap->common.height <= 0) return;
	if(rect->width >= INT16_MAX) rect->width = INT16_MAX;
	if(rect->height >= INT16_MAX) rect->height = INT16_MAX;

	cairo_scale(c, (double)rect->width / pixmap->common.width, (double)rect->height / pixmap->common.height);

	cairo_set_source_surface(c, pixmap->cairo.cs, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(c), CAIRO_FILTER_NEAREST);

	cairo_set_operator(handle.front_cairo, CAIRO_OPERATOR_OVER);

	cairo_paint(c);

	cairo_set_source_surface(selected_cairo, cs, rect->x, rect->y);
	cairo_paint(selected_cairo);

	cairo_destroy(c);
	cairo_surface_destroy(cs);
};

void MwLLCairoFrontSetup(struct _MwLLCairo* cairo, MwU8* data, MwU32 width, MwU32 height) {
	if(data) {
		cairo->front_cs = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, width * 4);
	} else {
		cairo->front_cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	}
	cairo->front_cairo = cairo_create(cairo->front_cs);
}

void MwLLCairoBackSetup(struct _MwLLCairo* cairo, MwU8* data, MwU32 width, MwU32 height) {
	if(data) {
		cairo->back_cs = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, width * 4);
	} else {
		cairo->back_cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	}
	cairo->back_cairo = cairo_create(cairo->back_cs);
}

void MwLLCairoFrontDestroy(struct _MwLLCairo* cairo) {
	cairo_destroy(cairo->front_cairo);
	cairo_surface_destroy(cairo->front_cs);
};
void MwLLCairoBackDestroy(struct _MwLLCairo* cairo) {
	cairo_destroy(cairo->back_cairo);
	cairo_surface_destroy(cairo->back_cs);
};

void MwLLCairoDestroy(struct _MwLLCairo handle) {
	// MwLLCairoFrontDestroy(&handle);
	// MwLLCairoBackDestroy(&handle);
}

static MwLL MwLLCreateImpl(MwLL parent, int x, int y, int width, int height) {
	MwLL r;
	r = malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));
	MwLLCreateCommon(r);

	r->cairo.width	= width;
	r->cairo.height = height;
	r->cairo.x	= x;
	r->cairo.y	= x;

	r->cairo.toplevel = !parent;
	r->cairo.parent	  = parent;

	MwLLCairoFrontSetup(&r->cairo, NULL, r->cairo.width, r->cairo.height);
	MwLLCairoBackSetup(&r->cairo, NULL, r->cairo.width, r->cairo.height);

	return r;
}
static void MwLLDestroyImpl(MwLL handle) {
	MwLLCairoDestroy(handle->cairo);
	free(handle);
}

static void MwLLBeginDrawImpl(MwLL handle) {
	handle->cairo.selected_cairo = handle->cairo.front_cairo;
}
static void MwLLEndDrawImpl(MwLL handle) {
}

static void MwLLPolygonImpl(MwLL handle, MwPoint* points, int points_count, MwLLColor color) {
	MwLLCairoPolygon(handle->cairo, points, points_count, color);
}
static void MwLLLineImpl(MwLL handle, MwPoint* points, MwLLColor color) {
	MwLLCairoLine(handle->cairo, points, color);
}
static MwLLColor MwLLAllocColorImpl(MwLL handle, int r, int g, int b) {
	MwLLColor c = malloc(sizeof(*c));
	MwLLColorUpdate(handle, c, r, g, b);
	return c;
}

static void MwLLColorUpdateImpl(MwLL handle, MwLLColor c, int r, int g, int b) {
	(void)handle;

	c->common.red	= r;
	c->common.green = g;
	c->common.blue	= b;
}
static void MwLLGetXYWHImpl(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h) {
	(void)handle;
	*x = handle->cairo.x;
	*y = handle->cairo.y;
	*w = handle->cairo.width;
	*h = handle->cairo.height;
}
static void MwLLSetXYImpl(MwLL handle, int x, int y) {
	handle->cairo.x = x;
	handle->cairo.y = y;
}
static void MwLLSetWHImpl(MwLL handle, int w, int h) {
	handle->cairo.width  = w;
	handle->cairo.height = h;
}
static void MwLLFreeColorImpl(MwLLColor color) {}

static MwBool lmao = MwFALSE;
static int    MwLLPendingImpl(MwLL handle) {
	   lmao = !lmao;
	   return lmao;
}
static void MwLLNextEventImpl(MwLL handle) {
	MwLLDispatch(handle, draw, NULL);
}
static void	  MwLLSetTitleImpl(MwLL handle, const char* title) {}
static MwLLPixmap MwLLCreatePixmapImpl(MwLL handle, unsigned char* data, int width, int height) {
	return MwLLCairoCreatePixmap(handle->cairo, data, width, height);
}
static void MwLLPixmapUpdateImpl(MwLLPixmap r) {
	MwLLCairoPixmapUpdate(r);
}
static void MwLLDestroyPixmapImpl(MwLLPixmap pixmap) {
	MwLLCairoPixmapUpdate(pixmap);
}

static void MwLLDrawPixmapImpl(MwLL handle, MwRect* rect, MwLLPixmap pixmap) {
	MwLLCairoDrawPixmap(handle->cairo, rect, pixmap);
}
static void   MwLLSetIconImpl(MwLL handle, MwLLPixmap pixmap) {}
static void   MwLLForceRenderImpl(MwLL handle) {}
static void   MwLLSetCursorImpl(MwLL handle, MwCursor* image, MwCursor* mask) {}
static void   MwLLDetachImpl(MwLL handle, MwPoint* point) {}
static void   MwLLShowImpl(MwLL handle, int show) {}
static void   MwLLMakePopupImpl(MwLL handle, MwLL parent) {}
static void   MwLLSetSizeHintsImpl(MwLL handle, int minx, int miny, int maxx, int maxy) {}
static void   MwLLMakeBorderlessImpl(MwLL handle, int toggle) {}
static void   MwLLFocusImpl(MwLL handle) {}
static void   MwLLGrabPointerImpl(MwLL handle, int toggle) {}
static void   MwLLSetClipboardImpl(MwLL handle, const char* text, int clipboard_type) {}
static void   MwLLGetClipboardImpl(MwLL handle, int clipboard_type) {}
static void   MwLLMakeToolWindowImpl(MwLL handle) {}
static void   MwLLGetCursorCoordImpl(MwLL handle, MwPoint* point) {}
static void   MwLLGetScreenSizeImpl(MwLL handle, MwRect* rect) {}
static void   MwLLBeginStateChangeImpl(MwLL handle) {}
static void   MwLLEndStateChangeImpl(MwLL handle) {}
static void   MwLLSetDarkThemeImpl(MwLL handle, int toggle) {}
static MwBool MwLLDoModernImpl(MwLL handle) {
	return MwFALSE;
}
static void MwLLRaiseImpl(MwLL handle) {}
static void MwLLClipImpl(MwLL handle, MwRect* rect) {}
static int  MwLLCairoCallInitImpl(void) {
	 if(cairo_load_funcs() != 0) {
		 return 1;
	 }
	 return 0;
}
#include "call.c"
CALL(Cairo);
