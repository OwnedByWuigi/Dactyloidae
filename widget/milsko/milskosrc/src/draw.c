#include <Mw/Milsko.h>

#ifdef NO_IMAGE
#elif defined(USE_STB_IMAGE)
#include "../external/stb_image.h"
#else
#include <png.h>
#include <jpeglib.h>
#include <jerror.h>
#endif

#include "../external/stb_ds.h"

static int get_color_diff(MwWidget handle) {
	if(MwGetInteger(handle, MwNmodernLook)) {
		return 40;
	} else {
		return 80;
	}
}

static int hex(const char* txt, int len) {
	int i;
	int r = 0;
	for(i = 0; i < len; i++) {
		char c = txt[i];
		int  n = 0;
		if('a' <= c && c <= 'f') {
			n = c - 'a' + 10;
		} else if('A' <= c && c <= 'F') {
			n = c - 'A' + 10;
		} else if('0' <= c && c <= 'g') {
			n = c - '0';
		}
		r = r << 4;
		r |= n;
	}
	return r;
}

static void color_set_disabled_if_disabled(MwWidget handle, MwLLColor rgb) {
	if(MwGetInteger(handle, MwNdisabled) == 1) {
		MwLLColor c = handle->parent == NULL ? NULL : MwParseColor(handle->parent, MwGetText(handle->parent, MwNbackground));

		if(c != NULL) {
			rgb->common.red	  = rgb->common.red + (c->common.red - rgb->common.red) * 0.5;
			rgb->common.green = rgb->common.green + (c->common.green - rgb->common.green) * 0.5;
			rgb->common.blue  = rgb->common.blue + (c->common.blue - rgb->common.blue) * 0.5;
			MwLLColorUpdate(handle->lowlevel, rgb, rgb->common.red, rgb->common.green, rgb->common.blue);

			MwLLFreeColor(c);
		}
	}
};

void MwParseColorNoAllocate(const char* text, MwRGB* rgb) {
	if(text[0] == '#' && strlen(text) == 4) {
		rgb->red   = hex(text + 1, 1);
		rgb->green = hex(text + 2, 1);
		rgb->blue  = hex(text + 3, 1);

		rgb->red |= rgb->red << 4;
		rgb->green |= rgb->green << 4;
		rgb->blue |= rgb->blue << 4;
	} else if(text[0] == '#' && strlen(text) == 7) {
		rgb->red   = hex(text + 1, 2);
		rgb->green = hex(text + 3, 2);
		rgb->blue  = hex(text + 5, 2);
	} else {
		MwParseColorNameNoAllocate(text, rgb);
		return;
	}
}

MwLLColor MwParseColor(MwWidget handle, const char* text) {
	MwRGB rgb;

	rgb.red	  = 0;
	rgb.green = 0;
	rgb.blue  = 0;

	MwParseColorNoAllocate(text, &rgb);

	return MwLLAllocColor(handle->lowlevel, rgb.red, rgb.green, rgb.blue);
}

MwLLColor MwLightenColor(MwWidget handle, MwLLColor color, int r, int g, int b) {
	int cr = color->common.red + r;
	int cg = color->common.green + g;
	int cb = color->common.blue + b;

	if(cr < 0) cr = 0;
	if(cg < 0) cg = 0;
	if(cb < 0) cb = 0;
	if(cr > 255) cr = 255;
	if(cg > 255) cg = 255;
	if(cb > 255) cb = 255;

	return MwLLAllocColor(handle->lowlevel, cr, cg, cb);
}

void MwDrawRect(MwWidget handle, MwRect* rect, MwLLColor color) {
	MwPoint p[4];

	p[0].x = rect->x;
	p[0].y = rect->y;

	p[1].x = rect->x + rect->width;
	p[1].y = rect->y;

	p[2].x = rect->x + rect->width;
	p[2].y = rect->y + rect->height;

	p[3].x = rect->x;
	p[3].y = rect->y + rect->height;

	MwLLPolygon(handle->lowlevel, p, 4, color);
}

