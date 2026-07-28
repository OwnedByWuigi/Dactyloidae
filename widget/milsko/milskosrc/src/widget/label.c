#include <Mw/Milsko.h>

#include "../../external/stb_ds.h"

#define Spacing 1

static int wcreate(MwWidget handle) {
	MwLabel lab = malloc(sizeof(*lab));
	memset(lab, 0, sizeof(*lab));

	handle->internal = lab;

	MwSetDefault(handle);
	MwSetInteger(handle, MwNalignment, MwALIGNMENT_CENTER);
	MwSetInteger(handle, MwNbold, 0);
	MwSetInteger(handle, MwNsevenSegment, 0);
	MwSetInteger(handle, MwNlength, 4);
	MwSetInteger(handle, MwNleftPadding, 0);

	return 0;
}

static void destroy(MwWidget handle) {
	MwLabel lab = handle->internal;

	hmfree(lab->segment);

	free(handle->internal);
}

static void draw_v(MwWidget handle, unsigned char* raw, int x, int y, int stride, MwLLColor text) {
	int cy, cx, b;

	int l_one = MwGetInteger(handle, MwNlength) - (MwGetInteger(handle, MwNlength) % 2);
	int s_one = (l_one * 3 / 4) - ((l_one * 3 / 4) % 2) + 1;

	for(cy = y; cy < y + l_one; cy++) {
		for(cx = x; cx < x + s_one; cx++) {
			unsigned char* px = &raw[(cy * stride + cx) * 4];
			px[0]		  = text->common.red;
			px[1]		  = text->common.green;
			px[2]		  = text->common.blue;
			px[3]		  = 255;
		}
	}

	b = y - (s_one - 1) / 2;
	for(cy = b; cy < b + (s_one - 1) / 2; cy++) {
		int w  = (cy - b) * 2 + 1;
		int b2 = x + (s_one - w) / 2;
		for(cx = b2; cx < b2 + w; cx++) {
			unsigned char* px;

			px    = &raw[(cy * stride + cx) * 4];
			px[0] = text->common.red;
			px[1] = text->common.green;
			px[2] = text->common.blue;
			px[3] = 255;
		}
	}

	b = y + l_one;
	for(cy = b; cy < b + (s_one - 1) / 2; cy++) {
		int w  = ((s_one - 1) / 2 - 1 - (cy - b)) * 2 + 1;
		int b2 = x + (s_one - w) / 2;
		for(cx = b2; cx < b2 + w; cx++) {
			unsigned char* px;

			px    = &raw[(cy * stride + cx) * 4];
			px[0] = text->common.red;
			px[1] = text->common.green;
			px[2] = text->common.blue;
			px[3] = 255;
		}
	}
}

static void draw_h(MwWidget handle, unsigned char* raw, int x, int y, int stride, MwLLColor text) {
	int cy, cx, b;

	int l_one = MwGetInteger(handle, MwNlength) - (MwGetInteger(handle, MwNlength) % 2);
	int s_one = (l_one * 3 / 4) - ((l_one * 3 / 4) % 2) + 1;

	for(cx = x; cx < x + l_one; cx++) {
		for(cy = y; cy < y + s_one; cy++) {
			unsigned char* px = &raw[(cy * stride + cx) * 4];
			px[0]		  = text->common.red;
			px[1]		  = text->common.green;
			px[2]		  = text->common.blue;
			px[3]		  = 255;
		}
	}

	b = x - (s_one - 1) / 2;
	for(cx = b; cx < b + (s_one - 1) / 2; cx++) {
		int w  = (cx - b) * 2 + 1;
		int b2 = y + (s_one - w) / 2;
		for(cy = b2; cy < b2 + w; cy++) {
			unsigned char* px;

			px    = &raw[(cy * stride + cx) * 4];
			px[0] = text->common.red;
			px[1] = text->common.green;
			px[2] = text->common.blue;
			px[3] = 255;
		}
	}

	b = x + l_one;
	for(cx = b; cx < b + (s_one - 1) / 2; cx++) {
		int w  = ((s_one - 1) / 2 - 1 - (cx - b)) * 2 + 1;
		int b2 = y + (s_one - w) / 2;
		for(cy = b2; cy < b2 + w; cy++) {
			unsigned char* px;

			px    = &raw[(cy * stride + cx) * 4];
			px[0] = text->common.red;
			px[1] = text->common.green;
			px[2] = text->common.blue;
			px[3] = 255;
		}
	}
}

