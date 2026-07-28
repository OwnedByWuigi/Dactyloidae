/*!
 * @file Mw/Resource/Cursor.h
 * @brief Cursor externs
 */

#ifndef __MW_RESOURCE_CURSOR_H__
#define __MW_RESOURCE_CURSOR_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Default cursor
 */
MWDECL MwCursor MwCursorDefault;

/*!
 * @brief Default cursor mask
 */
MWDECL MwCursor MwCursorDefaultMask;

/*!
 * @brief Arrow cursor
 */
MWDECL MwCursor MwCursorArrow;

/*!
 * @brief Arrow cursor mask
 */
MWDECL MwCursor MwCursorArrowMask;

/*!
 * @brief Cross cursor
 */
MWDECL MwCursor MwCursorCross;

/*!
 * @brief Cross cursor mask
 */
MWDECL MwCursor MwCursorCrossMask;

/*!
 * @brief Text cursor
 */
MWDECL MwCursor MwCursorText;

/*!
 * @brief Text cursor mask
 */
MWDECL MwCursor MwCursorTextMask;

/*!
 * @brief Hidden cursor
 */
MWDECL MwCursor MwCursorHidden;

/*!
 * @brief Hidden cursor mask
 */
MWDECL MwCursor MwCursorHiddenMask;

#ifdef __cplusplus
}
#endif

#endif