void MwDrawRectFading(MwWidget handle, MwRect* rect, MwLLColor color) {
	MwLLPixmap     pixmap;
	int	       y;
	double	       darken	  = 0.;
	int	       ColorDiff  = get_color_diff(handle);
	double	       darkenStep = (ColorDiff / 2.) / rect->height;
	unsigned long  sz	  = 1 * rect->height * 4;
	unsigned char* data	  = malloc(sz);
	MwRect	       r	  = *rect;
	if(rect->width <= 0 || rect->height <= 0) {
		free(data);
		return;
	}
	memset(data, 0, sz);

	r.x += 1;
	r.y += 1;
	r.width -= 2;
	r.height -= 2;

	for(y = 0; y < rect->height; y++) {
		MwLLColor col = MwLightenColor(handle, color, -darken, -darken, -darken);
		int	  idx = y * 4;
		color_set_disabled_if_disabled(handle, col);
		data[idx]     = col->common.red;
		data[idx + 1] = col->common.green;
		data[idx + 2] = col->common.blue;
		data[idx + 3] = 255;
		MwLLFreeColor(col);
		darken += darkenStep;
	}

	pixmap = MwLLCreatePixmap(handle->lowlevel, data, 1, rect->height);
	MwLLDrawPixmap(handle->lowlevel, &r, pixmap);
	MwLLDestroyPixmap(pixmap);

	free(data);
}

void MwDrawFrame(MwWidget handle, MwRect* rect, MwLLColor color, int invert) {
	MwDrawFrameWithBorder(handle, rect, color, invert, MwDefaultBorderWidth(handle));
}

static int translate_border(MwWidget handle, int border) {
	if(border == MwDEFAULT) return MwDefaultBorderWidth(handle);

	return border;
}

void MwDrawFrameWithBorder(MwWidget handle, MwRect* rect, MwLLColor color, int invert, int border) {
	int inv;

	border = translate_border(handle, border);

	if((inv = MwGetInteger(handle, MwNforceInverted)) != MwDEFAULT && inv) invert = 1;
	if(MwGetInteger(handle, MwNmodernLook)) {
		MwDrawFrameEx(handle, rect, color, invert, border, 0, 0);
	} else {
		int diff = get_color_diff(handle) / 3 * 2;

		if(border >= 2) {
			int i;
			int st = invert ? 1 : 0;
			for(i = st; i < st + 2; i++) {
				if((i % 2) == 0) MwDrawFrameEx(handle, rect, color, invert, border - 1, -diff, (handle->parent == NULL || handle->parent->lowlevel == NULL) ? 1 : 0);
				if((i % 2) == 1) MwDrawFrameEx(handle, rect, color, invert, 1, diff, 0);
			}
		} else {
			MwDrawFrameEx(handle, rect, color, invert, border, 0, 0);
		}
	}
}

void MwDrawWidgetBack(MwWidget handle, MwRect* rect, MwLLColor color, int invert, int border) {
	MwLLColor col;

	if(rect->width <= 0 || rect->height <= 0) return;

	if(border) {
		if(!handle->lowlevel->common.supports_transparency) {
			MwLLColor c = handle->parent == NULL ? NULL : MwParseColor(handle->parent, MwGetText(handle->parent, MwNbackground));

			if(c != NULL) {
				MwDrawRect(handle, rect, c);
				MwLLFreeColor(c);
			}
		}

		MwDrawFrameWithBorder(handle, rect, color, invert, border);

		if(border == MwDEFAULT) {
			int d = MwDefaultBorderWidth(handle) - MwDefaultThinBorderWidth(handle);

			rect->x -= d;
			rect->y -= d;
			rect->width += d * 2;
			rect->height += d * 2;
		}
	}

	if(rect->width <= 0 || rect->height <= 0) return;

	col = invert ? MwLightenColor(handle, color, -8, -8, -8) : MwLightenColor(handle, color, 0, 0, 0);
	color_set_disabled_if_disabled(handle, col);

	if(MwGetInteger(handle, MwNmodernLook)) {
		MwDrawRectFading(handle, rect, col);
	} else {
		MwDrawRect(handle, rect, col);
	}
	MwLLFreeColor(col);
}

