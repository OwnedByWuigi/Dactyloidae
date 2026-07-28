#ifdef USE_STB_TRUETYPE
#include <Mw/Milsko.h>

#include "../../external/stb_truetype.h"

struct _MwFLFont {
	stbtt_fontinfo font;
	void*	       data;
	float	       scale;
	int	       ascent;
	int	       descent;
};

static int stbtt_MwDrawText(MwWidget handle, MwFLFont ttf, MwPoint* point, const char* text, MwLLColor color) {
	unsigned char* px;
	int	       tw, th;
	MwRect	       r;
	MwLLPixmap     p;
	int	       ax, lsb;
	int	       x = 0, y = 0;

	tw = MwTextWidth(handle, ttf, text);
	th = MwTextHeight(handle, ttf, text);
	px = malloc(tw * th * 4);

	memset(px, 0, tw * th * 4);
	while(text[0] != 0) {
		int	       c, c2;
		int	       x0, y0, x1, y1, cx, cy;
		int	       ow, oh;
		unsigned char* out;
		int	       kern;
		const char*    old_text;

		text += MwUTF8ToUTF32(text, &c);

		stbtt_GetCodepointHMetrics(&ttf->font, c, &ax, &lsb);

		stbtt_GetCodepointBitmapBox(&ttf->font, c, ttf->scale, ttf->scale, &x0, &y0, &x1, &y1);
		ow  = x1 - x0;
		oh  = y1 - y0;
		out = malloc(ow * oh);
		stbtt_MakeCodepointBitmap(&ttf->font, out, ow, oh, ow, ttf->scale, ttf->scale, c);

		for(cy = 0; cy < oh; cy++) {
			for(cx = 0; cx < ow; cx++) {
				int	       ox  = x + ceil(lsb * ttf->scale) + cx;
				int	       oy  = y + ceil(ttf->ascent * ttf->scale) + y0 + cy;
				unsigned char* opx = &px[(oy * tw + ox) * 4];
				opx[0]		   = color->common.red;
				opx[1]		   = color->common.green;
				opx[2]		   = color->common.blue;
				/* overflow check */
				if(opx[3] + out[cy * ow + cx] < opx[3]) {
					opx[3] = 255;
				} else {
					opx[3] = out[cy * ow + cx];
				}
			}
		}

		x += ceil(ax * ttf->scale);

		old_text = text;
		text += MwUTF8ToUTF32(text, &c2);

		kern = stbtt_GetCodepointKernAdvance(&ttf->font, c, c2);

		x += ceil(kern * ttf->scale);

		text = old_text;

		free(out);
	}

	p	 = MwLoadRaw(handle, px, tw, th);
	r.x	 = point->x;
	r.y	 = point->y - th / 2;
	r.width	 = tw;
	r.height = th;

	MwLLDrawPixmap(handle->lowlevel, &r, p);
	MwLLDestroyPixmap(p);
	free(px);

	return 0;
}

static int stbtt_MwTextWidth(MwFLFont ttf, const char* text) {
	int ax, lsb;
	int tw = 0;
	int c;
	int x0, y0, x1, y1;
	int kern;

	while(text[0] != 0) {
		int	    c2;
		const char* old_text;

		text += MwUTF8ToUTF32(text, &c);

		stbtt_GetCodepointHMetrics(&ttf->font, c, &ax, &lsb);

		tw += ceil(ax * ttf->scale);

		old_text = text;
		text += MwUTF8ToUTF32(text, &c2);

		kern = stbtt_GetCodepointKernAdvance(&ttf->font, c, c2);

		tw += ceil(kern * ttf->scale);

		text = old_text;
	}

	if(tw > 0) {
		tw -= ceil(kern * ttf->scale);
		tw -= ceil(ax * ttf->scale);
		stbtt_GetCodepointBitmapBox(&ttf->font, c, ttf->scale, ttf->scale, &x0, &y0, &x1, &y1);
		tw += ceil(lsb * ttf->scale);
		tw += x1 - x0;
	}

	return tw + 1;
}

static int stbtt_MwTextHeight(MwFLFont ttf, int count) {
	return ceil((ttf->ascent - ttf->descent) * ttf->scale * count);
}

static void* stbtt_MwFontLoad(unsigned char* data, unsigned int size, int px) {
	MwFLFont ttf = malloc(sizeof(*ttf));
	ttf->data    = malloc(size);
	memcpy(ttf->data, data, size);
	stbtt_InitFont(&ttf->font, ttf->data, 0);

	ttf->scale = stbtt_ScaleForMappingEmToPixels(&ttf->font, px);
	stbtt_GetFontVMetrics(&ttf->font, &ttf->ascent, &ttf->descent, 0);

	return ttf;
}

static void stbtt_MwFontFree(void* handle) {
	MwFLFont ttf = handle;

	free(ttf->data);
	free(ttf);
}

int MwFL_STBTTSetup(void) {
	MwFLDrawText   = stbtt_MwDrawText;
	MwFLTextWidth  = stbtt_MwTextWidth;
	MwFLTextHeight = stbtt_MwTextHeight;
	MwFLFontLoad   = stbtt_MwFontLoad;
	MwFLFontFree   = stbtt_MwFontFree;
	return 0;
}

#endif
