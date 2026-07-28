#include <Mw/Milsko.h>

int (*MwFLDrawText)(MwWidget handle, MwFLFont font, MwPoint* point, const char* text, MwLLColor color) = NULL;
int (*MwFLTextWidth)(MwFLFont font, const char* text)						       = NULL;
int (*MwFLTextHeight)(MwFLFont font, int count)							       = NULL;
int (*MwFLTextHeightWithText)(MwFLFont ttf, const char* text)					       = NULL;
void* (*MwFLFontLoad)(unsigned char* data, unsigned int size, int px)				       = NULL;
void (*MwFLFontFree)(void* handle)								       = NULL;

#if defined(USE_FREETYPE2) || defined(USE_STB_TRUETYPE) || defined(USE_GDI_TEXT)
#define TTF
#endif

#define FontWidth 7
#define FontHeight 14

static void bitmap_MwDrawText(MwWidget handle, MwPoint* point, const char* text, int bold, MwLLColor color);

static int count_nl(const char* text) {
	int c = 1;
	int i = 0;
	while(text[i] != 0) {
		int out;
		int l = MwUTF8ToUTF32(text + i, &out);
		if(l == 0) break;
		i += l;

		if(out == '\n') c++;
	}

	return c;
}

static int is_bold(MwWidget handle, MwFLFont ttf) {
	size_t u = (size_t)ttf;
	int    s;

	if(u & MwFLFlagBold) return 1;

	s = MwGetInteger(handle, MwNbold);
	if(s == MwDEFAULT) return 0;
	return s;
}

#ifdef TTF
static int is_bitmap(MwWidget handle, MwFLFont ttf) {
	size_t u = (size_t)ttf;
	int    s;

	if(u & MwFLFlagBitmap) return 1;

	s = MwGetInteger(handle, MwNbitmapFont);
	if(s == MwDEFAULT) return 0;
	return s;
}

static int is_monospace(MwWidget handle, MwFLFont ttf) {
	size_t u = (size_t)ttf;
	int    s;

	if(u & MwFLFlagMonospace) return 1;

	s = MwGetInteger(handle, MwNuseMonospace);
	if(s == MwDEFAULT) return 0;
	return s;
}
#endif

#ifdef TTF

static MwFLFont assume_ttf(MwWidget handle, MwFLFont ttf) {
	if(is_bitmap(handle, ttf)) return NULL;

	if(is_monospace(handle, ttf)) return is_bold(handle, ttf) ? MwGetVoid(handle, MwNboldMonospaceFont) : MwGetVoid(handle, MwNmonospaceFont);
	return is_bold(handle, ttf) ? MwGetVoid(handle, MwNboldFont) : MwGetVoid(handle, MwNfont);
}
#endif

void MwDrawText(MwWidget handle, MwFLFont ttf, MwPoint* point, const char* text, int align, MwLLColor color) {
	MwPoint	  p	= *point;
	char*	  input = (char*)text;
	char*	  last	= input;
	int	  cp;
	MwLLColor fadedColor = NULL;

	if(MwGetInteger(handle, MwNdisabled) == 1) {
		MwLLColor c = handle->parent == NULL ? NULL : MwParseColor(handle->parent, MwGetText(handle->parent, MwNbackground));

		if(c != NULL) {
			fadedColor		 = MwLLAllocColor(handle->lowlevel, color->common.red, color->common.blue, color->common.green);
			fadedColor->common.red	 = fadedColor->common.red + (c->common.red - fadedColor->common.red) * 0.5;
			fadedColor->common.green = fadedColor->common.green + (c->common.green - fadedColor->common.green) * 0.5;
			fadedColor->common.blue	 = fadedColor->common.blue + (c->common.blue - fadedColor->common.blue) * 0.5;

			MwLLFreeColor(c);
		}
	}

#ifdef TTF
	if(ttf <= MwFLBuildFont(0xff) && !is_bitmap(handle, ttf)) ttf = assume_ttf(handle, ttf);
#endif

	p.y -= (MwTextHeight(handle, ttf, text) - (MwTextHeight(handle, ttf, text) / count_nl(text))) / 2;

	while(*input != 0) {
		input += MwUTF8ToUTF32(input, &cp);

		if(*input == 0 || cp == '\n') {
			char* line = malloc(input - last + 1);
			int   i;

			memcpy(line, last, input - last);

			line[input - last] = 0;

			for(i = 0; line[i] != 0; i++) {
				if(line[i] == '\r' || line[i] == '\n') {
					line[i] = 0;
					break;
				}
			}

			if(strlen(line) == 0) {
				free(line);

				last = input;

				continue;
			}

			p.x = point->x;
			if(align == MwALIGNMENT_CENTER) {
				p.x -= MwTextWidth(handle, ttf, line) / 2;
			} else if(align == MwALIGNMENT_END) {
				p.x -= MwTextWidth(handle, ttf, line);
			}

#ifdef TTF
			if(MwFLDrawText)
				if(is_bitmap(handle, ttf) || ttf == NULL || MwFLDrawText(handle, ttf, &p, line, fadedColor ? fadedColor : color))
#endif
					bitmap_MwDrawText(handle, &p, line, is_bold(handle, ttf), fadedColor ? fadedColor : color);

			p.y += MwTextHeight(handle, ttf, line);

			free(line);

			last = input;
		}
	}

	if(fadedColor) {
		MwLLFreeColor(fadedColor);
	}
}

