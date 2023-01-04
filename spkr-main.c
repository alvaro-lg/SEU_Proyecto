#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i8253.h>
#include <linux/mutex.h>

#include "constants.h" // Custom file for sharing constants

MODULE_AUTHOR("Alvaro Lopez Garcia <alvaro.lopezgar@alumnos.upm.es> &\
 Fabian Villalobos Cayoja <Fabian.villalobos@alumnos.upm.es>");
MODULE_DESCRIPTION("PC Speaker driver");
MODULE_LICENSE("GPL");

// Params
static unsigned int freq = 1000;
static unsigned int minor = 0;
module_param(freq, int, S_IRUGO);
module_param(minor, int, S_IRUGO);

// Funciones de spkr-io
extern void spkr_set_frequency(unsigned int frequency);
extern void spkr_on(void);
extern void spkr_off(void);
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
static struct mutex write_lock;

static int __init spkr_init(void) {

    // Variables
    unsigned long flags;
    
    // Debugging
    if (DEBUG) printk(KERN_ALERT "Loading module...");

    // Inicializacion de variables
    mutex_init(&write_lock);

    // Dando de alta al dispostivo
    alloc_chrdev_region(&dev, minor, COUNT, DEV_NAME);
    if (DEBUG) printk(KERN_ALERT "Major asignado: %d", MAJOR(dev));
    
    // Creando el dispositivo de caracteres
    cdev_init(&cdev, &fops);
    cdev_add(&cdev, dev, COUNT);

    // Dando de alta el dispositivo en sysfs
    class_ = class_create(THIS_MODULE, CLASS_NAME);
    dev_ = device_create(class_, NULL, dev, NULL, DEV_TYPE);

    // Using spkr-io functions
    raw_spin_lock_irqsave(&i8253_lock, flags); // Critical section

    spkr_set_frequency(freq);
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

    spkr_off();

    raw_spin_unlock_irqrestore(&i8253_lock, flags); // End of critical section
    
    // Dando de baja el dispositivo en sysfs
    device_destroy(class_, dev);
    class_destroy(class_);

    // Elminando el dispositivo de caracteres
    cdev_del(&cdev);
    
    // Dando de baja al dispositivo
    unregister_chrdev_region(dev, COUNT);
}

module_init(spkr_init);
module_exit(spkr_exit);

static int open(struct inode *inode, struct file *filp) {

    // TODO Quitar esto
    if (DEBUG) printk(KERN_ALERT "%d, %d, %d", filp->f_mode, FMODE_READ, FMODE_WRITE);
    
    if (filp->f_mode == FMODE_READ) { // Apertura en modo lectura
        if (DEBUG) printk(KERN_ALERT "Accediendo al fichero en modo escritura...");
    } else if (filp->f_mode == FMODE_WRITE) { // Apertura en modo escritura

        if (atomic_long_read(&write_lock.owner) > 0) { // Dispositivo libre
            mutex_lock(&write_lock);
            if (DEBUG) printk(KERN_ALERT "Accediendo al fichero en modo escritura...");
        } else { // Dispositivo ocupado
            if (DEBUG) printk(KERN_ALERT "Acceso al fichero en modo escritura bloqueado");
            return -EBUSY;
        }
    }
    return 0;
}

static int release(struct inode *inode, struct file *filp) {
    mutex_unlock(&write_lock);
    if (DEBUG) printk(KERN_ALERT "Liberando el acceso al fichero...");
    return 0;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    // TODO
    return count;
}