void MwDrawDiamond(MwWidget handle, MwRect* rect, MwLLColor color, int invert) {
	MwPoint	  p[6];
	int	  border    = MwDefaultBorderWidth(handle);
	int	  ColorDiff = get_color_diff(handle) + (MwGetInteger(handle, MwNmodernLook) ? 48 : 0);
	MwLLColor darker    = MwLightenColor(handle, color, -ColorDiff, -ColorDiff, -ColorDiff);
	MwLLColor lighter   = MwLightenColor(handle, color, ColorDiff, ColorDiff, ColorDiff);
	MwLLColor col	    = invert ? MwLightenColor(handle, color, -8, -8, -8) : MwLightenColor(handle, color, 0, 0, 0);
	color_set_disabled_if_disabled(handle, col);
	color_set_disabled_if_disabled(handle, darker);
	color_set_disabled_if_disabled(handle, lighter);

	p[0].x = rect->x;
	p[0].y = rect->y + rect->height / 2;

	p[1].x = rect->x + rect->width / 2;
	p[1].y = rect->y;

	p[2].x = rect->x + rect->width;
	p[2].y = rect->y + rect->height / 2;

	p[3].x = rect->x + rect->width - border;
	p[3].y = rect->y + rect->height / 2;

	p[4].x = rect->x + rect->width / 2;
	p[4].y = rect->y + border;

	p[5].x = rect->x + border;
	p[5].y = rect->y + rect->height / 2;

	MwLLPolygon(handle->lowlevel, p, 6, invert ? darker : lighter);

	p[0].x = rect->x;
	p[0].y = rect->y + rect->height / 2;

	p[1].x = rect->x + rect->width / 2;
	p[1].y = rect->y + rect->height;

	p[2].x = rect->x + rect->width;
	p[2].y = rect->y + rect->height / 2;

	p[3].x = rect->x + rect->width - border;
	p[3].y = rect->y + rect->height / 2;

	p[4].x = rect->x + rect->width / 2;
	p[4].y = rect->y + rect->height - border;

	p[5].x = rect->x + border;
	p[5].y = rect->y + rect->height / 2;

	MwLLPolygon(handle->lowlevel, p, 6, invert ? lighter : darker);

	p[0].x = rect->x + rect->width / 2;
	p[0].y = rect->y + border;

	p[1].x = rect->x + rect->width - border;
	p[1].y = rect->y + rect->height / 2;

	p[2].x = rect->x + rect->width / 2;
	p[2].y = rect->y + rect->height - border;

	p[3].x = rect->x + border;
	p[3].y = rect->y + rect->height / 2;

	MwLLPolygon(handle->lowlevel, p, 4, col);

	MwLLFreeColor(col);
	MwLLFreeColor(lighter);
	MwLLFreeColor(darker);
}

static void MwDrawFrameEx_simple(MwWidget handle, MwRect* rect, MwLLColor color, int invert, int border, int diff, int same) {
	MwPoint	  p[7];
	int	  ColorDiff = get_color_diff(handle);
	MwLLColor darker    = MwLightenColor(handle, color, -ColorDiff * 3 / 2 + diff, -ColorDiff * 3 / 2 + diff, -ColorDiff * 3 / 2 + diff);
	MwLLColor lighter   = same ? MwLightenColor(handle, darker, 0, 0, 0) : MwLightenColor(handle, color, ColorDiff - diff, ColorDiff - diff, ColorDiff - diff);
	color_set_disabled_if_disabled(handle, darker);
	color_set_disabled_if_disabled(handle, lighter);

	p[0].x = rect->x;
	p[0].y = rect->y;

	p[1].x = rect->x + rect->width;
	p[1].y = rect->y;

	p[2].x = rect->x + rect->width - border;
	p[2].y = rect->y + border;
	p[3].x = rect->x + border;
	p[3].y = rect->y + border;

	p[4].x = rect->x + border;
	p[4].y = rect->y + rect->height - border;

	p[5].x = rect->x;
	p[5].y = rect->y + rect->height;
	MwLLPolygon(handle->lowlevel, p, 6, invert ? darker : lighter);

	p[0].x = rect->x + rect->width;
	p[0].y = rect->y;

	p[1].x = rect->x + rect->width - border;
	p[1].y = rect->y + border;

	p[2].x = rect->x + rect->width - border;
	p[2].y = rect->y + rect->height - border;

	p[3].x = rect->x + border;
	p[3].y = rect->y + rect->height - border;

	p[4].x = rect->x;
	p[4].y = rect->y + rect->height;

	p[5].x = rect->x + rect->width;
	p[5].y = rect->y + rect->height;
	MwLLPolygon(handle->lowlevel, p, 6, invert ? lighter : darker);

	MwLLFreeColor(lighter);
	MwLLFreeColor(darker);

	rect->x += border;
	rect->y += border;
	rect->width -= border * 2;
	rect->height -= border * 2;
}

