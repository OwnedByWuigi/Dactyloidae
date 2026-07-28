#include <Mw/Milsko.h>

int main() {
	MwWidget w, cb;

	MwLibraryInit();

	w = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 5 + 320 + 5, 5 + 24 + 5,
			     MwNtitle, "combobox",
			     NULL);

	cb = MwCreateWidget(MwComboBoxClass, "combobox", w, 5, 5, 320, 24);

	MwComboBoxAdd(cb, -1, "wow!");
	MwComboBoxAdd(cb, -1, "insert");
	MwComboBoxAdd(cb, -1, "cool");
	MwComboBoxAdd(cb, -1, "text");
	MwComboBoxAdd(cb, -1, "here");
	MwComboBoxAdd(cb, -1, "!!!");

	MwLoop(w);
}
