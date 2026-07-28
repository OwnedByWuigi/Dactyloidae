/*!
 * @file Mw/Widget/ListBox.h
 * @brief ListBox widget
 */
#ifndef __MW_WIDGET_LISTBOX_H__
#define __MW_WIDGET_LISTBOX_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief ListBox widget class
 */
MWDECL MwClass MwListBoxClass;

/*!
 * @brief Sets item on the listbox
 * @param handle Widget
 * @param row Row
 * @param col Column
 * @param text Text
 * @return Index
 */
MwInline int MwListBoxSet(MwWidget handle, int row, int col, const char* text) {
	int out;

	MwVaWidgetExecute(handle, "mwListBoxSet", (void*)&out, row, col, text);

	return out;
}

/*!
 * @brief Sets the icon of the row
 * @param handle Widget
 * @param index Index
 * @param icon Icon
 */
MwInline void MwListBoxSetIcon(MwWidget handle, int index, MwLLPixmap icon) {
	MwVaWidgetExecute(handle, "mwListBoxSetIcon", NULL, index, icon);
}

/*!
 * @brief Deletes item from the listbox
 * @param handle Widget
 * @param index Index
 */
MwInline void MwListBoxDelete(MwWidget handle, int index) {
	MwVaWidgetExecute(handle, "mwListBoxDelete", NULL, index);
}

/*!
 * @brief Gets item from the listbox
 * @param handle Widget
 * @param index Index
 * @return Item
 */
MwInline const char* MwListBoxGet(MwWidget handle, int index) {
	const char* out;
	MwVaWidgetExecute(handle, "mwListBoxGet", (void*)&out, index);
	return out;
}

/*!
 * @brief Sets an item width of the listbox
 * @param handle Widget
 * @param index Column index
 * @param width Width
 */
MwInline void MwListBoxSetWidth(MwWidget handle, int index, int width) {
	MwVaWidgetExecute(handle, "mwListBoxSetWidth", NULL, index, width);
}

/*!
 * @brief Sets an item alignment of the listbox
 * @param handle Widget
 * @param index Column index
 * @param alignment Alignment
 */
MwInline void MwListBoxSetAlignment(MwWidget handle, int index, int alignment) {
	MwVaWidgetExecute(handle, "mwListBoxSetAlignment", NULL, index, alignment);
}

/*!
 * @brief Resets the listbox
 * @param handle Widget
 */
MwInline void MwListBoxReset(MwWidget handle) {
	MwVaWidgetExecute(handle, "mwListBoxReset", NULL);
}

#ifdef __cplusplus
}
#endif

#endif
