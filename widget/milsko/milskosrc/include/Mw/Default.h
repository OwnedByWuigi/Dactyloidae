/*!
 * @file Mw/Default.h
 * @brief Default properties
 */
#ifndef __MW_DEFAULT_H__
#define __MW_DEFAULT_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Default background color
 */
MWDECL const char* MwDefaultBackground;

/*!
 * @brief Default foreground color
 */
MWDECL const char* MwDefaultForeground;

/*!
 * @brief Default sub background color
 */
MWDECL const char* MwDefaultSubBackground;

/*!
 * @brief Default sub foreground color
 */
MWDECL const char* MwDefaultSubForeground;

/*!
 * @brief Default title background color
 */
MWDECL const char* MwDefaultTitleBackground;

/*!
 * @brief Default title foreground color
 */
MWDECL const char* MwDefaultTitleForeground;

/*!
 * @brief Default dark theme background color
 */
MWDECL const char* MwDefaultDarkBackground;

/*!
 * @brief Default dark theme foreground color
 */
MWDECL const char* MwDefaultDarkForeground;

/*!
 * @brief Default dark theme sub background color
 */
MWDECL const char* MwDefaultDarkSubBackground;

/*!
 * @brief Default dark theme sub foreground color
 */
MWDECL const char* MwDefaultDarkSubForeground;

/*!
 * @brief Default dark theme title background color
 */
MWDECL const char* MwDefaultDarkTitleBackground;

/*!
 * @brief Default dark theme title foreground color
 */
MWDECL const char* MwDefaultDarkTitleForeground;

/*!
 * @brief Default shadow difference
 */
MWDECL const int MwDefaultShadow;

/*!
 * @brief Gets default border width
 * @param handle Widget
 */
MWDECL int MWAPI MwDefaultBorderWidth(MwWidget handle);

/*!
 * @brief Gets default thin border width
 * @param handle Widget
 */
MWDECL int MwDefaultThinBorderWidth(MwWidget handle);

#ifdef __cplusplus
}
#endif

#endif
