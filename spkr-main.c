#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i8253.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "constants.h" // Custom file for sharing constants

MODULE_AUTHOR("Alvaro Lopez Garcia <alvaro.lopezgar@alumnos.upm.es> &\
 Fabian Villalobos Cayoja <Fabian.villalobos@alumnos.upm.es>");
MODULE_DESCRIPTION("PC Speaker driver");
MODULE_LICENSE("GPL");

// Params
static unsigned int freq = 0;
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
// Rutina de atención a la interrupcion del timer
static void interrupcion_temporizador(struct timer_list *t);

// Shared variables
static struct info_dispo {
    dev_t dev;
    struct cdev cdev;
    struct class *class_;
    struct device *dev_;
    struct mutex write_lock;
    struct file_operations fops;
    wait_queue_head_t lista_bloq;
} info;
static struct last_write_t {
    uint16_t hz, ms;
    uint8_t byte_lo, byte_hi;
    int parity;
} last_write;

static int __init spkr_init(void) {

    // Variables
    unsigned long flags;
    
    // Debugging
    if (DEBUG) printk(KERN_ALERT "Loading module...");

    // Inicializacion de variables
    mutex_init(&info.write_lock);
    last_write.parity = 0;
    info.fops = (struct file_operations) {
        .owner = THIS_MODULE,
        .open = open,
        .release = release,
        .write = write
    };
    init_waitqueue_head(&info.lista_bloq);

    // Dando de alta al dispostivo
    alloc_chrdev_region(&info.dev, minor, COUNT, DEV_NAME);
    if (DEBUG) printk(KERN_ALERT "Major asignado: %d", MAJOR(info.dev));
    
    // Creando el dispositivo de caracteres
    cdev_init(&info.cdev, &info.fops);
    cdev_add(&info.cdev, info.dev, COUNT);

    // Dando de alta el dispositivo en sysfs
    info.class_ = class_create(THIS_MODULE, CLASS_NAME);
    info.dev_ = device_create(info.class_, NULL, info.dev, NULL, DEV_TYPE);

    // Using spkr-io functions
    raw_spin_lock_irqsave(&i8253_lock, flags); // Critical section

    // Only producing sound when freq is specified
    if (freq > 0) {
        spkr_set_frequency(freq);
        spkr_on();
    }

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
    device_destroy(info.class_, info.dev);
    class_destroy(info.class_);

    // Elminando el dispositivo de caracteres
    cdev_del(&info.cdev);
    
    // Dando de baja al dispositivo
    unregister_chrdev_region(info.dev, COUNT);
}

module_init(spkr_init);
module_exit(spkr_exit);

static int open(struct inode *inode, struct file *filp) {

    if (filp->f_mode & FMODE_READ) { // Apertura en modo lectura

        if (DEBUG) printk(KERN_ALERT "Accediendo al fichero en modo lectura...");

    } else if (filp->f_mode & FMODE_WRITE) {

        if (atomic_long_read(&info.write_lock.owner) == 0) { // Dispositivo libre

            mutex_lock(&info.write_lock);
            if (DEBUG) printk(KERN_ALERT "Accediendo al fichero en modo escritura...");

        } else { // Dispositivo ocupado
        
            if (DEBUG) printk(KERN_ALERT "Acceso al fichero en modo escritura bloqueado");
            return -EBUSY;
        }
    }
    return 0;
}

static int release(struct inode *inode, struct file *filp) {
    mutex_unlock(&info.write_lock);
    if (DEBUG) printk(KERN_ALERT "Liberando el acceso al fichero...");
    return 0;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    
    // Variables
    int i;
    struct timer_list timer;
    unsigned long flags;

    // Inizialitation
    timer_setup(&timer, interrupcion_temporizador, 0);

    // Reading data on buffer
    for (i = 0; i < count; i++) {

        if ((i + last_write.parity) % WRITE_SIZE == 0) { // First byte of sound

            if (get_user(last_write.byte_lo, buf + i) != 0)
                return -EFAULT;

        } else if ((i + last_write.parity) % WRITE_SIZE == 1) { // Second byte of sound

            if (get_user(last_write.byte_hi, buf + i) != 0)
                return -EFAULT;
            last_write.ms = ((uint16_t) last_write.byte_hi << 8) | (uint16_t) last_write.byte_lo;

        } else if ((i + last_write.parity) % WRITE_SIZE == 2) { // Third byte of sound
            
            if (get_user(last_write.byte_lo, buf + i) != 0)
                return -EFAULT;

        } else { // Reading last byte of a hole sound

            if (get_user(last_write.byte_hi, buf + i) != 0)
                return -EFAULT;
            last_write.hz = ((uint16_t) last_write.byte_hi << 8) | (uint16_t) last_write.byte_lo;

            // Emmiting sound
            raw_spin_lock_irqsave(&i8253_lock, flags);
            spkr_set_frequency(last_write.hz);
            spkr_on();

            // Wating
            timer.expires = jiffies + msecs_to_jiffies(last_write.ms);
            add_timer(&timer);

            // Blocking process
            if (wait_event_interruptible(info.lista_bloq, timer.expires <= jiffies) != 0)
                return -ERESTARTSYS;

            // Muting speaker
            spkr_off();
            raw_spin_unlock_irqrestore(&i8253_lock, flags);
        }
    }

    // Changing parity accordingly
    last_write.parity = (last_write.parity + count) % WRITE_SIZE;

    return count;
}

void interrupcion_temporizador(struct timer_list *t) {

    // Waking up process
    wake_up_interruptible(&info.lista_bloq);
}