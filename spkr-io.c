#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/i8253.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <stdbool.h>

#include "constants.h" // Custom file for sharing constants

// Funciones del modulo
static void spkr_on(void);
static void spkr_off(void);
static void spkr_set_frequency(unsigned int freq);

static void spkr_init(void) {

    uint8_t ctrl_reg_val = 0xb6;

    // Device programming
    outb_p(ctrl_reg_val, REG_CTRL);
}

static void spkr_exit(void) {
    
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

static void spkr_set_frequency(unsigned int freq) {

    // Variables
    uint32_t div = PIT_TICK_RATE / freq;

    if (DEBUG) printk(KERN_ALERT "Reproduciendo sonido a frecuencia de %d Hz", freq);

    // Writting parameters
    outb_p((uint8_t) (div), REG_DATA);
    outb_p((uint8_t) (div >> 8), REG_DATA);
}