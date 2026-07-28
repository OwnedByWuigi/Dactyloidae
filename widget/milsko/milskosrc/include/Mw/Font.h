/*!
 * @file Mw/Font.h
 * @brief Font
 */
#ifndef __MW_FONT_H__
#define __MW_FONT_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Loads a TTF Font
 * @param data Data
 * @param size Data size
 * @return Font handle
 */
MWDECL void* MWAPI MwFontLoad(unsigned char* data, unsigned int size);

/*!
 * @brief Frees a font handle
 * @param handle Handle
 */
MWDECL void MWAPI MwFontFree(void* handle);

#ifdef __cplusplus
}
#endif

#endif
