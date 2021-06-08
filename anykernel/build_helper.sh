#!/bin/bash

[ -d out ] && rm -rf out && mkdir -p out || mkdir -p out

TC_DIR=${HOME}/android/TOOLS/proton-clang
CFG_DIR=$(pwd)/arch/arm64/configs
CFG=$CFG_DIR/holland2_defconfig

[ -d $TC_DIR ] \
&& echo -e "\nProton-Clang Present.\n" \
|| echo -e "\nProton-Clang Not Present. Downloading Around 500MB...\n" \
| mkdir -p $TC_DIR \
| git clone --depth=1 https://github.com/kdrag0n/proton-clang $TC_DIR \
| echo "Done."

echo -e "\nChecking Clang Version...\n"
PATH="$TC_DIR/bin:${PATH}" clang --version
echo -e "\n\nStarting Build...\n"

cp $CFG $CFG_DIR/final_defconfig

full_lto() {
echo -e "\n\nSetting Full LTO Configs...\n\n"
echo -e "\
CONFIG_LTO_CLANG=y
CONFIG_THINLTO=n
" >> $CFG_DIR/final_defconfig
}

thin_lto() {
full_lto
echo -e "\n\nSetting Thin LTO Configs...\n\n"
echo -e "\
CONFIG_THINLTO=y
" >> $CFG_DIR/final_defconfig
}

holland1() {
echo -e "\n\nSetting holland1 Configs...\n\n"
sed -i "\
/CONFIG_ARCH_MSM8953/d
/CONFIG_MACH_TENOR_G/d
/CONFIG_MSM_ISPIF/d
/CONFIG_PINCTRL_MSM8953/d
" $CFG_DIR/final_defconfig
echo -e "\
CONFIG_ARCH_MSM8937=y
CONFIG_IP_SCTP=y
CONFIG_LEDS_QPNP_VIBRATOR=y
CONFIG_MACH_TENOR_E=y
CONFIG_MSMB_CAMERA_LEGACY=y
CONFIG_MSM_ISPIF_V2=y
CONFIG_PINCTRL_MSM8937=y
CONFIG_QCOM_DCC=y
CONFIG_SYN_COOKIES=y
" >> $CFG_DIR/final_defconfig
}

mido() {
echo -e "\n\nSetting mido Configs...\n\n"
sed -i "\
/CONFIG_FINGERPRINT_GF3258/d
/CONFIG_MACH_TENOR_G/d
/CONFIG_TOUCHSCREEN_GT9XX_v28/d
" $CFG_DIR/final_defconfig
echo -e "\
CONFIG_F2FS_FS_ENCRYPTION=y
CONFIG_F2FS_FS_SECURITY=y
CONFIG_F2FS_FS=y
CONFIG_FINGERPRINT_FPC1020_C6=y
CONFIG_FINGERPRINT_GOODIX_GF3208_C6=y
CONFIG_IR_MCE_KBD_DECODER=n
CONFIG_IR_PWM=y
CONFIG_LEDS_AW2013=y
CONFIG_LIRC=y
CONFIG_MACH_XIAOMI_C6=y
CONFIG_MEDIA_RC_SUPPORT=y
CONFIG_MSMB_CAMERA_LEGACY=y
CONFIG_RC_DEVICES=y
CONFIG_TOUCHSCREEN_GT9XX_v24_TOOL=y
CONFIG_TOUCHSCREEN_GT9XX_v24_UPDATE=y
CONFIG_TOUCHSCREEN_GT9XX_v24=y
CONFIG_TOUCHSCREEN_IST3038C=y
" >> $CFG_DIR/final_defconfig
}

pcmake() {
PATH="$TC_DIR/bin:${PATH}" \
make	\
	O=out \
	ARCH=arm64 \
	CROSS_COMPILE="aarch64-linux-gnu-" \
	CROSS_COMPILE_ARM32="arm-linux-gnueabi-" \
	CONFIG_DEBUG_SECTION_MISMATCH=y \
	CONFIG_NO_ERROR_ON_MISMATCH=y \
	$1 $2 $3 || exit
}
