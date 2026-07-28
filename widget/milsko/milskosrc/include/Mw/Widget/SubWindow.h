/*!
 * @file Mw/Widget/SubWindow.h
 * @brief SubWindow widget
 */
#ifndef __MW_WIDGET_SUBWINDOW_H__
#define __MW_WIDGET_SUBWINDOW_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief SubWindow widget class
 */
MWDECL MwClass MwSubWindowClass;

/*!
 * @brief Get parent widget where widgets should be placed
 * @param handle Widget
 * @return Widget
 */
MwInline MwWidget MwSubWindowGetFrame(MwWidget handle) {
	MwWidget out;
	MwVaWidgetExecute(handle, "mwSubWindowGetFrame", &out);
	return out;
}

#ifdef __cplusplus
}
#endif

#endif
