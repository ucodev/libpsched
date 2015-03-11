#!/bin/sh

## Detect compiler ##
. ./lib/sh/compiler.inc

## Detect architecture ##
. ./lib/sh/arch.inc

## Target options ##
rm -f .ecflags
rm -f .elflags

if [ `uname` = "OpenBSD" ]; then
	printf -- "-DCONFIG_NO_SIGEVENT=1 -DCONFIG_USE_TIMER_UL=1 " > .ecflags
else
	printf -- "-lrt " > .elflags
fi

## Options ##
if [ $# -eq 1 ]; then
	if [ "${1}" == "fsma" ]; then
		printf -- "-DUSE_LIBFSMA " >> .ecflags
		printf -- "-lfsma " >> .elflags
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

