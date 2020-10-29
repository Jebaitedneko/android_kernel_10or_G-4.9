#!/bin/bash

[ -f ~/.bashrc ] && source ~/.bashrc
[ -f ~/.bash_profile ] && source ~/.bash_profile
[ -f ~/.profile ] && source ~/.profile

export USE_CCACHE=1
ccache -M 100G

[ -d out ] && rm -rf out && mkdir -p out || mkdir -p out

CFG_DIR=$(pwd)/arch/arm64/configs

CFG=$CFG_DIR/holland2_defconfig

TC_DIR=${HOME}/android/TOOLS/proton-clang

[ -d $TC_DIR ] \
&& echo -e "\nProton-Clang Present.\n" \
|| echo -e "\nProton-Clang Not Present. Downloading Around 500MB...\n" \
| mkdir -p $TC_DIR \
| git clone --depth=1 https://github.com/kdrag0n/proton-clang $TC_DIR \
| echo "Done."

echo -e "\nChecking Clang Version...\n"
PATH="$TC_DIR/bin:${PATH}" \
clang --version
echo -e "\n"
echo -e "\nStarting Build...\n"

cp $CFG $CFG_DIR/final_defconfig

nontreble() {
echo "CONFIG_MACH_NONTREBLE_DTS=y" >> $CFG_DIR/final_defconfig
echo "CONFIG_PRONTO_WLAN=m" >> $CFG_DIR/final_defconfig
}

noslmk() {
sed -i "/CONFIG_ANDROID_LOW_MEMORY_KILLER/d" $CFG_DIR/final_defconfig
sed -i "/CONFIG_ANDROID_SIMPLE_LMK/d" $CFG_DIR/final_defconfig
sed -i "/CONFIG_ANDROID_SIMPLE_LMK_MINFREE/d" $CFG_DIR/final_defconfig
sed -i "/CONFIG_ANDROID_SIMPLE_LMK_TIMEOUT_MSEC/d" $CFG_DIR/final_defconfig
echo "CONFIG_ANDROID_LOW_MEMORY_KILLER=y" >> $CFG_DIR/final_defconfig
echo "CONFIG_MEMCG=y" >> $CFG_DIR/final_defconfig
echo "CONFIG_MEMCG_SWAP=y" >> $CFG_DIR/final_defconfig
}

full_lto() {
echo "CONFIG_LTO_CLANG=y" >> $CFG_DIR/final_defconfig
}

thin_lto() {
full_lto
echo "CONFIG_THINLTO=y" >> $CFG_DIR/final_defconfig
}

pcmake() {
PATH="$TC_DIR/bin:${PATH}" \
make	\
	O=out \
	ARCH=arm64 \
	CC="ccache clang" \
	AR=llvm-ar \
	NM=llvm-nm \
	LD=ld.lld \
	STRIP=llvm-strip \
	OBJCOPY=llvm-objcopy \
	OBJDUMP=llvm-objdump \
	OBJSIZE=llvm-size \
	READELF=llvm-readelf \
	HOSTCC=clang \
	HOSTCXX=clang++ \
	HOSTAR=llvm-ar \
	HOSTLD=ld.lld \
	CROSS_COMPILE=aarch64-linux-gnu- \
	CROSS_COMPILE_ARM32=arm-linux-gnueabi- \
	CONFIG_DEBUG_SECTION_MISMATCH=y \
	CONFIG_NO_ERROR_ON_MISMATCH=y \
	$1 $2 $3 || exit
}

pcmod() {
[ -d "modules" ] && rm -rf modules || mkdir -p modules

pcmake INSTALL_MOD_PATH=../modules \
       INSTALL_MOD_STRIP=1 \
       modules_install || exit
}
