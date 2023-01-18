#include <stdbool.h>

#define DEBUG       true
#define COUNT       1
#define WRITE_SIZE  4
#define REG_CTRL    0x43
#define REG_DATA    0x42
#define B_PORT      0x61
#define DEV_NAME    "spkr"
#define CLASS_NAME  "speaker"
#define DEV_TYPE    "int_spkr"

#define SPKR_IOC_MAGIC '9'
#define SPKR_SET_MUTE_STATE _IOW(SPKR_IOC_MAGIC, 1, int)
#define SPKR_GET_MUTE_STATE _IOR(SPKR_IOC_MAGIC, 2, int)