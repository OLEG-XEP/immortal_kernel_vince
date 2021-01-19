#!/bin/bash


DEFCONFIG_NAME=immortal_vince_defconfig
# Set Start Time
START=$(date +"%s")

# Make Defconfig, Image.gz-dtb, Modules and Actually Firmware
ccache make $DEFCONFIG_NAME all firmware_install modules_install \
	CROSS_COMPILE_ARM32=arm-linux-gnueabi- \
	CROSS_COMPILE=aarch64-linux-gnu- \
	SUBARCH=arm64 \
	CC=clang \
	ARCH=arm64 \
	INSTALL_MOD_PATH=. \
	O=out \
	-j$(nproc --all)

# Cheking Image.gz-dtb and Build zImage
if
test -f out/arch/arm64/boot/Image.gz-dtb
then
sleep 0
else
sudo exit 1
fi

# Creating Kernel Headers (Po-Kolhoznomu)
PWD_KRNL=$(pwd)
HDR_PATH=$(pwd)/out/kernel-headers
mkdir -p $HDR_PATH
cp -r $(ls | grep -v out) $HDR_PATH
cd $HDR_PATH
make $DEFCONFIG_NAME modules_prepare \
	-j$(nproc --all)
rm -rf $(ls | grep -v include | grep -v scripts | grep -v Makefile | grep -v arch)
cd arch && rm -rf $(ls | grep -v arm64 | grep -v arm) && cd ..
cd $PWD_KRNL

# Clean Up AnyKernel3/ Folder
rm AnyKernel3/IMMORTAL* || sleep 0
rm AnyKernel3/Image.gz-dtb || sleep 0
rm -rf AnyKernel3/modules/system/* || sleep 0

# Import zImage and All Depends-files
KERNEL_REL=$(make kernelrelease O=out | grep "4\.9")
mkdir -p AnyKernel3/modules/system/lib/modules/${KERNEL_REL}/kernel
mkdir -p AnyKernel3/modules/system/vendor/firmware
mkdir -p AnyKernel3/modules/system/lib/modules/${KERNEL_REL}/build

cp out/arch/arm64/boot/Image.gz-dtb AnyKernel3/
cd out && cp -nrf --parents $(find -name *.ko | grep -v "lib\/modules") ../AnyKernel3/modules/system/lib/modules/${KERNEL_REL}/kernel && cd ..
cp -nrf $(find out/lib/modules -name modules.*) AnyKernel3/modules/system/lib/modules/${KERNEL_REL}/
cp -nrf $(find -name *.fw | grep -v AnyKernel3) $(find -name *.bin | grep -v AnyKernel3) AnyKernel3/modules/system/vendor/firmware
cp -rL $HDR_PATH/* AnyKernel3/modules/system/lib/modules/${KERNEL_REL}/build/

# Zipping Kernel
cd AnyKernel3/
zip -r -9 IMMORTAL_KERNEL-VINCE-$(date +"%d.%m.%y").zip *
cd ..

# Clean Up Working Directory
rm -rf $HDR_PATH
rm -r $(find -name *.ko) $(find -name *.fw | grep -v out) $(find -name *.bin | grep -v out)
rm out/arch/arm64/boot/Image.gz-dtb

# Successfull Build!
END=$(date +"%s")
DIFF=$(( END - START))
echo -e '\033[01;32m' "Kernel compiled successfully in $((DIFF / 60)) minute(s) and $((DIFF % 60)) seconds"
