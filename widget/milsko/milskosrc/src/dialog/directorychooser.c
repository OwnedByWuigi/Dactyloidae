#include <Mw/Milsko.h>

MwWidget MwDirectoryChooser(MwWidget handle, const char* title) {
	return MwFileChooserEx(handle, title, 1);
}
