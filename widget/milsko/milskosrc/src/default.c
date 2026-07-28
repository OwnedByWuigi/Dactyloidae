#include <Mw/Milsko.h>

const char* MwDefaultBackground	     = "#d2d2d2";
const char* MwDefaultForeground	     = "#000";
const char* MwDefaultSubBackground   = "#fff";
const char* MwDefaultSubForeground   = "#000";
const char* MwDefaultTitleBackground = "#008";
const char* MwDefaultTitleForeground = "#fff";

const char* MwDefaultDarkBackground	 = "#333";
const char* MwDefaultDarkForeground	 = "#ddd";
const char* MwDefaultDarkSubBackground	 = "#333";
const char* MwDefaultDarkSubForeground	 = "#ddd";
const char* MwDefaultDarkTitleBackground = "#008";
const char* MwDefaultDarkTitleForeground = "#fff";

const int MwDefaultShadow = -32;

int MwDefaultBorderWidth(MwWidget handle) {
	int bw = MwGetInteger(handle, MwNborderWidth);

	if(bw != MwDEFAULT) return bw;

	if(MwGetInteger(handle, MwNmodernLook)) {
		return 2;
	} else {
		return 2;
	}
}

int MwDefaultThinBorderWidth(MwWidget handle) {
	int bw = MwGetInteger(handle, MwNborderWidth);

	if(bw != MwDEFAULT) return bw;

	if(MwGetInteger(handle, MwNmodernLook)) {
		return 1;
	} else {
		return 2;
	}
}
