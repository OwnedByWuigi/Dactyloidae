#include <Mw/Milsko.h>

#define WIN_SIZE 464
#define PICKER_SIZE 364
#define IMG_POS_X(w) ((w - PICKER_SIZE) / 2)
#define IMG_POS_Y(h) ((h - PICKER_SIZE) / 2)
#define SCROLL_BAR_WIDTH 16
#define MARGIN (PICKER_SIZE / 32)
#define COLOR_DISPLAY_HEIGHT 12

#define HSV_HUE_SEXTANT 256.
#define HSV_HUE_STEPS (6. * HSV_HUE_SEXTANT)

#define HSV_HUE_MIN 0.
#define HSV_HUE_MAX (HSV_HUE_STEPS - 1.)
#define HSV_SAT_MIN 0.
#define HSV_SAT_MAX 255.
#define HSV_VAL_MIN 0.
#define HSV_VAL_MAX 255.

typedef struct _MwHSV {
	MwBool generated : 1;
	MwU16  h : 11;
	MwU8   s;
	MwU8   v;
} MwHSV;

typedef struct color_picker {
	MwWidget      parent;
	MwWidget      color_picker_img;
	MwWidget      value_slider;
	MwWidget      color_display_text;
	MwWidget      finish;
	MwLLPixmap    color_picker_pixmap;
	double	      value;
	MwRGB	      chosen_color;
	unsigned char color_picker_image_data[PICKER_SIZE * PICKER_SIZE * 4];
	MwHSV	      hue_table[101753];
	MwU8	      doUpdate;
} color_picker_t;

static void hsv2rgb(MwU32 h, MwU32 s, MwU32 v, MwU16* r, MwU16* g, MwU16* b) {
	MwU8  sextant = h >> 8;
	MwU16 ww;
	MwU32 h_fraction, d;

#define HSV_SWAPPTR(a, b) \
	do { \
		MwU16* tmp = a; \
		a	   = b; \
		b	   = tmp; \
	} while(0)

	if(sextant & 2) {
		HSV_SWAPPTR(r, b);
	}
	if(sextant & 4) {
		HSV_SWAPPTR(g, b);
	}
	if(!(sextant & 6)) {
		if(!(sextant & 1)) {
			HSV_SWAPPTR(r, g);
		}
	} else {
		if(sextant & 1) {
			HSV_SWAPPTR(r, g);
		}
	}

	*g = v; /* Top level */

	/* Perform actual calculations */

	/**
	 * Bottom level: v * (1.0 - s)
	 * --> (v * (255 - s) + error_corr + 1) / 256
	 */
	ww = v * (255 - (s)); /* We don't use ~s to prevent size-promotion side effects */
	ww += 1;	      /* Error correction */
	ww += ww >> 8;	      /* Error correction */
	*b = ww >> 8;

	h_fraction = h & 0xff; /* 0...255 */

	if(!(sextant & 1)) {
		/* *r = ...slope_up...; */
		d  = v * (MwU32)((0xff << 8) - (MwU16)(s * (0xff - h_fraction)));
		*r = d >> 16;
	} else {
		/* *r = ...slope_down...; */
		d  = v * (MwU32)((0xff << 8) - (MwU16)(s * h_fraction));
		*r = d >> 16;
	}
	return;
}

static void color_picker_image_update(color_picker_t* picker) {
	MwU16 y, x;
	int   n = 0;
	for(y = 0; y < PICKER_SIZE; y++) {
		for(x = 0; x < PICKER_SIZE; x++) {
			int    i    = ((y * PICKER_SIZE) + x) * 4;
			int    _x   = x - (PICKER_SIZE / 2);
			int    _y   = y - (PICKER_SIZE / 2);
			double dist = sqrt(_x * _x + _y * _y);

			if(dist >= 182.) {
				picker->color_picker_image_data[i]     = 0;
				picker->color_picker_image_data[i + 1] = 0;
				picker->color_picker_image_data[i + 2] = 0;
				picker->color_picker_image_data[i + 3] = 0;
			} else if(dist >= 180.) {
				picker->color_picker_image_data[i]     = dist;
				picker->color_picker_image_data[i + 1] = dist;
				picker->color_picker_image_data[i + 2] = dist;
				picker->color_picker_image_data[i + 3] = 255;
			} else {
				MwHSV hsv_v;
				MwRGB color;
				if(picker->hue_table[n].generated == 0) {
					double xd = (M_PI / 180.) * ((double)_x);
					double yd = (M_PI / 180.) * ((double)_y);

					float angle = atan2(yd, xd) - M_PI;
					float hue   = (angle * 180.) / M_PI;

					if(hue < 0.0) {
						hue += 360;
					}
					hsv_v.h			       = (hue) * (HSV_HUE_STEPS / 360.);
					hsv_v.s			       = (dist) * (HSV_SAT_MAX / 180.);
					picker->hue_table[n]	       = hsv_v;
					picker->hue_table[n].generated = 1;
				}

				hsv_v	= picker->hue_table[n];
				hsv_v.v = HSV_VAL_MAX - (picker->value * HSV_VAL_MAX);

				hsv2rgb(hsv_v.h, hsv_v.s, hsv_v.v, &color.red, &color.green, &color.blue);

				picker->color_picker_image_data[i]     = color.red;
				picker->color_picker_image_data[i + 1] = color.green;
				picker->color_picker_image_data[i + 2] = color.blue;

				picker->color_picker_image_data[i + 3] = 255;
				n++;
			}
		}
	}
	if(picker->color_picker_pixmap == NULL) {
		picker->color_picker_pixmap = MwLoadRaw(
		    picker->parent, picker->color_picker_image_data, PICKER_SIZE, PICKER_SIZE);
	} else {
		MwPixmapReloadRaw(
		    picker->color_picker_pixmap, picker->color_picker_image_data);
	}
	MwVaApply(picker->color_picker_img, MwNpixmap, picker->color_picker_pixmap, NULL);
	/* printf("%d\n", n); */
}

