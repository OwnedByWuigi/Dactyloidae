my @enabled_backends = ();

if (param_get("wayland") && not(param_get("tiny"))) {
    push(@enabled_backends, "wayland");
}
if (param_get("x11")) {
    push(@enabled_backends, "x11");
}

add_cflags("-DUSE_DBUS");

use_backend(@enabled_backends);

1;
