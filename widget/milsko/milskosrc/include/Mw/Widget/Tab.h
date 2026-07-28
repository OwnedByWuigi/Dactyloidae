/*!
 * @file Mw/Widget/Tab.h
 * @brief Tab widget
 */
#ifndef __MW_WIDGET_TAB_H__
#define __MW_WIDGET_TAB_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Tab widget class
 */
MWDECL MwClass MwTabClass;

/*!
 * @brief Adds a tab
 * @param handle Widget
 * @param name Tab name
 * @return Frame
 */
MwInline MwWidget MwTabAdd(MwWidget handle, const char* name) {
	MwWidget out;
	MwVaWidgetExecute(handle, "mwTabAdd", &out, name);
	return out;
}

/*!
 * @brief Gets a tab frame
 * @param handle Widget
 * @param name Tab name
 * @return Frame
 */
MwInline MwWidget MwTabGetFrame(MwWidget handle, const char* name) {
	MwWidget out;
	MwVaWidgetExecute(handle, "mwTabGetFrame", &out, name);
	return out;
}

/*!
 * @brief Focus on a tab frame
 * @param handle Widget
 * @param name Tab name
 * @return Frame
 */
MwInline void MwTabFocus(MwWidget handle, const char* name) {
	MwVaWidgetExecute(handle, "mwTabFocus", NULL, name);
}

#ifdef __cplusplus
}
#endif

#endif
