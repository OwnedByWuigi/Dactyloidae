# Tiny Config

Milsko's configure script supports the `--tiny` option for building the tiniest possible Milsko library (~400kb as of writing). Currently, what it does is:

- Add `-Oz` to the cflags
- (clang only) add `-flto=thin` to the cflags and linker flags
- Build stb-image with `STB_IMAGE_STATIC` (this is why you get warnings with stb_image when you use it)
- (Linux) Disable the Wayland backend
- (Linux) Disable dbus integration
