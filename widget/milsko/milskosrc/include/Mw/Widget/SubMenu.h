/*!
 * @file Mw/Widget/SubMenu.h
 * @brief SubMenu widget
 */
#ifndef __MW_WIDGET_SUBMENU_H__
#define __MW_WIDGET_SUBMENU_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief SubMenu widget class
 */
MWDECL MwClass MwSubMenuClass;

/*!
 * @brief Makes submenu appear
 * @param handle Handle
 * @param menu Menu
 * @param point Point
 * @param diff_calc Toggles different way to calculate positiion
 */
MwInline void MwSubMenuAppear(MwWidget handle, MwMenu menu, MwPoint* point, int diff_calc) {
	MwVaWidgetExecute(handle, "mwSubMenuAppear", NULL, menu, point, diff_calc);
}

/*!
 * @brief Calculates the size of submenu
 * @param handle Handle
 * @param menu Menu
 * @param rect Size
 */
MwInline void MwSubMenuGetSize(MwWidget handle, MwMenu menu, MwRect* rect) {
	MwVaWidgetExecute(handle, "mwSubMenuGetSize", NULL, menu, rect);
}

#ifdef __cplusplus
}
#endif

#endif