static void frame_border_complex(MwWidget handle, MwRect* rect, MwLLColor lighter, MwLLColor darker, int invert, int border, int diff, int same) {
	MwPoint p[7];

	(void)diff;
	(void)same;

	p[0].x = rect->x;
	p[0].y = rect->y;

	p[1].x = rect->x + rect->width;
	p[1].y = rect->y;

	p[2].x = rect->x + rect->width - border;
	p[2].y = rect->y + border;
	p[3].x = rect->x + border;
	p[3].y = rect->y + border;

	p[4].x = rect->x + border;
	p[4].y = rect->y + rect->height - border;

	p[5].x = rect->x;
	p[5].y = rect->y + rect->height;
	MwLLPolygon(handle->lowlevel, p, 6, invert ? lighter : darker);

	p[0].x = rect->x + rect->width;
	p[0].y = rect->y;

	p[1].x = rect->x + rect->width - border;
	p[1].y = rect->y + border;

	p[2].x = rect->x + rect->width - border;
	p[2].y = rect->y + rect->height - border;

	p[3].x = rect->x + border;
	p[3].y = rect->y + rect->height - border;

	p[4].x = rect->x;
	p[4].y = rect->y + rect->height;

	p[5].x = rect->x + rect->width;
	p[5].y = rect->y + rect->height;
	MwLLPolygon(handle->lowlevel, p, 6, invert ? lighter : darker);
}

static void MwDrawFrameEx_complex(MwWidget handle, MwRect* rect, MwLLColor color, int invert, int border, int diff, int same) {
	int	  ColorDiff = get_color_diff(handle);
	MwLLColor darker    = MwLightenColor(handle, color, -ColorDiff * 3 / 2 + diff, -ColorDiff * 3 / 2 + diff, -ColorDiff * 3 / 2 + diff);
	MwLLColor lighter   = same ? MwLightenColor(handle, darker, 0, 0, 0) : MwLightenColor(handle, color, (ColorDiff / 2) - diff, (ColorDiff / 2) - diff, (ColorDiff / 2) - diff);
	MwRect	  r	    = *rect;

	color_set_disabled_if_disabled(handle, darker);
	color_set_disabled_if_disabled(handle, lighter);

	frame_border_complex(handle, rect, lighter, darker, invert, border == 1 ? 1 : (border / 2), diff, same);

	if(border > 1) {
		r.x += border / 2;
		r.y += border / 2;
		r.width -= border;
		r.height -= border;
		frame_border_complex(handle, &r, lighter, darker, !invert, border / 2, diff, same);
	}

	MwLLFreeColor(lighter);
	MwLLFreeColor(darker);

	rect->x += border;
	rect->y += border;
	rect->width -= border * 2;
	rect->height -= border * 2;
}

void MwDrawFrameEx(MwWidget handle, MwRect* rect, MwLLColor color, int invert, int border, int diff, int same) {
	border = translate_border(handle, border);

	if(MwGetInteger(handle, MwNmodernLook)) {
		MwDrawFrameEx_complex(handle, rect, color, invert, border, diff, same);
	} else {
		MwDrawFrameEx_simple(handle, rect, color, invert, border, diff, same);
	}
}

