
Greetings - Welcome to the Milsko GUI Toolkit (Version 1.4) 

  This document contains a brief summary of the contents of this source 
distributions and building instructions for Milsko GUI Toolkit. 

                                  Requirements

  Milsko requires either 
  * A Windows environment with GDI (so anything NT or 9x) 
  * A Unix-like environment with Wayland and/or X11 for runtime. 

  To build Milsko for Windows, you must have one of following compilers: 
  * Visual C++ 6.0 or newer 
  * Borland C++ 5.5 or newer 
  * Open Watcom 2.0 or newer 
  * MinGW-w64 

  and for Unix-like and MacOS: 
  * GNU C Compiler 
  * Clang 


                                    Contents

            At the top level of this hierarchy there are seven directories:

            src -      Contains the source for Milsko library

            include -  Contains the headers for Milsko

            doc -      Contains the documentation for Milsko

            external - Contains the external dependency that Milsko uses

            resource - Contains the resources used by maintainers

            tools -    Contains the tools used by maintainers


                                Building Milsko

  Building Milsko depends on the platform you use, and the compiler you use. 

A. Visual C++
-------------

1) Run `nmake -f NTMakefile'. 

B. Borland C++
--------------

1) Run `make -f BorMakefile'. 

C. Open Watcom
--------------

1) Run `wmake -f WatMakefile'. 

D. MinGW-w64/GCC/Clang
----------------------

1) Determine if you need Vulkan and/or OpenGL. 

2) Either: 
   a.) Run `./configure'. For help, run `./configure --help'. 
   b.) Use CMake; i.e. `cmake -B build`. (if contributing, name the build 
folder build as that's what's included in the .gitignore) 

3) Run `make'. 

 -- Nishi  (nishi@nishi.boats) 
