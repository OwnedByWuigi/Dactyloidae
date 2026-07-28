#include <Mw/Milsko.h>
#include <Mw/StringDefs.h>

#define WIDGET_AMOUNT 16

#define H(h) {&h, #h}

typedef struct cls_pair {
	MwClass*    cls;
	const char* name;
} cls_pair_t;

cls_pair_t    classes[]	   = {H(MwBoxClass), H(MwButtonClass), H(MwCheckBoxClass), H(MwComboBoxClass), H(MwEntryClass), H(MwFrameClass), H(MwImageClass), H(MwLabelClass), H(MwListBoxClass), H(MwMenuClass), H(MwNumberEntryClass), H(MwProgressBarClass), H(MwRadioBoxClass), H(MwScrollBarClass), H(MwSeparatorClass), H(MwTreeViewClass), H(MwViewportClass)};
char*	      int_params[] = {/*MwNx, MwNy, MwNwidth, MwNheight,*/ MwNorientation, MwNminValue, MwNmaxValue, MwNvalue, MwNchangedBy, MwNareaShown, MwNchecked, MwNalignment, MwNbold, MwNmain, MwNleftPadding, MwNhasHeading, MwNhasBorder, MwNinverted, MwNmodernLook, MwNwaitMS, MwNhideInput, MwNsingleClickSelectable, MwNflat, MwNshowArrows, MwNpadding, MwNborderWidth, MwNfillArea, MwNratio, MwNfixedSize, MwNmargin, MwNbitmapFont, MwNsevenSegment, MwNlength, MwNforceInverted, MwNisRounded, MwNdarkTheme};
unsigned long maxxes[]	   = {UINT8_MAX, UINT16_MAX, UINT32_MAX, UINT64_MAX};

int main() {
	int	      i, j = 0, max = 0;
	char	      input;
	MwWidget      window, widget;
	unsigned long upper_limit;

	printf("How many iteration do you want to do? Type -1 if you wanna run it forever.\n");
	scanf("%d", &max);

	printf("What upper limit of int params do you want to test?\n");
	for(i = 0; i < (sizeof(maxxes) / sizeof(unsigned long)); i++) {
		printf("%d) %lu\n", i, maxxes[i]);
	}
get_input:
	input = getchar();
	switch(input) {
	case '0':
		upper_limit = maxxes[0];
		break;
	case '1':
		upper_limit = maxxes[1];
		break;
	case '2':
		upper_limit = maxxes[2];
		break;
	case '3':
		upper_limit = maxxes[3];
		break;
	default:
		upper_limit = maxxes[0];
		// printf("Invalid value. Pick 0-3.\n");
		// goto get_input;
	}

	MwLibraryInit();

	window = MwCreateWidget(MwWindowClass, "window", NULL, 0, 0, 400, 400);

	while(MwPending(window) && (max == -1 || j < max)) {
		int class_num = rand() % (sizeof(classes) / sizeof(cls_pair_t));
		widget	      = MwCreateWidget(*classes[class_num].cls, classes[class_num].name, window, 0, 0, 100, 100);

		printf("%s\n", classes[class_num].name);
		/* apply int params with random scale */
		for(i = 0; i < (sizeof(int_params) / sizeof(void*)); i++) {
			unsigned long number = rand() % upper_limit;
			printf("> %s = %lu\n", int_params[i], number);
			MwVaApply(widget, int_params[i], number, NULL);
		}
		MwStep(window);

		MwDestroyWidget(widget);

		printf("\n");

		j++;
	}

	MwFreeWidget(window);
}
