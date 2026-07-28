/* this demo does not work well on windows */
#include <Mw/Milsko.h>

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define SIZE 100

MwWidget buttons[6];
double	 deg = 0;

int ww, wh;

void MWAPI handler(MwWidget w, void* user, void* client) {
	int    i;
	double cdeg = deg;

	(void)w;
	(void)user;
	(void)client;

	for(i = 0; i < (int)(sizeof(buttons) / sizeof(buttons[0])); i++) {
		double rad = cdeg / 180 * M_PI;
		int    x   = ww / 2;
		int    y   = wh / 2;

		x += cos(rad) * (ww / 2 - SIZE / 2);
		y += sin(rad) * (wh / 2 - SIZE / 2);

		x -= SIZE / 2;
		y -= SIZE / 2;

		MwVaApply(buttons[i],
			  MwNx, x,
			  MwNy, y,
			  NULL);

		cdeg += 360 / (sizeof(buttons) / sizeof(buttons[0]));
	}

	deg += 120.0 / (1000.0 / MwWaitMS);
}

void MWAPI resize(MwWidget w, void* user, void* client) {
	(void)w;
	(void)user;
	(void)client;

	ww = MwGetInteger(w, MwNwidth);
	wh = MwGetInteger(w, MwNheight);
}

int main() {
	MwWidget window;
	int	 i;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "window", NULL, MwDEFAULT, MwDEFAULT, (ww = 500), (wh = 500),
				  MwNtitle, "rotate",
				  NULL);

	for(i = 0; i < (int)(sizeof(buttons) / sizeof(buttons[0])); i++) {
		const char* color = "";
		char	    fgcolor[5];
		int	    j;

		if(i == 0) color = "#f66";
		if(i == 1) color = "#6f6";
		if(i == 2) color = "#ff6";
		if(i == 3) color = "#66f";
		if(i == 4) color = "#f6f";
		if(i == 5) color = "#6ff";

		fgcolor[0] = '#';
		fgcolor[4] = 0;
		for(j = 0; j < 3; j++) {
			fgcolor[j + 1] = color[j + 1] == 'f' ? '6' : 'f';
		}

		buttons[i] = MwVaCreateWidget(MwButtonClass, "button", window, 0, 0, SIZE, SIZE,
					      MwNbackground, color,
					      MwNforeground, fgcolor,
					      MwNtext, "fancy",
					      NULL);
	}

	MwAddUserHandler(window, MwNtickHandler, handler, NULL);
	MwAddUserHandler(window, MwNresizeHandler, resize, NULL);

	MwLoop(window);
}
