/*!
 * @file Mw/BaseTypes.h
 * @brief Base type definitions
 */
#ifndef __MW_BASETYPES_H__
#define __MW_BASETYPES_H__

typedef struct _MwPoint	    MwPoint;
typedef struct _MwRect	    MwRect;
typedef struct _MwSizeHints MwSizeHints;
typedef struct _MwFont	    MwFont;
typedef struct _MwCursor    MwCursor;
typedef struct _MwRGB	    MwRGB;

typedef unsigned char MwBool;
#define MwTRUE ((MwBool)1)
#define MwFALSE ((MwBool)0)

#if __STDC_VERSION__ >= 199901L
#include <stdint.h>

typedef int64_t	 MwI64;
typedef uint64_t MwU64;

typedef int32_t	 MwI32;
typedef uint32_t MwU32;

typedef int16_t	 MwI16;
typedef uint16_t MwU16;

typedef int8_t	MwI8;
typedef uint8_t MwU8;

#define MW_OTHER_TYPES_DEFINED
#elif !defined(__STRICT_ANSI__) && (defined(__GNUC__) || defined(__clang__))
typedef long long	   MwI64;
typedef unsigned long long MwU64;
#elif defined(_MSC_VER) || defined(__BORLANDC__) || defined(__WATCOMC__)
typedef __int64		 MwI64;
typedef unsigned __int64 MwU64;

typedef __int32		 MwI32;
typedef unsigned __int32 MwU32;

typedef __int16		 MwI16;
typedef unsigned __int16 MwU16;

typedef __int8		MwI8;
typedef unsigned __int8 MwU8;

#define MW_OTHER_TYPES_DEFINED
#else
/* out of hope */
typedef long	      MwI64;
typedef unsigned long MwU64;
#endif

#ifdef MW_OTHER_TYPES_DEFINED
#undef MW_OTHER_TYPES_DEFINED
#else
typedef int	     MwI32;
typedef unsigned int MwU32;

typedef short	       MwI16;
typedef unsigned short MwU16;

typedef signed char   MwI8;
typedef unsigned char MwU8;
#endif

typedef MwI64 MwOffset;

struct _MwPoint {
	int x;
	int y;
};

struct _MwRect {
	int x;
	int y;
	int width;
	int height;
};

struct _MwSizeHints {
	int min_width;
	int min_height;
	int max_width;
	int max_height;
};

#define MwCursorDataHeight 16
struct _MwCursor {
	int	     width;
	int	     height;
	int	     x;
	int	     y;
	unsigned int data[MwCursorDataHeight];
};

struct _MwFont {
	int	      left;
	int	      top;
	unsigned char data[16];
};

struct _MwRGB {
	MwU16 red;
	MwU16 green;
	MwU16 blue;
};

#endif
