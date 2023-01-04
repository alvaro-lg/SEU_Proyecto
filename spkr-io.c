#include <linux/i8253.h>
#include <linux/io.h>

#include "constants.h" // Custom file for sharing constants

void spkr_on(void) {

    // Variables
    uint8_t tmp, act_mask = 0x03, ctrl_reg_val = 0xb6;

    if (DEBUG) printk(KERN_ALERT "Activating speaker...");

    // Device programming
    outb_p(ctrl_reg_val, REG_CTRL);

    // Activating speaker
    tmp = inb_p(B_PORT);
 	outb_p(tmp | act_mask, B_PORT);
}

void spkr_off(void) {

    // Variables
    uint8_t tmp, deact_mask = ~0x03;

    if (DEBUG) printk(KERN_ALERT "Deactivating speaker...");

    // Deactivating speaker
    tmp = inb_p(B_PORT);
 	outb_p(tmp & deact_mask, B_PORT);
}

void spkr_set_frequency(unsigned int freq) {

    // Variables
    uint32_t div = PIT_TICK_RATE / freq;

    if (DEBUG) printk(KERN_ALERT "Reproduciendo sonido a frecuencia de %d Hz", freq);

    // Writting parameters
    outb_p((uint8_t) (div), REG_DATA);
    outb_p((uint8_t) (div >> 8), REG_DATA);
}