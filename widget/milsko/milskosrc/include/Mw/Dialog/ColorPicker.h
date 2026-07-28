/*!
 * @file Mw/Dialog/ColorPicker.h
 * @brief Color picker
 */
#ifndef __MW_COLORPICKER_H__
#define __MW_COLORPICKER_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Creates a color picker
 * @param handle Widget
 * @param title Title text
 * @return Widget
 */
MWDECL MwWidget MWAPI MwColorPicker(MwWidget handle, const char* title);

#ifdef __cplusplus
}
#endif

#endif
