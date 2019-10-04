/*
 * ESC/VP 21 Serial USB driver
 * copyright y.morita <zousandazou@gmail.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/slab.h>
#include <linux/serial.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/version.h>

#define DRIVER_VERSION "v0.1"
#define DRIVER_DESC "ESC/VP 21 ESCVP driver"
#define DRIVER_AUTHOR "Morita Yasuaki <zousandazou@gmail.com>"

#define VENDOR_ID 0x04b8
#define PRODUCT_ID 0x0514
#define INTERFACE_CLASS 0xFF
#define INTERFACE_SUBCLASS 0xFF
#define INTERFACE_PROTOCOL 0xFF

#define CONFIDENTIAL 0x00

#define ESCVP_RTYPE CONFIDENTIAL
#define ESCVP_GET_RDATA_REQ CONFIDENTIAL
#define ESCVP_GET_RDATALEN_REQ CONFIDENTIAL

#define ENDPOINT 0x00
#define DEFAULT_POLL_MS 50 
#define URB_TRANSFER_BUFFER_SIZE 0x10000

static unsigned int poll_ms = DEFAULT_POLL_MS;
module_param(poll_ms, uint, S_IRUGO | S_IWUSR);

enum escvp_flag {
  ESCVP_FLAG_CTRL_BUSY
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0) 
static const struct usb_device_id id_table[] = {
  { USB_DEVICE_AND_INTERFACE_INFO(VENDOR_ID, PRODUCT_ID, INTERFACE_CLASS, INTERFACE_SUBCLASS,INTERFACE_PROTOCOL) },
  { }
};
#else
static const struct usb_device_id id_table[] = {
  { USB_DEVICE_INTERFACE_CLASS(VENDOR_ID, PRODUCT_ID, INTERFACE_CLASS) },
  { }
};
#endif
MODULE_DEVICE_TABLE(usb, id_table);

struct escvp_port {
  int open;
  struct urb *write_urb;
  struct urb *control_urb;
  struct usb_ctrlrequest *dr;
  spinlock_t lock;
  struct timer_list poll_timer;
  struct usb_serial_port *port; // loopback
  char *ctrl_buf;
  unsigned long flags;
};

static inline void escvp_set_port_private(struct usb_serial_port *port,
                                         struct escvp_port *data)
{
  usb_set_serial_port_data(port, (void *) data);
}

static inline struct escvp_port *escvp_get_port_private(struct usb_serial_port *port)
{
  return (struct escvp_port *)usb_get_serial_port_data(port);
}

static void escvp_get_rd_callback(struct urb *urb)
{
  char *data;
  struct escvp_port *vport;
  struct device *dev = &urb->dev->dev;
  int status = urb->status;
  struct tty_struct *tty;
  vport = urb->context;
  tty = tty_port_tty_get(&vport->port->port);
  switch (status) {
    case 0:
      break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
      dev_dbg(dev, "%s - urb shutdown with status: %d\n",__func__, status);
      goto out;
    default:
      dev_dbg(dev, "%s - nonzero urb status received: %d\n",__func__, status);
      goto out;
  }
  dev_dbg(dev, "%s urb buffer size is %d\n", __func__, urb->actual_length);
  data = urb->transfer_buffer;
  if (tty) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    tty_insert_flip_string(tty, data, urb->actual_length);
    tty_flip_buffer_push(tty);
#else
    tty_insert_flip_string(&vport->port->port, data, urb->actual_length);
    tty_flip_buffer_push(&vport->port->port);
#endif
    tty_kref_put(tty);
  }
out:
  clear_bit_unlock(ESCVP_FLAG_CTRL_BUSY, &vport->flags);
}

static int escvp_get_rd(struct escvp_port *vport, __u16 wlen)
{
  struct usb_device *dev = vport->port->serial->dev;
  struct usb_ctrlrequest *dr = vport->dr;
  unsigned char *buffer = vport->ctrl_buf;
  int ret;
  if (test_and_set_bit_lock(ESCVP_FLAG_CTRL_BUSY, &vport->flags))
    return -EBUSY;

  dr->bRequestType = ESCVP_RTYPE;
  dr->bRequest = ESCVP_GET_RDATA_REQ;
  dr->wValue = cpu_to_le16(0x00);
  dr->wIndex = cpu_to_le16(0x00);
  dr->wLength = cpu_to_le16(wlen);

  usb_fill_control_urb(vport->control_urb, dev, usb_rcvctrlpipe(dev, ENDPOINT),
                       (unsigned char *)dr, buffer, wlen,
                       escvp_get_rd_callback,vport);
  vport->control_urb->transfer_buffer_length = wlen;
  ret = 1; // usb_submit_urb(vport->control_urb, GFP_ATOMIC);
  if (ret)
    clear_bit_unlock(ESCVP_FLAG_CTRL_BUSY, &vport->flags);
  return ret;
}

static void escvp_get_rdlen_callback(struct urb *urb)
{
  __u16 *data;
  __u16 rdlen = 0x0000;
  int len;
  struct escvp_port *vport;
  struct device *dev = &urb->dev->dev;
  int status = urb->status;
  vport = urb->context;
  switch (status) {
    case 0:
      break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
      dev_dbg(dev, "%s - urb shutting down with status : %d\n", __func__, status);
      goto out;
    default:
      dev_dbg(dev, "%s - nonzero urb status received: %dn", __func__, status);
      goto out;
  }
  data = urb->transfer_buffer;
  rdlen = data[0];
  len = (int)rdlen;
out:
  clear_bit_unlock(ESCVP_FLAG_CTRL_BUSY, &vport->flags);
  if(rdlen != 0x0000){
    escvp_get_rd(vport,rdlen);
  }
}

static int escvp_get_rdlen(struct escvp_port *vport)
{
  struct usb_device *dev = vport->port->serial->dev;
  struct usb_ctrlrequest *dr = vport->dr;
  unsigned char *buffer = vport->ctrl_buf;
  int ret;
  if(test_and_set_bit_lock(ESCVP_FLAG_CTRL_BUSY, &vport->flags))
    return -EBUSY;
  dr->bRequestType = ESCVP_RTYPE;
  dr->bRequest = ESCVP_GET_RDATALEN_REQ;
  dr->wValue = cpu_to_le16(0x00);
  dr->wIndex = cpu_to_le16(0x00);
  dr->wLength = cpu_to_le16(0x02);
  
  usb_fill_control_urb(vport->control_urb, dev, usb_rcvctrlpipe(dev, ENDPOINT),
                       (unsigned char *)dr, buffer, 2,
                       escvp_get_rdlen_callback,vport);
  vport->control_urb->transfer_buffer_length = 2;
  ret = 1; // usb_submit_urb(vport->control_urb, GFP_ATOMIC);
  if (ret)
    clear_bit_unlock(ESCVP_FLAG_CTRL_BUSY, &vport->flags);
  return ret;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void escvp_get_rdlen_poll(unsigned long arg)
{
  struct escvp_port *vport = (struct escvp_port *) arg;
#else
static void escvp_get_rdlen_poll(struct timer_list *t)
{
  struct escvp_port *vport = from_timer(vport, t, poll_timer);
#endif
  escvp_get_rdlen(vport);
  mod_timer(&vport->poll_timer, jiffies + msecs_to_jiffies(poll_ms));
}

static int escvp_port_probe(struct usb_serial_port *port)
{
  struct escvp_port *vport;
  int status;
  printk(KERN_INFO "escvp: port probed, polling period = %d ms\n",poll_ms);
  vport = kzalloc(sizeof(struct escvp_port), GFP_KERNEL);
  if(vport == NULL){
    printk(KERN_ERR "escvp: out of memory error\n");
    return -ENOMEM;
  }
  vport->port = port;
  escvp_set_port_private(port, vport);
  spin_lock_init(&vport->lock);

  escvp_set_port_private(port, vport);
  vport->control_urb = usb_alloc_urb(0, GFP_KERNEL);
  vport->ctrl_buf = kmalloc(URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);
  vport->dr = kmalloc(sizeof(struct usb_ctrlrequest),GFP_KERNEL);
  if (!vport->control_urb || !vport->ctrl_buf || !vport->dr){
    printk(KERN_ERR "escvp: out of memory error/n");
    status = -ENOMEM;
    goto error;
  }
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
  init_timer(&vport->poll_timer);
  vport->poll_timer.function = escvp_get_rdlen_poll;
  vport->poll_timer.expires =
    jiffies + msecs_to_jiffies(poll_ms);
  vport->poll_timer.data = (unsigned long)vport;
  add_timer(&vport->poll_timer);
#else
  timer_setup(&vport->poll_timer, escvp_get_rdlen_poll, 0);
#endif
  return 0;
error:
  kfree(vport->dr);
  kfree(vport->ctrl_buf);
  usb_free_urb(vport->control_urb);
  kfree(vport);
  return status;
}

static int escvp_port_remove(struct usb_serial_port *port)
{
  struct escvp_port *vport;
  vport = escvp_get_port_private(port);
  del_timer_sync(&vport->poll_timer);
  usb_kill_urb(vport->control_urb);
  usb_free_urb(vport->control_urb);
  kfree(vport->ctrl_buf);
  kfree(vport->dr);
  kfree(vport);
  return 0;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0) 
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
  .usb_driver = &escvp_driver,
  .id_table = id_table,
  .num_ports = 1,
  .port_probe = escvp_port_probe,
  .port_remove = escvp_port_remove,
};
static int __init escvp_init(void)
{
  int ret;
  ret = usb_serial_register(&escvp_device);
  if (ret) {
    printk(KERN_ERR "escvp: registeration failed\n");
    return ret;
  }
  ret = usb_register(&escvp_driver);
  if (ret) {
    usb_serial_deregister(&escvp_device);
    printk(KERN_ERR "escvp: registeration failed\n");
    return ret;
  };
  return 0;
}
static void __exit escvp_exit(void)
{
  usb_deregister(&escvp_driver);
  usb_serial_deregister(&escvp_device);
}
#else
static struct usb_serial_driver escvp_device = {
  .driver = {
    .owner = THIS_MODULE,
    .name = "escvp",
  },
  .id_table = id_table,
  .num_ports = 1,
  .port_probe = escvp_port_probe,
  .port_remove = escvp_port_remove,
};
#endif

static struct usb_serial_driver * const serial_drivers[] = {
  &escvp_device, NULL  
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
module_init(escvp_init);
module_exit(escvp_exit);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
module_usb_serial_driver(escvp_driver, serial_drivers);
#else
module_usb_serial_driver(serial_drivers, id_table);
#endif
MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
