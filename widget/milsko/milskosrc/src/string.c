#include <Mw/Milsko.h>

char* MwStringDuplicate(const char* str) {
	char* r = malloc(strlen(str) + 1);
	strcpy(r, str);

	return r;
}

char* MwStringConcat(const char* str1, const char* str2) {
	char* r = malloc(strlen(str1) + strlen(str2) + 1);
	strcpy(r, str1);
	strcat(r, str2);

	return r;
}

void MwStringSize(char* out, MwOffset size) {
	if(size / 1024 == 0) {
		sprintf(out, "%d", (int)size);
	} else if(size / 1024 / 1024 == 0) {
		sprintf(out, "%.1fK", (double)size / 1024);
	} else if(size / 1024 / 1024 / 1024 == 0) {
		sprintf(out, "%.1fM", (double)size / 1024 / 1024);
	} else {
		sprintf(out, "%.1fG", (double)size / 1024 / 1024 / 1024);
	}
}

void MwStringTime(char* out, time_t t) {
	struct tm*  tm	     = localtime(&t);
	const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	if(tm == NULL) {
		sprintf(out, "localtime error");
	} else {
		sprintf(out, "%s %2d %02d:%02d %d", months[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, 1900 + tm->tm_year);
	}
}
void MwStringPrintIntoBuffer(char* out, MwU32 size, const char* fmt, ...) {
	va_list va;
	va_start(va, fmt);

#if __STDC_VERSION__ >= 199901L
	vsnprintf(out, size, fmt, va);
#else
	vsprintf(out, fmt, va);
#endif

	va_end(va);
};

MWDECL MwBool MwStringIsKeyUTF8(MwU32 key) {
	unsigned char bytes[4];
	int	      i;
	bytes[0] = (key & 0x000000FF);
	bytes[1] = (key & 0x0000FF00) >> 8;
	bytes[2] = (key & 0x00FF0000) >> 16;
	bytes[3] = (key & 0xFF000000) >> 24;

	for(i = 0; i < sizeof(MwU32); i++) {
		if(( // ASCII
		     // use bytes[0] <= 0x7F to allow ASCII control characters
		       bytes[0] == 0x09 ||
		       bytes[0] == 0x0A ||
		       bytes[0] == 0x0D ||
		       (0x20 <= bytes[0] && bytes[0] <= 0x7E))) {
			continue;
		}

		if(( // non-overlong 2-byte
		       (0xC2 <= bytes[0] && bytes[0] <= 0xDF) &&
		       (0x80 <= bytes[1] && bytes[1] <= 0xBF))) {
			continue;
		}

		if(( // excluding overlongs
		       bytes[0] == 0xE0 &&
		       (0xA0 <= bytes[1] && bytes[1] <= 0xBF) &&
		       (0x80 <= bytes[2] && bytes[2] <= 0xBF)) ||
		   ( // straight 3-byte
		       ((0xE1 <= bytes[0] && bytes[0] <= 0xEC) ||
			bytes[0] == 0xEE ||
			bytes[0] == 0xEF) &&
		       (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
		       (0x80 <= bytes[2] && bytes[2] <= 0xBF)) ||
		   ( // excluding surrogates
		       bytes[0] == 0xED &&
		       (0x80 <= bytes[1] && bytes[1] <= 0x9F) &&
		       (0x80 <= bytes[2] && bytes[2] <= 0xBF))) {
			continue;
		}

		if(( // planes 1-3
		       bytes[0] == 0xF0 &&
		       (0x90 <= bytes[1] && bytes[1] <= 0xBF) &&
		       (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
		       (0x80 <= bytes[3] && bytes[3] <= 0xBF)) ||
		   ( // planes 4-15
		       (0xF1 <= bytes[0] && bytes[0] <= 0xF3) &&
		       (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
		       (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
		       (0x80 <= bytes[3] && bytes[3] <= 0xBF)) ||
		   ( // plane 16
		       bytes[0] == 0xF4 &&
		       (0x80 <= bytes[1] && bytes[1] <= 0x8F) &&
		       (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
		       (0x80 <= bytes[3] && bytes[3] <= 0xBF))) {
			continue;
		}

		return 0;
	}

	return 1;
}
