#include <Mw/Milsko.h>

MwWidget window, menu, button, button2, button3, button4, button5, button6, button7;

void MWAPI handler(MwWidget handle, void* user_data, void* call_data) {
	(void)handle;
	(void)user_data;
	(void)call_data;

	printf("hello world!\n");
}

void MWAPI handler_theme(MwWidget handle, void* user_data, void* call_data) {
	(void)handle;
	(void)user_data;
	(void)call_data;

	MwVaApply(window,
		  MwNdarkTheme, MwGetInteger(window, MwNdarkTheme) == 0 ? 1 : 0,
		  NULL);
}

void MWAPI handler_look(MwWidget handle, void* user_data, void* call_data) {
	(void)handle;
	(void)user_data;
	(void)call_data;

	MwVaApply(window,
		  MwNmodernLook, MwGetInteger(window, MwNmodernLook) == 0 ? 1 : 0,
		  NULL);
}

void MWAPI handler_font(MwWidget handle, void* user_data, void* call_data) {
	(void)handle;
	(void)user_data;
	(void)call_data;

	MwVaApply(window,
		  MwNbitmapFont, MwGetInteger(window, MwNbitmapFont) == 0 ? 1 : 0,
		  NULL);
}

void MWAPI resize(MwWidget handle, void* user_data, void* call_data) {
	unsigned int w, h, mh;

	(void)user_data;
	(void)call_data;

	w = MwGetInteger(handle, MwNwidth);
	h = MwGetInteger(handle, MwNheight) - (mh = MwGetInteger(menu, MwNheight));

	MwVaApply(button,
		  MwNy, 50 + mh,
		  MwNwidth, (w - 50 * 2) / 4,
		  MwNheight, h - 125 - 50 * 3,
		  NULL);

	MwVaApply(button2,
		  MwNx, 50 + (w - 50 * 2) / 4,
		  MwNy, 50 + mh,
		  MwNwidth, (w - 50 * 2) / 4,
		  MwNheight, h - 125 - 50 * 3,
		  NULL);

	MwVaApply(button3,
		  MwNx, 50 + (w - 50 * 2) / 4 * 2,
		  MwNy, 50 + mh,
		  MwNwidth, (w - 50 * 2) / 4,
		  MwNheight, h - 125 - 50 * 3,
		  NULL);
	MwVaApply(button7,
		  MwNx, 50 + (w - 50 * 2) / 4 * 3,
		  MwNy, 50 + mh,
		  MwNwidth, (w - 50 * 2) / 4,
		  MwNheight, h - 125 - 50 * 3,
		  NULL);
	MwVaApply(button4,
		  MwNx, 50 + (w - 50 * 2) / 3 * 0,
		  MwNy, h - 50 - 125 + mh,
		  MwNwidth, (w - 50 * 2) / 3,
		  NULL);

	MwVaApply(button5,
		  MwNx, 50 + (w - 50 * 2) / 3 * 1,
		  MwNy, h - 50 - 125 + mh,
		  MwNwidth, (w - 50 * 2) / 3,
		  NULL);

	MwVaApply(button6,
		  MwNx, 50 + (w - 50 * 2) / 3 * 2,
		  MwNy, h - 50 - 125 + mh,
		  MwNwidth, (w - 50 * 2) / 3,
		  NULL);
}

int main() {
	MwMenu m, m2;

	MwLibraryInit();

	window	= MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 400, 400,
				   MwNtitle, "hello world",
				   NULL);
	menu	= MwCreateWidget(MwMenuClass, "menu", window, 0, 0, 0, 0);
	button	= MwVaCreateWidget(MwButtonClass, "button", window, 50, 50, 100, 125,
				   MwNtext, "theme",
				   NULL);
	button2 = MwVaCreateWidget(MwButtonClass, "button", window, 150, 50, 100, 125,
				   MwNtext, "look",
				   NULL);
	button3 = MwVaCreateWidget(MwButtonClass, "button", window, 250, 50, 100, 125,
				   MwNtext, "font",
				   NULL);
	button4 = MwVaCreateWidget(MwButtonClass, "button", window, 50, 225, 100, 125,
				   MwNtext, "lorem ipsum",
				   MwNbackground, "#f66",
				   MwNforeground, "#000",
				   NULL);
	button5 = MwVaCreateWidget(MwButtonClass, "button", window, 150, 225, 100, 125,
				   MwNtext, "lorem ipsum",
				   MwNbackground, "#6f6",
				   MwNforeground, "#000",
				   NULL);
	button6 = MwVaCreateWidget(MwButtonClass, "button", window, 250, 225, 100, 125,
				   MwNtext, "lorem ipsum",
				   MwNbackground, "#66f",
				   MwNforeground, "#000",
				   NULL);
	button7 = MwVaCreateWidget(MwButtonClass, "button", window, 250, 50, 100, 125,
				   MwNtext, "disabled",
				   MwNdisabled, 1,
				   NULL);

	MwAddUserHandler(window, MwNresizeHandler, resize, NULL);
	MwAddUserHandler(button, MwNactivateHandler, handler_theme, NULL);
	MwAddUserHandler(button2, MwNactivateHandler, handler_look, NULL);
	MwAddUserHandler(button3, MwNactivateHandler, handler_font, NULL);
	MwAddUserHandler(button4, MwNactivateHandler, handler, NULL);
	MwAddUserHandler(button5, MwNactivateHandler, handler, NULL);
	MwAddUserHandler(button6, MwNactivateHandler, handler, NULL);

	resize(window, NULL, NULL);

	m  = MwMenuAdd(menu, NULL, "test 1");
	m2 = MwMenuAdd(menu, m, "test 2");
	MwMenuAdd(menu, m2, "test 3");
	MwMenuAdd(menu, m2, "test 4");
	MwMenuAdd(menu, m2, "test 5");
	m2 = MwMenuAdd(menu, m, "test 6");
	MwMenuAdd(menu, m2, "test 7");
	MwMenuAdd(menu, m2, "test 8");
	m2 = MwMenuAdd(menu, m, "test 9");
	MwMenuAdd(menu, m2, "test 10");
	m2 = MwMenuAdd(menu, m2, "test 11");
	MwMenuAdd(menu, m2, "test 12");
	MwMenuAdd(menu, m2, "test 13");
	MwMenuAdd(menu, NULL, "?test 14");

	MwLoop(window);
}
