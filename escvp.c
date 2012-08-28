/*
 * ESCVP/21 Serial USB driver
 * Copyright Morita Yasuaki <zousandazou@gmail.com>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/serial.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/kfifo.h>

/* drvier's version */
#define DRIVER_VERSION   "v0.1"
#define DRIVER_DESC  "ESC/VP21 VCOM driver"
#define DRIVER_AUTHOR "Morita Yasuaki <zousandazou@gmail.com>"

/* ESC/VP21 vendor id * product id */

#define VENDOR_ID 0x04b8
#define PRODUCT_ID  0x0514

static int debug;

static const struct usb_device_id id_table[] = {
  { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
  { },
};

MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver escvp_driver = {
  .name = "escvp",
  .probe = usb_serial_probe,
  .disconnect = usb_serial_disconnect,
  .id_table = id_table,
  .no_dynamic_id = 1,
};

static struct usb_serial_driver escvp_device = {
  .driver = {
    .owner = THIS_MODULE,
    .name = "escvp",
  },
  .id_table = id_table,
  .usb_driver = &escvp_driver,
  .num_ports = 1,
};

static int __init escvp_init(void)
{
  int retval;
  retval = usb_serial_register(&escvp_device);
  if (retval)
    return retval;
  retval = usb_register(&escvp_driver);
  if (retval)
    usb_serial_deregister(&escvp_device);
  return retval;
}

static void __exit escvp_exit(void)
{
  usb_deregister(&escvp_driver);
  usb_serial_deregister(&escvp_device);
}

module_init(escvp_init);
module_exit(escvp_exit);

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
