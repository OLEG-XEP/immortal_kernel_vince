#!/system/bin/sh


for mod in $(find /system -name *.ko)
do insmod $mod
done
