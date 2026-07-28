#!/bin/sh
for out in src/font/ttf.c src/font/boldttf.c; do
	echo '#include <Mw/Milsko.h>' > $out
	echo '' >> $out
	echo '#if defined(USE_STB_TRUETYPE) || defined(USE_FREETYPE2)' >> $out
done
xxd -n MwTTFData -i resource/font/IBMPlexMono.ttf | sed s/_len/Size/ >> src/font/ttf.c
xxd -n MwBoldTTFData -i resource/font/IBMPlexMono-Bold.ttf | sed s/_len/Size/ >> src/font/boldttf.c
for out in src/font/ttf.c src/font/boldttf.c; do
	echo '#endif' >> $out
done
