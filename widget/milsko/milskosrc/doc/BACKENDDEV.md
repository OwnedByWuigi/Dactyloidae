# Backend dev

The basic process of creating a new Milsko backend is

- Adding a new include header into `include/Mw/LowLevel`. It must include;
  - `struct _MwLLBackendName`: Any details about a window or a subwidget
  - `struct _MwLLBackendNameColor`: An 8-bit RGB Color. You'll probably never impl this, this is for platforms like X11/GDI where they have a secondary color representation that can be stored.
  - `struct _MwLLBackendNamePixmap`: Self-explanatory. This is also used for the soft gradients in the modern theme.
  - `MwLLBackendNameCallInitImpl(void)` called by `MwLibraryInit` to address and platform globals.
  - Note that all of those structs are actually unions. They should all start with their `MwLLCommon...` equivalants.
- Fill out `Mw/LowLevel.h` appopriately, adding an include to your new file and the structs in the appropriate unions. These need to be guarded by a `USE_BACKENDNAME` macro which you'll define later when you modify the build system.
- Creating a new impl file in `src/backend`
  - Backends actually return a table of function pointers that you have to implement with static functions. Your file should end with `#include "call.c"` and then `CALL(BackendName);`, and somewhere you should have the impl for `MwLLBackendNameCallInitImpl(void)`.
  - Prefer to keep the backend file to one file if you can, though obviously exceptions like the Wayland backend exist because it's simply too complex to sanely stuff into one file.
- Add `MwLLBackendNameCallInitImpl` to the `MwLibraryInit` impl in `src/core.c`.
- Add a new enum variation to `MwLLBackends` in `Mw/Include/LowLevel.h`; your impl of `MwLLCreateImpl` will set `common.type` to this so the user can check which backend they're on (important for platforms with multiple supported backends).
- Either
  - a.) Modify `CMakeLists.txt` for your platform.
  - b.) Add a new perl file in `pl/ostype` that `./configure` can reference, and modify `pl/rules.pl`; If you don't know how to use perl it's fine, just copy a file like the `Linux.pl` or `Windows.pl`, the functions you'll have to change for your platform are self explanatory. Ensure it ends with `1;`.
  - c.) Both.
  - Both are recommended, but some platforms like Classic Mac OS can't be used with the `./configure` script, and that's fine.

The functions you'll have to impl are too many to list here so you should start by just copying `x11.c` and then replacing all the function impls with your own.

\*\*Pay attention to some of these functions' comments in LowLevel.h as they might not be as you expect; for example, `MwLLSetDarkThemeImpl` is for setting the accent color to dark theme on platforms like modern Windows (dark theme itself is set with `MwSetInteger(handle, MwNdarktheme, 1)`).
