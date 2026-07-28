add_incdir2("-I/usr/X11R7/include -I/usr/pkg/include");
add_libdir(
    "-L/usr/X11R7/lib -Wl,-R/usr/X11R7/lib -L/usr/pkg/lib -Wl,-R/usr/pkg/lib");
use_backend("x11");
add_libs("-lpthread");

add_cflags("-DUSE_DBUS");

1;
