#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include <stdio.h>

int main(int argc, char** argv) {
	unsigned char* rgba;
	int	       width, height, bpp;
	int	       i;

	if(argc != 3) {
		printf("Usage: %s input name\n", argv[0]);
		return 1;
	}

	rgba = stbi_load(argv[1], &width, &height, &bpp, 4);

	printf("MwU32 %s[] = {\n", argv[2]);
	printf("	(%d << 16) | %d,\n", width, height);
	for(i = 0; i < width * height; i++) {
		unsigned char* px = &rgba[i * 4];
		unsigned int   p  = 0;
		int	       j;

		for(j = 0; j < 4; j++) {
			p = p << 8;
			p = p | px[j];
		}

		printf("	0x%x,\n", p);
	}
	printf("	0\n");
	printf("};\n");

	free(rgba);
}
