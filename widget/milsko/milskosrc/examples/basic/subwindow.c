#include <Mw/Milsko.h>

#define Shift 16

static void MWAPI resize(MwWidget handle, void* user, void* client) {
	MwWidget f = MwSubWindowGetFrame(handle);
	MwVaApply(user,
		  MwNwidth, MwGetInteger(f, MwNwidth),
		  MwNheight, MwGetInteger(f, MwNheight),
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

	px[0] = MwLoadIcon(w, MwIconClock);
	px[1] = MwLoadIcon(w, MwIconError);
	px[2] = MwLoadIcon(w, MwIconInfo);
	px[3] = MwLoadIcon(w, MwIconNews);
	px[4] = MwLoadIcon(w, MwIconNote);
	px[5] = MwLoadIcon(w, MwIconWarning);

	for(i = 0; i < 16; i++) {
		char	 buf[32];
		MwWidget sw, f, img;

		sprintf(buf, "Sub window %d", i);

		sw = MwVaCreateWidget(MwSubWindowClass, "swnd", w, i * Shift, i * Shift, 128, 128,
				      MwNtitle, buf,
				      MwNiconPixmap, px[rand() % 6],
				      NULL);

		f = MwSubWindowGetFrame(sw);

		img = MwVaCreateWidget(MwImageClass, "img", f, 0, 0, MwGetInteger(f, MwNwidth), MwGetInteger(f, MwNheight),
				       MwNpixmap, px[rand() % 6],
				       NULL);

		MwAddUserHandler(sw, MwNresizeHandler, resize, img);
	}

	MwLoop(w);
}