void MwDrawTriangle(MwWidget handle, MwRect* rect, MwLLColor color, int invert, int direction) {
	MwPoint	  p1[4], p2[4], p3[4], p4[3];
	const int border    = MwGetInteger(handle, MwNmodernLook) ? 2 : MwDefaultBorderWidth(handle);
	int	  ColorDiff = get_color_diff(handle);
	MwLLColor darker    = MwLightenColor(handle, color, -ColorDiff, -ColorDiff, -ColorDiff);
	MwLLColor lighter   = MwLightenColor(handle, color, ColorDiff, ColorDiff, ColorDiff);
	MwLLColor col	    = invert ? MwLightenColor(handle, color, -8, -8, -8) : MwLightenColor(handle, color, 0, 0, 0);
	double	  deg	    = 30 * ((direction == MwEAST || direction == MwWEST) ? 2 : 1);
	double	  c	    = cos(deg / 180 * M_PI);
	double	  s	    = sin(deg / 180 * M_PI);

	color_set_disabled_if_disabled(handle, col);
	color_set_disabled_if_disabled(handle, darker);
	color_set_disabled_if_disabled(handle, lighter);

	if(direction == MwNORTH) {
		p1[0].x = rect->x + rect->width / 2;
		p1[0].y = rect->y;

		p1[1].x = rect->x;
		p1[1].y = rect->y + rect->height;

		p1[2].x = rect->x + c * border;
		p1[2].y = rect->y + rect->height - s * border;

		p1[3].x = rect->x + rect->width / 2;
		p1[3].y = rect->y + border;

		p2[0].x = rect->x + rect->width / 2;
		p2[0].y = rect->y;

		p2[1].x = rect->x + rect->width;
		p2[1].y = rect->y + rect->height;

		p2[2].x = rect->x + rect->width - c * border;
		p2[2].y = rect->y + rect->height - s * border;

		p2[3].x = rect->x + rect->width / 2;
		p2[3].y = rect->y + border;

		p3[0].x = rect->x + c * border;
		p3[0].y = rect->y + rect->height - s * border;

		p3[1].x = rect->x;
		p3[1].y = rect->y + rect->height;

		p3[2].x = rect->x + rect->width;
		p3[2].y = rect->y + rect->height;

		p3[3].x = rect->x + rect->width - c * border;
		p3[3].y = rect->y + rect->height - s * border;

		MwLLPolygon(handle->lowlevel, p1, 4, invert ? darker : lighter);
		MwLLPolygon(handle->lowlevel, p2, 4, invert ? lighter : darker);
		MwLLPolygon(handle->lowlevel, p3, 4, invert ? lighter : darker);

		p4[0].x = rect->x + c * border;
		p4[0].y = rect->y + rect->height - s * border;

		p4[1].x = rect->x + rect->width - c * border;
		p4[1].y = rect->y + rect->height - s * border;

		p4[2].x = rect->x + rect->width / 2;
		p4[2].y = rect->y + border;
	} else if(direction == MwSOUTH) {
		p1[0].x = rect->x;
		p1[0].y = rect->y;

		p1[1].x = rect->x + c * border;
		p1[1].y = rect->y + s * border;

		p1[2].x = rect->x + rect->width - c * border;
		p1[2].y = rect->y + s * border;

		p1[3].x = rect->x + rect->width;
		p1[3].y = rect->y;

		p2[0].x = rect->x;
		p2[0].y = rect->y;

		p2[1].x = rect->x + c * border;
		p2[1].y = rect->y + s * border;

		p2[2].x = rect->x + rect->width / 2;
		p2[2].y = rect->y + rect->height - border;

		p2[3].x = rect->x + rect->width / 2;
		p2[3].y = rect->y + rect->height;

		p3[0].x = rect->x + rect->width;
		p3[0].y = rect->y;

		p3[1].x = rect->x + rect->width / 2;
		p3[1].y = rect->y + rect->height;

		p3[2].x = rect->x + rect->width / 2;
		p3[2].y = rect->y + rect->height - border;

		p3[3].x = rect->x + rect->width - c * border;
		p3[3].y = rect->y + s * border;

		MwLLPolygon(handle->lowlevel, p1, 4, invert ? darker : lighter);
		MwLLPolygon(handle->lowlevel, p2, 4, invert ? darker : lighter);
		MwLLPolygon(handle->lowlevel, p3, 4, invert ? lighter : darker);

		p4[0].x = rect->x + c * border;
		p4[0].y = rect->y + s * border;

		p4[1].x = rect->x + rect->width - c * border;
		p4[1].y = rect->y + s * border;

		p4[2].x = rect->x + rect->width / 2;
		p4[2].y = rect->y + rect->height - border;
	} else if(direction == MwEAST) {
		p1[0].x = rect->x;
		p1[0].y = rect->y;

		p1[1].x = rect->x + c * border;
		p1[1].y = rect->y + s * border;

		p1[2].x = rect->x + c * border;
		p1[2].y = rect->y + rect->height - s * border;

		p1[3].x = rect->x;
		p1[3].y = rect->y + rect->height;

		p2[0].x = rect->x;
		p2[0].y = rect->y;

		p2[1].x = rect->x + rect->width;
		p2[1].y = rect->y + rect->height / 2;

		p2[2].x = rect->x + rect->width - border;
		p2[2].y = rect->y + rect->height / 2;

		p2[3].x = rect->x + c * border;
		p2[3].y = rect->y + s * border;

		p3[0].x = rect->x + c * border;
		p3[0].y = rect->y + rect->height - s * border;

		p3[1].x = rect->x;
		p3[1].y = rect->y + rect->height;

		p3[2].x = rect->x + rect->width;
		p3[2].y = rect->y + rect->height / 2;

		p3[3].x = rect->x + rect->width - border;
		p3[3].y = rect->y + rect->height / 2;

		MwLLPolygon(handle->lowlevel, p1, 4, invert ? darker : lighter);
		MwLLPolygon(handle->lowlevel, p2, 4, invert ? darker : lighter);
		MwLLPolygon(handle->lowlevel, p3, 4, invert ? lighter : darker);

		p4[0].x = rect->x + rect->width - border;
		p4[0].y = rect->y + rect->height / 2;

		p4[1].x = rect->x + c * border;
		p4[1].y = rect->y + rect->height - s * border;

		p4[2].x = rect->x + c * border;
		p4[2].y = rect->y + s * border;
	} else if(direction == MwWEST) {
		p1[0].x = rect->x;
		p1[0].y = rect->y + rect->height / 2;

		p1[1].x = rect->x + border;
		p1[1].y = rect->y + rect->height / 2;

		p1[2].x = rect->x + rect->width - c * border;
		p1[2].y = rect->y + s * border;

		p1[3].x = rect->x + rect->width;
		p1[3].y = rect->y;

		p2[0].x = rect->x;
		p2[0].y = rect->y + rect->height / 2;

		p2[1].x = rect->x + border;
		p2[1].y = rect->y + rect->height / 2;

		p2[2].x = rect->x + rect->width - c * border;
		p2[2].y = rect->y + rect->height - s * border;

		p2[3].x = rect->x + rect->width;
		p2[3].y = rect->y + rect->height;

		p3[0].x = rect->x + rect->width;
		p3[0].y = rect->y;

		p3[1].x = rect->x + rect->width - c * border;
		p3[1].y = rect->y + s * border;

		p3[2].x = rect->x + rect->width - c * border;
		p3[2].y = rect->y + rect->height - s * border;

		p3[3].x = rect->x + rect->width;
		p3[3].y = rect->y + rect->height;

		MwLLPolygon(handle->lowlevel, p1, 4, invert ? darker : lighter);
		MwLLPolygon(handle->lowlevel, p2, 4, invert ? lighter : darker);
		MwLLPolygon(handle->lowlevel, p3, 4, invert ? lighter : darker);

		p4[0].x = rect->x + border;
		p4[0].y = rect->y + rect->height / 2;

		p4[1].x = rect->x + rect->width - c * border;
		p4[1].y = rect->y + rect->height - s * border;

		p4[2].x = rect->x + rect->width - c * border;
		p4[2].y = rect->y + s * border;
	}
	MwLLPolygon(handle->lowlevel, p4, 3, col);

	MwLLFreeColor(col);
	MwLLFreeColor(lighter);
	MwLLFreeColor(darker);
}

