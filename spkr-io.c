#include <linux/i8253.h>
#include <linux/io.h>

#include "constants.h" // Custom file for sharing constants

void spkr_on(void) {

    // Variables
    uint8_t tmp, act_mask = 0x03, ctrl_reg_val = 0xb6;

    if (DEBUG) printk(KERN_ALERT "Activating speaker...");

    // Device programming
    outb(ctrl_reg_val, REG_CTRL);

    // Activating speaker
    tmp = inb(B_PORT);
 	outb(tmp | act_mask, B_PORT);
}

void spkr_off(void) {

    // Variables
    uint8_t tmp, deact_mask = ~0x03;

    if (DEBUG) printk(KERN_ALERT "Deactivating speaker...");

    // Deactivating speaker
    tmp = inb(B_PORT);
 	outb(tmp & deact_mask, B_PORT);
}

void spkr_set_frequency(unsigned int freq) {

    // Variables
    uint32_t div;
    unsigned long flags;

    if (DEBUG) printk(KERN_ALERT "Reproduciendo sonido a frecuencia de %d Hz", freq);

    if (freq > 0) {

        div = PIT_TICK_RATE / freq;

        raw_spin_lock_irqsave(&i8253_lock, flags);

        // Writting parameters
        outb_p((uint8_t) (div), REG_DATA);
        outb((uint8_t) (div >> 8), REG_DATA);

        raw_spin_unlock_irqrestore(&i8253_lock, flags);
    } else {
        spkr_off();
    }
}