static void draw_seven_segment(MwWidget handle) {
	MwRect	       r;
	MwLLColor      base   = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor      text   = MwParseColor(handle, MwGetText(handle, MwNforeground));
	MwLLColor      shadow = MwLightenColor(handle, base, MwDefaultShadow, MwDefaultShadow, MwDefaultShadow);
	int	       align;
	const char*    str   = MwGetText(handle, MwNtext);
	MwLLPixmap     bgpx  = MwGetVoid(handle, MwNbackgroundPixmap);
	MwLabel	       lab   = handle->internal;
	int	       l_one = MwGetInteger(handle, MwNlength) - (MwGetInteger(handle, MwNlength) % 2);
	int	       s_one = (l_one * 3 / 4) - ((l_one * 3 / 4) % 2) + 1;
	MwLLPixmap     px;
	unsigned char* raw;
	int	       w = 0, h = s_one * 3 + l_one * 2, i;
	int	       x = 0;

	if(str == NULL) str = "";

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	MwDrawRect(handle, &r, base);
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &r, bgpx);

	align = MwGetInteger(handle, MwNalignment);

	/* so - this mode cannot do unicode.
	 * but you wouldn't show unicode on 7 segment anyways
	 */

	/*     L
	 * S <--->
	 *
	 *   S
	 *   ^
	 *   |
	 * L |
	 *   |
	 *   v
	 */

	for(i = 0; (hmgeti(lab->segment, i) != -1) || str[i] != 0; i++) {
		if(i > 0 && ((hmgeti(lab->segment, i) != -1) || str[i] != '.')) w += Spacing;
		if(hmgeti(lab->segment, i) != -1 || ('0' <= str[i] && str[i] <= '9') || ('A' <= str[i] && str[i] <= 'F') || ('a' <= str[i] && str[i] <= 'f')) {
			w += l_one + s_one * 2;
		} else if(str[i] == ':' || str[i] == ' ') {
			w += s_one;
		}
	}

	w++;
	h++;

	raw = malloc(w * h * 4);
	memset(raw, 0, w * h * 4);

	for(i = 0; (hmgeti(lab->segment, i) != -1) || str[i] != 0; i++) {
		if((hmgeti(lab->segment, i) != -1) || ('0' <= str[i] && str[i] <= '9') || ('A' <= str[i] && str[i] <= 'F') || ('a' <= str[i] && str[i] <= 'f')) {
			int la = 0, lb = 0, lc = 0, ld = 0, le = 0, lf = 0, lg = 0;
			int j;

			/* https://en.wikipedia.org/wiki/File:7_Segment_Display_with_Labeled_Segments.svg */

			if(hmgeti(lab->segment, i) != -1) {
				unsigned char c = hmget(lab->segment, i);

				if(c & (1 << 0)) la = 1;
				if(c & (1 << 1)) lb = 1;
				if(c & (1 << 2)) lc = 1;
				if(c & (1 << 3)) ld = 1;
				if(c & (1 << 4)) le = 1;
				if(c & (1 << 5)) lf = 1;
				if(c & (1 << 6)) lg = 1;
			} else {
				if(str[i] == '0') {
					la = lb = lc = ld = le = lf = 1;
				} else if(str[i] == '1') {
					lb = lc = 1;
				} else if(str[i] == '2') {
					la = lb = ld = le = lg = 1;
				} else if(str[i] == '3') {
					la = lb = lc = ld = lg = 1;
				} else if(str[i] == '4') {
					lb = lc = lf = lg = 1;
				} else if(str[i] == '5') {
					la = lc = ld = lf = lg = 1;
				} else if(str[i] == '6') {
					la = lc = ld = le = lf = lg = 1;
				} else if(str[i] == '7') {
					la = lb = lc = 1;
				} else if(str[i] == '8') {
					la = lb = lc = ld = le = lf = lg = 1;
				} else if(str[i] == '9') {
					la = lb = lc = ld = lf = lg = 1;
				} else if(str[i] == 'A' || str[i] == 'a') {
					la = lb = lc = le = lf = lg = 1;
				} else if(str[i] == 'B' || str[i] == 'b') {
					lc = ld = le = lf = lg = 1;
				} else if(str[i] == 'C' || str[i] == 'c') {
					ld = le = lg = 1;
				} else if(str[i] == 'D' || str[i] == 'd') {
					lb = lc = ld = le = lg = 1;
				} else if(str[i] == 'E' || str[i] == 'e') {
					la = ld = le = lf = lg = 1;
				} else if(str[i] == 'F' || str[i] == 'f') {
					la = le = lf = lg = 1;
				}
			}

			for(j = 1; j >= 0; j--) {
				MwLLColor cl = j == 1 ? shadow : text;

				if(la) draw_h(handle, raw, x + s_one + j, j, w, cl);
				if(lb) draw_v(handle, raw, x + s_one + l_one + j, s_one + j, w, cl);
				if(lc) draw_v(handle, raw, x + s_one + l_one + j, s_one * 2 + l_one + j, w, cl);
				if(ld) draw_h(handle, raw, x + s_one + j, s_one * 2 + l_one * 2 + j, w, cl);
				if(le) draw_v(handle, raw, x + j, s_one * 2 + l_one + j, w, cl);
				if(lf) draw_v(handle, raw, x + j, s_one + j, w, cl);
				if(lg) draw_h(handle, raw, x + s_one + j, s_one + l_one + j, w, cl);
			}

			x += l_one + s_one * 2;
		} else if(str[i] == ':') {
			int cy, cx;
			int j;
			for(j = 1; j >= 0; j--) {
				MwLLColor cl = j == 1 ? shadow : text;
				for(cy = 1; cy < h - 1; cy++) {
					int h = s_one / 2;
					int c = (l_one - h) / 2 + s_one;

					int c1 = (c <= cy && cy <= (c + h)) ? 1 : 0;
					int c2 = ((s_one + l_one + c) <= cy && cy <= (s_one + l_one + c + h)) ? 1 : 0;

					if(c1 || c2) {
						for(cx = x; cx < x + s_one; cx++) {
							unsigned char* px = &raw[((cy + j) * w + (cx + j)) * 4];
							px[0]		  = cl->common.red;
							px[1]		  = cl->common.green;
							px[2]		  = cl->common.blue;
							px[3]		  = 255;
						}
					}
				}
			}
			x += s_one;
		} else if(str[i] == ' ') {
			x += s_one;
		} else if(str[i] == '.') {
			int cy, cx;
			int j;
			for(j = 1; j >= 0; j--) {
				MwLLColor cl = j == 1 ? shadow : text;
				for(cy = h - (s_one - 1) / 2 - 1; cy < h; cy++) {
					for(cx = x - Spacing - (s_one - 1) / 2 - 1; cx < (x - Spacing); cx++) {
						unsigned char* px = &raw[((cy + j) * w + (cx + j)) * 4];
						px[0]		  = cl->common.red;
						px[1]		  = cl->common.green;
						px[2]		  = cl->common.blue;
						px[3]		  = 255;
					}
				}
			}
			continue;
		}

		x += Spacing;
	}

	px = MwLoadRaw(handle, raw, w, h);

	r.y	 = (r.height - h) / 2;
	r.height = h;
	if(align == MwALIGNMENT_CENTER) {
		r.x = (r.width - w) / 2;
	} else if(align == MwALIGNMENT_BEGINNING) {
		r.x = 0;
	} else if(align == MwALIGNMENT_END) {
		r.x = r.width - w;
	}
	r.width = w;
	MwLLDrawPixmap(handle->lowlevel, &r, px);

	free(raw);

	MwLLDestroyPixmap(px);
}

