#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_BDF_H

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
	FT_Library	lib;
	FT_Face		face;
	int		i;
	int		ay = 0;
	int		ax = 0;
	int		sy = 0;
	int		sx = 0;
	BDF_PropertyRec rec;

	FT_Init_FreeType(&lib);
	if(FT_New_Face(lib, argv[1], 0, &face)) {
		fprintf(stderr, "bad 1\n");
		return 1;
	}

	if(FT_Set_Pixel_Sizes(face, 0, atoi(argv[2]))) {
		fprintf(stderr, "bad 2\n");
		return 1;
	}

	FT_Get_BDF_Property(face, "COPYRIGHT", &rec);

	printf("#include <Mw/Milsko.h>\n");
	printf("\n");
	printf("/**\n");
	printf(" * Copyright notice:\n");
	printf(" *   \"%s\"\n", rec.u.atom);
	printf(" */\n");
	printf("MwFont MwFontData[] = {\n");
	for(i = 0; i < 0x100; i++) {
		int	      y, x;
		FT_Bitmap     bitmap;
		unsigned char n;
		if(FT_Load_Char(face, i, FT_LOAD_RENDER | FT_LOAD_COLOR)) {
			continue;
		}

		FT_Bitmap_Init(&bitmap);
		FT_Bitmap_Convert(lib, &face->glyph->bitmap, &bitmap, 1);

		printf("\t{%d, %d, {", face->glyph->bitmap_left, face->glyph->bitmap_top);
		for(y = 0; y < atoi(argv[2]) - bitmap.rows; y++) {
			printf("0%s", y == atoi(argv[2]) - 1 ? "" : ", ");
		}
		for(y = 0; y < bitmap.rows; y++) {
			n = 0;
			for(x = 0; x < bitmap.width; x++) {
				unsigned char b = bitmap.buffer[bitmap.width * y + x] ? 1 : 0;
				n		= n << 1;
				n		= n | b;
			}
			printf("%d%s", n, y == (bitmap.rows - 1) ? "" : ", ");
		}
		printf("}}%s /* %d */\n", i == (0x100 - 1) ? "" : ", ", i);

		FT_Bitmap_Done(lib, &bitmap);

		sx += face->glyph->advance.x >> 6;
	}
	printf("};\n");
}
