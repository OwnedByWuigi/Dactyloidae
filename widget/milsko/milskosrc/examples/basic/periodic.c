#include <Mw/Milsko.h>

#include <stdarg.h>

#define Columns 5
#define Padding 20
#define PaddingContent 2

MwWidget window, menu, table;
MwMenu	 e;

static void MWAPI resize(MwWidget handle, void* user, void* call) {
	int w = MwGetInteger(window, MwNwidth);
	int h = MwGetInteger(window, MwNheight);

	MwVaApply(table,
		  MwNx, Padding,
		  MwNy, MwGetInteger(menu, MwNheight) + Padding,
		  MwNwidth, w - Padding * 2,
		  MwNheight, h - MwGetInteger(menu, MwNheight) - Padding * 2,
		  NULL);
}

static void MWAPI resize_content(MwWidget handle, void* user, void* call) {
	MwWidget* ws = MwGetChildren(handle);
	if(ws != NULL) {
		int pw = MwGetInteger(ws[0], "IpWidth");
		int ph = MwGetInteger(ws[0], "IpHeight");

		if(pw < 0) {
			pw += MwGetInteger(handle, MwNwidth);
		} else if(pw == 0) {
			pw = MwGetInteger(handle, MwNwidth);
		}

		if(ph < 0) {
			ph += MwGetInteger(handle, MwNheight);
		} else if(ph == 0) {
			ph = MwGetInteger(handle, MwNheight);
		}

		MwVaApply(ws[0],
			  MwNx, (MwGetInteger(handle, MwNwidth) - pw) / 2,
			  MwNy, (MwGetInteger(handle, MwNheight) - ph) / 2,
			  MwNwidth, pw,
			  MwNheight, ph,
			  NULL);

		free(ws);
	}
}

static void MWAPI resize_frame(MwWidget handle, void* user, void* call) {
	MwWidget* ws = MwGetChildren(handle);
	if(ws != NULL) {
		int w = MwGetInteger(handle, MwNwidth);
		int h = MwGetInteger(handle, MwNheight);

		if(ws[0] != NULL) {
			MwVaApply(ws[0],
				  MwNx, MwDefaultBorderWidth(handle) + PaddingContent,
				  MwNy, MwDefaultBorderWidth(handle) + PaddingContent,
				  MwNwidth, w - (MwDefaultBorderWidth(handle) + PaddingContent) * 2,
				  MwNheight, h - (MwDefaultBorderWidth(handle) + PaddingContent) * 2,
				  NULL);
		}
		free(ws);
	}
}

static MwWidget frame(const char* name, int width, int height, MwClass cl, ...) {
	MwWidget f     = MwVaCreateWidget(MwFrameClass, "frame", table, 0, 0, 0, 0,
					  MwNhasBorder, 1,
					  MwNinverted, 1,
					  NULL);
	MwWidget table = MwVaCreateWidget(MwBoxClass, "table", f, 0, 0, 0, 0,
					  MwNorientation, MwVERTICAL,
					  NULL);
	MwWidget label = MwVaCreateWidget(MwLabelClass, "label", table, 0, 0, 0, 0,
					  MwNtext, name,
					  MwNalignment, MwALIGNMENT_BEGINNING,
					  MwNfixedSize, 20,
					  NULL);
	va_list	 va;

	if(cl != NULL) {
		MwWidget frame = MwCreateWidget(MwFrameClass, "frame", table, 0, 0, 0, 0);
		MwWidget w;

		va_start(va, cl);
		w = MwVaListCreateWidget(cl, "widget", frame, 0, 0, 0, 0, va);
		va_end(va);

		MwVaApply(w,
			  "IpWidth", width,
			  "IpHeight", height,
			  NULL);

		MwAddUserHandler(frame, MwNresizeHandler, resize_content, NULL);

		MwVaApply(f,
			  "VhandledWidget", w,
			  MwNcolumnSpan, MwGetInteger(w, MwNcolumnSpan),
			  MwNrowSpan, MwGetInteger(w, MwNrowSpan),
			  NULL);
	}

	MwAddUserHandler(f, MwNresizeHandler, resize_frame, NULL);

	resize_frame(f, NULL, NULL);

	return f;
}

static MwWidget child(MwWidget w) {
	return MwGetVoid(w, "VhandledWidget");
}

static void MWAPI menu_handler(MwWidget handle, void* user, void* call) {
	if(call == e) MwDestroyWidget(window);
}

