#!/bin/bash

kernel_dir=.
ak_dir=anykernel/anykernel3
image=$kernel_dir/out/arch/arm64/boot/Image.gz-dtb
pre=` cat $kernel_dir/out/.config | grep Linux | cut -f 3 -d " "  `
fmt=`date +%d\.%m\.%y_%r | sed 's/:/./g;s/ PM/.PM/g;s/ AM/.AM/g'`

[ -d $ak_dir ] && echo -e "\nAnykernel 3 Present.\n" \
|| mkdir -p $ak_dir \
| git clone --depth=1 https://github.com/osm0sis/AnyKernel3 $ak_dir
rm $ak_dir/anykernel.sh
cp $ak_dir/../anykernel.sh $ak_dir

cp -r $ak_dir ./ak_dir_working
cp $image ./Image.gz-dtb
mv Image.gz-dtb ./ak_dir_working && cd ak_dir_working

zip -r ${pre}_MOCHI_${model}_${fmt}.zip . -x '*.git*' '*modules*' '*patch*' '*ramdisk*' 'LICENSE' 'README.md'
mv *.zip ../out
cd ..
rm -rf ak_dir_working
