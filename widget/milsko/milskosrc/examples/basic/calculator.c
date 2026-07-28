#include <Mw/Milsko.h>

MwWidget widgets[18];
MwWidget inp;

/* probably enough for most tasks */
double stack[512];
int    sp = 0;

static void MWAPI resize(MwWidget handle, void* client, void* user) {
	int ww = MwGetInteger(handle, MwNwidth);
	int wh = MwGetInteger(handle, MwNheight) - MwGetInteger(inp, MwNheight);
	int i;

	MwVaApply(inp,
		  MwNwidth, ww,
		  NULL);

	for(i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++) {
		int x = i % 4;
		int y = i / 4;
		if(widgets[i] != NULL) {
			MwVaApply(widgets[i],
				  MwNwidth, ww / 4,
				  MwNheight, wh / 5,
				  MwNx, ww / 4 * x,
				  MwNy, wh / 5 * y + MwGetInteger(inp, MwNheight),
				  NULL);
		}
	}
}

static void MWAPI activate(MwWidget handle, void* client, void* user) {
	const char* n = MwGetText(handle, MwNtext);

	if(('0' <= n[0] && n[0] <= '9') || n[0] == '.') {
		const char* s = MwGetText(inp, MwNtext);
		char*	    r;

		if(n[0] == '.' && s != NULL) {
			int i;
			for(i = 0; s[i] != 0; i++) {
				if(s[i] == '.') return;
			}
		}

		if(s == NULL) s = "";

		r = MwStringConcat(s, n);
		MwVaApply(inp,
			  MwNtext, r,
			  NULL);
		free(r);
	} else if(n[0] == 'E') {
		const char* s = MwGetText(inp, MwNtext);

		if(s != NULL) {
			stack[sp++] = atof(s);

			MwVaApply(inp,
				  MwNtext, "",
				  NULL);
		}
	} else if(n[0] == 'C') {
		MwVaApply(inp,
			  MwNtext, "",
			  NULL);
	} else if(n[0] == '+' || n[0] == '-' || n[0] == '*' || n[0] == '/') {
		if(sp >= 2) {
			double sv = stack[--sp];
			double fv = stack[--sp];
			double v;
			char   buf[256];

			if(n[0] == '+') {
				v = fv + sv;
			} else if(n[0] == '-') {
				v = fv - sv;
			} else if(n[0] == '*') {
				v = fv * sv;
			} else if(n[0] == '/') {
				v = fv / sv;
			}

			MwStringPrintIntoBuffer(buf, 256, "%g", v);

			MwVaApply(inp,
				  MwNtext, buf,
				  NULL);
		}
	}
}

int main() {
	MwWidget w;
	int	 i;

	for(i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++) widgets[i] = NULL;

	MwLibraryInit();

	w = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 150, 150,
			     MwNtitle, "Calculator",
			     NULL);

	inp = MwCreateWidget(MwEntryClass, "entry", w, 0, 0, 0, 24);

	widgets[0] = MwVaCreateWidget(MwButtonClass, "7", w, 0, 0, 0, 0,
				      MwNtext, "7",
				      NULL);

	widgets[1] = MwVaCreateWidget(MwButtonClass, "8", w, 0, 0, 0, 0,
				      MwNtext, "8",
				      NULL);

	widgets[2] = MwVaCreateWidget(MwButtonClass, "9", w, 0, 0, 0, 0,
				      MwNtext, "9",
				      NULL);

	widgets[3] = MwVaCreateWidget(MwButtonClass, "/", w, 0, 0, 0, 0,
				      MwNtext, "/",
				      NULL);

	widgets[4] = MwVaCreateWidget(MwButtonClass, "4", w, 0, 0, 0, 0,
				      MwNtext, "4",
				      NULL);

	widgets[5] = MwVaCreateWidget(MwButtonClass, "5", w, 0, 0, 0, 0,
				      MwNtext, "5",
				      NULL);

	widgets[6] = MwVaCreateWidget(MwButtonClass, "6", w, 0, 0, 0, 0,
				      MwNtext, "6",
				      NULL);

	widgets[7] = MwVaCreateWidget(MwButtonClass, "*", w, 0, 0, 0, 0,
				      MwNtext, "*",
				      NULL);

	widgets[8] = MwVaCreateWidget(MwButtonClass, "1", w, 0, 0, 0, 0,
				      MwNtext, "1",
				      NULL);

	widgets[9] = MwVaCreateWidget(MwButtonClass, "2", w, 0, 0, 0, 0,
				      MwNtext, "2",
				      NULL);

	widgets[10] = MwVaCreateWidget(MwButtonClass, "3", w, 0, 0, 0, 0,
				       MwNtext, "3",
				       NULL);

	widgets[11] = MwVaCreateWidget(MwButtonClass, "-", w, 0, 0, 0, 0,
				       MwNtext, "-",
				       NULL);

	widgets[12] = MwVaCreateWidget(MwButtonClass, "0", w, 0, 0, 0, 0,
				       MwNtext, "0",
				       NULL);

	widgets[13] = MwVaCreateWidget(MwButtonClass, ".", w, 0, 0, 0, 0,
				       MwNtext, ".",
				       NULL);

	widgets[16] = MwVaCreateWidget(MwButtonClass, "E", w, 0, 0, 0, 0,
				       MwNtext, "E",
				       NULL);

	widgets[15] = MwVaCreateWidget(MwButtonClass, "+", w, 0, 0, 0, 0,
				       MwNtext, "+",
				       NULL);

	widgets[17] = MwVaCreateWidget(MwButtonClass, "C", w, 0, 0, 0, 0,
				       MwNtext, "C",
				       NULL);

	for(i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++) {
		if(widgets[i] != NULL) MwAddUserHandler(widgets[i], MwNactivateHandler, activate, NULL);
	}

	MwAddUserHandler(w, MwNresizeHandler, resize, NULL);

	resize(w, NULL, NULL);

	MwLoop(w);
}
