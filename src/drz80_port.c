#include "drz80_port.h"
#include <string.h>
#include <stdio.h>

unsigned char *z80_readmap[64];
unsigned char *z80_writemap[64];

void (*z80_writemem)(unsigned int address, unsigned char data);
unsigned char (*z80_readmem)(unsigned int address);
void (*z80_writeport)(unsigned int port, unsigned char data);
unsigned char (*z80_readport)(unsigned int port);

UINT8 z80_last_fetch;

// called only if internal xmap rebase fails 
unsigned int dz80_rebase_pc(unsigned short address)
{
    fprintf(stderr,"dz80_rebase_pc %x\n", address);
    //unsigned char* ptr = z80_readmap[address >> 13]; 
    unsigned char* ptr = z80_readmap[(address) >> 10];
    //unsigned char* ptr = z80_readmap[address]; 
    //fprintf(stderr,"ptr %x2\n", ptr, zram[0]);

    drZ80.Z80PC_BASE = (unsigned int) ptr;
    //drZ80.Z80PC = drZ80.Z80PC_BASE + (address & 0x1FFF);
    //fprintf(stderr,"dz80_rebase_pc ok %p\n", drZ80.Z80PC);
    //return drZ80.Z80PC;
    return drZ80.Z80PC_BASE + (address & 0x1FFF);
}

unsigned int dz80_rebase_sp(unsigned short address)
{
    fprintf(stderr, "dz80_rebase_sp %x\n",address);
    unsigned char* ptr = z80_readmap[address >> 13]; 
    drZ80.Z80SP_BASE = (unsigned int ) ptr;
    //drZ80.Z80SP = drZ80.Z80SP_BASE + (address & 0x1FFF);
    //fprintf(stderr,"dz80_rebase_sp ok %p\n", drZ80.Z80SP);
    //return drZ80.Z80SP;
    return drZ80.Z80SP_BASE + (address & 0x1FFF);
}


static void dz80_noop_irq_ack(void) {}

unsigned char* z80_readmem_dummy(unsigned int address)
{
    fprintf(stderr, "z80_writemem_dummy\n");
    return z80_readmap[0];
}


void z80_writemem_dummy(unsigned int address, unsigned char data)
{
    fprintf(stderr, "z80_writemem_dummy\n");
}

unsigned short drZ80_read16(unsigned short address)
{
    fprintf(stderr, "drZ80_read16\n");
    return 0;
}


void drZ80_write16(unsigned short data,unsigned short address)
{
    fprintf(stderr, "drZ80_write16\n");

    //drZ80_write8(data & 0xFF,address);
    //drZ80_write8(data >> 8,address + 1);
}

void drZ80_writeport16(UINT16 port, UINT8 value)
{
    fprintf(stderr, "Write port %d=%d\n",port, value);
}

UINT8 drZ80_readport16(UINT16 port)
{
    fprintf(stderr, "Read port %d\n",port);
    return 0;
}

void z80_init(const void *config, int (*irqcallback)(int))
{
    fprintf(stderr, "z80_init\n");

    memset(&drZ80, 0, sizeof(drZ80));
    //drZ80.Z80SP = 0x1000;
    drZ80.z80_rebasePC = dz80_rebase_pc;
    drZ80.z80_rebaseSP = dz80_rebase_sp;
    drZ80.z80_read8    = (void*)z80_readmem;
    drZ80.z80_read16   = drZ80_read16;
    drZ80.z80_write8   = (void*)z80_writemem;
    drZ80.z80_write16  = drZ80_read16;
    drZ80.z80_in      = z80_readport; 
    drZ80.z80_out     = z80_writeport; 

    drZ80.Z80PC = dz80_rebase_pc(0);
    drZ80.z80_irq_callback = irqcallback;

}

void z80_reset(void)
{
    fprintf(stderr, "z80_reset\n");
  drZ80.Z80A = 0x00		<<24;
  drZ80.Z80F = (1<<2)	<<24;  // set ZFlag
  drZ80.Z80BC = 0x0000	<<16;
  drZ80.Z80DE = 0x0000	<<16;
  drZ80.Z80HL = 0x0000	<<16;
  drZ80.Z80A2 = 0x00	<<24;
  drZ80.Z80F2 = 1<<2	<<24;  // set ZFlag
  drZ80.Z80BC2 = 0x0000	<<16;
  drZ80.Z80DE2 = 0x0000	<<16;
  drZ80.Z80HL2 = 0x0000	<<16;
  drZ80.Z80IX = 0xFFFF	<<16;
  drZ80.Z80IY = 0xFFFF	<<16;
  
  drZ80.Z80I = 0x00;
  drZ80.Z80IM = 0x00;
  drZ80.Z80_IRQ = 0x00;
  drZ80.Z80IF = 0x00;

}


void z80_run(unsigned int cycles)
{
    fprintf(stderr, "z80_run %d\n", cycles);
    int r = DrZ80Run(&drZ80, cycles);
    fprintf(stderr, "z80_run r=%d\n", r);
}

void z80_exit(void)
{
}

void z80_debug(char *dstr)
{
}

void z80_set_irq_line(unsigned int state)
{

    fprintf(stderr, "z80_set_irq_line\n");
    Z80.Z80_IRQ = state;
}

void z80_set_nmi_line(unsigned int state)
{
    fprintf(stderr, "z80_set_nmi_line\n");

}

unsigned char memory[0x10000]; // 64KB of memory

unsigned char read8(unsigned short addr) {
    fprintf(stderr, "z80_read8\n");
    return memory[addr];
}

void write8(unsigned char data, unsigned short addr) {

    fprintf(stderr, "z80_write8\n");
    memory[addr] = data;
}

unsigned char in(unsigned short port) {
    fprintf(stderr, "z80_in\n");
    return 0;
}

void out(unsigned short port, unsigned char data) {

    fprintf(stderr, "z80_out\n");
}

unsigned int rebasePC(unsigned short address) {

    fprintf(stderr, "rebasePC\n");
    drZ80.Z80PC_BASE = (unsigned int)memory;
    drZ80.Z80PC = drZ80.Z80PC_BASE + address;
    return drZ80.Z80PC_BASE + address;
}

unsigned int rebaseSP(unsigned short new_sp) {
    fprintf(stderr, "rebaseSP\n");
    return (unsigned int)new_sp;
}

void z80_main() {
    drZ80.Z80PC = 0;
    drZ80.Z80SP = 0x1000;

    drZ80.z80_read8 = read8;
    drZ80.z80_write8 = write8;
    drZ80.z80_in = in;
    drZ80.z80_out = out;
    drZ80.z80_rebasePC = rebasePC;
    drZ80.z80_rebaseSP = rebaseSP;
    drZ80.Z80PC = rebasePC(0);

    unsigned char rom[] = {
        0x76        // HALT
    };

    memcpy(memory, rom, sizeof(rom));
    fprintf(stderr, "DrZ80Run ...\n");
    DrZ80Run(&drZ80, 1); // Run for 1 cycle

    fprintf(stderr, "DrZ80Run ok\n");
}




