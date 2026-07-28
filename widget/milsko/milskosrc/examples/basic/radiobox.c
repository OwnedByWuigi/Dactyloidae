#include <Mw/Milsko.h>

int main() {
	MwWidget window;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "test", NULL, MwDEFAULT, MwDEFAULT, 8 + 16 + 8 + 16 * 10 + 8, 8 + 16 + 8 + 16 + 8,
				  MwNtitle, "radiobox",
				  NULL);

	MwVaCreateWidget(MwRadioBoxClass, "cb1", window, 8, 8, 16, 16,
			 NULL);
	MwVaCreateWidget(MwRadioBoxClass, "cb2", window, 8, 8 + 16 + 8, 16, 16,
			 MwNchecked, 1,
			 NULL);

	MwVaCreateWidget(MwLabelClass, "lab1", window, 8 + 16 + 8, 8, 16 * 10, 16,
			 MwNtext, "lorem ipsum 1",
			 MwNalignment, MwALIGNMENT_BEGINNING,
			 NULL);
	MwVaCreateWidget(MwLabelClass, "lab2", window, 8 + 16 + 8, 8 + 16 + 8, 16 * 10, 16,
			 MwNtext, "lorem ipsum 2",
			 MwNalignment, MwALIGNMENT_BEGINNING,
			 NULL);

	MwLoop(window);
}
