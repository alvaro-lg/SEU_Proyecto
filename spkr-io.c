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

#define DEBUG       true
#define REG_CTRL    0x43
#define REG_DATA    0x42
#define B_PORT      0x61

// Frequency param
static int freq = 1000;
module_param(freq, int, S_IRUGO);

static void spkr_on(void);
static void spkr_off(void);
static void spkr_set_frequency(void);

static int __init spkr_init(void) {

    // Variables
    unsigned long flags;
    uint8_t ctrl_reg_val = 0xb6;

    // Debugging
    if (DEBUG) printk(KERN_ALERT "Loading module...");
    
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

module_init(spkr_init);
module_exit(spkr_exit);