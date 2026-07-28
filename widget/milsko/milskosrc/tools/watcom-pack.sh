#!/bin/sh
set -e
wmake -f WatMakefile
rm -rf milsko
mkdir -p milsko-examples
cp src/Mw.dll milsko-examples/
cp examples/basic/*.exe milsko-examples/
for i in examples/gldemos/*.exe; do
	cp $i milsko-examples/gl`echo $i | rev | cut -d/ -f1 | rev`
done
rm -f milsko-examples.zip
cp resource/logo/logo.png examples/picture.jpg examples/picture.png milsko-examples/
zip -rv milsko-examples.zip milsko-examples
