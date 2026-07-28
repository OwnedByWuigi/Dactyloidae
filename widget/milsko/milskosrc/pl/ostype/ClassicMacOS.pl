use_backend("classicmacos");

print(
"Compiling for Classic Mac OS via ./configure currently outputs a broken executable and it only supports PowerPC. Help is wanted. In the mean time, use CMake."
);

$cc = $ENV{RETRO68_TOOLCHAIN_PATH} . "/bin/powerpc-apple-macos-gcc";
$ar = $ENV{RETRO68_TOOLCHAIN_PATH} . "/bin/powerpc-apple-macos-ar";
add_cflags("-DCLASSIC_MAC_OS");
add_cflags("-DSTBI_NO_THREAD_LOCALS");

1;
