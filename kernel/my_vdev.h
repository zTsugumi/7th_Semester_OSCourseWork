#ifndef __MY_VDEV_H__
#define __MY_VDEV_H__

#define MODULE_NAME "VDEV"

#define VDEV_MAJOR 42
#define VDEV_MINOR 0
#define VDEV_DEV_COUNT 1

#define I8042_KBD_IRQ 1
#define I8042_STATUS_REG 0x64
#define I8042_DATA_REG 0x60

#define SCANCODE_RELEASED_MASK 0x80
#define SCANCODE_LALT_MASK 0x38

#define CMD_MAP 0
#define CMD_SPD 1

#define BUF_SIZE 64

/********************************** STRUCTURE ***********************************/
static struct vdev { // Wrapper struct for char device
  struct cdev cdev;
  spinlock_t lock;
  u8 buf[2]; // buffer to store last 2 pressed key
  char map[8]; // map for mouse movement: UP, DOWN, LEFT, RIGHT
  int spd; // mouse movement speed
} devs[1];

static struct input_dev* mouse_dev;

static struct class* dev_class;

static struct tasklet_struct* mouse_tasklet;

/********************************** INTERFACE ***********************************/
/*
 * Return the value of the DATA register
 */
static inline u8 i8042_read_data(void);

/*
 * Check if a given scancode corresponds to key press or release
 */
static int is_key_pressed(u8);

/*
 * Put scancode to device data
 */
static void put_scancode(struct vdev*, u8);

/*
 * Return a character of a given scancode
 */
static int scancode_to_ascii(u8);

/*
 * Mouse tasklet handler
 */
void mouse_tasklet_handler(unsigned long);

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

#endif
