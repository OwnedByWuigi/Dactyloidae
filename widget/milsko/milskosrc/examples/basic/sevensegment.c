#include <Mw/Milsko.h>

int main() {
	MwWidget wnd, label;
	int	 W = 480, H = 40;

	MwLibraryInit();

	wnd = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, W, H,
			       MwNtitle, "seven segment",
			       NULL);

	label = MwVaCreateWidget(MwLabelClass, "label", wnd, 0, 0, W, H,
				 MwNtext, " 123456:78.90 abcdef",
				 MwNsevenSegment, 1,
				 MwNalignment, MwALIGNMENT_BEGINNING,
				 MwNlength, 8,
				 NULL);

	MwLabelSetSevenSegment(label, 0, (1 << 0) | (1 << 3) | (1 << 6));

	MwLoop(wnd);
}
