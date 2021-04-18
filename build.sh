#!/bin/bash

# Prepared by Jebaitedneko <jebaitedneko@gmail.com>

KROOT=$(pwd)

DARCH=arm64
DEFCG=holland2_defconfig
ZNAME=holland2
CACHE=1
TOOLC=3

MODIR=$KROOT/out/modules
OSDIR=$KROOT/out/arch/$DARCH/boot

AKDIR=$OSDIR/anykernel3
UFDTD=$OSDIR/libufdt

YYLL1=$KROOT/scripts/dtc/dtc-lexer.lex.c_shipped
YYLL2=$KROOT/scripts/dtc/dtc-lexer.l
[ -f $YYLL1 ] && sed -i "s/extern YYLTYPE yylloc/YYLTYPE yylloc/g;s/YYLTYPE yylloc/extern YYLTYPE yylloc/g" $YYLL1
[ -f $YYLL2 ] && sed -i "s/extern YYLTYPE yylloc/YYLTYPE yylloc/g;s/YYLTYPE yylloc/extern YYLTYPE yylloc/g" $YYLL2

get_gcc49() {
	ISGCC=1
	TC_64=$KROOT/../gcc-4.9-64
	TC_32=$KROOT/../gcc-4.9-32

	if [ ! -d $TC_64 ]; then
		mkdir -p $TC_64
		git clone \
			--depth=1 \
			--single-branch \
			https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-android-4.9 \
			-b lineage-18.1 \
			$TC_64
	fi

	if [ ! -d $TC_32 ]; then
		mkdir -p $TC_32
		git clone \
			--depth=1 \
			--single-branch \
			https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9 \
			-b lineage-18.1 \
			$TC_32
	fi

	CROSS="$TC_64/bin/aarch64-linux-android-"
	CROSS_ARM32="$TC_32/bin/arm-linux-androideabi-"

	MAKEOPTS=""
}

get_gcc11() {
	ISGCC=1
	TC_64=$KROOT/../gcc-11-64
	TC_32=$KROOT/../gcc-11-32

	if [ ! -d $TC_64 ]; then
		mkdir -p $TC_64
		git clone \
			--depth=1 \
			--single-branch \
			https://github.com/mvaisakh/gcc-arm64 \
			-b gcc-master \
			$TC_64
	fi

	if [ ! -d $TC_32 ]; then
		mkdir -p $TC_32
		git clone \
			--depth=1 \
			--single-branch \
			https://github.com/mvaisakh/gcc-arm \
			-b gcc-master \
			$TC_32
	fi

	CROSS="$TC_64/bin/aarch64-elf-"
	CROSS_ARM32="$TC_32/bin/arm-eabi-"

	MAKEOPTS=""
}

get_proton_clang() {
	ISCLANG=1
	TC=$KROOT/../clang-proton

	if [ ! -d $TC ]; then
		mkdir -p $TC
		git clone \
			--depth=1 \
			--single-branch \
			https://github.com/kdrag0n/proton-clang \
			-b master \
			$TC
	fi

	CROSS="$TC/bin/aarch64-linux-gnu-"
	CROSS_ARM32="$TC/bin/arm-linux-gnueabi-"

	MAKEOPTS="CC=clang LD=ld.lld AR=llvm-ar AS=llvm-as NM=llvm-nm STRIP=llvm-strip \
				OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump READELF=llvm-readelf \
				HOSTCC=clang HOSTCXX=clang++ HOSTAR=llvm-ar HOSTAS=llvm-as HOSTLD=ld.lld"
}

case $TOOLC in
	1) echo -e "\nSelecting GCC-4.9...\n" && get_gcc49 ;;
	2) echo -e "\nSelecting EVA-GCC-11...\n" && get_gcc11 ;;
	3) echo -e "\nSelecting PROTON-CLANG-13...\n" && get_proton_clang ;;
esac

if [[ $DARCH = "arm" ]]; then
	CROSS_COMPILE=$CROSS_ARM32
else
	CROSS_COMPILE=$CROSS
fi
export CROSS_COMPILE

export CROSS_COMPILE_ARM32=$CROSS_ARM32

