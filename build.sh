#!/bin/bash

# Do not touch this.

source anykernel/build_helper.sh

[[ $1 == 'g'    ]] && export model=HOLLAND2

BUILD_START=$(date +"%s")

make O=out ARCH=arm64 final_defconfig

[[ $2 != 'dtb' ]] && pcmake -j$(nproc --all) || pcmake dtbs

DIFF=$(($(date +"%s") - $BUILD_START))
echo -e "\nBuild completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."

[[ $2 != 'dtb' ]] && ./anykernel/build_zip.sh $1

unset model
rm arch/arm64/configs/final_defconfig
