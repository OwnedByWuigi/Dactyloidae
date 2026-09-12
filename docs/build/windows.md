# How to compile Dactyloidae

Step 1. Install Git for Windows (win7 users should install 2.46.2)

Step 2. Enable .NET 3.5 in optionalfeatures.exe\
NOTE: Win8 users may need to use this https://github.com/abbodi1406/dotNetFx35W81

Step 3. Install VS 2019 Community with Desktop Developement with C++ and "C++ Windows XP Support for VS 2017 (v141)\
and MSVC v141 - VS 2017 C++ x64/x86 build tools (v14.16)\
Make sure you also get 10.0.19041 sdk

Step 4. Get the DirectX SDK https://www.microsoft.com/en-us/download/details.aspx?id=6812\
(NOTE if you get a S2023 error in the end you can safely ignore it)

Step 5. Get and Install MozillaBuild from here https://ftp.mozilla.org/pub/mozilla/libraries/win32/MozillaBuildSetup-3.2.exe

Step 6. Copy shell-msvc-dactyl.bat from docs folder to your mozilla-build directory (usually at C:\mozilla-build)

Step 7. Run shell-msvc-dactyl.bat from the mozilla-build directory.

Step 8. Select the architecture that you want to build Dactyloidae for

Step 9. CD to the Repo (msys uses unix style dirs, c:\ is /c/ for example)

Step 10. Run ./build-msvc-win

Step 10.5. After the build, if you really feel fancy, you can test your build before packaging it with  ``./mach run``

Step 11. Run ``./mach installer``  (if you want to build an installer)
