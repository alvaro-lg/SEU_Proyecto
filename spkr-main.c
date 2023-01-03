#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i8253.h>

#include "constants.h" // Custom file for sharing constants
#include "spkr-io.c" // Including hardware-related files

MODULE_AUTHOR("Alvaro Lopez Garcia <alvaro.lopezgar@alumnos.upm.es> &\
 Fabian Villalobos Cayoja <Fabian.villalobos@alumnos.upm.es>");
MODULE_DESCRIPTION("PC Speaker driver");
MODULE_LICENSE("GPL");

// Params
static unsigned int freq = 1000;
static unsigned int minor = 0;
module_param(freq, int, S_IRUGO);
module_param(minor, int, S_IRUGO);

// Funciones de servicio de dispositivo
static int open(struct inode *inode, struct file *filp);
static int release(struct inode *inode, struct file *filp);
static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

// Shared variables
static dev_t dev;
static struct cdev cdev;
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = open,
    .release = release,
    .write = write
};
static struct class *class_;
static struct device *dev_;

static int __init main_init(void) {

    // Variables
    unsigned long flags;
    
    // Debugging
    if (DEBUG) printk(KERN_ALERT "Loading module...");

    // Dando de alta al dispostivo
    if (alloc_chrdev_region(&dev, minor, COUNT, DEV_NAME) < 0) {
        if (DEBUG) printk(KERN_INFO "Major number allocation is failed\n");
        return 1; 
    } else {
        if (DEBUG) printk(KERN_ALERT "Major asignado: %d", MAJOR(dev));
    }
    
    // Creando el dispositivo de caracteres
    cdev_init(&cdev, &fops);
    cdev_add(&cdev, dev, COUNT);

    // Dando de alta el dispositivo en sysfs
    class_ = class_create(THIS_MODULE, CLASS_NAME);
    dev_ = device_create(class_, NULL, dev, NULL, DEV_TYPE);

    // Using spkr-io functions
    raw_spin_lock_irqsave(&i8253_lock, flags); // Critical section

    spkr_init();
    spkr_set_frequency(freq);
    spkr_on();

    raw_spin_unlock_irqrestore(&i8253_lock, flags); // End of critical section
    return 0;
}

static void __exit main_exit(void) {

    // Variables
    unsigned long flags;

    // Debugging message
    if (DEBUG) printk(KERN_ALERT "Unloading module...");

    raw_spin_lock_irqsave(&i8253_lock, flags); // Critical section

    spkr_off();
    spkr_exit();

    raw_spin_unlock_irqrestore(&i8253_lock, flags); // End of critical section
    
    // Dando de baja el dispositivo en sysfs
    device_destroy(class_, dev);
    class_destroy(class_);

    // Elminando el dispositivo de caracteres
    cdev_del(&cdev);
    
    // Dando de baja al dispositivo
    unregister_chrdev_region(dev, COUNT);
}

module_init(main_init);
module_exit(main_exit);

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
    return count;
}