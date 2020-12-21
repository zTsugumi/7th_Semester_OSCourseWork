/*
 * Author: zTsugumi
 * 
 * A kernel module to create a virtual device (vdev) driver that used 
 * to control mouse movement by keyboard keystroke (Ctrl + <symbol>)
 * 
 * HOW IT WORKS?
 * 1. vdev gets the configuration from user through fops (by default "wsad")
 * 2. TOP-HALF
 *    After loaded to kernel, vdev will captures all the interrupt from i0842 
 *    controller, read the scancode on the data port (0x60) and put the it
 *    into the device data
 * 3. BOTTOM-HALF
 *    The captured scancode will then be scheduled by a tasklet to handle
 *    the conversion to mouse movement (if the correct keys are pressed)
 */

#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/device.h> // for creating device file
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kdev_t.h> // for creating device file
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h> // for user access

#include "my_vdev.h"

MODULE_DESCRIPTION(MODULE_NAME);
MODULE_AUTHOR("zTsugumi");
MODULE_LICENSE("GPL");

/********************************** STRUCTURE ***********************************/
static struct vdev { // Wrapper struct for char device
  struct cdev cdev;
  spinlock_t lock;
  u8 buf[2]; // a buffer to store last 2 pressed key
  char config[4]; // a map for mouse movement: UP, DOWN, LEFT, RIGHT
} devs[1];

static struct class* dev_class;

/********************************** INTERFACE ***********************************/
/*
 * Return the value of the DATA register
 */
static inline u8 i8042_read_data(void);

/*
 * Check if a given scancode corresponds to key press or release
 */
static int is_key_pressed(unsigned int);

/*
 * Put scancode to device data
 */
static void put_scancode(struct vdev*, u8);

/*
 * Keyboard interrupt handler
 */
irqreturn_t kbd_interrupt_handler(int, void*);

/*
 * Driver functions
 */
static int vdev_open(struct inode*, struct file*);
static int vdev_release(struct inode*, struct file*);
// User space -> Device: get config from user
static ssize_t vdev_write(struct file*, const char __user*, size_t, loff_t*);

static const struct file_operations vdev_fops = {
  .owner = THIS_MODULE,
  .open = vdev_open,
  .release = vdev_release,
  .write = vdev_write,
};

/*********************************** TASKLET ************************************/

/********************************** INTERRUPT ***********************************/
static inline u8 i8042_read_data(void)
{
  u8 val;
  val = inb(I8042_DATA_REG);
  return val;
}

static int is_key_pressed(unsigned int scancode)
{
  return !(scancode & SCANCODE_RELEASED_MASK);
}

static void put_scancode(struct vdev* data, u8 scancode)
{

  data->buf[0] = data->buf[1];
  data->buf[1] = scancode;

  pr_info("[0]: 0x%x, [1]: 0x%x", data->buf[0], data->buf[1]);

  if (data->buf[0] == SCANCODE_LALT_MASK
      && data->buf[1] == SCANCODE_W_MASK)
    pr_info("Accepted");
}

irqreturn_t kbd_interrupt_handler(int irq_no, void* dev_id)
{
  u8 scancode = 0;
  int pressed;

  scancode = i8042_read_data();
  pressed = is_key_pressed(scancode);

  if (pressed) {
    struct vdev* data = (struct vdev*)dev_id;

    spin_lock(&data->lock);
    put_scancode(data, scancode);
    spin_unlock(&data->lock);
  } else {
    // WIP
  }

  // Report the interrupt as not handled
  // so that the original driver can
  // process it
  return IRQ_NONE;
}

/******************************* DRIVER FUNCTIONS *******************************/
static int vdev_open(struct inode* inode, struct file* file)
{
  struct vdev* data = container_of(inode->i_cdev, struct vdev, cdev);

  file->private_data = data;
  pr_info("Device %s opened\n", MODULE_NAME);

  return 0;
}

static int vdev_release(struct inode* inode, struct file* file)
{
  pr_info("Device %s closed\n", MODULE_NAME);
  return 0;
}

