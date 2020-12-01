#!/bin/bash
set -e

SCRIPT="$(readlink -f $0)"
THIS="$(dirname $SCRIPT)"
cd $THIS
ARCH=arm DISTRO=14 $THIS/../kernel_compile.sh $@
