my @enabled_backends = ();

add_incdir2("-I/usr/local/include");
add_libdir("-L/usr/local/lib -Wl,-R/usr/local/lib");

if (param_get("wayland") && not(param_get("tiny"))) {
    push(@enabled_backends, "wayland");
}
if (param_get("x11")) {
    push(@enabled_backends, "x11");
}

use_backend(@enabled_backends);

add_libs("-lpthread");

add_cflags("-DUSE_DBUS");

1;
