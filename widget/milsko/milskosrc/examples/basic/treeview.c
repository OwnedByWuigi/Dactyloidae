#include <Mw/Milsko.h>

MwWidget wmain;

void MWAPI destroy(MwWidget handle, void* user, void* call) {
	(void)handle;
	(void)call;
	MwDestroyWidget(user);
}

void MWAPI activate(MwWidget handle, void* user, void* call) {
	char	 msg[256];
	MwWidget msgbox;

	(void)user;

	MwStringPrintIntoBuffer(msg, 256, "You pressed: %s", MwTreeViewGet(handle, call));

	msgbox = MwMessageBox(wmain, msg, "wow", MwMB_ICONINFO | MwMB_BUTTONOK);
	MwAddUserHandler(MwMessageBoxGetChild(msgbox, MwMB_BUTTONOK), MwNactivateHandler, destroy, msgbox);
}

int main() {
	MwWidget   tv;
	MwLLPixmap px;
	int	   i;
	void *	   p = NULL, *r;

	MwLibraryInit();

	wmain = MwCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 5 + 640 + 5, 5 + 480 + 5);
	tv    = MwCreateWidget(MwTreeViewClass, "tree", wmain, 5, 5, 640, 480);

	px = MwLoadIcon(tv, MwIconInfo);

	MwSetInteger(tv, MwNleftPadding, 16);

	MwAddUserHandler(tv, MwNactivateHandler, activate, NULL);

	for(i = 0; i < 10; i++) {
		void* old = p;

		MwTreeViewAdd(tv, old, px, "World");
		p = MwTreeViewAdd(tv, old, px, "Hello");
		if(i == 5) r = p;
		MwTreeViewAdd(tv, old, px, "Goodbye");
	}

	MwLoop(wmain);
}