static void draw_normal(MwWidget handle) {
	MwRect	    r;
	MwPoint	    p;
	MwLLColor   base   = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor   text   = MwParseColor(handle, MwGetText(handle, MwNforeground));
	MwLLColor   shadow = MwLightenColor(handle, base, MwDefaultShadow, MwDefaultShadow, MwDefaultShadow);
	int	    align;
	const char* str	 = MwGetText(handle, MwNtext);
	MwLLPixmap  bgpx = MwGetVoid(handle, MwNbackgroundPixmap);

	if(str == NULL) str = "";

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);

	MwDrawRect(handle, &r, base);
	if(bgpx != NULL) MwLLDrawPixmap(handle->lowlevel, &r, bgpx);

	align = MwGetInteger(handle, MwNalignment);

	r.width -= MwGetInteger(handle, MwNleftPadding);

#if 1
	if(align == MwALIGNMENT_CENTER) {
		p.x = r.width / 2;
	} else if(align == MwALIGNMENT_BEGINNING) {
		p.x = MwTextWidth(handle, NULL, str) / 2;
	} else if(align == MwALIGNMENT_END) {
		p.x = r.width - MwTextWidth(handle, NULL, str) / 2;
	}
#else
	p.x = 0;
#endif
	p.y = r.height / 2;

	p.x += MwGetInteger(handle, MwNleftPadding);

	p.x += 1;
	p.y += 1;
	MwDrawText(handle, NULL, &p, str, MwALIGNMENT_CENTER, shadow);

	p.x -= 1;
	p.y -= 1;
	MwDrawText(handle, NULL, &p, str, MwALIGNMENT_CENTER, text);

	MwLLFreeColor(shadow);
	MwLLFreeColor(text);
	MwLLFreeColor(base);
}

