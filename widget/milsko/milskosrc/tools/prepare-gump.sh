#!/bin/sh
for i in linux win32 win64 doxygen; do
	rm -rf ../milsko-$i
	cp -rf . ../milsko-$i
	rm -rf ../milsko-$i/.svn
done
