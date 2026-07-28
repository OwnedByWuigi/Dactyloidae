#include <Mw/Milsko.h>

/* this code was taken from my old code... :) */

#define CAST_I32(x) ((int)(x))

static int utf8_count(unsigned char c) {
	if(c < 0x80) {
		return 1;
	}
	if(0xc2 <= c && c < 0xe0) {
		return 2;
	}
	if(0xe0 <= c && c < 0xf0) {
		return 3;
	}
	if(0xf0 <= c && c < 0xf8) {
		return 4;
	}
	return 0;
}

static int utf8_later(unsigned char c) {
	return 0x80 <= c && c < 0xc0;
}

int MwUTF8ToUTF32(const char* input, int* output) {
	const unsigned char* inbuf = (const unsigned char*)input;
	int		     b	   = utf8_count(inbuf[0]);
	if(b == 0) return 0;

	if(b == 1) *output = inbuf[0];
	if(b == 2) {
		if(!utf8_later(inbuf[1])) return 0;
		if((inbuf[0] & 0x1e) == 0) return 0;

		*output = CAST_I32(inbuf[0] & 0x1f) << 6;
		*output |= CAST_I32(inbuf[1] & 0x3f);
	}
	if(b == 3) {
		if(!utf8_later(inbuf[1]) || !utf8_later(inbuf[2])) return 0;
		if((inbuf[0] & 0x0f) == 0 && (inbuf[1] & 0x20) == 0) return 0;

		*output = CAST_I32(inbuf[0] & 0x0f) << 12;
		*output |= CAST_I32(inbuf[1] & 0x3f) << 6;
		*output |= CAST_I32(inbuf[2] & 0x3f);
	}
	if(b == 4) {
		if(!utf8_later(inbuf[1]) || !utf8_later(inbuf[2]) || !utf8_later(inbuf[3])) return 0;
		if((inbuf[0] & 0x07) == 0 && (inbuf[1] & 0x30) == 0) return 0;

		*output = CAST_I32(inbuf[0] & 0x07) << 18;
		*output |= CAST_I32(inbuf[1] & 0x3f) << 12;
		*output |= CAST_I32(inbuf[2] & 0x3f) << 6;
		*output |= CAST_I32(inbuf[3] & 0x3f);
	}

	return b;
}

int MwUTF8Length(const char* input) {
	int out;
	int len = 0;

	while(input[0] != 0) {
		int new;

		input += (new = MwUTF8ToUTF32(input, &out));
		len++;

		if(new == 0) return -1;
	}

	return len;
}

int MwUTF8Copy(const char* src, int srcskip, char* dst, int dstskip, int len) {
	int i;
	int out;
	int total = 0;
	for(i = 0; i < srcskip; i++) src += MwUTF8ToUTF32(src, &out);
	for(i = 0; i < dstskip; i++) dst += MwUTF8ToUTF32(dst, &out);
	for(i = 0; i < len; i++) {
		int len;
		if(src[0] == 0) break;

		len = MwUTF8ToUTF32(src, &out);

		memcpy(dst, src, len);

		src += len;
		dst += len;
		total += len;
	}
	dst[0] = 0;

	return total;
}

int MwUTF32ToUTF8(int input, char* output) {
	if(input < 128) {
		output[0] = input;
		return 1;
	} else if(input < 2048) {
		output[0] = 0xc0 | (input >> 6);
		output[1] = 0x80 | (input & 0x3f);
		return 2;
	} else if(input < 65536) {
		output[0] = 0xe0 | (input >> 12);
		output[1] = 0x80 | ((input >> 6) & 0x3f);
		output[2] = 0x80 | (input & 0x3f);
		return 3;
	} else {
		output[0] = 0xf0 | (input >> 18);
		output[1] = 0x80 | ((input >> 12) & 0x3f);
		output[2] = 0x80 | ((input >> 6) & 0x3f);
		output[3] = 0x80 | (input & 0x3f);
		return 4;
	}
}
