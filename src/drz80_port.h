#ifndef DRZ80_PORT_H
#define DRZ80_PORT_H

#include "osd_cpu.h"
#include "drz80.h"


enum
{
  /* line states */
  CLEAR_LINE = 0, /* clear (a fired, held or pulsed) line */
  ASSERT_LINE,    /* assert an interrupt immediately */
  HOLD_LINE,      /* hold interrupt line until acknowledged */
  PULSE_LINE     /* pulse interrupt line for one instruction */
};



struct DrZ80 drZ80;
#define  Z80 drZ80

extern unsigned char *z80_readmap[64];
extern unsigned char *z80_writemap[64];

extern void (*z80_writemem)(unsigned int address, unsigned char data);
extern unsigned char (*z80_readmem)(unsigned int address);
extern void (*z80_writeport)(unsigned int port, unsigned char data);
extern unsigned char (*z80_readport)(unsigned int port);

extern UINT8 z80_last_fetch;
extern void z80_init(const void *config, int (*irqcallback)(int));
extern void z80_reset(void);

extern void z80_run(unsigned int cycles);

extern void z80_exit(void);

extern void z80_debug(char *dstr);

extern void z80_set_irq_line(unsigned int state);
extern void z80_set_nmi_line(unsigned int state);
#endif
