#!/bin/bash

# Do not touch this.

source anykernel/build_helper.sh

[[ $3 == 'e'                ]] && holland1

[[ $1 == 'n' || $1 == 'dn'  ]] && nontreble
[[ $2 == 's'                ]] && noslmk
[[ $2 == 'flto'             ]] && full_lto
[[ $2 == 'tlto'             ]] && thin_lto

BUILD_START=$(date +"%s")
make O=out ARCH=arm64 final_defconfig

[[ $1 == 'n' || $1 == 't'   ]] && pcmake -j$(nproc --all)

BUILD_END=$(date +"%s")
DIFF=$(($BUILD_END - $BUILD_START))
echo -e "\nBuild completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."

[[ $1 == 'dt' || $1 == 'dn' ]] && pcmake dtbs
[[ $1 == 'n'                ]] && pcmod

[[ $1 == 'n' || $1 == 't'   ]] && ./anykernel/build_zip.sh $1

rm arch/arm64/configs/final_defconfig
