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

static bool debug;

static int escvp_attach(struct usb_serial *serial);

static const struct usb_device_id id_table[] = {
  { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
  { },
};

MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_serial_driver escvp_device = {
  .driver = {
    .owner = THIS_MODULE,
    .name = "escvp",
  },
  .id_table = id_table,
  .num_ports = 1,
  .attach = escvp_startup,
};

static struct usb_serial_driver * const serial_drivers[] = {
  &escvp_device, NULL
};

static int escvp_startup(struct usb_serial *serial)
{
  if (serial->num_bulk_out == 0){
    printk(KERN_INFO "ESC/VP21 VCOM: missing bulk out endpoint\n");
    return -EINVAL;
  }
  printk(KERN_INFO "ESC/VP21 VCOM: usb-serial attached\n");
  return 0;
}


module_usb_serial_driver(serial_drivers, id_table);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable debug");
