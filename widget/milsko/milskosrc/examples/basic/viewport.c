#include <Mw/Milsko.h>

MwWidget vp;

static void MWAPI resize(MwWidget handle, void* user, void* call) {
	int w = MwGetInteger(handle, MwNwidth);
	int h = MwGetInteger(handle, MwNheight);

	(void)user;
	(void)call;

	MwVaApply(vp,
		  MwNwidth, w - 10 - 100,
		  MwNheight, h - 10 - 100,
		  NULL);
}

int main() {
	MwWidget   w;
	MwLLPixmap px;

	MwLibraryInit();

	w = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 640, 480, MwNtitle, "test", NULL);

	vp = MwCreateWidget(MwViewportClass, "vp", w, 5 + 50, 5 + 50, 640 - 10 - 100, 480 - 10 - 100);

	px = MwLoadImage(vp, "examples/picture.png");

	if(px == NULL) px = MwLoadImage(vp, "../examples/picture.png");

	if(px == NULL) px = MwLoadImage(vp, "picture.png");

	MwVaCreateWidget(MwImageClass, "image", MwViewportGetViewport(vp), 0, 0, 1024, 1024,
			 MwNpixmap, px,
			 NULL);
	MwViewportSetSize(vp, 1024, 1024);

	MwAddUserHandler(w, MwNresizeHandler, resize, NULL);

	MwLoop(w);
}