static void draw(MwWidget handle) {
	MwLabel lab = handle->internal;
	if(!lab->draw) {
		lab->draw = draw_normal;
	}
	lab->draw(handle);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNtext) == 0 || strcmp(key, MwNalignment) == 0 || strcmp(key, MwNsevenSegment) == 0 || strcmp(key, MwNlength) == 0 || strcmp(key, MwNleftPadding) == 0) MwForceRender(handle);

	if(strcmp(key, MwNsevenSegment) == 0) {
		MwLabel lab = handle->internal;
		if(MwGetInteger(handle, MwNsevenSegment))
			lab->draw = draw_seven_segment;
		else
			lab->draw = draw_normal;
	}
}

static void mwLabelSetSevenSegmentImpl(MwWidget handle, int index, unsigned char data) {
	MwLabel lab = handle->internal;

	hmput(lab->segment, index, data);
}

static void func_handler(MwWidget handle, const char* name, void* out, va_list va) {
	(void)out;

	if(strcmp(name, "mwLabelSetSevenSegment") == 0) {
		int index = va_arg(va, int);
		int data  = va_arg(va, int);

		mwLabelSetSevenSegmentImpl(handle, index, data);
	}
}

MwClassRec MwLabelClassRec = {
    wcreate,	  /* create */
    destroy,	  /* destroy */
    draw,	  /* draw */
    NULL,	  /* click */
    NULL,	  /* parent_resize */
    prop_change,  /* prop_change */
    NULL,	  /* mouse_move */
    NULL,	  /* mouse_up */
    NULL,	  /* mouse_down */
    NULL,	  /* key */
    func_handler, /* execute */
    NULL,	  /* tick */
    NULL,	  /* resize */
    NULL,	  /* children_update */
    NULL,	  /* children_prop_change */
    NULL,	  /* clipboard */
    NULL,	  /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwLabelClass = &MwLabelClassRec;
