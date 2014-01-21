ESC/VP21 VCOM driver
====================

This software is a ESC/VP21 VCOM driver
This is Linux loadable kernel module.
Linux kernel version may needs greater than or equal 3.2.0.

How to build this module
------------------------

Compiling this module require linux-headers correspond to your working kernel.

```bash
# cd /path/to/escvp
# make 
```

Registering and Loading
-----------------------

```bash
# sudo ./install.sh
```

Usage
-----

If USB connection probed,
a device file will be created as ttyUSB port.

```bash
# lsusb | grep Epson
# ls /dev/ttyUSB*
/dev/ttyUSB0
# echo -e "ver?\r" > /dev/ttyUSB0 && cat -A /dev/ttyUSB0
```
