/*!
 * @file Mw/LowLevel/ClassicMacOS.h
 * @brief ClassicMacOS Backend
 * @warning This is used internally
 */
#ifndef __MW_LOWLEVEL_ClassicMacOS_H__
#define __MW_LOWLEVEL_ClassicMacOS_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/LowLevel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _MwLLClassicMacOS {
	struct _MwLLCommon common;

	MwLL parent;

	WindowRef     window;
	ControlHandle control;
	EventRecord   event;
	Rect	      winRect;
};

struct _MwLLClassicMacOSColor {
	struct _MwLLCommonColor common;
};

struct _MwLLClassicMacOSPixmap {
	struct _MwLLCommonPixmap common;
	BitMap			 bmp;
};

MWDECL int MwLLClassicMacOSCallInit(void);

#ifdef __cplusplus
}
#endif

#endif
