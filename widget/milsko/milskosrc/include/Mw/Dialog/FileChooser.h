/*!
 * @file Mw/Dialog/FileChooser.h
 * @brief File chooser
 */
#ifndef __MW_FILECHOOSER_H__
#define __MW_FILECHOOSER_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Creates a file chooser
 * @param handle Widget
 * @param title Title text
 * @return Widget
 */
MWDECL MwWidget MWAPI MwFileChooser(MwWidget handle, const char* title);

/*!
 * @brief Creates a file chooser
 * @param handle Widget
 * @param title Title text
 * @param dir Show directory only or not
 * @return Widget
 */
MWDECL MwWidget MWAPI MwFileChooserEx(MwWidget handle, const char* title, int dir);

#ifdef __cplusplus
}
#endif

#endif
