/*!
 * @file Mw/LowLevel/GDI.h
 * @brief GDI Backend
 * @warning This is used internally
 */
#ifndef __MW_LOWLEVEL_GDI_H__
#define __MW_LOWLEVEL_GDI_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/LowLevel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _MwLLGDI {
	struct _MwLLCommon common;

	HINSTANCE hInstance;
	HWND	  hWnd;
	HDC	  hDC;
	HCURSOR	  cursor;
	HICON	  icon;

	HRGN clip;

	int grabbed;
	int force_render;
	int get_clipboard;
	int get_darktheme;
};

struct _MwLLGDIColor {
	struct _MwLLCommonColor common;

	COLORREF color;
	HBRUSH	 brush;
};

struct _MwLLGDIPixmap {
	struct _MwLLCommonPixmap common;

	RGBQUAD* quad;
	HBITMAP	 hBitmap;
	HBITMAP	 hMask;
	HBITMAP	 hMask2;
};

MWDECL int     MwLLGDICallInit(void);
MWDECL HCURSOR MwLLGDICreateCursor(MwCursor* image, MwCursor* mask);

#ifdef __cplusplus
}
#endif

#endif
