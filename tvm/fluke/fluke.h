#ifndef FLUKE_H
#define FLUKE_H

#include "tvm.h"
#include "fluke_uart.h"
#include "tvm_uart.h"

#define MT_TVM
#define MT_DEFINES

/* 0MiB - 32MiB SDRAM */
#define SDRAM_BOTTOM  0x00000000  /* address of the base of SDRAM */
#define SDRAM_TOP 0x02000000  /* address of the top of SDRAM */

/* >= 128 KiB dynamic memory */
#define DMEM_START  0x00020000  /* address in SDRAM of dynamic memory */


WORD arm7tdmi_get_time(ECTX ectx);
void sleep_until(WORD timeout);
void sleep(void);

void debug_print_chr (const unsigned char c);
void debug_print_hex (unsigned int val);
void debug_print_str (const char *str);

/* Values grabbed from the fluke firmwares fluke.h */
#define B_RTS       (1 << 18)
#define B_CTS       (1 << 11)
#endif /* FLUKE_H */