#!/bin/bash


# Standard exports
DEVICE=Vince
DEFCONFIG=immortal_defconfig		#Most Editable
KERNEL_HOME=$(pwd)
EXTRA_TOOLS_DIR=$KERNEL_HOME/extra_tools
EXTRA_DRIVERS_DIR=$EXTRA_TOOLS_DIR/drivers
ANYKERNEL_DIR=$KERNEL_HOME/AnyKernel3-$DEVICE
OUT_DIR=$KERNEL_HOME/out
MODULES_DIR=$ANYKERNEL_DIR/modules/system/lib
FIRMWARE_DIR=$ANYKERNEL_DIR/modules/vendor/firmware
BINS_DIR=$ANYKERNEL_DIR/modules/system/bin
KERNEL_NAME=Immortal-StormBreaker-Kernel-$(date +%d-%m-%y)-$DEVICE-V5.zip


export ARCH=arm64                       #Editable
export SUBARCH=$ARCH
export USE_CCACHE=1
export CCACHE_DIR=~/.ccache
export JOBS=4


# Smart exports
GCC_or_CLANG=1				#1 - GCC, 2 - CLANG
if [ "$GCC_or_CLANG" -eq "1" ]
then	#For use GCC
GCC_PREF64=aarch64-linux-gnu			#Editable
GCC_PREF32=arm-linux-gnueabi			#Editable
GCC_PREFIX64=aarch64-linux-gnu-			#Editable
GCC_PREFIX32=arm-linux-gnueabi-			#Editable
GCC_PATH64=/usr					#Most Editable
GCC_PATH32=/usr                         	#Most Editable
GCC_BIN64=$GCC_PATH64/bin
GCC_BIN32=$GCC_PATH32/bin
GCC_LIB32=$GCC_PATH32/lib/$GCC_PREF32		#Editable
GCC_LIB64=$GCC_PATH64/lib/$GCC_PREF64		#Editable
GCC_LIBS=$GCC_LIB64:$GCC_LIB32
GCC_BINS=$GCC_BIN64:$GCC_BIN32
export LD_LIBRARY_PATH=$GCC_LIBS:$LD_LIBRARY_PATH
export PATH=$GCC_BINS:$PATH
export CROSS_COMPILE=$GCC_PREFIX64
export CROSS_COMPILE_ARM32=$GCC_PREFIX32
else	#For use Clang
GCC_PREF64=aarch64-linux-gnu                    #Editable
GCC_PREF32=arm-linux-gnueabi                    #Editable
GCC_PREFIX64=aarch64-linux-gnu-			#Editable
GCC_PREFIX32=arm-linux-gnueabi-			#Editable
CLANG_PATH=/usr					#Most Editable
GCC_PATH64=/usr					#Most Editable
GCC_PATH32=/usr					#Most Editable
CLANG_BIN=$CLANG_PATH/lib/llvm-11/bin
GCC_BIN64=$GCC_PATH64/bin
GCC_BIN32=$GCC_PATH32/bin
CLANG_LIB64=$CLANG_PATH/lib/llvm-11/lib		#Editable
CLANG_LIB32=$CLANG_PATH/lib/llvm-11/lib64	#Editable
GCC_LIB64=$GCC_PATH64/lib/$GCC_PREF64           #Editable
GCC_LIB32=$GCC_PATH32/lib/$GCC_PREF32           #Editable
GCC_BINS=$GCC_BIN64:$GCC_BIN32
GCC_LIBS=$GCC_LIB64:$GCC_LIB32
CLANG_LIBS=$CLANG_LIB64:$CLANG_LIB32
export LD_LIBRARY_PATH=$CLANG_LIBS:$GCC_LIBS:$LD_LIBRARY_PATH
export PATH=$CLANG_BIN:$GCC_BINS:$PATH
export CROSS_COMPILE=$GCC_PREFIX64
export CLANG_TRIPLE=$GCC_PREFIX64
export CROSS_COMPILE_ARM32=$GCC_PREFIX32
VALUES="OBJCOPY=llvm-objcopy \
	OBJDUMP=llvm-objdump \
	STRIP=llvm-strip \
	NM=llvm-nm \
	AR=llvm-ar \
	AS=llvm-as \
	CC=clang "
fi


######################################################
######################################################
######################################################

# Make
make $DEFCONFIG all firmware_install modules_install \
	INSTALL_MOD_PATH=. \
	O=$OUT_DIR \
	$VALUES \
	-j$JOBS


# Second exports
MODULES=$OUT_DIR/lib/modules
UNAME=$(ls $MODULES)
HEADERS_DIR=$OUT_DIR/headers
FIRMWARES="$(find $OUT_DIR -name *.fw) $(find $OUT_DIR -name *.bin)"
EXTRA_FIRMWARES=$EXTRA_TOOLS_DIR/firmwares/*
IMAGE=$OUT_DIR/arch/arm64/boot/Image.gz-dtb
BINS=$EXTRA_TOOLS_DIR/bins/*

# Checking of $IMAGE
[ -e  $IMAGE ] || exit



######################################################
######################################################
######################################################

# Create kernel headers
mkdir -p $HEADERS_DIR
cp -r $(ls | grep -v out) $HEADERS_DIR
cd $HEADERS_DIR

make $DEFCONFIG modules_prepare \
	$VALUES \
	-j$JOBS

rm -r $(ls | grep -vw arch | grep -v include | grep -v scripts | grep -v Makefile)
cd arch
rm -r $(ls | grep -v arm)
cd $HEADERS_DIR
rm -r arch/*/boot
cp $OUT_DIR/Module.symvers .
cd $KERNEL_HOME


######################################################
######################################################
######################################################

# Copying
rm -rf $MODULES/$UNAME/build $MODULES/$UNAME/source
mkdir -p $FIRMWARE_DIR $MODULES_DIR $BINS_DIR $MODULES/$UNAME/build

cp -nr $HEADERS_DIR/* $MODULES/$UNAME/build
cp -nr $MODULES $MODULES_DIR
cp -nr $FIRMWARES $EXTRA_FIRMWARES $FIRMWARE_DIR
cp -nr $IMAGE $ANYKERNEL_DIR
cp -nr $BINS $BINS_DIR

# Zipping
cd $ANYKERNEL_DIR
zip -9 -r $KERNEL_NAME * >/dev/null 2>&1
mv $KERNEL_NAME $KERNEL_HOME
cd $KERNEL_HOME


######################################################
######################################################
######################################################

g(){
# Cleaning Up
rm $ANYKERNEL_DIR/Image.gz-dtb
rm -rf $ANYKERNEL_DIR/modules
rm -rf $HEADERS_DIR
rm -rf $MODULES
}
