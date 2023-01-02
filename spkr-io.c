#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <stdbool.h>

MODULE_LICENSE("Dual BSD/GPL");
/*MODULE_AUTHOR("Alvaro Lopez Garcia <alvaro.lopezgar@alumnos.upm.es> &\
 Fabian Villalobos Cayoja <Fabian.villalobos@alumnos.upm.es>");*/

#define DEBUG true
#define REG_CTRL    0x43
#define REG_DATA    0x42
#define B_PORT      0x61

// TODO: Remove when done
#define EXAMPLE_FREQ 1000
#define EXAMPLE_TIME 10

static void spkr_on(void);
static void spkr_off(void);
static void spkr_set_frequency(int freq, int time);

static int __init spkr_init(void) {

    if (DEBUG) printk(KERN_ALERT "Module loaded");

    spkr_on();
    spkr_set_frequency(EXAMPLE_FREQ, EXAMPLE_TIME);
    spkr_off();

    return 0;
}

static void __exit spkr_exit(void) {
    if (DEBUG) printk(KERN_ALERT "Module unloaded");
}

static void spkr_on(void) {

    // Variables
    uint8_t tmp, ctrl = 0xb6, act_mask = 0x03;

    // Selecting timer and operation mode
    outb(REG_CTRL, ctrl);

    // Activating speaker
 	tmp = inb(B_PORT);
 	outb(B_PORT, tmp | act_mask);
}

static void spkr_off(void) {

    // Variables
    uint8_t tmp, deact_mask = !0x03;

    // Deactivating speaker
 	tmp = inb(B_PORT);
 	outb(B_PORT, tmp & deact_mask);
}

static void spkr_set_frequency(int freq, int time) {

    // Variables
    uint32_t div = PIT_TICK_RATE / freq;
    
    // Writting parameters
    outb(REG_DATA, div);
    outb(REG_DATA, div >> 8);
}

module_init(spkr_init);
module_exit(spkr_exit);