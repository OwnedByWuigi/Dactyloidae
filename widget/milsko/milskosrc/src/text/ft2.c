#ifdef USE_FREETYPE2
#include <Mw/Milsko.h>

#include <ft2build.h>
#include FT_FREETYPE_H

static struct ft_table {
	FT_Error (*FT_Init_FreeType)(FT_Library* alibrary);
	FT_Error (*FT_New_Memory_Face)(FT_Library     library,
				       const FT_Byte* file_base,
				       FT_Long	      file_size,
				       FT_Long	      face_index,
				       FT_Face*	      aface);
	FT_Error (*FT_Set_Pixel_Sizes)(FT_Face face,
				       FT_UInt pixel_width,
				       FT_UInt pixel_height);
	FT_Error (*FT_Load_Char)(FT_Face  face,
				 FT_ULong char_code,
				 FT_Int32 load_flags);
	FT_Error (*FT_Get_Kerning)(FT_Face    face,
				   FT_UInt    left_glyph,
				   FT_UInt    right_glyph,
				   FT_UInt    kern_mode,
				   FT_Vector* akerning);
	FT_Error (*FT_Done_Face)(FT_Face face);
	FT_Error (*FT_Done_FreeType)(FT_Library library);

} ft_table;

struct _MwFLFont {
	FT_Library library;
	FT_Face	   face;
	void*	   data;
	int	   px;
};
static int ft2_MwDrawText(MwWidget handle, MwFLFont ttf, MwPoint* point, const char* text, MwLLColor color) {
	int	       tw, th;
	unsigned char* px;
	MwLLPixmap     p;
	MwRect	       r;
	int	       x = 0, y = 0;

	tw = MwTextWidth(handle, ttf, text);
	th = MwTextHeight(handle, ttf, text);
	px = malloc(tw * th * 4);

	memset(px, 0, tw * th * 4);
	while(text[0] != 0) {
		FT_Bitmap*  bmp;
		int	    cy, cx;
		int	    c, c2;
		FT_Vector   vec;
		const char* old_text;

		text += MwUTF8ToUTF32(text, &c);

		ft_table.FT_Load_Char(ttf->face, c, FT_LOAD_RENDER);
		bmp = &ttf->face->glyph->bitmap;

		for(cy = 0; cy < bmp->rows; cy++) {
			for(cx = 0; cx < bmp->width; cx++) {
				int	       ox  = x + cx + ttf->face->glyph->bitmap_left;
				int	       oy  = y + (ttf->face->height * ttf->px / ttf->face->units_per_EM) - ttf->face->glyph->bitmap_top + cy + (ttf->face->descender * ttf->px / ttf->face->units_per_EM);
				unsigned char* opx = &px[(oy * tw + ox) * 4];
				opx[0]		   = color->common.red;
				opx[1]		   = color->common.green;
				opx[2]		   = color->common.blue;
				/* overflow check */
				if(opx[3] + bmp->buffer[cy * bmp->pitch + cx] < opx[3]) {
					opx[3] = 255;
				} else {
					opx[3] = bmp->buffer[cy * bmp->pitch + cx];
				}
			}
		}

		x += (ttf->face->glyph->advance.x >> 6);

		old_text = text;
		text += MwUTF8ToUTF32(text, &c2);

		ft_table.FT_Get_Kerning(ttf->face, c, c2, FT_KERNING_DEFAULT, &vec);

		x += vec.x >> 6;

		text = old_text;
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

static int ft2_MwTextWidth(MwFLFont ttf, const char* text) {
	int	  tw = 0;
	FT_Vector vec;

	while(text[0] != 0) {
		int	    c, c2;
		const char* old_text;

		text += MwUTF8ToUTF32(text, &c);

		ft_table.FT_Load_Char(ttf->face, c, FT_LOAD_DEFAULT);

		tw += ttf->face->glyph->advance.x >> 6;

		old_text = text;
		text += MwUTF8ToUTF32(text, &c2);

		ft_table.FT_Get_Kerning(ttf->face, c, c2, FT_KERNING_DEFAULT, &vec);

		tw += vec.x >> 6;

		text = old_text;
	}

	if(tw > 0) {
		tw -= vec.x >> 6;
		tw -= ttf->face->glyph->advance.x >> 6;
		tw += ttf->face->glyph->bitmap_left + ttf->face->glyph->bitmap.width;
	}

	return tw + 1;
}

static int ft2_MwTextHeight(MwFLFont ttf, int count) {
	return ceil((ttf->face->height * ttf->px / ttf->face->units_per_EM) * count);
}

static void* ft2_MwFontLoad(unsigned char* data, unsigned int size, int px) {
	MwFLFont ttf = malloc(sizeof(*ttf));
	ttf->data    = malloc(size);
	memcpy(ttf->data, data, size);
	ft_table.FT_Init_FreeType(&ttf->library);
	ft_table.FT_New_Memory_Face(ttf->library, ttf->data, size, 0, &ttf->face);

	ttf->px = px;
	ft_table.FT_Set_Pixel_Sizes(ttf->face, 0, ttf->px);

	return ttf;
}

static void ft2_MwFontFree(void* handle) {
	MwFLFont ttf = handle;

	ft_table.FT_Done_Face(ttf->face);
	ft_table.FT_Done_FreeType(ttf->library);

	free(ttf->data);
	free(ttf);
}

int MwFL_FT2Setup(void) {
	/* Try and load FreeType2. If it fails, return 1, signifying we default to stbtt.  */
	void* ftlib;
#ifdef __APPLE__
	if(!(ftlib = MwDynamicOpen("libfreetype.dylib"))) {
#else
	if(!(ftlib = MwDynamicOpen("libfreetype.so"))) {
#endif
		return 1;
	}

	ft_table.FT_Init_FreeType   = MwDynamicSymbol(ftlib, "FT_Init_FreeType");
	ft_table.FT_New_Memory_Face = MwDynamicSymbol(ftlib, "FT_New_Memory_Face");
	ft_table.FT_Set_Pixel_Sizes = MwDynamicSymbol(ftlib, "FT_Set_Pixel_Sizes");
	ft_table.FT_Load_Char	    = MwDynamicSymbol(ftlib, "FT_Load_Char");
	ft_table.FT_Get_Kerning	    = MwDynamicSymbol(ftlib, "FT_Get_Kerning");
	ft_table.FT_Done_Face	    = MwDynamicSymbol(ftlib, "FT_Done_Face");
	ft_table.FT_Done_FreeType   = MwDynamicSymbol(ftlib, "FT_Done_FreeType");

	MwFLDrawText   = ft2_MwDrawText;
	MwFLTextWidth  = ft2_MwTextWidth;
	MwFLTextHeight = ft2_MwTextHeight;
	MwFLFontLoad   = ft2_MwFontLoad;
	MwFLFontFree   = ft2_MwFontFree;
	return 0;
}
#endif
