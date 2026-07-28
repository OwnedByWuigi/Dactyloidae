#include <Mw/Milsko.h>

MwWidget window, instructions, text1, text2, cur_text;

static void MWAPI resize(MwWidget handle, void* user_data, void* call_data) {
	unsigned int w, h;

	(void)user_data;
	(void)call_data;

	w = MwGetInteger(handle, MwNwidth);
	h = MwGetInteger(handle, MwNheight);

	MwVaApply(instructions,
		  MwNy, 50,
		  MwNwidth, w - 50 * 2,
		  MwNheight, h - 125 - 50 * 3,
		  NULL);

	MwVaApply(text1,
		  MwNy, 150,
		  MwNwidth, w - 25 * 2,
		  MwNheight, h - 125 - 50 * 3,
		  NULL);
	MwVaApply(text2,
		  MwNy, 250,
		  MwNwidth, w - 25 * 2,
		  MwNheight, h - 125 - 50 * 3,
		  NULL);
}
static void MWAPI clipboard(MwWidget handle, void* user_data, void* call_data) {
	char* clipboard = call_data;

	(void)handle;
	(void)user_data;

	if(clipboard != NULL) {
		MwVaApply(cur_text, MwNtext, clipboard, NULL);
		MwForceRender(cur_text);
	}
}

static void MWAPI tick(MwWidget handle, void* user_data, void* call_data) {
	cur_text = text1;
	MwGetClipboard(handle, MwCLIPBOARD_MAIN);
	cur_text = text2;
	MwGetClipboard(handle, MwCLIPBOARD_PRIMARY);
	MwForceRender(window);
}

int main() {
	MwLibraryInit();

	window	     = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 400, 400,
					MwNtitle, "clipboard",
					NULL);
	instructions = MwVaCreateWidget(MwLabelClass, "button", window, 50, 50, 300, 125,
					MwNtext, "clipboard contents will show up below.",
					NULL);
	text1	     = MwVaCreateWidget(MwLabelClass, "label", window, 25, 150, 300, 75,
					MwNtext, "",
					NULL);
	text2	     = MwVaCreateWidget(MwLabelClass, "label2", window, 25, 250, 300, 75,
					MwNtext, "",
					NULL);

	cur_text = text1;

	MwAddUserHandler(window, MwNresizeHandler, resize, NULL);
	MwAddUserHandler(window, MwNclipboardHandler, clipboard, NULL);
	MwAddUserHandler(instructions, MwNclipboardHandler, clipboard, NULL);
	MwAddUserHandler(cur_text, MwNclipboardHandler, clipboard, NULL);
	MwAddUserHandler(window, MwNtickHandler, tick, NULL);

	resize(window, NULL, NULL);

	MwLoop(window);
}
