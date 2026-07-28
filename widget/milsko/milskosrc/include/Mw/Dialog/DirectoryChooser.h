/*!
 * @file Mw/Dialog/DirectoryChooser.h
 * @brief Directory chooser
 */
#ifndef __MW_DIRECTORYCHOOSER_H__
#define __MW_DIRECTORYCHOOSER_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Creates a directory chooser
 * @param handle Widget
 * @param title Title text
 * @return Widget
 */
MWDECL MwWidget MWAPI MwDirectoryChooser(MwWidget handle, const char* title);

#ifdef __cplusplus
}
#endif

#endif
