/*!
 * @file Mw/TrueType.h
 * @brief TrueType utilities
 */
#ifndef __MW_TRUETYPE_H__
#define __MW_TRUETYPE_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Get the TrueType font information
 * @param data Font data
 * @param size Font size
 * @return Information
 */
MWDECL MwTTFInfo MwTTFGetInfo(unsigned char* data, int size);

/*!
 * @brief Free the TrueType font information
 * @param info Information
 */
MWDECL void MwTTFFreeInfo(MwTTFInfo info);

#ifdef __cplusplus
}
#endif

#endif
