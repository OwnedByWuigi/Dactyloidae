#include <Mw/Milsko.h>

MwWidget fpicker;
MwWidget window;
MwWidget button;
MwWidget mb;

MwBool fpicker_wait = MwFALSE;
MwBool msgbox_wait  = MwFALSE;

void MWAPI ok(MwWidget handle, void* user, void* call) {
	(void)handle;
	(void)call;
	msgbox_wait = MwFALSE;
}

void MWAPI file_callback(MwWidget handle, void* user_data, void* call_data) {
	mb = MwMessageBox(handle, call_data, "File chosen", MwMB_ICONERROR | MwMB_BUTTONOK);

	(void)user_data;

	MwAddUserHandler(MwMessageBoxGetChild(mb, MwMB_BUTTONOK), MwNactivateHandler, ok, mb);
	MwAddUserHandler(mb, MwNcloseHandler, ok, mb);

	msgbox_wait = MwTRUE;

	while(msgbox_wait) {
		MwStep(mb);
	}

	fpicker_wait = MwFALSE;
}

void MWAPI file_picker(MwWidget handle, void* user_data, void* call_data) {
	fpicker = MwFileChooserEx(handle, "file chooser", 0);

	(void)handle;
	(void)user_data;
	(void)call_data;

	MwAddUserHandler(fpicker, MwNfileChosenHandler, file_callback, NULL);

	MwSetText(fpicker, MwNbackground, MwGetInteger(fpicker, MwNdarkTheme) ? MwDefaultDarkBackground : MwDefaultBackground);

	fpicker_wait = MwTRUE;

	while(fpicker_wait) {
		MwStep(fpicker);
	}

	MwDestroyWidget(fpicker);
	MwDestroyWidget(mb);
}

int main() {
	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT,
				  MwDEFAULT, 640, 480, MwNtitle, "color picker", NULL);
	MwSetText(window, MwNbackground, MwGetInteger(window, MwNdarkTheme) ? MwDefaultBackground : MwDefaultDarkBackground);

	button = MwVaCreateWidget(MwButtonClass, "button", window, 160, 180, 320, 120,
				  MwNtext, "select file",
				  NULL);
	MwSetText(button, MwNbackground, MwGetInteger(window, MwNdarkTheme) ? MwDefaultBackground : MwDefaultDarkBackground);

	MwAddUserHandler(button, MwNactivateHandler, file_picker, NULL);

	MwLoop(window);
}
