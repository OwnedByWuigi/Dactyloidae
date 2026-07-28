/*!
 * @file Mw/Widget/ScrollBar.h
 * @brief ScrollBar widget
 */
#ifndef __MW_WIDGET_SCROLLBAR_H__
#define __MW_WIDGET_SCROLLBAR_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief ScrollBar widget class
 */
MWDECL MwClass MwScrollBarClass;

/*!
 * @brief Calculates a visible length of scrollbar
 * @param handle Widget
 * @return Visible length
 */
MwInline int MwScrollBarGetVisibleLength(MwWidget handle) {
	int out;
	MwVaWidgetExecute(handle, "mwScrollBarGetVisibleLength", &out, NULL);
	return out;
}

#ifdef __cplusplus
}
#endif

#endif
