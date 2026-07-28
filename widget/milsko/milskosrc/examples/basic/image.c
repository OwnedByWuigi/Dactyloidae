#include <Mw/Milsko.h>

int main() {
	MwWidget   window, image, image2, image3;
	MwLLPixmap px, px2, px3;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "window", NULL, MwDEFAULT, MwDEFAULT, 500, 500,
				  MwNtitle, "image image",
				  NULL);
	image  = MwCreateWidget(MwImageClass, "image", window, 50, 50, 200, 400);
	image2 = MwCreateWidget(MwImageClass, "image", window, 250, 50, 200, 200);
	image3 = MwCreateWidget(MwImageClass, "image", window, 250, 250, 200, 200);
	px     = MwLoadImage(window, "examples/picture.png");
	px2    = MwLoadImage(window, "examples/picture.jpg");
	px3    = MwLoadImage(window, "resource/logo/logo.png");

	if(px == NULL) px = MwLoadImage(window, "../examples/picture.png");
	if(px2 == NULL) px2 = MwLoadImage(window, "../examples/picture.jpg");
	if(px3 == NULL) px3 = MwLoadImage(window, "../resource/logo/logo.png");

	if(px == NULL) px = MwLoadImage(window, "picture.png");
	if(px2 == NULL) px2 = MwLoadImage(window, "picture.jpg");
	if(px3 == NULL) px3 = MwLoadImage(window, "logo.png");

	MwVaApply(window,
		  MwNiconPixmap, px,
		  NULL);

	MwVaApply(image,
		  MwNpixmap, px,
		  NULL);
	MwVaApply(image2,
		  MwNpixmap, px2,
		  NULL);
	MwVaApply(image3,
		  MwNpixmap, px3,
		  NULL);

	MwLoop(window);
}
