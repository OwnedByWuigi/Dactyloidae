add_incdir2("-I/usr/local/include");
add_libdir("-L/usr/local/lib -Wl,-R/usr/local/lib");
use_backend("x11");
add_libs("-lpthread");

add_cflags("-DUSE_DBUS");

1;
