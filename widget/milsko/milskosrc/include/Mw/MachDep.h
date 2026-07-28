/*!
 * @file Mw/MachDep.h
 * @brief Machine dependent headers and macros
 */
#ifndef __MW_MACHDEP_H__
#define __MW_MACHDEP_H__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <assert.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199409L
#include <wchar.h>
#define MW_HAS_WCHAR
#endif
#ifdef _WIN32
#include <direct.h>

#ifdef _MILSKO
#include <windows.h>
#include <mmsystem.h>
#ifndef GWLP_USERDATA
#define GWLP_USERDATA GWL_USERDATA
#define GWLP_WNDPROC GWL_WNDPROC
#define GCLP_HICON GCL_HICON
#define GCLP_HCURSOR GCL_HCURSOR
#define SetWindowLongPtr SetWindowLong
#define GetWindowLongPtr GetWindowLong
#define SetClassLongPtr SetClassLong
#define GetClassLongPtr GetClassLong
#endif
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020a
#define GET_WHEEL_DELTA_WPARAM(x) ((short)HIWORD(x))
#endif
#endif
#elif defined(CLASSIC_MAC_OS)
#include <unistd.h>
#include <MacTypes.h>
#include <CFBundle.h>
#include <CodeFragments.h>
#include <Timer.h>
#include <MacWindows.h>
#include <TextEdit.h>
#include <Dialogs.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#endif

#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
#include <dirent.h>
#include <dlfcn.h>
#endif

#ifdef __HAIKU__
#ifdef __cplusplus
#include <Window.h>
#include <View.h>
#include <Screen.h>
#include <Application.h>
#include <Locker.h>
#include <Picture.h>
#include <Bitmap.h>
#endif
#include <OS.h>
#endif

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#ifdef USE_DBUS
#include <dbus/dbus.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265
#endif

/* Windows */
#if defined(_MILSKO) && defined(_MILSKO_BUILD) && defined(_WIN32)
#define MWDECL extern __declspec(dllexport)
#elif defined(_WIN32)
#define MWDECL extern __declspec(dllimport)
/* GCC/Clang */
/* First check if we have __has_attribute and can check for the existence of visibility  */
#elif defined(__has_attribute)
#if __has_attribute(visibility)
#define MWDECL extern __attribute__((visibility("default")))
#else
#define MWDECL extern
#endif
/* It might be that we're on an old version of GCC (like the one Apple ships with older versions of Mac OS).
 * If we're on gcc 3 or above, assume it's there
 */
#elif __GNUC__ >= 3
#define MWDECL extern __attribute__((visibility("default")))
#else
/* all else fails */
#define MWDECL extern
#endif

#if defined(_WIN32)
#define MWAPI __cdecl
#else
#define MWAPI
#endif

#define MwInline static __inline

#endif
