#include <Mw/Milsko.h>

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

char* MwACPToUTF8(const char* input) {
#if defined(_WIN32) && defined(MW_HAS_WCHAR)
	int mbbytes = (strlen(input) + 1) * 4;
	int wbytes  = 0;
	int len;

	wchar_t* wout;
	char*	 mbout = malloc(mbbytes);

	wbytes = MultiByteToWideChar(CP_ACP, 0, input, strlen(input), NULL, 0) * sizeof(wchar_t);
	if(wbytes == 0) {
		free(mbout);
		return MwStringDuplicate(input);
	}

	wout = malloc(wbytes + sizeof(wchar_t));
	len  = wbytes / sizeof(wchar_t);

	memset(wout, 0, wbytes);
	memset(mbout, 0, mbbytes);

	len = MultiByteToWideChar(CP_ACP, 0, input, strlen(input), wout, len);
	if(len == 0) {
		free(mbout);
		free(wout);
		return MwStringDuplicate(input);
	}
	wout[len] = 0;

	len = WideCharToMultiByte(CP_UTF8, 0, wout, len, mbout, mbbytes, 0, 0);
	if(len == 0) {
		free(mbout);
		free(wout);
		return MwStringDuplicate(input);
	}
	mbout[len] = 0;

	free(wout);

	return mbout;
#else
	return MwStringDuplicate(input);
#endif
}

char* MwUTF8ToACP(const char* input) {
#if defined(_WIN32) && defined(MW_HAS_WCHAR)
	int mbbytes = (strlen(input) + 1) * 4;
	int wbytes  = 0;
	int len;

	wchar_t* wout;
	char*	 mbout = malloc(mbbytes);

	wbytes = MultiByteToWideChar(CP_UTF8, 0, input, strlen(input), NULL, 0) * sizeof(wchar_t);
	if(wbytes == 0) {
		free(mbout);
		return MwStringDuplicate(input);
	}

	wout = malloc(wbytes + sizeof(wchar_t));
	len  = wbytes / sizeof(wchar_t);

	memset(wout, 0, wbytes);
	memset(mbout, 0, mbbytes);

	len = MultiByteToWideChar(CP_UTF8, 0, input, strlen(input), wout, len);
	if(len == 0) {
		free(mbout);
		free(wout);
		return MwStringDuplicate(input);
	}
	wout[len] = 0;

	len = WideCharToMultiByte(CP_ACP, 0, wout, len, mbout, mbbytes, 0, 0);
	if(len == 0) {
		free(mbout);
		free(wout);
		return MwStringDuplicate(input);
	}
	mbout[len] = 0;

	free(wout);

	return mbout;
#else
	return MwStringDuplicate(input);
#endif
}