int main() {
	int	   i, j;
	MwWidget   f, w, b;
	MwLLPixmap px;
	int	   index;
	MwMenu	   m, m2;
	void*	   v;
	MwRect	   rc;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "window", NULL, MwDEFAULT, MwDEFAULT, 800, 800,
				  MwNtitle, "Milsko Periodic Table",
				  NULL);

	MwGetScreenSize(window, &rc);
	if(rc.height < 800) {
		MwVaApply(window,
			  MwNx, (rc.width - (rc.height - 64)) / 2,
			  MwNy, (rc.height - (rc.height - 64)) / 2,
			  MwNwidth, rc.height - 64,
			  MwNheight, rc.height - 64,
			  NULL);
	}

	px = MwLoadIcon(window, MwIconError);

	menu = MwCreateWidget(MwMenuClass, "menu", window, 0, 0, 0, 0);

	m = MwMenuAdd(menu, NULL, "File");
	MwMenuAdd(menu, m, "New");
	MwMenuAdd(menu, m, "Open");
	MwMenuAdd(menu, m, "Save");
	MwMenuAdd(menu, m, "Save As");
	MwMenuAdd(menu, m, "Print");
	e = MwMenuAdd(menu, m, "Exit");

	m = MwMenuAdd(menu, NULL, "Edit");
	MwMenuAdd(menu, m, "Undo");
	MwMenuAdd(menu, m, "----");
	MwMenuAdd(menu, m, "Cut");
	MwMenuAdd(menu, m, "Copy");
	MwMenuAdd(menu, m, "Paste");
	MwMenuAdd(menu, m, "----");
	MwMenuAdd(menu, m, "Clear");
	MwMenuAdd(menu, m, "Delete");

	m = MwMenuAdd(menu, NULL, "View");
	MwMenuAdd(menu, m, "Stack");
	MwMenuAdd(menu, m, "Tile");
	MwMenuAdd(menu, m, "----");
	MwMenuAdd(menu, m, "Day");
	MwMenuAdd(menu, m, "Week");
	MwMenuAdd(menu, m, "Month");
	MwMenuAdd(menu, m, "Year");

	m  = MwMenuAdd(menu, NULL, "Options");
	m2 = MwMenuAdd(menu, m, "Font");
	MwMenuAdd(menu, m2, "Small");
	MwMenuAdd(menu, m2, "Medium");
	MwMenuAdd(menu, m2, "Large");
	m2 = MwMenuAdd(menu, m, "Direction");
	MwMenuAdd(menu, m2, "Up");
	MwMenuAdd(menu, m2, "Down");
	MwMenuAdd(menu, m2, "Left");
	MwMenuAdd(menu, m2, "Right");
	MwMenuAdd(menu, m, "Case Sensitive");
	MwMenuAdd(menu, m, "Word Wrap");

	m = MwMenuAdd(menu, NULL, "?Help");
	MwMenuAdd(menu, m, "On Context");
	MwMenuAdd(menu, m, "On Window");
	MwMenuAdd(menu, m, "Tutorial");
	MwMenuAdd(menu, m, "On Version");

	MwAddUserHandler(menu, MwNmenuHandler, menu_handler, NULL);

	table = MwVaCreateWidget(MwTableClass, "table", window, 0, 0, 0, 0,
				 MwNwaitLayout, 1,
				 MwNcolumns, 5,
				 NULL);

	f = frame("Button", -PaddingContent, -PaddingContent, MwBoxClass,
		  MwNorientation, MwVERTICAL,
		  NULL);
	b = MwVaCreateWidget(MwButtonClass, "btn", child(f), 0, 0, 0, 0,
			     MwNtext, "Press me",
			     NULL);
	b = MwVaCreateWidget(MwButtonClass, "btn_disabled", child(f), 0, 0, 0, 0,
			     MwNtext, "Cannot press me",
			     MwNdisabled, 1,
			     NULL);

	MwVaCreateWidget(MwLabelClass, "label", table, 0, 0, 0, 0,
			 MwNbold, 1,
			 MwNtext, "The Periodic Table of Milsko Widgets",
			 MwNcolumnSpan, Columns - 2,
			 NULL);

	for(i = 0; i < 2; i++) {
		f = frame(i == 0 ? "CheckBox" : "RadioBox", -PaddingContent, -PaddingContent, MwBoxClass,
			  MwNmargin, PaddingContent,
			  NULL);
		w = child(f);
		b = MwVaCreateWidget(MwBoxClass, "table1", w, 0, 0, 0, 0,
				     MwNorientation, MwVERTICAL,
				     MwNfixedSize, 16,
				     NULL);
		for(j = 0; j < 6; j++) MwVaCreateWidget(i == 0 ? MwCheckBoxClass : MwRadioBoxClass, i == 0 ? "cb" : "rb", b, 0, 0, 0, 0, MwNdisabled, (j == 5) ? 1 : 0, NULL);
		b = MwVaCreateWidget(MwBoxClass, "table1", w, 0, 0, 0, 0,
				     MwNorientation, MwVERTICAL,
				     NULL);
		for(j = 0; j < 6; j++) {
			char buf[32];
			sprintf(buf, "%sBox %d", i == 0 ? "Check" : "Radio", j + 1);
			MwVaCreateWidget(MwLabelClass, "label", b, 0, 0, 0, 0,
					 MwNtext, buf,
					 MwNalignment, MwALIGNMENT_BEGINNING,
					 NULL);
		}
	}

	f = frame("Calendar", -PaddingContent, -PaddingContent, MwCalendarClass,
		  MwNscale, 1,
		  NULL);

	f = frame("ComboBox", -PaddingContent, 24, MwComboBoxClass, NULL);
	w = child(f);
	MwComboBoxAdd(w, -1, "Hello!");

	f = frame("Entry", -PaddingContent, 24, MwEntryClass, NULL);

	f = frame("Image", -PaddingContent, -PaddingContent, MwImageClass,
		  MwNpixmap, px,
		  NULL);

	f = frame("Label", -PaddingContent, -PaddingContent, MwLabelClass,
		  MwNtext, "Epic text\nNewline can be used too!",
		  NULL);

	f     = frame("ListBox", -PaddingContent, -PaddingContent, MwListBoxClass,
		      MwNhasHeading, 1,
		      NULL);
	w     = child(f);
	index = MwListBoxSet(w, -1, 0, "Epic title...");
	index = MwListBoxSet(w, -1, 0, "Hello");

	f = frame("NumberEntry", -PaddingContent, 24, MwNumberEntryClass, NULL);

	f = frame("ProgressBar", -PaddingContent, 24, MwProgressBarClass,
		  MwNvalue, 25,
		  NULL);

	f = frame("ScrollBar", -PaddingContent, 16, MwScrollBarClass,
		  MwNorientation, MwHORIZONTAL,
		  NULL);

	f = frame("Separator", -PaddingContent, -PaddingContent, MwSeparatorClass, NULL);

	f = frame("SubWindow", -PaddingContent, -PaddingContent, MwViewportClass,
		  MwNcolumnSpan, 2,
		  NULL);
	w = child(f);
	MwViewportSetSize(w, 512, 512);
	MwVaCreateWidget(MwSubWindowClass, "swnd", MwViewportGetViewport(w), 32, 32, 256, 128,
			 MwNtitle, "Sub window",
			 NULL);
	MwVaCreateWidget(MwSubWindowClass, "swnd", MwViewportGetViewport(w), 64, 64, 256, 128,
			 MwNtitle, "Sub window 2",
			 NULL);

	f = frame("Tab", -PaddingContent, -PaddingContent, MwTabClass,
		  MwNcolumnSpan, 2,
		  NULL);
	w = child(f);
	MwSetText(MwTabAdd(w, "ABC"), MwNbackground, "#f00");
	MwSetText(MwTabAdd(w, "DEF"), MwNbackground, "#0f0");
	MwSetText(MwTabAdd(w, "GHI"), MwNbackground, "#00f");

	f = frame("TreeView", -PaddingContent, -PaddingContent, MwTreeViewClass, NULL);
	w = child(f);
	v = MwTreeViewAdd(w, NULL, NULL, "abc");
	v = MwTreeViewAdd(w, v, NULL, "def");
	v = MwTreeViewAdd(w, v, NULL, "ghi");

	f = frame("Viewport", -PaddingContent, -PaddingContent, MwViewportClass, NULL);
	w = child(f);
	MwViewportSetSize(w, 256, 256);
	MwVaCreateWidget(MwButtonClass, "btn", MwViewportGetViewport(w), 64, 64, 128, 128,
			 MwNtext, "Thing",
			 NULL);

	MwAddUserHandler(window, MwNresizeHandler, resize, NULL);

	resize(window, NULL, NULL);

	MwVaApply(table,
		  MwNwaitLayout, 0,
		  NULL);

	MwLoop(window);

	MwLLDestroyPixmap(px);
}
