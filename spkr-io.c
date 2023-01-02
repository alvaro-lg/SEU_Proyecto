#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/i8253.h>
#include <stdbool.h>

MODULE_AUTHOR("Alvaro Lopez Garcia <alvaro.lopezgar@alumnos.upm.es> &\
 Fabian Villalobos Cayoja <Fabian.villalobos@alumnos.upm.es>");
MODULE_DESCRIPTION("PC Speaker driver");
MODULE_LICENSE("GPL");

#define DEBUG true
#define REG_CTRL    0x43
#define REG_DATA    0x42
#define B_PORT      0x61

static int freq = 500;
module_param(freq, int, S_IRUGO);

static void spkr_on(void);
static void spkr_off(void);
static void spkr_set_frequency(void);

static int __init spkr_init(void) {

    unsigned long flags;
    
    if (DEBUG) printk(KERN_ALERT "Loading module...");
    
    // Critical section
    raw_spin_lock_irqsave(&i8253_lock, flags);
    spkr_on();
    raw_spin_unlock_irqrestore(&i8253_lock, flags);

    return 0;
}

static void __exit spkr_exit(void) {

    if (DEBUG) printk(KERN_ALERT "Unloading module...");

    spkr_off();
}

static void spkr_on(void) {

    // Variables
    uint8_t tmp, ctrl = 0xb6, act_mask = 0x03;

    // Selecting timer and operation mode
    outb_p(REG_CTRL, ctrl);
    if (DEBUG) printk(KERN_ALERT "Escribiendo %#x en %#x", ctrl, REG_CTRL);

    // Writting frecuency params
    spkr_set_frequency();

    // Activating speaker
 	tmp = inb_p(B_PORT);
 	outb_p(B_PORT, tmp | act_mask);
    if (DEBUG) printk(KERN_ALERT "Escribiendo %#x en %#x", tmp | act_mask, B_PORT);
}

static void spkr_off(void) {

    // Variables
    uint8_t tmp, deact_mask = ~0x03;

    // Deactivating speaker
 	tmp = inb_p(B_PORT);
 	outb(B_PORT, tmp & deact_mask);
    if (DEBUG) printk(KERN_ALERT "Escribiendo %#x en %#x", tmp & deact_mask, B_PORT);
}

static void spkr_set_frequency() {

    // Variables
    uint32_t div = PIT_TICK_RATE / freq;

    // Writting parameters
    outb_p(REG_DATA, (uint8_t) (div));
    outb(REG_DATA, (uint8_t) (div >> 8));

    // Debugging messages
    if (DEBUG) printk(KERN_ALERT "Escribiendo %#x en %#x", (uint8_t) (div), REG_DATA);
    if (DEBUG) printk(KERN_ALERT "Escribiendo %#x en %#x", (uint8_t) (div >> 8), REG_DATA);
    if (DEBUG) printk(KERN_ALERT "Reproduciendo sonido a frecuencia %d Hz", freq);
}

module_init(spkr_init);
module_exit(spkr_exit);