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
HEADERS_DIR=$OUT_DIR/kernel-headers
KERNEL_NAME=Immortal-NetHunter-Kernel-$(date +%d-%m-%y)-$DEVICE-V5.zip

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
FIRMWARES="$(find $OUT_DIR -name *.fw) $(find $OUT_DIR -name *.bin)"
EXTRA_FIRMWARES=$EXTRA_TOOLS_DIR/firmwares/*
IMAGE=$OUT_DIR/arch/arm64/boot/Image.gz-dtb
#WCNSS_FIRMWARES=drivers/staging/prima/firmware_bin/*
#WCNSS_DIR=$FIRMWARE_DIR/wlan/prima

# Checking of $IMAGE
if [ -e  $IMAGE ]
then
sleep 0
else
exit
fi



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

rm -r $(ls | grep -v arch | grep -v scripts | grep -v include | grep -v Makefile)
cd arch
rm -r $(ls | grep -v arm)
cd -
tar czf kernel-headers.tar.xz *
mv kernel-headers.tar.xz $ANYKERNEL_DIR
cd $KERNEL_HOME


######################################################
######################################################
######################################################

# Copying
mkdir -p $FIRMWARE_DIR $MODULES_DIR
rm -rf $MODULES/$UNAME/build $MODULES/$UNAME/source

cp -nr $MODULES $MODULES_DIR
cp -nr $FIRMWARES $EXTRA_FIRMWARES $FIRMWARE_DIR
cp -nr $IMAGE $ANYKERNEL_DIR


# Zipping
cd $ANYKERNEL_DIR
zip -9 -r $KERNEL_NAME *
mv $KERNEL_NAME $KERNEL_HOME
cd $KERNEL_HOME


######################################################
######################################################
######################################################

# Cleaning Up
rm $ANYKERNEL_DIR/Image.gz-dtb
rm -rf $ANYKERNEL_DIR/modules*
rm -rf $MODULES
rm -rf $ANYKERNEL_DIR/kernel-headers.tar.xz
