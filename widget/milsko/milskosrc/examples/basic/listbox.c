#include <Mw/Milsko.h>

#include "../harvard.c"

MwWidget w_main;

void MWAPI destroy(MwWidget handle, void* user, void* call) {
	(void)handle;
	(void)call;
	MwDestroyWidget(user);
}

void MWAPI activate(MwWidget handle, void* user, void* call) {
	char	 msg[256];
	MwWidget msgbox;

	(void)user;

	MwStringPrintIntoBuffer(msg, 256, "You pressed: %s", MwListBoxGet(handle, *(int*)call));

	msgbox = MwMessageBox(w_main, msg, "wow", MwMB_ICONINFO | MwMB_BUTTONOK);
	MwAddUserHandler(MwMessageBoxGetChild(msgbox, MwMB_BUTTONOK), MwNactivateHandler, destroy, msgbox);
}

int main() {
	MwWidget   lb;
	int	   len = sizeof(harvard) / sizeof(harvard[0]) - 1;
	int	   i;
	int	   index;
	MwLLPixmap px;

	MwLibraryInit();

	w_main = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 640, 480,
				  MwNtitle, "listbox",
				  NULL);
	lb     = MwCreateWidget(MwListBoxClass, "listbox", w_main, 5, 5, 630, 470);

	px = MwLoadIcon(lb, MwIconInfo);

	MwSetInteger(lb, MwNleftPadding, 20);

	index = MwListBoxSet(lb, -1, 0, "Harvard sentence");
	MwListBoxSet(lb, index, -1, "Length");

	for(i = 0; i < len; i++) {
		char sz[16];
		MwStringPrintIntoBuffer(sz, 16, "%d", (int)strlen(harvard[i]));
		index = MwListBoxSet(lb, -1, 0, harvard[i]);
		MwListBoxSet(lb, index, -1, sz);
		MwListBoxSetIcon(lb, index, px);
	}

	MwAddUserHandler(lb, MwNlistBoxActivateHandler, activate, NULL);
	MwVaApply(lb,
		  MwNhasHeading, 1,
		  NULL);
	MwListBoxSetWidth(lb, 0, -128);

	MwLoop(w_main);
}
