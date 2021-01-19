# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers

## AnyKernel setup
# begin properties
properties() { '
kernel.string=Immortal Kernel Based On Dark Ages Ultimo
do.devicecheck=1
do.modules=1
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=vince
supported.versions=10, 11, 9
'; } # end properties

# shell variables
block=$(find /dev -name boot | head -n 1);
is_slot_device=0;
ramdisk_compression=auto;


## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;


## AnyKernel file attributes
# set permissions/ownership for included ramdisk files
set_perm_recursive 0 0 755 644 $ramdisk/*;
set_perm_recursive 0 0 750 750 $ramdisk/init* $ramdisk/sbin;


## AnyKernel install
dump_boot;

ui_print "Backuping /system/build.prop to /system/build.prop.bak
"
cp /system/build.prop /system/build.prop.bak
ui_print "Change value in /system/build.prop for fix error on device boot
"
sed -i 's/ro.treble.enabled=true/ro.treble.enabled=false/g' /system/build.prop
ui_print "Installing Magisk Module for Automaticly Activate (insmod) All Modules (*.ko-files) at Start Your Android
"
cp -rf AutoInsmodModules /data/adb/modules

UNAME=$(ls /tmp/anykernel/modules/system/lib/modules/)

if
	test -d /data/local/nhsystem
then
	if
		test -d /data/local/nhsystem/kali-armhf
	then
		NH_PATH=/data/local/nhsystem/kali-armhf
	else
		NH_PATH=/data/local/nhsystem/kali-arm64
	fi
	mkdir -p $NH_PATH/lib/modules/
	cp -rf /tmp/anykernel/modules/system/lib/modules/$UNAME $NH_PATH/lib/modules/
	chmod 777 -R $NH_PATH/lib/modules/$UNAME
else
	sleep 0
fi

write_boot;
## end install

