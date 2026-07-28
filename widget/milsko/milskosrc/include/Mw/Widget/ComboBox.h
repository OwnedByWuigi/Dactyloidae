/*!
 * @file Mw/Widget/ComboBox.h
 * @brief ComboBox widget
 */
#ifndef __MW_WIDGET_COMBOBOX_H__
#define __MW_WIDGET_COMBOBOX_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief ComboBox widget class
 */
MWDECL MwClass MwComboBoxClass;

/*!
 * @brief Adds the entry to combobox
 * @param handle Widget
 * @param index Index
 * @param text Text
 */
MwInline void MwComboBoxAdd(MwWidget handle, int index, const char* text) {
	MwVaWidgetExecute(handle, "mwComboBoxAdd", NULL, index, text);
}

/*!
 * @brief Gets the entry from combobox
 * @param handle Widget
 * @param index Index
 * @return Text
 */
MwInline const char* MwComboBoxGet(MwWidget handle, int index) {
	const char* text;
	MwVaWidgetExecute(handle, "mwComboBoxGet", (void*)&text, index);

	return text;
}

#ifdef __cplusplus
}
#endif

#endif
