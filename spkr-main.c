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
#include <linux/kfifo.h>
#include <asm/uaccess.h>

#include "constants.h" // Custom file for sharing constants

MODULE_AUTHOR("Alvaro Lopez Garcia <alvaro.lopezgar@alumnos.upm.es> &\
 Fabian Villalobos Cayoja <Fabian.villalobos@alumnos.upm.es>");
MODULE_DESCRIPTION("PC Speaker driver");
MODULE_LICENSE("GPL");

// Params
static unsigned int freq = 0;
static unsigned int minor = 0;
static unsigned int buffersize = 0;
module_param(freq, int, S_IRUGO);
module_param(minor, int, S_IRUGO);
module_param(buffersize, int, S_IRUGO);

// Funciones de spkr-io
extern void spkr_set_frequency(unsigned int frequency);
extern void spkr_on(void);
extern void spkr_off(void);
// Funciones de servicio de dispositivo
static int open(struct inode *inode, struct file *filp);
static int release(struct inode *inode, struct file *filp);
static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static long ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
void programar_sonido(void);
// Rutina de atención a la interrupcion del timer
void interrupcion_temporizador(struct timer_list *t);

// Shared variables
static struct info_dispo {
    bool muted;
    dev_t dev;
    struct cdev cdev;
    struct class *class_;
    struct device *dev_;
    struct file_operations fops;
    atomic_t opened;
    spinlock_t mute_slock;
    spinlock_t irq_slock;
    wait_queue_head_t lista_bloq;
} info;
static struct last_write_t {
    uint16_t hz, ms;
    uint8_t byte_lo, byte_hi;
    int parity;
} last_write;
static struct kfifo int_buff;
static struct timer_list timer;

static int __init spkr_init(void) {

    // Variables
    int ret;
    
    // Debugging
    if (DEBUG) printk(KERN_ALERT "Loading module...");

    // Inicializacion de variables
    timer_setup(&timer, interrupcion_temporizador, 0);
    init_waitqueue_head(&info.lista_bloq);
    spin_lock_init(&info.mute_slock);
    spin_lock_init(&info.irq_slock);
    atomic_set(&info.opened, false);
    info.muted = false;
    info.fops = (struct file_operations) {
        .owner = THIS_MODULE,
        .open = open,
        .release = release,
        .write = write,
        .unlocked_ioctl =  ioctl
    };
    if (buffersize == 0)
        last_write.parity = 0;
    else {
        ret = kfifo_alloc(&int_buff, min((int) buffersize, SOUND_SIZE), GFP_KERNEL);
        if (ret) {
            printk(KERN_ALERT "Error en kfifo_alloc\n");
            return ret;
        }
    }

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
    spkr_set_frequency(freq);
    spkr_on();
    
    return 0;
}

static void __exit spkr_exit(void) {

    // Debugging message
    if (DEBUG) printk(KERN_ALERT "Unloading module...");

    spkr_off();

    if (buffersize != 0) {
        kfifo_free(&int_buff);
    }
    
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

        if (!atomic_xchg(&info.opened, true)) { // Dispositivo libre

            if (DEBUG) printk(KERN_ALERT "Accediendo al fichero en modo escritura...");

        } else { // Dispositivo ocupado
        
            if (DEBUG) printk(KERN_ALERT "Acceso al fichero en modo escritura bloqueado");
            return -EBUSY;
        }
    }
    return 0;
}

