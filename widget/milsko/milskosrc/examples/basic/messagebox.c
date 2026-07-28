#include <Mw/Milsko.h>

void MWAPI ok(MwWidget handle, void* user, void* call) {
	(void)handle;
	(void)call;
	MwDestroyWidget(user);
}

void MWAPI spawn(MwWidget handle, void* user, void* call) {
	MwWidget mb = MwMessageBox(user, "news has arrived!", "title", MwMB_ICONNEWS | MwMB_BUTTONOK);

	(void)handle;
	(void)call;

	MwAddUserHandler(MwMessageBoxGetChild(mb, MwMB_BUTTONOK), MwNactivateHandler, ok, mb);
	MwAddUserHandler(mb, MwNcloseHandler, ok, mb);
}

void MWAPI spawn2(MwWidget handle, void* user, void* call) {
	MwWidget mb = MwMessageBox(user, "something went wrong!", "title", MwMB_ICONERROR | MwMB_BUTTONOK);

	(void)handle;
	(void)call;

	MwAddUserHandler(MwMessageBoxGetChild(mb, MwMB_BUTTONOK), MwNactivateHandler, ok, mb);
	MwAddUserHandler(mb, MwNcloseHandler, ok, mb);
}

void MWAPI spawn3(MwWidget handle, void* user, void* call) {
	int i;

	(void)handle;
	(void)call;

	for(i = 0; i <= MwMB_ICONCLOCK; i++) {
		MwWidget mb = MwMessageBox(user, "messagebox test", "title", i | MwMB_BUTTONOK);
		MwAddUserHandler(MwMessageBoxGetChild(mb, MwMB_BUTTONOK), MwNactivateHandler, ok, mb);
		MwAddUserHandler(mb, MwNcloseHandler, ok, mb);
	}
}

int main() {
	MwWidget msg, btn, btn2, btn3;

	MwLibraryInit();

	msg  = MwVaCreateWidget(MwWindowClass, "test", NULL, MwDEFAULT, MwDEFAULT, 300, 100, MwNtitle, "test", NULL);
	btn  = MwVaCreateWidget(MwButtonClass, "button", msg, 8, 8, 300 - 16, (100 - 16) / 2, MwNtext, "press me!", NULL);
	btn2 = MwVaCreateWidget(MwButtonClass, "button", msg, 8, 8 + (100 - 16) / 2, (300 - 16) / 2, (100 - 16) / 2, MwNtext, "press me!", NULL);
	btn3 = MwVaCreateWidget(MwButtonClass, "button", msg, 8 + (300 - 16) / 2, 8 + (100 - 16) / 2, (300 - 16) / 2, (100 - 16) / 2, MwNtext, "press me!", NULL);

	MwAddUserHandler(btn, MwNactivateHandler, spawn, msg);
	MwAddUserHandler(btn2, MwNactivateHandler, spawn2, msg);
	MwAddUserHandler(btn3, MwNactivateHandler, spawn3, msg);

	MwLoop(msg);
}
