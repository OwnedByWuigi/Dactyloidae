/*!
 * @file Mw/Widget/Menu.h
 * @brief Menu widget
 */
#ifndef __MW_WIDGET_MENU_H__
#define __MW_WIDGET_MENU_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Menu widget class
 */
MWDECL MwClass MwMenuClass;

/*!
 * @brief Adds a menu
 * @param handle Widget
 * @param menu Menu
 * @param name Menu name
 * @return Menu
 */
MwInline MwMenu MwMenuAdd(MwWidget handle, MwMenu menu, const char* name) {
	MwMenu out;
	MwVaWidgetExecute(handle, "mwMenuAdd", &out, menu, name);
	return out;
}

#ifdef __cplusplus
}
#endif

#endif