static void MWAPI color_picker_click(MwWidget handle, void* user, void* call) {
	color_picker_t* picker = user;
	MwMouse*	mouse  = call;
	char		hexColor[8];
	int		i;
	char		fgColor[8];
	int		fr, fg, fb;

	(void)handle;
	(void)user;
	(void)call;

	/* color_picker_image_update(picker); */

	i = ((mouse->point.y * PICKER_SIZE) + mouse->point.x) * 4;

	picker->chosen_color.red   = picker->color_picker_image_data[i];
	picker->chosen_color.green = picker->color_picker_image_data[i + 1];
	picker->chosen_color.blue  = picker->color_picker_image_data[i + 2];

	MwStringPrintIntoBuffer(hexColor, 8, "#%02X%02X%02X", picker->chosen_color.red & 0xff, picker->chosen_color.green & 0xff, picker->chosen_color.blue & 0xff);

	fr = picker->chosen_color.red > 128 ? 0 : 255;
	fg = picker->chosen_color.green > 128 ? 0 : 255;
	fb = picker->chosen_color.blue > 128 ? 0 : 255;

	MwStringPrintIntoBuffer(fgColor, 8, "#%02X%02X%02X", fr, fg, fb);
	MwSetText(picker->color_display_text, MwNforeground, fgColor);

	MwSetText(picker->color_display_text, MwNbackground, hexColor);
	MwSetText(picker->color_display_text, MwNtext, hexColor);
}
static void MWAPI color_picker_on_change_value(MwWidget handle, void* user,
					       void* call) {
	color_picker_t* picker = user;

	int value = MwGetInteger(handle, MwNvalue);
	int diff  = MwGetInteger(handle, MwNchangedBy);

	(void)diff;
	(void)call;

	picker->value = ((double)value / 1024.);

	picker->doUpdate = 1;
	/* color_picker_image_update(picker); */
}

static void MWAPI color_picker_tick(MwWidget handle, void* user,
				    void* call) {
	color_picker_t* picker = user;

	(void)handle;
	(void)call;

	if(picker->doUpdate == 1) {
		color_picker_image_update(picker);
		picker->doUpdate = 0;
	}
}

static void color_picker_destroy(color_picker_t* picker) {
	MwLLDestroyPixmap(picker->color_picker_pixmap);
	free(picker);
}

static void MWAPI color_picker_close(MwWidget handle, void* user,
				     void* call) {
	color_picker_t* picker = user;

	(void)call;

	color_picker_destroy(picker);
	MwDestroyWidget(handle);
}

static void MWAPI color_display_text_change(MwWidget handle, void* user,
					    void* call) {
	color_picker_t* picker = user;
	char		hexColor[9];
	char		fgColor[9];
	MwLLColor	color;
	int		fr, fg, fb;

	(void)call;

	memcpy(&hexColor, MwGetText(handle, MwNtext), 8);

	color = MwParseColor(handle, hexColor);

	fr = color->common.red > 128 ? 0 : 255;
	fg = color->common.green > 128 ? 0 : 255;
	fb = color->common.blue > 128 ? 0 : 255;

	MwStringPrintIntoBuffer(fgColor, 9, "#%02X%02X%02X", fr, fg, fb);
	MwSetText(picker->color_display_text, MwNforeground, fgColor);

	MwSetText(picker->color_display_text, MwNbackground, hexColor);
	MwSetText(picker->color_display_text, MwNtext, hexColor);

	picker->chosen_color.red   = color->common.red;
	picker->chosen_color.green = color->common.green;
	picker->chosen_color.blue  = color->common.blue;

	MwLLFreeColor(color);
}

