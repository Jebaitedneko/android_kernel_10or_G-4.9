#!/bin/bash

# Do not touch this.

source anykernel/build_helper.sh

[[ $2 == 'f' ]] && full_lto
[[ $2 == 't' ]] && thin_lto

[[ $1 == 'g'    ]] && export model=HOLLAND2
[[ $1 == 'e'    ]] && holland1 && export model=HOLLAND1
[[ $1 == 'm'    ]] && mido && export model=MIDO

BUILD_START=$(date +"%s")

make O=out ARCH=arm64 final_defconfig

[[ $2 != 'dtb' && $2 != 'cust' ]] && pcmake -j$(nproc --all) || [[ $2 = 'cust' ]] && pcmake_cust $3 $4 $5 || pcmake dtbs

DIFF=$(($(date +"%s") - $BUILD_START))
echo -e "\nBuild completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."

[[ $2 != 'dtb' && $2 != 'cust' ]] && ./anykernel/build_zip.sh $1

unset model
rm arch/arm64/configs/final_defconfig
