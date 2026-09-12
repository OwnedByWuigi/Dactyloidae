dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CLANG_CL_PGO], [
if test -n "$CLANG_CL"; then
    dnl Override the MSVC/GCC flags: clang-cl uses LLVM instrumentation.
    PROFILE_GEN_CFLAGS="-clang:-fprofile-instr-generate"
    PROFILE_USE_CFLAGS='-clang:-fprofile-instr-use="$(DEPTH)/merged.profdata"'
    PROFILE_GEN_LDFLAGS=
    PROFILE_USE_LDFLAGS=

    if test -n "$MOZ_PGO"; then
        AC_PATH_PROG(LLVM_PROFDATA, llvm-profdata.exe)
        if test -z "$LLVM_PROFDATA"; then
            AC_MSG_ERROR([clang-cl PGO requires llvm-profdata from the compiler's LLVM installation on PATH])
        fi
        dnl The training harness uses native Windows Python, not an MSYS shell.
        LLVM_PROFDATA="$(cd "$(dirname "$LLVM_PROFDATA")" && pwd -W)/$(basename "$LLVM_PROFDATA")"
        dnl clang-cl emits a default-library directive for its profile runtime.
        dnl We invoke the linker directly, so supply the runtime search path.
        clang_resource_dir=`$CC -print-resource-dir | tr '\\' '/'`
        if test ! -d "$clang_resource_dir/lib/windows"; then
            AC_MSG_ERROR([clang-cl PGO requires the Windows compiler-rt profile runtime])
        fi
        PROFILE_GEN_LDFLAGS="-LIBPATH:\"$clang_resource_dir/lib/windows\""
    fi
fi
AC_SUBST(LLVM_PROFDATA)
])
