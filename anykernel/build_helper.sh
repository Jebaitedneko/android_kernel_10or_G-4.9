#!/bin/bash

[ -d out ] && rm -rf out && mkdir -p out || mkdir -p out

TC_DIR=${HOME}/android/TOOLS/GCC
CFG_DIR=$(pwd)/arch/arm64/configs
CFG=$CFG_DIR/msm8953-perf_defconfig

[ -d $TC_DIR/arm64 ] \
&& echo -e "EVA-GCC arm64 Present.\n" \
|| echo -e "EVA-GCC arm64 Not Present. Downloading...\n" \
| mkdir -p $TC_DIR/arm64 \
| git clone --depth=1 https://github.com/mvaisakh/gcc-arm64 -b lld-integration $TC_DIR/arm64 \
| echo "Done."

[ -d $TC_DIR/arm ] \
&& echo -e "EVA-GCC arm Present.\n" \
|| echo -e "EVA-GCC arm Not Present. Downloading...\n" \
| mkdir -p $TC_DIR/arm \
| git clone --depth=1 https://github.com/mvaisakh/gcc-arm -b lld-integration $TC_DIR/arm \
| echo "Done."

echo -e "Starting Build...\n"

cp $CFG $CFG_DIR/final_defconfig

pcmake() {
	PATH="$TC_DIR/arm64/bin:$TC_DIR/arm/bin:${PATH}" \
	make	\
		O=out \
		ARCH=arm64 \
		CC="ccache $TC_DIR/arm64/bin/aarch64-elf-gcc" \
		LD="ccache ld.lld" \
		AR="ccache llvm-ar" \
		AS="ccache llvm-as" \
		NM="ccache llvm-nm" \
		LD="ccache ld.lld" \
		STRIP="ccache llvm-strip" \
		OBJCOPY="ccache llvm-objcopy" \
		OBJDUMP="ccache llvm-objdump"\
		OBJSIZE="ccache llvm-size" \
		READELF="ccache llvm-readelf" \
		CROSS_COMPILE="aarch64-elf-" \
		CROSS_COMPILE_ARM32="arm-eabi-" \
		$1 $2 $3 || exit
}
