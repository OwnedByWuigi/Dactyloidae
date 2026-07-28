/*!
 * @file Mw/Dialog/MessageBox.h
 * @brief Message box
 */
#ifndef __MW_MESSAGEBOX_H__
#define __MW_MESSAGEBOX_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Creates a message box
 * @param handle Widget
 * @param text Text
 * @param title Title text
 * @param flag Flag
 * @return Widget
 */
MWDECL MwWidget MWAPI MwMessageBox(MwWidget handle, const char* text, const char* title, unsigned int flag);

/*!
 * @brief Gets a child of the message box
 * @param handle Widget
 * @param child Child
 */
MWDECL MwWidget MWAPI MwMessageBoxGetChild(MwWidget handle, int child);

#ifdef __cplusplus
}
#endif

#endif
