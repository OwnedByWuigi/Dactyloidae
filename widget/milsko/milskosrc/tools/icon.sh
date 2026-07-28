#!/bin/sh
echo '/*!' > include/Mw/Resource/Icon.h
echo ' * @file Mw/Resource/Icon.h' >> include/Mw/Resource/Icon.h
echo ' * @brief Icon' >> include/Mw/Resource/Icon.h
echo ' */' >> include/Mw/Resource/Icon.h
echo '#ifndef __MW_RESOURCE_ICON_H__' >> include/Mw/Resource/Icon.h
echo '#define __MW_RESOURCE_ICON_H__' >> include/Mw/Resource/Icon.h
echo '' >> include/Mw/Resource/Icon.h
echo '#include <Mw/MachDep.h>' >> include/Mw/Resource/Icon.h
echo '' >> include/Mw/Resource/Icon.h
echo '#ifdef __cplusplus' >> include/Mw/Resource/Icon.h
echo 'extern "C" {' >> include/Mw/Resource/Icon.h
echo '#endif' >> include/Mw/Resource/Icon.h
echo '' >> include/Mw/Resource/Icon.h
for i in resource/icon/*.png; do
	LOWER=`echo $i | rev | cut -d"/" -f1 | rev | cut -d"." -f1`
	OUT=src/icon/$LOWER.c
	NAME=`echo -n $LOWER | perl -e 'my $l = <>;$l =~ s/^(.)/uc($1)/e;print($l);'`
	NAME=MwIcon$NAME
	GEO=`convert $i json:- 2>/dev/null | jq -r '(.[0].image.geometry.width | tostring) + "x" + (.[0].image.geometry.height | tostring)'`
	WIDTH=`echo $GEO | cut -dx -f1`
	HEIGHT=`echo $GEO | cut -dx -f2`
	echo '#include <Mw/Milsko.h>' > $OUT
	echo >> $OUT
	echo "MwU32 $NAME[] = {" >> $OUT
	echo "	($WIDTH << 16) | $HEIGHT," >> $OUT
	convert $i txt:- 2>/dev/null | grep -oE '#[0-9a-fA-F]{8}' | sed 's/^#/	0x/' | sed -E 's/$/,/' >> $OUT
	echo "	0" >> $OUT
	echo "};" >> $OUT
	echo $NAME
done | while read a; do
	echo $a
	echo '/*!' >> include/Mw/Resource/Icon.h
	echo " * @brief `echo $a | sed s/MwIcon//` icon" >> include/Mw/Resource/Icon.h
	echo ' */' >> include/Mw/Resource/Icon.h
	echo "MWDECL MwU32 $a[];" >> include/Mw/Resource/Icon.h
	echo '' >> include/Mw/Resource/Icon.h
done
echo '' >> include/Mw/Resource/Icon.h
echo '#ifdef __cplusplus' >> include/Mw/Resource/Icon.h
echo '}' >> include/Mw/Resource/Icon.h
echo '#endif' >> include/Mw/Resource/Icon.h
echo '' >> include/Mw/Resource/Icon.h
echo '#endif' >> include/Mw/Resource/Icon.h
