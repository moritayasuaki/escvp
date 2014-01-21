#!/bin/bash
if [ -e /sys/module/usbserial/drivers/usb:escvp ]; then
  echo "vcom module was loaded, unload now.."
  /sbin/rmmod escvp
fi
cp escvp.ko /lib/modules/`uname -r`/kernel/drivers/usb/serial/
/sbin/depmod -a
/sbin/modinfo escvp
/sbin/modprobe escvp