static int release(struct inode *inode, struct file *filp) {
    atomic_set(&info.opened, false);
    if (DEBUG) printk(KERN_ALERT "Liberando el acceso al fichero...");
    return 0;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    
    // Variables
    int i, cpy_size, ret;
    unsigned int copied;
    bool muted;

    if (buffersize == 0) { // Escritura sin buffer

        // Reading data on buffer
        for (i = 0; i < count; i++) {

            switch ((i + last_write.parity) % SOUND_SIZE) {
                case 0: // First byte of sound

                    if (get_user(last_write.byte_lo, buf + i) != 0)
                        return -EFAULT;
                    break;
                case 1: // Second byte of sound

                    if (get_user(last_write.byte_hi, buf + i) != 0)
                        return -EFAULT;
                    last_write.ms = ((uint16_t) last_write.byte_hi << 8) | (uint16_t) last_write.byte_lo;
                    break;
                case 2: // Third byte of sound
                    
                    if (get_user(last_write.byte_lo, buf + i) != 0)
                        return -EFAULT;
                    break;
                case 3: // Reading last byte of a hole sound

                    if (get_user(last_write.byte_hi, buf + i) != 0)
                        return -EFAULT;
                    last_write.hz = ((uint16_t) last_write.byte_hi << 8) | (uint16_t) last_write.byte_lo;

                    // Emmiting sound
                    spkr_set_frequency(last_write.hz);
                    spin_lock_bh(&info.mute_slock);
                    muted = info.muted;
                    spin_unlock_bh(&info.mute_slock);

                    if (!muted && last_write.hz > 0) spkr_on();

                    // Wating
                    timer.expires = jiffies + msecs_to_jiffies(last_write.ms);
                    add_timer(&timer);

                    // Blocking process
                    if (wait_event_interruptible(info.lista_bloq, timer.expires <= jiffies) != 0)
                        return -ERESTARTSYS;
                    break;
            }
        }

        // Changing parity accordingly
        last_write.parity = (last_write.parity + count) % SOUND_SIZE;

    } else { // Escritura con buffer

        i = 0;

        while (i < count) {
            
            // Copying next chunk of bytes
            cpy_size = min((int) count - i, (int) kfifo_avail(&int_buff));
            ret = kfifo_from_user(&int_buff, buf + i, cpy_size, &copied);

            if (DEBUG) printk(KERN_ALERT "Copiando %d Bytes al buffer interno...", cpy_size);

            i += cpy_size;

            if (kfifo_len(&int_buff) >= SOUND_SIZE) {

                programar_sonido();

                // Blocking process
                if (wait_event_interruptible(info.lista_bloq, timer.expires <= jiffies) != 0)
                    return -ERESTARTSYS;
            }
        }
    }

    // Muting speaker
    spkr_off();

    return count;
}

void interrupcion_temporizador(struct timer_list *timer) {

    spin_lock_bh(&info.irq_slock);

    if (buffersize == 0 || kfifo_len(&int_buff) < SOUND_SIZE)
        wake_up_interruptible(&info.lista_bloq);
    else
        programar_sonido();

    spin_unlock_bh(&info.irq_slock);
}

void programar_sonido(void) {

    // Auxiliary variables
    unsigned int ms = 0, hz = 0, ret;
    bool muted;
    
    // Extracting parameters
    ret = kfifo_out(&int_buff, &ms, 2);
    ret = kfifo_out(&int_buff, &hz, 2);

    // Emmiting sound if not muted
    spkr_set_frequency(hz);

    spin_lock_bh(&info.mute_slock);
    muted = info.muted;
    spin_unlock_bh(&info.mute_slock);

    if (!muted && hz > 0) spkr_on();

    // Wating
    timer.expires = jiffies + msecs_to_jiffies(ms);
    add_timer(&timer); 
}

static long ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

    // Variables
    int aux;

    switch (cmd) {
        case SPKR_SET_MUTE_STATE:
            
            // Getting the integer value
            if (copy_from_user(&aux, (int*) arg, sizeof(int)) != 0)
                return -EFAULT;

            spin_lock_bh(&info.mute_slock);
            if (aux == 0) { // Reactivacion del altavoz
                info.muted = false;
                spkr_on();
            } else { // Desactivacion del altavoz
                info.muted = true;
                spkr_off();
            }
            spin_unlock_bh(&info.mute_slock);
            break;
        case SPKR_GET_MUTE_STATE:

            // Returning the value of muted
            spin_lock_bh(&info.mute_slock);
            aux = info.muted;
            spin_unlock_bh(&info.mute_slock);

            // Setting the return value
            if (copy_to_user((int*) arg, &aux, sizeof(int)) != 0)
                return -EFAULT;
            
            break;
    default:
        return -ENOTTY;
    }
    return 0;
}