#if defined(NO_IMAGE)
#elif !defined(USE_STB_IMAGE)
static void PNGCAPI user_error(png_structp png, const char* str) {
	(void)str;

	longjmp(png_jmpbuf(png), 1);
}

static void PNGCAPI user_warning(png_structp png, const char* str) {
	(void)png;
	(void)str;
}

static unsigned char* load_png(FILE* f, int* w, int* h) {
	png_structp    png  = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop      info = png_create_info_struct(png);
	int	       depth, type, i;
	unsigned char* data;
	unsigned char* row;

	if(setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, NULL);
		return NULL;
	}

	png_set_error_fn(png, NULL, user_error, user_warning);

	png_init_io(png, f);
	png_read_info(png, info);

	*w    = png_get_image_width(png, info);
	*h    = png_get_image_height(png, info);
	depth = png_get_bit_depth(png, info);
	type  = png_get_color_type(png, info);

	if(depth == 16) png_set_strip_16(png);
	if(type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
	if(type == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
	if(png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
	if(type == PNG_COLOR_TYPE_RGB || type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_PALETTE) png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	if(type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
	png_read_update_info(png, info);

	data = malloc((*w) * (*h) * 4);

	row = malloc(png_get_rowbytes(png, info));
	for(i = 0; i < (*h); i++) {
		png_read_row(png, row, NULL);
		memcpy(&data[i * (*w) * 4], row, png_get_rowbytes(png, info));
	}
	free(row);

	png_destroy_read_struct(&png, &info, NULL);

	return data;
}

static void jpeg_err(j_common_ptr cinfo) {
	jmp_buf* j = cinfo->client_data;

	longjmp(*j, 1);
}

static unsigned char* load_jpeg(FILE* f, int* w, int* h) {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr	      jerr;
	jmp_buf*		      j = malloc(sizeof(*j));
	unsigned char*		      data;
	unsigned char*		      row;
	int			      i;

	if(setjmp(*j)) {
		jpeg_destroy_decompress(&cinfo);
		free(j);
		return NULL;
	}

	cinfo.client_data = j;
	cinfo.err	  = jpeg_std_error(&jerr);
	jerr.error_exit	  = jpeg_err;

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, f);
	jpeg_read_header(&cinfo, TRUE);

	jpeg_start_decompress(&cinfo);

	*w = cinfo.output_width;
	*h = cinfo.output_height;

	data = malloc((*w) * (*h) * 4);
	row  = malloc((*h) * cinfo.num_components);
	for(i = 0; i < (*h); i++) {
		int j;
		jpeg_read_scanlines(&cinfo, &row, 1);

		for(j = 0; j < (*w); j++) {
			unsigned char* from = &row[j * cinfo.num_components];
			unsigned char* to   = &data[(i * (*w) + j) * 4];

			memcpy(to, from, 3);
			to[3] = 255;
		}
	}
	free(row);

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	free(j);

	return data;
}
#endif

static unsigned char* load_image(const char* path, int* w, int* h) {
#if defined(NO_IMAGE)
	return NULL;
#elif defined(USE_STB_IMAGE)
	int ch;

	return stbi_load(path, w, h, &ch, 4);
#else
	FILE*	       f = fopen(path, "rb");
	unsigned char* d;
	if(f == NULL) return NULL;

	if((d = load_png(f, w, h)) != NULL) return d;
	fseek(f, 0, SEEK_SET);

	if((d = load_jpeg(f, w, h)) != NULL) return d;
	fseek(f, 0, SEEK_SET);

	fclose(f);

	return NULL;
#endif
}

MwLLPixmap MwLoadImage(MwWidget handle, const char* path) {
	int	       width, height;
	unsigned char* rgb = load_image(path, &width, &height);
	MwLLPixmap     px;

	if(rgb == NULL) return NULL;

	px = MwLoadRaw(handle, rgb, width, height);

	free(rgb);

	return px;
}

MwLLPixmap MwLoadRaw(MwWidget handle, unsigned char* rgb, int width, int height) {
	MwLLPixmap px = MwLLCreatePixmap(handle->lowlevel, rgb, width, height);

	px->common.user = handle;
	MwPixmapReloadRaw(px, rgb);

	return px;
}

void MwPixmapReloadRaw(MwLLPixmap px, unsigned char* rgb) {
	int	  i;
	MwWidget  handle = px->common.user;
	MwLLColor base	 = handle->bgcolor == NULL ? MwParseColor(handle, MwGetText(handle, MwNbackground)) : handle->bgcolor;

	memset(px->common.raw, 0, px->common.width * px->common.height * 4);
	for(i = 0; i < px->common.width * px->common.height; i++) {
		unsigned char* pin  = &rgb[i * 4];
		unsigned char* pout = &px->common.raw[i * 4];
		double	       a    = pin[3];

		a /= 255;
		if(a != 0) {
			pout[0] = pin[0] * a;
			pout[1] = pin[1] * a;
			pout[2] = pin[2] * a;

			pout[0] += base->common.red * (1 - a);
			pout[1] += base->common.green * (1 - a);
			pout[2] += base->common.blue * (1 - a);
			pout[3] = 255;
		}
	}

	if(handle->bgcolor == NULL) MwLLFreeColor(base);

	MwLLPixmapUpdate(px);
}

unsigned char* MwPixmapGetRaw(MwLLPixmap pixmap) {
	return pixmap->common.raw;
}

void MwPixmapGetSize(MwLLPixmap pixmap, MwRect* rect) {
	rect->width  = pixmap->common.width;
	rect->height = pixmap->common.height;
}

void MwColorGet(MwLLColor color, int* red, int* green, int* blue) {
	*red   = color->common.red;
	*green = color->common.green;
	*blue  = color->common.blue;
}

MwLLPixmap MwLoadIcon(MwWidget handle, MwU32* data) {
	int	       width  = (data[0] >> 16) & 0xffff;
	int	       height = (data[0]) & 0xffff;
	unsigned char* rgba   = malloc(width * height * 4);
	MwLLPixmap     px;
	int	       i;

	memset(rgba, 0, width * height * 4);

	for(i = 0; i < width * height; i++) {
		unsigned char* px = &rgba[i * 4];

		px[0] = (data[i + 1] >> 24) & 0xff;
		px[1] = (data[i + 1] >> 16) & 0xff;
		px[2] = (data[i + 1] >> 8) & 0xff;
		px[3] = data[i + 1] & 0xff;
	}

	px = MwLoadRaw(handle, rgba, width, height);

	free(rgba);

	return px;
}

typedef struct color {
	char* key;
	char* value;
	int   r;
	int   g;
	int   b;
	int   a;
} color_t;

MwLLPixmap MwLoadXPM(MwWidget handle, char** data) {
	int	       col, row, colors, cpp;
	unsigned char* rgb;
	MwLLPixmap     px;
	color_t*       c = NULL;
	int	       i, y, x;
	char*	       comp;

	sh_new_strdup(c);

	sscanf(data[0], "%d %d %d %d", &col, &row, &colors, &cpp);

	for(i = 0; i < colors; i++) {
		char  k[128];
		char* v = data[i + 1] + cpp + 3;
		int   ind;
		if(strcmp(v, "None") == 0) {
			memcpy(k, data[i + 1], cpp);
			k[cpp] = 0;

			shput(c, k, v);
			ind = shgeti(c, k);

			c[ind].r = 0;
			c[ind].g = 0;
			c[ind].b = 0;
			c[ind].a = 0;
		} else {
			MwLLColor color = MwParseColor(handle, v);

			memcpy(k, data[i + 1], cpp);
			k[cpp] = 0;

			shput(c, k, v);
			ind = shgeti(c, k);

			c[ind].r = color->common.red;
			c[ind].g = color->common.green;
			c[ind].b = color->common.blue;
			c[ind].a = 255;

			MwLLFreeColor(color);
		}
	}

	rgb	  = malloc(row * col * 4);
	comp	  = malloc(cpp + 1);
	comp[cpp] = 0;
	for(y = 0; y < row; y++) {
		for(x = 0; x < col; x++) {
			unsigned char* pout = &rgb[(y * col + x) * 4];
			color_t	       colent;
			memcpy(comp, &data[1 + colors + y][x * cpp], cpp);

			colent = shgets(c, comp);

			pout[0] = colent.r;
			pout[1] = colent.g;
			pout[2] = colent.b;
			pout[3] = colent.a;
		}
	}
	free(comp);

	px = MwLoadRaw(handle, rgb, col, row);

	free(rgb);

	shfree(c);

	return px;
}
