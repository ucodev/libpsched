#!/bin/sh

## Detect compiler ##
. ./lib/sh/compiler.inc

## Detect architecture ##
. ./lib/sh/arch.inc

## Target options ##
if [ `uname` = "OpenBSD" ]; then
	echo "-DCONFIG_NO_SIGEVENT=1 -DCONFIG_USE_TIMER_UL=1" > .defines
fi

## Options ##
if [ $# -eq 1 ]; then
	if [ "${1}" == "fsma" ]; then
		echo "-DUSE_LIBFSMA" > .ecflags
		echo "-lfsma" > .elflags
	fi
else
	touch .ecflags
	touch .elflags
fi

## Build ##
make

if [ $? -ne 0 ]; then
	echo "Build failed."
	exit 1
fi

touch .done

echo "Build completed."

exit 0

