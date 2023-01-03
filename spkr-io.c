#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/i8253.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <stdbool.h>

MODULE_AUTHOR("Alvaro Lopez Garcia <alvaro.lopezgar@alumnos.upm.es> &\
 Fabian Villalobos Cayoja <Fabian.villalobos@alumnos.upm.es>");
MODULE_DESCRIPTION("PC Speaker driver");
MODULE_LICENSE("GPL");

// Constants
#define DEBUG       true
#define REG_CTRL    0x43
#define REG_DATA    0x42
#define B_PORT      0x61
#define COUNT       1
static const char DEV_NAME[] = "spkr";
static const char CLASS_NAME[] = "speaker";
static const char DEV_TYPE[] = "int_spkr";

// Params
static int freq = 1000;
static int minor = 1;
module_param(freq, int, S_IRUGO);
module_param(minor, int, S_IRUGO);

// Funciones del modulo
static void spkr_on(void);
static void spkr_off(void);
static void spkr_set_frequency(void);
// Funciones de servicio de dispositivo
static int open(struct inode *inode, struct file *filp);
static int release(struct inode *inode, struct file *filp);
static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

// Shared variables
static dev_t *dev;
static struct cdev *cdev;
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = open,
    .release = release,
    .write = write
};
static struct class *class_;
static struct device *dev_;

static int __init spkr_init(void) {
    
    // Variables
    unsigned long flags;
    uint8_t ctrl_reg_val = 0xb6;

    // Debugging
    if (DEBUG) printk(KERN_ALERT "Loading module...");

    alloc_chrdev_region(dev, minor, COUNT, DEV_NAME);

    // Dando de alta al dispostivo
    if (alloc_chrdev_region(dev, minor, COUNT, DEV_NAME) < 0) {
        if (DEBUG) printk(KERN_INFO "Major number allocation is failed\n");
        return 1; 
    } else {
        if (DEBUG) printk(KERN_ALERT "Major asignado: %d", MAJOR(*dev));
    }
    
    // Creando el dispositivo de caracteres
    cdev_init(cdev, &fops);
    cdev_add(cdev, *dev, COUNT);

    // Dando de alta el dispositivo en sysfs
    class_ = class_create(THIS_MODULE, CLASS_NAME);
    dev_ = device_create(class_, NULL, *dev, NULL, DEV_TYPE);
    
    raw_spin_lock_irqsave(&i8253_lock, flags); // Critical section

    // Device programming
    outb_p(REG_CTRL, ctrl_reg_val);

    spkr_set_frequency();
    spkr_on();

    raw_spin_unlock_irqrestore(&i8253_lock, flags); // End of critical section
    return 0;
}

static void __exit spkr_exit(void) {
    
    // Variables
    unsigned long flags;

    // Debugging message
    if (DEBUG) printk(KERN_ALERT "Unloading module...");

    raw_spin_lock_irqsave(&i8253_lock, flags); // Critical section

    // Disabling the speaker
    spkr_off();

    raw_spin_unlock_irqrestore(&i8253_lock, flags); // End of critical section
    
    // Dando de baja el dispositivo en sysfs
    device_destroy(class_, *dev);
    class_destroy(class_);

    // Elminando el dispositivo de caracteres
    cdev_del(cdev);
    
    // Dando de baja al dispositivo
    unregister_chrdev_region(*dev, COUNT);
}

static void spkr_on(void) {

    // Variables
    uint8_t tmp, act_mask = 0x03;

    if (DEBUG) printk(KERN_ALERT "Activating speaker...");

    // Activating speaker
    tmp = inb_p(B_PORT);
 	outb_p(tmp | act_mask, B_PORT);
}

static void spkr_off(void) {

    // Variables
    uint8_t tmp, deact_mask = ~0x03;

    if (DEBUG) printk(KERN_ALERT "Deactivating speaker...");

    // Deactivating speaker
    tmp = inb_p(B_PORT);
 	outb_p(tmp & deact_mask, B_PORT);
}

static void spkr_set_frequency(void) {

    // Variables
    uint32_t div = PIT_TICK_RATE / freq;

    if (DEBUG) printk(KERN_ALERT "Reproduciendo sonido a frecuencia de %d Hz", freq);

    // Writting parameters
    outb_p(REG_DATA, (uint8_t) (div));
    outb_p(REG_DATA, (uint8_t) (div >> 8));
}

static int open(struct inode *inode, struct file *filp) {
    // TODO
    return 0;
}

static int release(struct inode *inode, struct file *filp) {
    // TODO
    return 0;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    // TODO
    return (ssize_t) 0;
}

module_init(spkr_init);
module_exit(spkr_exit);