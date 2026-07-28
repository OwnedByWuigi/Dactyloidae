#include <Mw/Milsko.h>

MwWidget window, box;

void MWAPI resize(MwWidget handle, void* user, void* client) {
	int ww = MwGetInteger(handle, MwNwidth);
	int wh = MwGetInteger(handle, MwNheight);

	(void)user;
	(void)client;

	MwVaApply(box,
		  MwNwidth, ww,
		  MwNheight, wh,
		  NULL);
}

int main() {
	MwWidget box2, box3, box4;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 600, 200,
				  MwNtitle, "box",
				  NULL);

	box = MwVaCreateWidget(MwBoxClass, "box", window, 0, 0, 0, 0,
			       MwNpadding, 10,
			       MwNmargin, 10,
			       NULL);

	box2 = MwVaCreateWidget(MwBoxClass, "box2", box, 0, 0, 0, 0,
				MwNmargin, 10,
				MwNorientation, MwVERTICAL,
				NULL);

	box3 = MwVaCreateWidget(MwBoxClass, "box3", box, 0, 0, 0, 0,
				MwNmargin, 10,
				MwNorientation, MwVERTICAL,
				NULL);

	box4 = MwVaCreateWidget(MwBoxClass, "box4", box, 0, 0, 0, 0,
				MwNmargin, 10,
				MwNorientation, MwVERTICAL,
				MwNfixedSize, 40,
				NULL);

	MwVaCreateWidget(MwButtonClass, "btn1", box2, 0, 0, 0, 0,
			 MwNbackground, "#a00",
			 NULL);

	MwVaCreateWidget(MwButtonClass, "btn2", box2, 0, 0, 0, 0,
			 MwNbackground, "#0a0",
			 MwNratio, 2,
			 NULL);

	MwVaCreateWidget(MwButtonClass, "btn3", box2, 0, 0, 0, 0,
			 MwNbackground, "#00a",
			 NULL);

	MwVaCreateWidget(MwButtonClass, "btn4", box3, 0, 0, 0, 0,
			 MwNbackground, "#a0a",
			 NULL);

	MwVaCreateWidget(MwButtonClass, "btn5", box4, 0, 0, 0, 0,
			 MwNbackground, "#0aa",
			 NULL);

	MwAddUserHandler(window, MwNresizeHandler, resize, NULL);

	resize(window, NULL, NULL);

	MwLoop(window);
}