static ssize_t vdev_write(struct file* file, const char __user* user_buffer,
    size_t size, loff_t* offset)
{
  struct vdev* data = (struct vdev*)file->private_data;

  // Malformed config
  if (size != 4)
    pr_info("User config incorrect!");
  else {
    if (copy_from_user(data->config, user_buffer, size))
      return -EFAULT;
    pr_info("User config: %c%c%c%c",
        data->config[0],
        data->config[1],
        data->config[2],
        data->config[3]);
  }

  return size;
}

static int __init vdev_init(void)
{
  int err;
  dev_t devnum = MKDEV(VDEV_MAJOR, VDEV_MINOR);

  /* 1. register char device */
  err = register_chrdev_region(devnum, VDEV_DEV_COUNT, MODULE_NAME);
  if (err != 0) {
    pr_err("register_region failed: %d\n", err);
    goto out;
  }

  /* 2. request the keyboard I/O ports */
  if (request_region(I8042_DATA_REG + 1, 1, MODULE_NAME) == NULL) {
    err = -EBUSY;
    goto out_unregister;
  }
  if (request_region(I8042_STATUS_REG + 1, 1, MODULE_NAME) == NULL) {
    err = -EBUSY;
    goto out_unregister;
  }

  /* 3. init spinlock + default config */
  spin_lock_init(&devs[0].lock);
  devs[0].config[0] = 'w';
  devs[0].config[1] = 's';
  devs[0].config[2] = 'a';
  devs[0].config[3] = 'd';

  /* 4. register IRQ handler for keyboard IRQ (IRQ1) */
  err = request_irq(
      I8042_KBD_IRQ, // IRQ line
      kbd_interrupt_handler,
      IRQF_SHARED, // share interrupt line with other vdev driver (i8042)
      MODULE_NAME, // use this to show dev in /proc/interrupts
      &devs[0]); // for share interrupt, dev_id can't be NULL
  if (err != 0) {
    pr_err("request_irq failed: %d\n", err);
    goto out_release_regions;
  }

  /* 5. add char dev to system */
  cdev_init(&devs[0].cdev, &vdev_fops);
  err = cdev_add(&devs[0].cdev, devnum, VDEV_DEV_COUNT);
  if (err != 0) {
    pr_err("cdev_add failed: %d\n", err);
    goto out_release_regions;
  }

  /* 6. create struct class + device file */
  if ((dev_class = class_create(THIS_MODULE, MODULE_NAME)) == NULL) {
    err = -1;
    pr_err("class_create failed\n");
    goto out_cdev_del;
  }

  if ((device_create(dev_class, NULL, devnum, NULL, MODULE_NAME)) == NULL) {
    err = -1;
    pr_err("device_create failed\n");
    goto out_class_destroy;
  }

  pr_notice("Driver %s loaded\n", MODULE_NAME);
  return 0;

out_class_destroy:
  class_destroy(dev_class);

out_cdev_del:
  cdev_del(&devs[0].cdev);

out_release_regions:
  release_region(I8042_STATUS_REG + 1, 1);
  release_region(I8042_DATA_REG + 1, 1);

out_unregister:
  unregister_chrdev_region(devnum, VDEV_DEV_COUNT);

out:
  return err;
}

static void __exit vdev_exit(void)
{
  dev_t devnum = MKDEV(VDEV_MAJOR, VDEV_MINOR);

  /* 1. delete char device from system*/
  cdev_del(&devs[0].cdev);

  /* 2. free irq */
  free_irq(I8042_KBD_IRQ, &devs[0]);

  /* 3. release keyboard I/O ports */
  release_region(I8042_STATUS_REG + 1, 1);
  release_region(I8042_DATA_REG + 1, 1);

  /* 4. unregister char device */
  unregister_chrdev_region(devnum, VDEV_DEV_COUNT);

  /* 5. destroy struct class + device file */
  device_destroy(dev_class, devnum);
  class_destroy(dev_class);

  pr_notice("Driver %s unloaded\n", MODULE_NAME);
}

module_init(vdev_init);
module_exit(vdev_exit);