static void MWAPI color_picker_finish(MwWidget handle, void* user,
				      void* call) {
	color_picker_t* picker = user;

	(void)call;

	MwDispatchUserHandler(picker->parent, MwNcolorChosenHandler, &picker->chosen_color);

	color_picker_destroy(picker);
	MwDestroyWidget(handle->parent);
}

color_picker_t* color_picker_setup(MwWidget parent, int w, int h) {
	color_picker_t* picker = malloc(sizeof(*picker));
	memset(picker, 0, sizeof(*picker));

	picker->parent = parent;

	picker->color_picker_img =
	    MwVaCreateWidget(MwImageClass, "image", picker->parent, IMG_POS_X(w), IMG_POS_Y(h),
			     PICKER_SIZE, PICKER_SIZE, NULL);

	/* picker->color_picker_image_data = malloc(PICKER_SIZE * PICKER_SIZE * 4); */

	picker->color_picker_pixmap = NULL;
	picker->value		    = 0;

	/* color_picker_image_update(picker); */
	picker->doUpdate = 1;

	MwAddUserHandler(picker->color_picker_img, MwNmouseDownHandler,
			 color_picker_click, picker);

	picker->color_display_text = MwCreateWidget(
	    MwEntryClass, "colorDisplayText", picker->parent, IMG_POS_X(w) + (PICKER_SIZE / 2) - ((PICKER_SIZE / 4) / 2),
	    IMG_POS_Y(h) - (PICKER_SIZE / 16) - MARGIN, (PICKER_SIZE / 4), PICKER_SIZE / 16);

	MwSetText(picker->color_display_text, MwNbackground, "#FFFFFF");
	MwSetText(picker->color_display_text, MwNtext, "#FFFFFF");
	/* MwSetInteger(picker->color_display_text, Mwnali, MwALIGNMENT_CENTER); */

	MwAddUserHandler(picker->color_display_text, MwNactivateHandler,
			 color_display_text_change, picker);
	picker->value_slider = MwVaCreateWidget(
	    MwScrollBarClass, "value-slider", picker->parent,
	    /* x */
	    IMG_POS_X(w) + PICKER_SIZE + MARGIN,

	    /* y */
	    IMG_POS_Y(h),

	    /* width */
	    SCROLL_BAR_WIDTH,

	    /* height */
	    PICKER_SIZE,

	    MwNorientation, MwVERTICAL, MwNminValue, 0, MwNmaxValue, 1024, NULL);

	MwAddUserHandler(picker->value_slider, MwNchangedHandler,
			 color_picker_on_change_value, picker);

	picker->finish = MwCreateWidget(
	    MwButtonClass, "colorPickerFinish", picker->parent, IMG_POS_X(w),
	    IMG_POS_Y(h) + PICKER_SIZE + MARGIN, PICKER_SIZE, (WIN_SIZE - PICKER_SIZE - MARGIN * 4) / 2);
	MwSetText(picker->finish, MwNtext, "Select");
	MwSetInteger(picker->finish, MwNhasBorder, 1);
	MwSetInteger(picker->finish, MwNinverted, 1);

	MwAddUserHandler(picker->finish, MwNactivateHandler,
			 color_picker_finish, picker);

	memset(picker->hue_table, 0, sizeof(picker->hue_table));

	return picker;
}

MwWidget MwColorPicker(MwWidget handle, const char* title) {
	color_picker_t* wheel;
	MwWidget	window;
	MwSizeHints	sh;
	int		wx;
	int		wy;

	if(handle == NULL) {
		wx = wy = MwDEFAULT;
	} else {
		wx = MwGetInteger(handle, MwNx) + (MwGetInteger(handle, MwNwidth) - WIN_SIZE) / 2;
		wy = MwGetInteger(handle, MwNy) + (MwGetInteger(handle, MwNheight) - WIN_SIZE) / 2;
	}

	sh.min_width = sh.max_width = WIN_SIZE;
	sh.min_height = sh.max_height = WIN_SIZE;

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, wx, wy,
				  WIN_SIZE, WIN_SIZE,
				  MwNtitle, title,
				  MwNsizeHints, &sh,
				  NULL);
	if(handle != NULL) MwReparent(window, handle);

	wheel = color_picker_setup(window, WIN_SIZE, WIN_SIZE);

	MwAddUserHandler(window, MwNcloseHandler, color_picker_close, wheel);
	MwAddUserHandler(window, MwNtickHandler, color_picker_tick, wheel);
	MwAddTickList(window);

	MwLLBeginStateChange(window->lowlevel);
	MwLLMakePopup(window->lowlevel, handle == NULL ? NULL : handle->lowlevel);
	MwLLEndStateChange(window->lowlevel);

	return window;
}
