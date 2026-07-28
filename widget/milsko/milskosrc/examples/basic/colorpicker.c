#include <Mw/Milsko.h>

MwWidget fpicker;
MwWidget window;
MwWidget button;

void MWAPI file_callback(MwWidget handle, void* user_data, void* call_data) {
	char   hexColor[8];
	MwRGB* rgb = call_data;

	(void)handle;
	(void)user_data;

	rgb->red &= 0xff;
	rgb->green &= 0xff;
	rgb->blue &= 0xff;
	MwStringPrintIntoBuffer(hexColor, 8, "#%02X%02X%02X", rgb->red, rgb->green, rgb->blue);
	MwSetText(window, MwNbackground, hexColor);
}

void MWAPI file_picker(MwWidget handle, void* user_data, void* call_data) {
	MwWidget cpicker = MwColorPicker(window, "cpicker");

	(void)handle;
	(void)user_data;
	(void)call_data;

	MwAddUserHandler(cpicker, MwNcolorChosenHandler, file_callback, NULL);

	MwSetText(cpicker, MwNbackground, MwGetInteger(cpicker, MwNdarkTheme) ? MwDefaultDarkBackground : MwDefaultBackground);
}

int main() {
	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT,
				  MwDEFAULT, 640, 480, MwNtitle, "color picker", NULL);
	MwSetText(window, MwNbackground, MwGetInteger(window, MwNdarkTheme) ? MwDefaultBackground : MwDefaultDarkBackground);

	button = MwVaCreateWidget(MwButtonClass, "button", window, 160, 180, 320, 120,
				  MwNtext, "change window background",
				  NULL);
	MwSetText(button, MwNbackground, MwGetInteger(window, MwNdarkTheme) ? MwDefaultBackground : MwDefaultDarkBackground);

	MwAddUserHandler(button, MwNactivateHandler, file_picker, NULL);

	MwLoop(window);
}
