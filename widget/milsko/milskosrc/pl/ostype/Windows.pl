$library_prefix    = "";
$library_suffix    = ".dll";
$executable_suffix = ".exe";
$math              = "";
add_ldflags("-Wl,--out-implib,src/libMw.dll.a -static-libgcc");
use_backend("gdi");
add_libs("-lwinmm");

1;
