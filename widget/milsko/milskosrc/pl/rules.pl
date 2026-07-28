new_object("src/*.c");

my $gl_libs = "";
if (param_get("tiny")) {
    add_cflags("-Oz");
    if (defined($ENV{CC}) && grep(/^clang$/, $ENV{CC})) {
        add_libs("-flto=full");
        add_cflags("-flto=full");
    }
    add_cflags("-fdata-sections -ffunction-sections");
    add_libs("-Wl,--gc-sections");

    # these don't actually disable either, they're here for the final output.
    param_set("wayland", 0);
    param_set("dbus",    0);
}
if (param_get("classic-theme")) {
    add_cflags("-DUSE_CLASSIC_THEME");
}
if (grep(/^x11$/, @backends)) {
    add_cflags("-DUSE_X11");
    new_object("src/backend/x11.c");

    $gl_libs = "-lGL -lGLU";

    if (param_get("xrender")) {
        add_cflags("-DUSE_XRENDER");
    }
}

if (grep(/^gdi$/, @backends)) {
    add_cflags("-DUSE_GDI");
    new_object("src/backend/gdi.c");
    add_libs("-lgdi32");

    $gl_libs = "-lopengl32 -lglu32";
}

if (grep(/^classicmacos$/, @backends)) {

    #add_cflags("-DCLASSIC_MAC_OS");
    new_object("src/backend/classicmacos.c");

    $gl_libs = "";
}

if (grep(/^wayland$/, @backends)) {
    add_cflags("-DUSE_WAYLAND");
    new_object("src/backend/cairo.c");
    new_object("src/backend/wayland/wayland.c");
    new_object("src/backend/wayland/buffer.c");
    new_object("src/backend/wayland/interfaces.c");
    new_object("src/backend/wayland/region.c");

    if (param_get("opengl")) {
        add_cflags("-DWAYLAND_LOAD_OPENGL");
    }

    scan_wayland_protocol_core();
    scan_wayland_protocol("stable",   "xdg-shell",           "");
    scan_wayland_protocol("stable",   "viewporter",          "");
    scan_wayland_protocol("stable",   "tablet",              "-v2");
    scan_wayland_protocol("staging",  "xdg-toplevel-icon",   "-v1");
    scan_wayland_protocol("staging",  "cursor-shape",        "-v1");
    scan_wayland_protocol("unstable", "xdg-decoration",      "-unstable-v1");
    scan_wayland_protocol("unstable", "primary-selection",   "-unstable-v1");
    scan_wayland_protocol("unstable", "pointer-constraints", "-unstable-v1");
    scan_wayland_protocol("unstable", "relative-pointer",    "-unstable-v1");
    scan_wayland_protocol_from_file("wlr-layer-shell",
        "wlr-layer-shell-unstable-v1.xml");

    $gl_libs = "-lGL -lGLU";
}

if (not(param_get("dbus")) && not(param_get("tiny"))) {
    $cflags =~ s/( |^)-DUSE_DBUS( |$)/ /;
}

if (grep(/( |^)-DUSE_DBUS( |$)/, $cflags)) {
    add_cflags(`pkg-config --cflags dbus-1`);
}

if (grep(/^cocoa$/, @backends) || param_get("gnustep")) {
    add_cflags("-DUSE_COCOA");
    new_object("src/backend/cocoa.m");

    if (param_get("gnustep")) {
        add_incdir("-I" . `gnustep-config --variable=GNUSTEP_SYSTEM_HEADERS`);
        add_libdir("-L" . `gnustep-config --variable=GNUSTEP_SYSTEM_LIBRARIES`);
        add_cflags("-fobjc-runtime=gnustep-2.0 -fblocks");
        add_ldflags("-lobjc -lgnustep-base -lm -lgnustep-gui");
    }

    if (param_get("opengl")) {
        new_object("src/widget/opengl_cocoa.m");
    }

# John Apple my man literally everybody and their mother who knows what opengl is knows its deprecated on macos and i'm not having you spam the console with this
    add_cflags("-DGL_SILENCE_DEPRECATION");

    $gl_libs = "-framework OpenGL";
}

