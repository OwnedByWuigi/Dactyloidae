/*!
 * @file Mw/Widget/Label.h
 * @brief Label widget
 */
#ifndef __MW_WIDGET_LABEL_H__
#define __MW_WIDGET_LABEL_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>
#include <Mw/Core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Label widget class
 */
MWDECL MwClass MwLabelClass;

/*!
 * @brief Sets the custom data for seven segment
 * @param handle Widget
 * @param index Index
 * @param data Data, `GFEDCBA` as 7-bit value
 * @note See https://en.wikipedia.org/wiki/File:7_Segment_Display_with_Labeled_Segments.svg for what alphabets mean here
 * */
MwInline void MwLabelSetSevenSegment(MwWidget handle, int index, int data) {
	MwVaWidgetExecute(handle, "mwLabelSetSevenSegment", NULL, index, data);
}

#ifdef __cplusplus
}
#endif

#endif
