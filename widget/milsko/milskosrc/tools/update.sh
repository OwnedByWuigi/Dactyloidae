#!/bin/sh
MAKE=make
if which gmake >/dev/null 2>&1; then
	MAKE=gmake
fi
./tools/genmk.pl
$MAKE format