if (grep(/^haiku$/, @backends)) {
    add_cflags("-DUSE_HAIKU");
    new_object("src/backend/haiku.cc");
    add_libs("-lbe");
}

if (param_get("stb-image")) {
    add_cflags("-DUSE_STB_IMAGE");
    if (param_get("tiny")) {
        add_cflags("-DSTB_IMAGE_STATIC");
        add_cflags("-DSTB_IMAGE_IMPLEMENTATION");
    }
}
else {
    new_object("external/libjpeg/src/*.c");
    new_object("external/libpng/src/*.c");
    add_incdir("-Iexternal/libjpeg/include -Iexternal/libpng/include");
}
if (param_get("stb-truetype")) {
    add_cflags("-DUSE_STB_TRUETYPE");
}
if (param_get("allow-sloppy-focus")) {
    add_cflags("-DALLOW_SLOPPY_FOCUS");
}

add_incdir("-Iexternal/libz/include");

if (param_get("freetype2")) {
    add_cflags("-DUSE_FREETYPE2");
    if (!$cross) {
        add_cflags(`pkg-config --cflags freetype2`);
    }
}

if (param_get("gdi-text")) {
    add_cflags("-DUSE_GDI_TEXT");
}

if (param_get("vulkan") && param_get("vulkan-string-helper")) {
    add_cflags("-DHAS_VK_ENUM_STRING_HELPER");
}

new_object("src/icon/*.c");
new_object("src/cursor/*.c");
new_object("src/text/*.c");
new_object("src/text/font/*.c");

new_object("src/widget/box.c");
new_object("src/widget/button.c");
new_object("src/widget/calendar.c");
new_object("src/widget/checkbox.c");
new_object("src/widget/combobox.c");
new_object("src/widget/entry.c");
new_object("src/widget/frame.c");
new_object("src/widget/image.c");
new_object("src/widget/label.c");
new_object("src/widget/listbox.c");
new_object("src/widget/menu.c");
new_object("src/widget/numberentry.c");
new_object("src/widget/progressbar.c");
new_object("src/widget/radiobox.c");
new_object("src/widget/scrollbar.c");
new_object("src/widget/separator.c");
new_object("src/widget/submenu.c");
new_object("src/widget/subwindow.c");
new_object("src/widget/tab.c");
new_object("src/widget/table.c");
new_object("src/widget/treeview.c");
new_object("src/widget/viewport.c");
new_object("src/widget/window.c");

new_object("src/widget/opengl.c");
new_object("src/widget/vulkan.c");

if (param_get("opengl")) {
    add_cflags("-DMW_OPENGL");
}
if (param_get("vulkan")) {
    add_cflags("-DMW_VULKAN");
}

new_object("src/dialog/*.c");

new_object("src/abstract/*.c");

new_object("external/*.c");
new_object("external/libz/src/*.c");

new_example("examples/basic/example");
new_example("examples/basic/rotate");
new_example("examples/basic/image");
new_example("examples/basic/scrollbar");
new_example("examples/basic/checkbox");
new_example("examples/basic/radiobox");
new_example("examples/basic/messagebox");
new_example("examples/basic/viewport");
new_example("examples/basic/listbox");
new_example("examples/basic/progressbar");
new_example("examples/basic/colorpicker");
new_example("examples/basic/combobox");
new_example("examples/basic/treeview");
new_example("examples/basic/box");
new_example("examples/basic/clipboard");
new_example("examples/basic/sevensegment");
new_example("examples/basic/calculator");
new_example("examples/basic/filechooser");
new_example("examples/basic/periodic");
new_example("examples/basic/subwindow");
new_example("examples/basic/tab");

if (param_get("opengl")) {
    new_example("examples/gldemos/boing",    $gl_libs);
    new_example("examples/gldemos/clock",    $gl_libs);
    new_example("examples/gldemos/cube",     $gl_libs);
    new_example("examples/gldemos/gears",    $gl_libs);
    new_example("examples/gldemos/triangle", $gl_libs);
    new_example("examples/gldemos/tripaint", $gl_libs);
}

if (param_get("vulkan")) {
    new_example("examples/vkdemos/vulkan");
}

1;