if [[ ! -f ${CROSS_COMPILE}gcc ]]; then
	if [[ ! -f ${CROSS_COMPILE%/*}/clang ]]; then
		echo "CROSS_COMPILE not set properly." && exit
	else
		echo -e "$( ${CROSS_COMPILE%/*}/clang -v )"
	fi
else
	echo -e "$( ${CROSS_COMPILE}gcc -v )"
fi

if [[ ! -f ${CROSS_COMPILE_ARM32}gcc ]]; then
	if [[ ! -f ${CROSS_COMPILE_ARM32%/*}/clang ]]; then
		echo "CROSS_COMPILE_ARM32 not set properly." && exit
	else
		echo -e "$( ${CROSS_COMPILE_ARM32%/*}/clang -v )"
	fi
else
	echo -e "$( ${CROSS_COMPILE_ARM32}gcc -v )"
fi

[ -d out ] && rm -rf out | mkdir -p out || mkdir -p out

BUILD_START=$(date +"%s")

echo -e "\n\nmake `echo -e $MAKEOPTS` O=out ARCH=$DARCH $DEFCG\n\n"
make `echo -e $MAKEOPTS` O=out ARCH=$DARCH $DEFCG || exit

PREFIX=` grep Linux $KROOT/out/.config | cut -f 3 -d " "  `
FORMAT=` date | sed "s/ /-/g;s/:/./g" `

MODULE=` grep "=m" $KROOT/out/.config `
ISDTBO=` grep "DT_OVERLAY=y" $KROOT/out/.config `

[[ $ISDTBO ]] && export DTC_EXT=$(which dtc)

echo -e "\n\nmake `echo -e $MAKEOPTS` O=out ARCH=$DARCH -j$((`nproc`+4))\n\n"

if [[ $ISGCC ]]; then
	if [[ $CACHE ]]; then
		echo "Using ccache with gcc"
		make `echo -e $MAKEOPTS` O=out ARCH=$DARCH CC="ccache ${CROSS_COMPILE}gcc" -j$((`nproc`+8)) || exit
	else
		make `echo -e $MAKEOPTS` O=out ARCH=$DARCH -j$((`nproc`+8)) || exit
	fi
else
	if [[ $ISCLANG ]]; then
		if [[ $CACHE ]]; then
			echo "Using ccache with clang"
			make `echo -e $MAKEOPTS` O=out ARCH=$DARCH CC="ccache clang" -j$((`nproc`+8)) || exit
		else
			make `echo -e $MAKEOPTS` O=out ARCH=$DARCH -j$((`nproc`+8)) || exit
		fi
	fi
fi

if [[ $MODULE ]]; then
	[ -d $MODIR ] && rm -rf $MODIR | mkdir -p $MODIR || mkdir -p $MODIR
	echo "Making modules..."
	make O=out ARCH=$DARCH INSTALL_MOD_PATH=$MODIR INSTALL_MOD_STRIP=1 modules_install || exit
	echo "Done."
fi

[ -d $KROOT/.git ] && git restore $YYLL1 $YYLL2

DIFF=$(($(date +"%s") - $BUILD_START))
echo -e "\nBuild completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."

make_dtboimg() {
	(
		cd $OSDIR
		[ ! -d $UFDTD ] && git clone --depth=1 --single-branch https://android.googlesource.com/platform/system/libufdt $UFDTD
		chmod +x $UFDTD/utils/src/mkdtboimg.py
		echo "Making dtbo.img..."
		python3 $UFDTD/utils/src/mkdtboimg.py create dtbo.img $OSDIR/dts/qcom/*.dtbo
		echo "Done."
	)
}
[[ $ISDTBO ]] && make_dtboimg

[ ! -d $AKDIR ] && git clone --depth=1 --single-branch https://github.com/osm0sis/AnyKernel3 -b master $AKDIR

cat << EOF > $AKDIR/anykernel.sh
# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers

properties() { '
kernel.string=generic
do.devicecheck=0
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
device.name1=generic
'; }

block=/dev/block/bootdevice/by-name/boot;
is_slot_device=0;
ramdisk_compression=auto;

. tools/ak3-core.sh;
chmod -R 750 $ramdisk/*;
chown -R root:root $ramdisk/*;

dump_boot;

ui_print "*******************************************"
ui_print "Updating Kernel and Patching cmdline..."
ui_print "*******************************************"

patch_cmdline firmware_class.path firmware_class.path=/vendor/firmware_mnt/image
patch_cmdline lpm_levels.sleep_disabled lpm_levels.sleep_disabled=0
patch_cmdline androidboot.selinux androidboot.selinux=permissive

write_boot;
EOF

if [[ $MODULE ]]; then
	sed -i "s/do.modules=0/do.modules=1/g" $AKDIR/anykernel.sh
fi

chmod +x $AKDIR/anykernel.sh

make_dtimg() {
	wget -q https://raw.githubusercontent.com/LineageOS/android_system_tools_dtbtool/lineage-18.1/dtbtool.c -O $KROOT/out/dtbtool.c
	cc $KROOT/out/dtbtool.c -o $OSDIR/dts
	(
		cd $KROOT/dts
		echo "Making dt.img using dtbtool..."
		dtbtool -v -s 2048 -o $OSDIR/dt.img
		echo "Done."
	)
}

(
	cd $AKDIR

	if [[ ! -f $OSDIR/Image.gz-dtb ]]; then
		if [[ ! -f $OSDIR/Image.gz ]]; then
			if [[ ! -f $OSDIR/Image ]]; then
				echo "No kernels found. Exiting..." && exit
			else
				cp $OSDIR/Image $AKDIR && make_dtimg
			fi
		else
			cp $OSDIR/Image.gz $AKDIR && make_dtimg
		fi
	else
		cp $OSDIR/Image.gz-dtb $AKDIR
	fi

	if [[ ! -f $OSDIR/dtbo.img ]]; then
		if [[ ! -f $OSDIR/dt.img ]]; then
			echo "Using appended dtb..."
		else
			cp $OSDIR/dt.img $AKDIR
		fi
	else
		cp $OSDIR/dtbo.img $AKDIR
	fi

	if [[ $MODULE ]]; then
		find $KROOT/out/modules -type f -iname "*.ko" -exec cp {} $AKDIR/modules/system/lib/modules/ \;
		zip -r ${PREFIX}_${FORMAT}.zip . -x '*.git*' '*patch*' '*ramdisk*' 'LICENSE' 'README.md'
	else
		zip -r ${ZNAME}_${PREFIX}_${FORMAT}.zip . -x '*.git*' '*modules*' '*patch*' '*ramdisk*' 'LICENSE' 'README.md'
	fi
	mv *.zip $KROOT/out
)
