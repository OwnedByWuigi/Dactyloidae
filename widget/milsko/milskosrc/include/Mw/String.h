/*!
 * @file Mw/String.h
 * @brief String utilities
 */
#ifndef __MW_STRING_H__
#define __MW_STRING_H__

#include <Mw/MachDep.h>
#include <Mw/TypeDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Duplicates a string
 * @param str String
 * @return String
 */
MWDECL char* MWAPI MwStringDuplicate(const char* str);

/*!
 * @brief Concatenates 2 strings
 * @param str1 String
 * @param str2 String
 * @return String
 */
MWDECL char* MWAPI MwStringConcat(const char* str1, const char* str2);

/*!
 * @brief Converts size to string
 * @param out Output
 * @param size Size
 */
MWDECL void MWAPI MwStringSize(char* out, MwOffset size);

/*!
 * @brief Converts time to string
 * @param out Output
 * @param t Time
 */
MWDECL void MWAPI MwStringTime(char* out, time_t t);

/*!
 * @brief Prints up to n characters into a bufferMwStringPrintIntoBuffer.
 * @note On compilers that support C99, this uses vsnprintf to safely print into a buffer; on compilers that don't, vsprintf is used.
 * @param out Buffer
 * @param size Max size
 */
MWDECL void MWAPI MwStringPrintIntoBuffer(char* out, MwU32 size, const char* fmt, ...);

/*!
 * @brief Check if the given key is UTF8
 * @param key Input
 * @return whether its utf8
 */
MWDECL MwBool MWAPI MwStringIsKeyUTF8(MwU32 key);

#ifdef __cplusplus
}
#endif

#endif
