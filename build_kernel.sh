#!/bin/bash

# Do not touch this.

source anykernel/build_helper.sh

[[ $1 == 'n' || $1 == 'dn'  ]] && nontreble
[[ $2 == 's'                ]] && noslmk

make O=out ARCH=arm64 final_defconfig

[[ $1 == 'n' || $1 == 't'   ]] && pcmake -j$(nproc --all)
[[ $1 == 'dt' || $1 == 'dn' ]] && pcmake dtbs
[[ $1 == 'n'                ]] && pcmod

[[ $1 == 'n' || $1 == 't'   ]] && ./anykernel/build_zip.sh $1

rm arch/arm64/configs/final_defconfig
