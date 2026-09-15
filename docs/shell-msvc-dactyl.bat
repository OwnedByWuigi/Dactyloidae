@ECHO OFF

SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

SET CYGWIN=
SET INCLUDE=
SET LIB=
IF NOT DEFINED MOZ_NO_RESET_PATH (
  SET PATH=%SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem;%SystemRoot%\System32\WindowsPowerShell\v1.0
)


SET MOZILLABUILD=%~dp0

SET WINCURVERKEY=HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion
REG QUERY "%WINCURVERKEY%" /v "ProgramFilesDir (x86)" >nul 2>nul
IF NOT ERRORLEVEL 1 (
  SET WIN64=1
) ELSE (
  SET WIN64=0
)

SET BUILD_ARCH=
:ASK_ARCH
ECHO.
ECHO What architecture will you build for?
ECHO   [1] 64-bit
ECHO   [2] 32-bit
ECHO.
SET /P ARCH_CHOICE="Enter 1 or 2: "

IF "!ARCH_CHOICE!" == "1" (
  SET BUILD_ARCH=x64
) ELSE IF "!ARCH_CHOICE!" == "2" (
  SET BUILD_ARCH=x86
) ELSE (
  ECHO Invalid choice. Please enter 1 or 2.
  GOTO :ASK_ARCH
)

ECHO Target architecture: !BUILD_ARCH!
SET VCVARSALL=
SET VS2019_BASE=%ProgramFiles(x86)%\Microsoft Visual Studio\2019

IF EXIST "!VS2019_BASE!\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  SET VCVARSALL=!VS2019_BASE!\Community\VC\Auxiliary\Build\vcvarsall.bat
  SET VS2019_EDITION=Community
) ELSE IF EXIST "!VS2019_BASE!\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
  SET VCVARSALL=!VS2019_BASE!\Professional\VC\Auxiliary\Build\vcvarsall.bat
  SET VS2019_EDITION=Professional
) ELSE IF EXIST "!VS2019_BASE!\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
  SET VCVARSALL=!VS2019_BASE!\Enterprise\VC\Auxiliary\Build\vcvarsall.bat
  SET VS2019_EDITION=Enterprise
) ELSE IF EXIST "!VS2019_BASE!\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
  SET VCVARSALL=!VS2019_BASE!\BuildTools\VC\Auxiliary\Build\vcvarsall.bat
  SET VS2019_EDITION=BuildTools
)

CALL "!VCVARSALL!" !BUILD_ARCH! -vcvars_ver=14.16
IF ERRORLEVEL 1 (
  ECHO             vcvarsall.bat failed.
  ECHO             Ensure the v141 + v141 xp toolset is installed via the
  ECHO             VS 2019 installer ^(Individual components tab^).
  pause
  exit
)
ECHO Build environment ready.

REM ============================================================
REM  Launch shell
REM ============================================================
START %MOZILLABUILD%msys\bin\mintty -e %MOZILLABUILD%msys\bin\console %MOZILLABUILD%msys\bin\bash --login
EXIT /B
