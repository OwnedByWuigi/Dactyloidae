/*!
 * @file Mw/Widget/Window.h
 * @brief Window widget
 */
#ifndef __MW_WIDGET_WINDOW_H__
#define __MW_WIDGET_WINDOW_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Window widget class
 */
MWDECL MwClass MwWindowClass;

/*!
 * @brief Makes window borderless
 * @param handle Widget
 * @param toggle Toggle
 */
MwInline void MwWindowMakeBorderless(MwWidget handle, int toggle) {
	MwVaWidgetExecute(handle, "mwWindowMakeBorderless", NULL, toggle);
}

/*!
 * @brief Whether the window should close
 * @param handle Widget
 * @return bool
 */
MwInline MwBool MwWindowShouldClose(MwWidget handle) {
	MwBool res = MwFALSE;
	MwVaWidgetExecute(handle, "mwWindowShouldClose", NULL, &res);
	return res;
}

#ifdef __cplusplus
}
#endif

#endif
