#include <Mw/Milsko.h>

int main() {
	MwWidget window;
	int	 i;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 500, 500,
				  MwNtitle, "main",
				  NULL);

	for(i = 0; i < 500 / 16; i++) {
		MwVaCreateWidget(MwScrollBarClass, "scroll", window, 16 * i, 0, 16, 500,
				 MwNorientation, MwVERTICAL,
				 NULL);
	}

	MwLoop(window);
}
