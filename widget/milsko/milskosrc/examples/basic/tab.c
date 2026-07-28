#include <Mw/Milsko.h>

MwWidget tab;

static void MWAPI resize(MwWidget handle, void* user, void* client) {
	MwVaApply(tab,
		  MwNwidth, MwGetInteger(handle, MwNwidth),
		  MwNheight, MwGetInteger(handle, MwNheight),
		  NULL);
}

int main() {
	MwWidget   w;
	int	   i;
	MwLLPixmap px[6];

	MwLibraryInit();

	w = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 800, 600,
			     MwNtitle, "subwindow",
			     NULL);

	tab = MwCreateWidget(MwTabClass, "tab", w, 0, 0, 800, 600);

	for(i = 0; i < 16; i++) {
		char	 buf[32];
		MwWidget sw, f, img;

		sprintf(buf, "Tab %d", i);

		MwTabAdd(tab, buf);
	}

	MwVaApply(MwTabGetFrame(tab, "Tab 1"),
		  MwNiconPixmap, MwLoadIcon(tab, MwIconWarning),
		  NULL);

	MwAddUserHandler(w, MwNresizeHandler, resize, NULL);

	resize(w, NULL, NULL);

	MwLoop(w);
}