int MwTextWidth(MwWidget handle, MwFLFont ttf, const char* text) {
	int   len = 0;
	int   st;
	int   cp;
	char* input = (char*)text;
	char* last  = input;
#ifndef TTF
	(void)handle;
	(void)ttf;
#endif

#ifdef TTF
	if(ttf <= MwFLBuildFont(0xff)) ttf = assume_ttf(handle, ttf);
#endif

	while(*input != 0) {
		input += MwUTF8ToUTF32(input, &cp);

		if(*input == 0 || cp == '\n') {
			char* line = malloc(input - last + 1);
			int   i;

			memcpy(line, last, input - last);

			line[input - last] = 0;

			for(i = 0; line[i] != 0; i++) {
				if(line[i] == '\r' || line[i] == '\n') {
					line[i] = 0;
					break;
				}
			}

#ifdef TTF
			if(MwFLTextWidth && !MwGetInteger(handle, MwNbitmapFont) && ttf != NULL && (st = MwFLTextWidth(ttf, line)) != -1) {
			} else
#endif
				st = MwUTF8Length(line) * FontWidth;

			if(st > len) len = st;

			free(line);

			last = input;
		}
	}

	return len;
}

int MwTextHeight(MwWidget handle, MwFLFont ttf, const char* text) {
	int c = count_nl(text);
#ifdef TTF
	int st;
#endif

#ifdef TTF
	if(ttf <= MwFLBuildFont(0xff)) ttf = assume_ttf(handle, ttf);
#else
	(void)handle;
	(void)text;
	(void)ttf;
#endif

#ifdef TTF
	if(MwFLTextHeight)
		if(!MwGetInteger(handle, MwNbitmapFont) && ttf != NULL && (st = MwFLTextHeight(ttf, c)) != -1) return st;
	if(MwFLTextHeightWithText)
		if(!MwGetInteger(handle, MwNbitmapFont) && ttf != NULL && (st = MwFLTextHeightWithText(ttf, text)) != -1) return st;
#endif
	return FontHeight * c;
}

void* MwFontLoad(unsigned char* data, unsigned int size) {
	if(MwFLFontLoad)
		return MwFLFontLoad(data, size, 14);
	return NULL;
}

void MwFontFree(void* handle) {
	if(MwFLFontFree) MwFLFontFree(handle);
}

static void bitmap_MwDrawText(MwWidget handle, MwPoint* point, const char* text, int bold, MwLLColor color) {
	int	       i = 0, x, y, sx, sy;
	int	       tw, th;
	unsigned char* px;
	MwRect	       r;
	MwLLPixmap     p;

	if(strlen(text) == 0) text = " ";
	tw = MwTextWidth(handle, NULL, text);
	th = MwTextHeight(handle, NULL, text);
	px = malloc(tw * th * 4);

	memset(px, 0, tw * th * 4);

	sx = 0;
	sy = 0;

	while(text[i] != 0) {
		int out;
		i += MwUTF8ToUTF32(text + i, &out);

		if(out > 0xff) {
			out = 0;
		}

		if(out == '\n') {
			sx = 0;
			sy += FontHeight;
		} else {
			for(y = 0; y < FontHeight; y++) {
				for(x = 0; x < FontWidth; x++) {
					unsigned char* ppx = &px[((sy + y) * tw + sx + x) * 4];
					if((bold ? MwBoldFontData : MwFontData)[out].data[y] & (1 << ((FontWidth - 1) - x))) {
						ppx[0] = color->common.red;
						ppx[1] = color->common.green;
						ppx[2] = color->common.blue;
						ppx[3] = 255;
					} else {
						ppx[0] = 0;
						ppx[1] = 0;
						ppx[2] = 0;
						ppx[3] = 0;
					}
				}
			}
			sx += FontWidth;
		}
	}

	p	 = MwLoadRaw(handle, px, tw, th);
	r.x	 = point->x;
	r.y	 = point->y - th / 2;
	r.width	 = tw;
	r.height = th;

	MwLLDrawPixmap(handle->lowlevel, &r, p);
	MwLLDestroyPixmap(p);
	free(px);
}

typedef int (*call_t)(void);
void MwFLSetup(void) {
	call_t calls[] = {
#ifdef USE_GDI_TEXT
	    MwFL_GDISetup,
#endif
#ifdef USE_FREETYPE2
	    MwFL_FT2Setup,
#endif
#ifdef USE_STB_TRUETYPE
	    MwFL_STBTTSetup,
#endif
	    NULL};
	int i;

	for(i = 0; calls[i] != NULL; i++) {
		if(calls[i]() == 0) return;
	}
}
