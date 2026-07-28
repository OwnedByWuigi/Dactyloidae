/*!
 * @file Mw/Resource/Font.h
 * @brief Default font resources
 */
#ifndef __MW_RESOURCE_FONT_H__
#define __MW_RESOURCE_FONT_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Default font data
 */
MWDECL MwFont MwFontData[];

/*!
 * @brief Default bold font data
 */
MWDECL MwFont MwBoldFontData[];

/*!
 * @brief Default TTF font data
 */
MWDECL unsigned char MwTTFData[];

/*!
 * @brief Default TTF font size
 */
MWDECL unsigned int MwTTFDataSize;

/*!
 * @brief Default bold TTF font data
 */
MWDECL unsigned char MwBoldTTFData[];

/*!
 * @brief Default bold TTF font size
 */
MWDECL unsigned int MwBoldTTFDataSize;

/*!
 * @brief Default monospace TTF font data
 */
MWDECL unsigned char MwMonospaceTTFData[];

/*!
 * @brief Default monospace TTF font size
 */
MWDECL unsigned int MwMonospaceTTFDataSize;

/*!
 * @brief Default bold monospace TTF font data
 */
MWDECL unsigned char MwBoldMonospaceTTFData[];

/*!
 * @brief Default bold monospace TTF font size
 */
MWDECL unsigned int MwBoldMonospaceTTFDataSize;

#ifdef __cplusplus
}
#endif

#endif
