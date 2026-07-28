/*!
 * @file Mw/Abstract/Directory.h
 * @brief Directory functions
 */
#ifndef __MW_ABSTRACT_DIRECTORY_H__
#define __MW_ABSTRACT_DIRECTORY_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Opens a directory
 * @param path Path
 * @return Handle
 */
MWDECL void* MWAPI MwDirectoryOpen(const char* path);

/*!
 * @brief Closes a directory
 * @param handle Handle
 */
MWDECL void MWAPI MwDirectoryClose(void* handle);

/*!
 * @brief Reads a directory
 * @param handle Handle
 * @return Directory entry
 */
MWDECL MwDirectoryEntry* MWAPI MwDirectoryRead(void* handle);

/*!
 * @brief Frees a directory entry
 * @param entry Entry
 */
MWDECL void MWAPI MwDirectoryFreeEntry(MwDirectoryEntry* entry);

/*!
 * @brief Gets a current directory
 * @param Directory
 */
MWDECL char* MWAPI MwDirectoryCurrent(void);

/*!
 * @brief Joins 2 paths
 * @param a Path
 * @param b Path
 * @return Path
 */
MWDECL char* MWAPI MwDirectoryJoin(char* a, char* b);

#ifdef __cplusplus
}
#endif

#endif
