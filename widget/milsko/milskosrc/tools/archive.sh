#!/bin/sh
MAKE=make
if which gmake >/dev/null 2>&1; then
	MAKE=gmake
fi
VERSION=`cat include/Mw/Version.h | grep -E 'MwVERSION' | grep -oE '"[^"]+"' | sed 's/"//g'`

$MAKE clean
OLD=`pwd`
rm -rf /tmp/milsko-$VERSION *.tar.gz
mkdir -p /tmp/milsko-$VERSION

cp -rf * /tmp/milsko-$VERSION/

cd /tmp

tar czvf /tmp/milsko-$VERSION.tar.gz milsko-$VERSION

rm -rf /tmp/milsko-$VERSION
mv /tmp/milsko-$VERSION.tar.gz $OLD/
