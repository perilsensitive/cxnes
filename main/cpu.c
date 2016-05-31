/*
  cxNES - NES/Famicom Emulator
  Copyright (C) 2011-2016 Ryan Jackson

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation.; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "emu.h"

/* Allow DMC DMA to only affect $4016/$4017 reads (1)
   or all memory-mapped I/O reads (0). 0 is technically
   more correct, but somewhat glitchy at the moment.
*/
#define DMA_AFFECTS_IO_ONLY 1

#define CPU_MEM_SIZE 0x10000
#define CPU_ADDR_MAX 0xffff
#define CPU_PAGE_SHIFT 10
#define CPU_PAGE_SIZE (1 << CPU_PAGE_SHIFT)
#define CPU_PAGE_MASK  (CPU_PAGE_SIZE - 1)
#define CPU_PAGE_COUNT (CPU_MEM_SIZE / CPU_PAGE_SIZE)

#define N_FLAG (1 << 7)
#define V_FLAG (1 << 6)
#define U_FLAG (1 << 5)
#define B_FLAG (1 << 4)
#define D_FLAG (1 << 3)
#define I_FLAG (1 << 2)
#define Z_FLAG (1 << 1)
#define C_FLAG (1 << 0)

#define IRQ_VECTOR 0xfffe
#define RESET_VECTOR 0xfffc
#define NMI_VECTOR 0xfffa

#define IRQ_FLAG(x) (1 << (x))

enum {
	DMC_DMA_STEP_NONE,
	DMC_DMA_STEP_RDY,
	DMC_DMA_STEP_DUMMY,
	DMC_DMA_STEP_ALIGN,
	DMC_DMA_STEP_XFER,
};

struct cpu_state {
	uint8_t A;
	uint8_t X;
	uint8_t Y;
	uint8_t P;
	uint8_t S;
	uint16_t PC;
	uint8_t data_bus;
	uint16_t address_bus;
	uint32_t cycles;
	uint32_t interrupt_times[IRQ_MAX + 1];
	uint32_t dmc_dma_timestamp;
	uint32_t oam_dma_timestamp;
	uint32_t dma_timestamp;
	int dmc_dma_addr;
	int jammed;
	int interrupts;
	int interrupt_mask;
	int polled_interrupts;
	int step_cycles;
	int board_run_timestamp;
	int resetting;
	int dmc_dma_step;
	int oam_dma_step;
	int oam_dma_addr;
	int is_opcode_fetch;

	cpu_read_handler_t *read_handlers[CPU_MEM_SIZE];
	cpu_write_handler_t *write_handlers[CPU_MEM_SIZE];
	uint8_t *read_pagetable[CPU_PAGE_COUNT];
	uint8_t *write_pagetable[CPU_PAGE_COUNT];
	uint32_t frame_cycles;
	int type;
	int cpu_clock_divider;
	int debug;

	struct emu *emu;
};

static struct state_item cpu_state_items[] = {
	STATE_8BIT(cpu_state, A),
	STATE_8BIT(cpu_state, X),
	STATE_8BIT(cpu_state, Y),
	STATE_8BIT(cpu_state, P),
	STATE_8BIT(cpu_state, S),
	STATE_16BIT(cpu_state, PC),
	STATE_8BIT(cpu_state, data_bus),
	STATE_32BIT(cpu_state, cycles),
	STATE_32BIT_ARRAY(cpu_state, interrupt_times),
	STATE_32BIT(cpu_state, dmc_dma_timestamp),
	STATE_16BIT(cpu_state, dmc_dma_addr),
	STATE_8BIT(cpu_state, jammed), /* BOOLEAN */
	STATE_32BIT(cpu_state, interrupts),
	STATE_32BIT(cpu_state, interrupt_mask),
	STATE_32BIT(cpu_state, polled_interrupts),
	STATE_32BIT(cpu_state, step_cycles),
	STATE_32BIT(cpu_state, board_run_timestamp),
	STATE_8BIT(cpu_state, resetting), /* BOOLEAN */
	STATE_8BIT(cpu_state, dmc_dma_step),
	STATE_16BIT(cpu_state, oam_dma_step),
	STATE_32BIT(cpu_state, frame_cycles),
	STATE_16BIT(cpu_state, oam_dma_addr),
	STATE_32BIT(cpu_state, oam_dma_timestamp),
	STATE_32BIT(cpu_state, dma_timestamp),
	STATE_ITEM_END(),
};

static int cpu_do_oam_dma(struct cpu_state *cpu);
CPU_WRITE_HANDLER(cpu_dma_ppu_oam_write_handler);
void cpu_oam_dma(struct cpu_state *cpu, int addr, int odd_cycle);
static void write_dma_transfer(struct cpu_state *cpu, int addr);
static void read_dma_transfer(struct cpu_state *cpu, int addr);
static inline void update_interrupt_status(struct cpu_state *cpu);

#define push(x) { write_mem(cpu, 0x100 + cpu->S, (x)); cpu->S--; }
#define pop() { cpu->S++; read_mem(cpu, 0x100 + cpu->S); }

#define transfer(x, y) { *(y) = (x); set_zn_flags(*(y)); }
#define increment(x) { (*(x))++; set_zn_flags(*(x)); }
#define decrement(x) { (*(x))--; set_zn_flags(*(x)); }
#define set_zn_flags(value)	\
{				\
	cpu->P = (cpu->P & ~(N_FLAG|Z_FLAG)) | \
		((value) ? (value) & N_FLAG : Z_FLAG); \
}

#define clear_flag(x) { cpu->P &= ~(x); }
#define set_flag(x) { cpu->P |= (x); }
#define jam() { cpu->jammed = 1; printf("jammed (PC: %x opcode %02x)\n", \
					cpu->PC - 1, opcode); }


#define load_imm(y) { *(y) = operand; set_zn_flags(*(y)); cpu->PC++; }

static inline void write_mem(struct cpu_state *cpu, int addr, int value)
{
	addr &= 0xffff;

	if (cpu->dma_timestamp != ~0 &&
	    cpu->cycles >= cpu->dma_timestamp) {
		write_dma_transfer(cpu, addr);	
	}
	cpu->cycles += cpu->cpu_clock_divider;
	if (cpu->write_handlers[addr]) {
		cpu->write_handlers[addr](cpu->emu, addr,
					  value, cpu->cycles);
	} else if (cpu->write_pagetable[addr >> CPU_PAGE_SHIFT]) {
		cpu->write_pagetable[addr >> CPU_PAGE_SHIFT][addr] =
			value;
	}

	cpu->data_bus = value;			
}

static inline void read_mem(struct cpu_state *cpu, int addr)
{
	uint8_t value = cpu->data_bus;

	addr &= 0xffff;

	if (cpu->dma_timestamp != ~0 &&
	    cpu->cycles >= cpu->dma_timestamp) {
		read_dma_transfer(cpu, addr);
	}
	

	cpu->cycles += cpu->cpu_clock_divider;
	if (cpu->read_pagetable[addr >> CPU_PAGE_SHIFT]) {
		value = cpu->read_pagetable[addr >> CPU_PAGE_SHIFT][addr];	
	}
	
	if (cpu->read_handlers[addr]) {
		value = cpu->read_handlers[addr](cpu->emu, addr, value,
						 cpu->cycles);
	}

	cpu->data_bus = value;
}

static inline void load(struct cpu_state *cpu, uint8_t *y)
{
	read_mem(cpu, cpu->address_bus);
	*(y) = cpu->data_bus;
	set_zn_flags(*(y));
}

static inline void abs_addr(struct cpu_state *cpu, uint8_t operand)
{
	read_mem(cpu, cpu->PC + 1);
	cpu->address_bus = operand |
		((uint16_t)cpu->data_bus << 8);
	cpu->PC += 2;
}

static inline void indir_idx_addr_read(struct cpu_state *cpu, uint8_t operand)
{
	cpu->PC++;
	read_mem(cpu, operand);
	cpu->address_bus = cpu->data_bus;
	operand = (operand + 1) & 0xff;
	read_mem(cpu, operand);
	cpu->address_bus |= cpu->data_bus << 8;
	if (((cpu->address_bus + cpu->Y) ^ cpu->address_bus) & 0x100) {
		read_mem(cpu, (cpu->address_bus & 0xff00) |
			 ((cpu->address_bus + cpu->Y) & 0xff));
	}
	cpu->address_bus += cpu->Y;
}

static inline void indir_idx_addr_write(struct cpu_state *cpu, uint8_t operand)
{
	cpu->PC++;
	read_mem(cpu, operand);
	cpu->address_bus = cpu->data_bus;
	operand = (operand + 1) & 0xff;
	read_mem(cpu, operand);
	cpu->address_bus |= cpu->data_bus << 8;
	read_mem(cpu, (cpu->address_bus & 0xff00) |
		 ((cpu->address_bus + cpu->Y) & 0xff));
	cpu->address_bus += cpu->Y;
}

static inline void idx_indir_addr(struct cpu_state *cpu, uint8_t operand)
{
	cpu->PC++;
	read_mem(cpu, operand);
	operand += cpu->X;
	operand &= 0xff;
	read_mem(cpu, operand);
	cpu->address_bus = cpu->data_bus;
	operand++;
	operand &= 0xff;
	read_mem(cpu, operand);
	cpu->address_bus |= cpu->data_bus << 8;
}

static inline void indir_addr(struct cpu_state *cpu, uint8_t operand)
{
	uint16_t tmp;

	tmp = operand;
	cpu->PC++;
	read_mem(cpu, cpu->PC);
	tmp |= cpu->data_bus << 8;

	cpu->PC++;
	read_mem(cpu, tmp);
	cpu->address_bus = cpu->data_bus;
	tmp = (tmp & 0xff00) | ((tmp + 1) & 0xff);
	read_mem(cpu, tmp);
	cpu->address_bus |= cpu->data_bus << 8;
}

static inline void pla(struct cpu_state *cpu)
{
	read_mem(cpu, 0x100 + cpu->S);
	pop();
	cpu->A = cpu->data_bus;
	set_zn_flags(cpu->A);
}

static inline void ora(struct cpu_state *cpu)
{
	read_mem(cpu, cpu->address_bus);
	cpu->A |= cpu->data_bus;
	set_zn_flags(cpu->A);
}

static inline void eor(struct cpu_state *cpu)
{
	read_mem(cpu, cpu->address_bus);
	cpu->A ^= cpu->data_bus;
	set_zn_flags(cpu->A);
}

static inline void ora_imm(struct cpu_state *cpu, uint8_t operand)
{
	cpu->A |= operand;
	set_zn_flags(cpu->A);
	cpu->PC++;
}

static inline void dec(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);		
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);	
	value--;
	write_mem(cpu, cpu->address_bus, value);	
	set_zn_flags(value);
}

static inline void inc(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);		
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);	
	value++;
	write_mem(cpu, cpu->address_bus, value);	
	set_zn_flags(value);
}

/* Note: interrupts are polled after the operand fetch.  In the !condition
   case, that's handled already by the update_interrupt_status() call at the
   bottom of the outer main loop.  For the condition == true case, we handle
   it here and then skip the usual call to update_interrupt_status().
*/
static inline void branch(struct cpu_state *cpu, int operand, int condition)
{
	uint16_t old_pc;

	cpu->PC++;
	old_pc = cpu->PC;
	if (condition) {
		update_interrupt_status(cpu);
		read_mem(cpu, cpu->PC);
		cpu->PC += (int8_t)operand;
		if ((cpu->PC & 0xff00) ^ (old_pc & 0xff00)) {	
			/* PC is now LSB of cpu->PC | MSB of old_pc */
			read_mem(cpu, (old_pc & 0xff00) | (cpu->PC & 0xff));
			update_interrupt_status(cpu);			
			/* PC is now cpu->PC */
		} else {		
			if (cpu->interrupts & (IRQ_IRQ_MASK|IRQ_NMI_MASK))  {
				if (cpu->P & I_FLAG)
					cpu->interrupt_mask = IRQ_NMI_MASK|IRQ_RESET_MASK;
				else
					cpu->interrupt_mask = IRQ_ALL_MASK;
			} else {
				cpu->interrupt_mask = IRQ_RESET_MASK;
			}
		}
		cpu->polled_interrupts = 1;
	}
}

static inline void jsr(struct cpu_state *cpu, uint8_t operand)
{
	uint16_t addr;

	addr = operand;
	cpu->PC++;

	read_mem(cpu, 0x100 + cpu->S);
	push(cpu->PC >> 8);
	push(cpu->PC & 0xff);

	read_mem(cpu, cpu->PC);
	addr |= cpu->data_bus << 8;
	cpu->PC = addr;
}

static inline void rti(struct cpu_state *cpu)
{
	read_mem(cpu, 0x100 + cpu->S);
	pop();
	cpu->P = cpu->data_bus & ~B_FLAG;
	if (cpu->P & I_FLAG)
		cpu->interrupt_mask = IRQ_NMI_MASK|IRQ_RESET_MASK;
	else
		cpu->interrupt_mask = IRQ_ALL_MASK;

	pop();
	cpu->PC = cpu->data_bus;
	pop();
	cpu->PC |= cpu->data_bus << 8;
}

static inline void rts(struct cpu_state *cpu)
{
	read_mem(cpu, 0x100 + cpu->S);

	pop();
	cpu->PC = cpu->data_bus;
	pop();
	cpu->PC |= cpu->data_bus << 8;

	read_mem(cpu, cpu->PC);
	cpu->PC++;
}

/* Jumps, subroutines */

/* Addition / subtraction */
/* (use value ^ 0xff for sbc */

static inline void add(struct cpu_state *cpu, uint8_t value)
{
	uint32_t tmp;

	tmp = cpu->A + value;

	if (cpu->P & C_FLAG)
		tmp++;

	cpu->P &= ~(Z_FLAG|N_FLAG|C_FLAG|V_FLAG);

	/* Set overflow flag */
	if (~(cpu->A ^ value) & (cpu->A ^ tmp) & 0x80)
		cpu->P |= V_FLAG;

	if (tmp & (1 << 8))
	    cpu->P |= C_FLAG;

	cpu->A = tmp;
	set_zn_flags(cpu->A);
}

/* Comparison */
static inline void compare(struct cpu_state *cpu, uint8_t value, uint8_t reg)
{
	int tmp;

	tmp = reg - value;

	cpu->P &= ~(Z_FLAG|N_FLAG|C_FLAG);

	if (!(tmp & (1 << 8)))
	    cpu->P |= C_FLAG;

	set_zn_flags(tmp);
}

static inline void bit(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);		
	value = cpu->data_bus;
	cpu->P &= ~(N_FLAG|V_FLAG|Z_FLAG);
	cpu->P |= value & (N_FLAG|V_FLAG);
	if (!(value & cpu->A))
		cpu->P |= Z_FLAG;
}

static inline void asl_a(struct cpu_state *cpu)
{
	cpu->P = (cpu->P & ~C_FLAG) | ((cpu->A & 0x80) >> 7);	
	cpu->A <<= 1;
	set_zn_flags(cpu->A);
}

static inline void asl(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);		
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);		
	cpu->P = (cpu->P & ~C_FLAG) | ((value & 0x80) >> 7);	
	value <<= 1;
	set_zn_flags(value);
	write_mem(cpu, cpu->address_bus, value);
}

static inline void rol_a(struct cpu_state *cpu)
{
	uint8_t tmp;
	tmp = cpu->P;
	cpu->P = (cpu->P & ~C_FLAG) | ((cpu->A & 0x80) >> 7);
	cpu->A = (cpu->A << 1) | (tmp & C_FLAG);
	set_zn_flags(cpu->A);
}

static inline void rol(struct cpu_state *cpu)
{
	uint8_t value;
	uint8_t tmp;
	read_mem(cpu, cpu->address_bus);		
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);		
	tmp = cpu->P;
	cpu->P = (cpu->P & ~C_FLAG) | ((value & 0x80) >> 7);
	value = (value << 1) | (tmp & C_FLAG);
	set_zn_flags(value);
	write_mem(cpu, cpu->address_bus, value);
}

static inline void ror_a(struct cpu_state *cpu)
{
	uint8_t tmp;
	tmp = cpu->P;
	cpu->P = (cpu->P & ~C_FLAG) | (cpu->A & 0x01);
	cpu->A = (cpu->A >> 1) | ((tmp & C_FLAG) << 7);
	set_zn_flags(cpu->A);
}

static inline void ror(struct cpu_state *cpu)
{
	uint8_t value;
	uint8_t tmp;
	read_mem(cpu, cpu->address_bus);		
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);		
	tmp = cpu->P;
	cpu->P = (cpu->P & ~C_FLAG) | (value & 0x01);
	value = (value >> 1) | ((tmp & C_FLAG) << 7);
	set_zn_flags(value);
	write_mem(cpu, cpu->address_bus, value);
}

static inline void lsr_a(struct cpu_state *cpu)
{
	cpu->P = (cpu->P & ~C_FLAG) | (cpu->A & 0x01);	
	cpu->A >>= 1;
	set_zn_flags(cpu->A);
}

static inline void lsr(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);		
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);		
	cpu->P = (cpu->P & ~C_FLAG) | (value & 0x01);	
	value >>= 1;
	set_zn_flags(value);
	write_mem(cpu, cpu->address_bus, value);
}

static inline void plp(struct cpu_state *cpu)
{
	read_mem(cpu, 0x100 + cpu->S);
	pop();
	cpu->P = cpu->data_bus & ~B_FLAG;
}

static inline void shs(struct cpu_state *cpu, int absaddr)
{
	uint8_t value = cpu->A & cpu->X;
	cpu->S = value;
	value &= (absaddr >> 8) + 1;
	write_mem(cpu, cpu->address_bus, value);
}

static inline void sya(struct cpu_state *cpu, int absaddr)
{
	if ((absaddr & 0xff00) != (cpu->address_bus & 0xff00)) {
		cpu->address_bus &= ((int)(cpu->Y) << 8) | 0xff;
	}
	write_mem(cpu, cpu->address_bus, cpu->Y & (absaddr >> 8));
}

static inline void sxa(struct cpu_state *cpu, int absaddr)
{
	if ((absaddr & 0xff00) != (cpu->address_bus & 0xff00)) {
		cpu->address_bus &= ((int)(cpu->X) << 8) | 0xff;
	}
	write_mem(cpu, cpu->address_bus, cpu->X & (absaddr >> 8));
}

static inline void las(struct cpu_state *cpu)
{
	read_mem(cpu, cpu->address_bus);
	cpu->S &= cpu->data_bus;
	cpu->X = cpu->S;
	cpu->A = cpu->S;
	set_zn_flags(cpu->A);
}

static inline void slo(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);
	cpu->P = (cpu->P & ~C_FLAG) | ((value & 0x80) >> 7);
	value <<= 1;
	write_mem(cpu, cpu->address_bus, value);
	cpu->A |= value;
	set_zn_flags(cpu->A);
}

static inline void sre(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);
	cpu->P = (cpu->P & ~C_FLAG) | (value & 0x01);
	value >>= 1;
	write_mem(cpu, cpu->address_bus, value);
	cpu->A ^= value;
	set_zn_flags(cpu->A);
}

static inline void rla(struct cpu_state *cpu)
{
	uint8_t value, tmp;
	read_mem(cpu, cpu->address_bus);
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);
	tmp = cpu->P;
	cpu->P = (cpu->P & ~C_FLAG) | ((value & 0x80) >> 7);
	value = (value << 1) | (tmp & C_FLAG);
	set_zn_flags(value);
	write_mem(cpu, cpu->address_bus, value);
	cpu->A &= value;
	set_zn_flags(cpu->A);
}

static inline void rra(struct cpu_state *cpu)
{
	uint8_t value, tmp;
	read_mem(cpu, cpu->address_bus);
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);
	tmp = cpu->P;
	cpu->P = (cpu->P & ~C_FLAG) | (value & 0x01);
	value = (value >> 1) | ((tmp & C_FLAG) << 7);
	set_zn_flags(value);
	write_mem(cpu, cpu->address_bus, value);
	add(cpu, value);
}

static inline void dcp(struct cpu_state *cpu)
{
	uint8_t value;
	uint32_t tmp;
	read_mem(cpu, cpu->address_bus);
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);
	value--;
	write_mem(cpu, cpu->address_bus, value);
	tmp = cpu->A - value;

	cpu->P &= ~(Z_FLAG|N_FLAG|C_FLAG);

	if (!(tmp & (1 << 8)))
	    cpu->P |= C_FLAG;

	set_zn_flags(tmp);
}

static inline void isb(struct cpu_state *cpu)
{
	uint8_t value;
	read_mem(cpu, cpu->address_bus);
	value = cpu->data_bus;
	write_mem(cpu, cpu->address_bus, value);
	value++;
	write_mem(cpu, cpu->address_bus, value);
	set_zn_flags(value);

	add(cpu, (value ^ 0xff));
}

static inline void anc(struct cpu_state *cpu, uint8_t value)
{
	cpu->A &= (value);
	cpu->P &= ~(C_FLAG);
	set_zn_flags(cpu->A);
	cpu->P |= (cpu->P >> 7);
}

static inline void sbx(struct cpu_state *cpu, uint8_t value)
{
	uint16_t tmp = cpu->A & cpu->X;

	tmp = (cpu->A & cpu->X) - (value);	
	cpu->X = tmp;

	cpu->P &= ~(Z_FLAG|N_FLAG|C_FLAG);

	if (!(tmp & (1 << 8)))
	    cpu->P |= C_FLAG;

	set_zn_flags(cpu->X);
	cpu->PC++;
}

static inline void sha(struct cpu_state *cpu, int x)
{
	uint8_t value = cpu->A & cpu->X & ((x >> 8) + 1);
	write_mem(cpu, cpu->address_bus, value);
}

static inline void asr(struct cpu_state *cpu, uint8_t x)
{
	cpu->A &= (x);	
	cpu->P = (cpu->P & ~C_FLAG) | (cpu->A & 0x01);
	cpu->A >>= 1;
	set_zn_flags(cpu->A);
}

static inline void arr(struct cpu_state *cpu, uint8_t x)
{
	uint8_t tmp;

	cpu->A &= (x);				
	tmp = cpu->P;
	cpu->P = (cpu->P & ~C_FLAG) | (cpu->A & 0x01);
	cpu->A = (cpu->A >> 1) | ((tmp & C_FLAG) << 7);
	set_zn_flags(cpu->A);
	cpu->P &= ~(C_FLAG|V_FLAG);
	if (cpu->A & (1 << 6))
		cpu->P |= C_FLAG;

	if (((cpu->A & (1 << 6)) >> 1) ^ (cpu->A & (1 << 5)))
		cpu->P |= V_FLAG;
}

uint32_t cpu_get_cycles(struct cpu_state *cpu)
{
	return cpu->cycles;
}

uint32_t cpu_get_frame_cycles(struct cpu_state * cpu)
{
	return cpu->frame_cycles;
}

void cpu_set_read_handler(struct cpu_state *cpu, int addr, size_t size,
			  int mask, cpu_read_handler_t * h)
{
	int i;

	if (addr < 0 || addr >= CPU_MEM_SIZE)
		return;

	for (i = addr; i < addr + size; i++) {
		if (i >= CPU_MEM_SIZE)
			break;

		if (!mask || ((i & mask) == addr))
			cpu->read_handlers[i] = h;
	}
}

cpu_read_handler_t *cpu_get_read_handler(struct cpu_state *cpu, int addr)
{
	if (addr < 0 || addr > 0xffff)
		return NULL;

	return cpu->read_handlers[addr];
}

void cpu_set_write_handler(struct cpu_state *cpu, int addr, size_t size,
			   int mask, cpu_write_handler_t * h)
{
	int i;

	if (addr < 0 || addr >= CPU_MEM_SIZE)
		return;

	for (i = addr; i < addr + size; i++) {
		if (i >= CPU_MEM_SIZE)
			break;

		if (!mask || ((i & mask) == addr)) {
			/* printf("mask: %x i: %x addr: %x (i&mask): %x\n", */
			/*        mask, i, addr, i&mask); */

			cpu->write_handlers[i] = h;
		}
	}
}

cpu_write_handler_t *cpu_get_write_handler(struct cpu_state *cpu, int addr)
{
	if (addr < 0 || addr > 0xffff)
		return NULL;

	return cpu->write_handlers[addr];
}

static void read_dma_transfer(struct cpu_state *cpu, int addr)
{
	uint8_t data;
#if 1
	/* FIXME should only be ==, not >= */
	if (cpu->cycles > cpu->dma_timestamp) {
		log_err("DEBUG read_mem: cycles should never be greater "
			"than dma timestamp (%d vs %d)\n", cpu->cycles,
			cpu->dma_timestamp);
	}
#endif

	if (cpu->dmc_dma_step == DMC_DMA_STEP_NONE)
		cpu->dmc_dma_step = DMC_DMA_STEP_RDY;


	/* FIXME do actual read_mem() calls */
	switch (cpu->dmc_dma_step) {
	case DMC_DMA_STEP_NONE:
		break;
	case DMC_DMA_STEP_RDY:
		cpu->dmc_dma_step = DMC_DMA_STEP_DUMMY;
		cpu->dmc_dma_timestamp += cpu->cpu_clock_divider;
		cpu->dma_timestamp += cpu->cpu_clock_divider;

		if (cpu->oam_dma_step >= 256) {
			/* FIXME should be read_mem() with side effects */
			cpu->cycles += cpu->cpu_clock_divider;
		} else {
			break;
		}
	case DMC_DMA_STEP_DUMMY:
		cpu->dmc_dma_step = DMC_DMA_STEP_ALIGN;
		cpu->dmc_dma_timestamp += cpu->cpu_clock_divider;
		cpu->dma_timestamp += cpu->cpu_clock_divider;

		if (cpu->oam_dma_step >= 256) {
			/* FIXME should be read_mem() with side effects */
			cpu->cycles += cpu->cpu_clock_divider;
		} else {
			break;
		}
	case DMC_DMA_STEP_ALIGN:
		cpu->dmc_dma_step = DMC_DMA_STEP_XFER;
		cpu->dmc_dma_timestamp += cpu->cpu_clock_divider;
		cpu->dma_timestamp += cpu->cpu_clock_divider;

		if (cpu->oam_dma_step >= 256) {
			/* FIXME should be read_mem() with side effects */
			cpu->cycles += cpu->cpu_clock_divider;
		} else {
			break;
		}
	case DMC_DMA_STEP_XFER:
		cpu->dmc_dma_step = DMC_DMA_STEP_NONE;
		cpu->dmc_dma_timestamp = ~0;
		cpu->dma_timestamp = ~0;
		read_mem(cpu, cpu->dmc_dma_addr);

		data = cpu->data_bus;
		apu_dmc_load_buf(cpu->emu->apu, data, &cpu->dmc_dma_timestamp,
				 &cpu->dmc_dma_addr, cpu->cycles);

		if (cpu->oam_dma_timestamp < cpu->dmc_dma_timestamp)
			cpu->dma_timestamp = cpu->oam_dma_timestamp;
		else
			cpu->dma_timestamp = cpu->dmc_dma_timestamp;

		if (cpu->oam_dma_step < 256) {
			/* Re-align to finish OAM DMA  */
			/* FIXME should be read_mem() with side effects */
			cpu->cycles += cpu->cpu_clock_divider;
		}
		break;
	}
}

static void write_dma_transfer(struct cpu_state *cpu, int addr)
{
#if 1
	/* FIXME should only be ==, not >= */
	if (cpu->cycles > cpu->dma_timestamp) {
		log_err("DEBUG write_mem: cycles should never be greater "
			"than dma timestamp (%d vs %d)\n", cpu->cycles,
			cpu->dma_timestamp);
	}
#endif

	if (cpu->dmc_dma_step == DMC_DMA_STEP_NONE)
		cpu->dmc_dma_step = DMC_DMA_STEP_RDY;

	switch (cpu->dmc_dma_step) {
	case DMC_DMA_STEP_NONE:
		break;
	case DMC_DMA_STEP_RDY:
		cpu->dmc_dma_timestamp += cpu->cpu_clock_divider;
		cpu->dma_timestamp += cpu->cpu_clock_divider;
		cpu->dmc_dma_step = DMC_DMA_STEP_DUMMY;
		break;
	case DMC_DMA_STEP_DUMMY:
		cpu->dmc_dma_timestamp += cpu->cpu_clock_divider;
		cpu->dma_timestamp += cpu->cpu_clock_divider;
		cpu->dmc_dma_step = DMC_DMA_STEP_ALIGN;
		break;
	case DMC_DMA_STEP_ALIGN:
		cpu->dmc_dma_timestamp += cpu->cpu_clock_divider;
		cpu->dma_timestamp += cpu->cpu_clock_divider;
		cpu->dmc_dma_step = DMC_DMA_STEP_XFER;
		break;
	case DMC_DMA_STEP_XFER:
		/* Shouldn't ever get here */
		break;
	}
}

void cpu_set_pagetable_entry(struct cpu_state *cpu, int page,
			     int size, uint8_t * data, int rw)
{
	uint8_t *ptr;

	ptr = data;
	if (ptr)
		ptr -= page;

	page >>= CPU_PAGE_SHIFT;

	if (size < CPU_PAGE_SIZE) {
		size = CPU_PAGE_SIZE;
		ptr = NULL;
	}

	while (size) {
		if (rw & CPU_PAGE_READ)
			cpu->read_pagetable[page] = ptr;

		if (rw & CPU_PAGE_WRITE)
			cpu->write_pagetable[page] = ptr;

		page++;
		size -= CPU_PAGE_SIZE;
	}
}

uint8_t cpu_peek(struct cpu_state *cpu, int addr)
{
	/* if (read_handlers[addr]) { */
	/*      return read_handlers[addr](addr, cpu->data_bus, cpu->cycles); */
	/* } */
	/* else */ if (cpu->read_pagetable[addr >> CPU_PAGE_SHIFT]) {
		return cpu->read_pagetable[addr >> CPU_PAGE_SHIFT][addr];
	}

	return cpu->data_bus;
}

static inline void calculate_step_cycles(struct cpu_state *cpu)
{
	int i;
	uint32_t next_time = cpu->frame_cycles;

	for (i = IRQ_NMI; i < IRQ_MAX + 1; i++) {
		if (cpu->interrupt_times[i] < next_time)
			next_time = cpu->interrupt_times[i];
	}

	if (cpu->board_run_timestamp < next_time)
		next_time = cpu->board_run_timestamp;

	cpu->step_cycles = next_time;
}

/* IRQ implementation note:

   The timstamps in interrupt_times[] indicate the first possible
   cycle that the cpu could poll the level/edge detectors and decide
   to handle the interrupt.  Technically the IRQ/NMI line goes low
   during or before phi1 of the previous cycle and is detected by the
   level/edge detectors during phi2.

   update_interrupt_status() must be called *after* any cycle where
   the detectors would be polled.
 */
static inline void update_interrupt_status(struct cpu_state *cpu)
{
	int i;
	int recalc;

	recalc = 0;

	for (i = IRQ_NMI; i < IRQ_MAX + 1; i++) {
		if (cpu->cycles > cpu->interrupt_times[i]) {
			if (!cpu->polled_interrupts) {
				cpu->interrupts |= IRQ_FLAG(i);
				cpu->interrupt_times[i] = ~0;
			} else {
				/* The previous instruction already
				   polled for interrupts, so don't do
				   it again here.  It will be at least
				   one more cycle until the detectors
				   will be polled again, so adjust
				   timestamps accordingly.
				*/
				cpu->interrupt_times[i] +=
					cpu->cpu_clock_divider;
			}
			recalc = 1;
		}
	}

	if (recalc)
		calculate_step_cycles(cpu);

}

#define IDX_IND 0
#define ZP   1
#define IMM  2
#define ABS  3
#define IND_IDX 4
#define IDX 5
#define ABS_X 6
#define ABS_Y 7
#define REL   8
#define IMP   9
#define IND   10

static void decode_opcode(struct cpu_state *cpu, int addr)
{
	uint8_t opcode, operand0, operand1;
	char *mnemonic;
	const char *fmt = NULL;
	int mode;
	int opcode_addr = addr;

	/* (opcode & 0x0f) == 0x0a */
	/* Mode is implied */
	char *misc_instructions[16] = {
		"ASL", NULL, "ROL", NULL, "LSR", NULL, "ROR", NULL,
		"TXA", "TXS", "TAX", "TSX", "DEX", NULL, "NOP", NULL,
	};

	/* opcode & 0x0f == 0x00 */
	char *branches_subroutines[16] = {
		"BRK", "BPL", "JSR", "BMI", "RTI", "BVC", "RTS", "BVS",
		NULL, "BCC", "LDY", "BCS", "CPY", "BNE", "CPX", "BEQ",
	};

	int branches_subroutines_modes[16] = {
		IMP, REL, ABS, REL, IMP, REL, IMP, REL,
		-1, REL, IMM, REL, IMM, REL, IMM, REL,
	};

	/* opcode & 0x0f == 0x08 */
	/* mode is implied */
	char *flags_stack[16] = {
		"PHP", "CLC", "PLP", "SEC", "PHA", "CLI", "PLA", "SEI",
		"DEY", "TYA", "TAY", "CLV", "INY", "CLD", "INX", "SED",
	};

	/* indexed by [opcode & 0x3][(opcode & 0xe0) >> 5] */
	char *normal_opcodes[3][8] = {
		{NULL, "BIT", "JMP", "JMP", "STY", "LDY", "CPY", "CPX"},
		{"ORA", "AND", "EOR", "ADC", "STA", "LDA", "CMP", "SBC"},
		{"ASL", "ROL", "LSR", "ROR", "STX", "LDX", "DEC", "INC"},
	};

	/* indexed by [opcode & 0x3][(opcode & 0x1c) >> 2] */
	int normal_addr_modes[3][8] = {
		{IMM, ZP, -1, ABS, -1, IDX, -1, ABS_X},
		{IDX_IND, ZP, IMM, ABS, IND_IDX, IDX, ABS_Y, ABS_X},
		{IMM, ZP, IMP, ABS, -1, IDX, -1, ABS_X},
	};

	mnemonic = NULL;
	mode = -1;
	opcode = cpu_peek(cpu, addr);
	operand0 = cpu_peek(cpu, addr + 1);
	operand1 = cpu_peek(cpu, addr + 2);

	switch (opcode & 0x0f) {
	case 0x00:
		mnemonic = branches_subroutines[(opcode & 0xf0) >> 4];
		mode = branches_subroutines_modes[(opcode & 0xf0) >> 4];
		break;
	case 0x08:
		mnemonic = flags_stack[(opcode & 0xf0) >> 4];
		mode = IMP;
		break;
	case 0x0a:
		mnemonic = misc_instructions[(opcode & 0xf0) >> 4];
		mode = IMP;
		break;
	default:
		if ((opcode & 0x03) < 3) {
			mnemonic =
			    normal_opcodes[opcode & 0x03][(opcode & 0xe0) >> 5];
			mode =
			    normal_addr_modes[opcode & 0x03][(opcode & 0x1c) >>
							     2];
			if (opcode == 0x6c)
				mode = IND;
		}
	}

	switch (mode) {
	case IDX_IND:
		fmt = "% 7d  $%04X:  %s  ($%02hX,X)";
		addr = operand0;
		break;
	case ZP:
		fmt = "% 7d  $%04X:  %s  $%02hX    ";
		addr = operand0;
		break;
	case IMM:
		fmt = "% 7d  $%04X:  %s  #$%02hX   ";
		addr = operand0;
		break;
	case ABS:
		fmt = "% 7d  $%04X:  %s  $%04X  ";
		addr = (operand1 << 8) | operand0;
		break;
	case IND_IDX:
		fmt = "% 7d  $%04X:  %s  ($%02hX),Y";
		addr = operand0;
		break;
	case IND:
		fmt = "% 7d  $%04X:  %s  ($%04hX)  ";
		addr = (operand1 << 8) | operand0;
		break;
	case IDX:
		fmt = "% 7d  $%04X:  %s  $%02hX,X  ";
		addr = operand0;
		break;
	case ABS_X:
		fmt = "% 7d  $%04X:  %s  $%04X,X";
		addr = (operand1 << 8) | operand0;
		break;
	case ABS_Y:
		fmt = "% 7d  $%04X:  %s  $%04X,Y";
		addr = (operand1 << 8) | operand0;
		break;
	case REL:
		fmt = "% 7d  $%04X:  %s  $%04X  ";
		addr = (addr + 2 + (int8_t) operand0);
		break;
	}

//      printf("opcode: %x\n", opcode);
	if (mode != -1) {
		if (mode == IMP)
			printf("% 7d  $%04X:  %s         ", cpu->cycles,
			       opcode_addr, mnemonic);
		else
			printf(fmt, cpu->cycles, opcode_addr, mnemonic, addr);
	} else {
		printf("% 7d  $%04x:  unknown opcode %02x", cpu->cycles,
		       opcode_addr, opcode);
	}
	printf("  A:$%02X  X:$%02X  Y:$%02X  S:$%02X  P:%c%c%c%c%c%c%c%c",
	       cpu->A, cpu->X, cpu->Y, cpu->S,
	       cpu->P & N_FLAG ? 'N' : '.',
	       cpu->P & V_FLAG ? 'V' : '.',
	       cpu->P & U_FLAG ? 'U' : '.',
	       cpu->P & B_FLAG ? 'B' : '.',
	       cpu->P & D_FLAG ? 'D' : '.',
	       cpu->P & I_FLAG ? 'I' : '.',
	       cpu->P & Z_FLAG ? 'Z' : '.', cpu->P & C_FLAG ? 'C' : '.');

	printf("\n");
}

static void brk(struct cpu_state *cpu)
{
	int vector;
	uint32_t start_cycles;

	start_cycles = cpu->cycles - cpu->cpu_clock_divider;

	if (cpu->interrupts & IRQ_RESET_MASK) {
		/* memset(cpu->interrupt_times, 0xff, */
		/*        sizeof(cpu->interrupt_times)); */
		vector = RESET_VECTOR;
		cpu->P &= ~B_FLAG;

		/* FIXME do we clear other interrupts as well? */
		cpu->interrupts &= ~IRQ_RESET_MASK;

		/* "Push" PC and flags onto stack (except
		   we're reading, not writing) */
		read_mem(cpu, 0x100 + cpu->S);
		cpu->S--;
		read_mem(cpu, 0x100 + cpu->S);
		cpu->S--;
		read_mem(cpu, 0x100 + cpu->S);
		cpu->S--;
		if (cpu->debug)
			printf("RESET %d\n", start_cycles);
	} else {
		/* If actual BRK, move PC to next opcode */
		if (cpu->P & B_FLAG)
			cpu->PC++;

		/* Push PC onto stack */
		push(cpu->PC >> 8);
		push(cpu->PC & 0xff);

		/* Push flags onto stack and clear B flag if set */
		push(cpu->P | U_FLAG);

		update_interrupt_status(cpu);

		/* Jump to NMI vector if NMI line low, IRQ vector otherwise */
		if (cpu->interrupts & IRQ_NMI_MASK) {
			vector = NMI_VECTOR;
			if (cpu->debug) {
				printf("NMI (%x) %d\n",
				       cpu->interrupts & IRQ_NMI_MASK,
				       start_cycles);
			}
		} else {
			vector = IRQ_VECTOR;

			//cpu->interrupts &= ~(1 << 31);
			if (cpu->debug) {
				printf("%s (%x) %d\n",
				       cpu->P & B_FLAG ? "BRK" : "IRQ",
				       cpu->interrupts & IRQ_IRQ_MASK,
				       start_cycles);
			}
		}
		cpu->P &= ~B_FLAG;
		/* ACK any NMIs if set (hack to simulate edge-triggered behavior) */
		cpu->interrupts &= ~IRQ_NMI_MASK;
	}

	cpu->P |= I_FLAG;
	cpu->interrupt_mask = IRQ_RESET_MASK;

	/* Get PC from appropriate vector */
	read_mem(cpu, vector);
	cpu->PC = cpu->data_bus;
	read_mem(cpu, vector + 1);
	cpu->PC |= cpu->data_bus << 8;
}

void cpu_reset(struct cpu_state *cpu, int hard)
{
	int i;

	if (hard) {
		cpu->A = 0;
		cpu->X = 0;
		cpu->Y = 0;
		cpu->P = 0;
		cpu->S = 0;
		cpu->PC = 0;
		cpu->data_bus = 0;
		cpu->cycles = 0;
		cpu->dmc_dma_addr = 0;
		cpu->jammed = 0;
		cpu->polled_interrupts = 0;
		cpu->interrupts = 0;
		cpu->interrupt_mask = 0;
		cpu->step_cycles = 0;
		cpu->board_run_timestamp = ~0;
		cpu->resetting = 0;
		memset(cpu->interrupt_times, 0xff,
		       sizeof(cpu->interrupt_times));
	}

	for (i = 0; i <= IRQ_MAX; i++) {
		if (cpu->interrupt_times[i] != ~0)
			cpu->interrupt_times[i] -= cpu->cycles;
	}

	cpu->oam_dma_step = 256;
	cpu->jammed = 0;
	cpu->cycles = 0;
	cpu->resetting = 1;
	cpu->dmc_dma_timestamp = ~0;
	cpu->oam_dma_timestamp = ~0;
	cpu->dma_timestamp = ~0;
	cpu->dmc_dma_step = DMC_DMA_STEP_NONE;

}

void cpu_set_trace(struct cpu_state *cpu, int enabled)
{
	if (enabled < 0)
		enabled = !cpu->debug;

	cpu->debug = enabled;
}

int cpu_apply_config(struct cpu_state *cpu)
{
	cpu->debug = cpu->emu->config->cpu_trace_enabled;

	return 0;
}

void cpu_board_run_schedule(struct cpu_state *cpu, uint32_t cycles)
{
	if (cpu->board_run_timestamp == ~0) {
		cpu->board_run_timestamp = cycles;
	}

	if (cycles < cpu->step_cycles)
		cpu->step_cycles = cycles;
}

void cpu_board_run_cancel(struct cpu_state *cpu)
{
	uint32_t old;

	old = cpu->board_run_timestamp;
	cpu->board_run_timestamp = ~0;

	if (old == cpu->step_cycles)
		calculate_step_cycles(cpu);

}

void cpu_interrupt_schedule(struct cpu_state *cpu, unsigned int intr,
			    uint32_t cycles)
{
	if (intr == IRQ_RESET || intr > IRQ_MAX)
		return;

	if (cpu->interrupt_times[intr] == ~0 &&
	    !(cpu->interrupts & IRQ_FLAG(intr))) {
		cycles += cpu->cpu_clock_divider;
		//if (intr == IRQ_APU_FRAME)
		//	printf("scheduling for %d\n", cycles);
		cpu->interrupt_times[intr] = cycles;
		if (cycles < cpu->step_cycles)
			cpu->step_cycles = cycles;
	}

}

void cpu_interrupt_cancel(struct cpu_state *cpu, unsigned int intr)
{
	uint32_t cycles;

	if (intr >= IRQ_MAX + 1)
		return;

	cycles = cpu->interrupt_times[intr];
	cpu->interrupt_times[intr] = ~0;

	if (cycles == cpu->step_cycles) {
//              printf("cancelling: %d %d %d\n", intr, cpu->interrupt_times[intr], cpu->cycles);
		calculate_step_cycles(cpu);
	}
}

int cpu_interrupt_ack(struct cpu_state *cpu, unsigned int intr)
{
	if (cpu->interrupts & IRQ_FLAG(intr)) {
		cpu->interrupts &= ~IRQ_FLAG(intr);
		return 1;
	}

	return 0;
}

void cpu_set_type(struct cpu_state *cpu, int type)
{
	switch (type) {
	case CPU_TYPE_RP2A03:
		cpu->cpu_clock_divider = 12;
		log_dbg("CPU: RP2A03\n");
		break;
	case CPU_TYPE_RP2A07:
		cpu->cpu_clock_divider = 16;
		log_dbg("CPU: RP2A07\n");
		break;
	case CPU_TYPE_DENDY:
		cpu->cpu_clock_divider = 15;
		log_dbg("CPU: DENDY\n");
		break;
	default:
		log_dbg("invalid cpu type %d\n", type);
	}

	cpu->frame_cycles = 0;
	cpu->type = type;
	cpu->emu->cpu_clock_divider = cpu->cpu_clock_divider;
}

int cpu_get_type(struct cpu_state *cpu)
{
	return cpu->type;
}

int cpu_init(struct emu *emu)
{
	struct cpu_state *cpu;
	int addr;

	cpu = malloc(sizeof(*cpu));
	if (!cpu)
		return 0;

	emu->cpu = cpu;
	cpu->emu = emu;
	cpu->type = -1;

	for (addr = 0; addr < CPU_MEM_SIZE; addr++) {
		cpu->read_handlers[addr] = NULL;
		cpu->write_handlers[addr] = NULL;
	}

	return 1;
}

void cpu_cleanup(struct cpu_state *cpu)
{
	cpu->emu->cpu = NULL;
	free(cpu);
}

void cpu_set_frame_cycles(struct cpu_state *cpu, uint32_t cycles)
{
	if (cpu->frame_cycles != cycles) {
		cpu->frame_cycles = cycles;
		calculate_step_cycles(cpu);
	}
}

uint32_t cpu_run(struct cpu_state *cpu)
{
	while (cpu->cycles < cpu->frame_cycles) {
		calculate_step_cycles(cpu);
		/* printf("ni: %d, cycles: %d\n", */
		/*        cpu->step_cycles, cpu->cycles); */

		while (cpu->cycles <= cpu->step_cycles) {
			uint8_t opcode;
			uint8_t operand;
			uint16_t absaddr = 0;
			unsigned int index;

			cpu->address_bus = 0;

			if (cpu->oam_dma_step < 256) {
				if (cpu_do_oam_dma(cpu))
					goto end_of_frame;
			}

			cpu->polled_interrupts = 0;

			if (cpu->jammed) {
				cpu->cycles = cpu->frame_cycles;
				break;
			}

			if (cpu->resetting) {
				cpu->resetting = 0;
				cpu->interrupts |= IRQ_RESET_MASK;
				cpu->interrupt_mask |= IRQ_RESET_MASK;
				continue;
			}

			if (cpu->debug & !(cpu->interrupts & IRQ_RESET_MASK))
				decode_opcode(cpu, cpu->PC);
			cpu->is_opcode_fetch = 1;
			read_mem(cpu, cpu->PC);
			cpu->is_opcode_fetch = 0;
			opcode = cpu->data_bus;

			/* NMI, IRQ and RESET are all handled by the logic for BRK */
			if (cpu->interrupts & cpu->interrupt_mask) {
				opcode = 0x00;
			} else {
				cpu->PC++;
				if (opcode == 0x00)
					cpu->P |= B_FLAG;
			}

			read_mem(cpu, cpu->PC);
			operand = cpu->data_bus;

			if (cpu->P & I_FLAG)
				cpu->interrupt_mask =
				    IRQ_NMI_MASK | IRQ_RESET_MASK;
			else
				cpu->interrupt_mask = IRQ_ALL_MASK;

			/*
			   Most opcodes are handled by by first calling the relevant
			   address mode macro to calculate the address and store it in
			   'addr', then passing it to the function or macro that
			   actually does the operation (like and, eor, bit, etc.).
			 */

			/* Left out the opcode mnemonics to keep this short.  It should
			   be reasonably clear from the code what each opcode does and
			   what its addressing mode is. */
			switch (opcode) {
			case 0x0b:
			case 0x2b:
				anc(cpu, operand);
				cpu->PC++;
				continue;
			case 0x69:
				add(cpu, operand);
				cpu->PC++;
				continue;
			case 0x29:
				/* AND (immediate) */
				cpu->A &= operand;
				set_zn_flags(cpu->A);
				cpu->PC++;
				continue;
			case 0x6b:
				arr(cpu, operand);
				cpu->PC++;
				continue;
			case 0x0a:
				asl_a(cpu);
				continue;
			case 0x4b:
				asr(cpu, operand);
				cpu->PC++;
				continue;
			case 0x90:
				branch(cpu, operand, !(cpu->P & C_FLAG));
				continue;
			case 0xb0:
				branch(cpu, operand, cpu->P & C_FLAG);
				continue;
			case 0x10:
				branch(cpu, operand, !(cpu->P & N_FLAG));
				continue;
			case 0x30:
				branch(cpu, operand, cpu->P & N_FLAG);
				continue;
			case 0x50:
				branch(cpu, operand, !(cpu->P & V_FLAG));
				continue;
			case 0x70:
				branch(cpu, operand, cpu->P & V_FLAG);
				continue;
			case 0xd0:
				branch(cpu, operand, !(cpu->P & Z_FLAG));
				continue;
			case 0xf0:
				branch(cpu, operand, cpu->P & Z_FLAG);
				continue;
			case 0x00:
				brk(cpu);
				continue;
			case 0x18:
				clear_flag(C_FLAG);
				continue;
			case 0xd8:
				clear_flag(D_FLAG);
				continue;
			case 0x58:
				clear_flag(I_FLAG);
				continue;
			case 0xb8:
				clear_flag(V_FLAG);
				continue;
			case 0x38:
				set_flag(C_FLAG);
				continue;
			case 0xf8:
				set_flag(D_FLAG);
				continue;
			case 0x78:
				set_flag(I_FLAG);
				continue;

			case 0x1a:
			case 0x3a:
			case 0x5a:
			case 0x7a:
			case 0xda:
			case 0xea:
			case 0xfa:
				continue;

			case 0xc9:
				compare(cpu, operand, cpu->A);
				cpu->PC++;
				continue;
			case 0xe0:
				compare(cpu, operand, cpu->X);
				cpu->PC++;
				continue;
			case 0xc0:
				compare(cpu, operand, cpu->Y);
				cpu->PC++;
				continue;
			case 0x8b:
				cpu->A = cpu->X;
				cpu->A &= operand;
				set_zn_flags(cpu->A);
				cpu->PC++;
				continue;
			case 0x80:
			case 0x82:
			case 0x89:
			case 0xc2:
			case 0xe2:
				cpu->PC++;
				continue;
			case 0x9a:
				cpu->S = cpu->X;
				continue;
			case 0xca:
				decrement(&cpu->X);
				continue;
			case 0x88:
				decrement(&cpu->Y);
				continue;
			case 0x49:
				cpu->A ^= operand;
				set_zn_flags(cpu->A);
				cpu->PC++;
				continue;
			case 0xe8:
				increment(&cpu->X);
				continue;
			case 0xc8:
				increment(&cpu->Y);
				continue;
			case 0x02:
			case 0x12:
			case 0x22:
			case 0x32:
			case 0x42:
			case 0x52:
			case 0x62:
			case 0x72:
			case 0x92:
			case 0xb2:
			case 0xd2:
			case 0xf2:
				jam();
				continue;
			case 0x20:
				jsr(cpu, operand);
				continue;
			case 0xa9:
				load_imm(&cpu->A);
				continue;
			case 0xab:
				load_imm(&cpu->A);
				cpu->X = cpu->A;
				continue;
			case 0xa2:
				load_imm(&cpu->X);
				continue;
			case 0xa0:
				load_imm(&cpu->Y);
				continue;
			case 0x4a:
				lsr_a(cpu);
				continue;
			case 0x09:
				ora_imm(cpu, operand);
				continue;
			case 0x48:
				/* PHA */
				push(cpu->A);
				continue;
			case 0x08:
				/* PHP */
				push(cpu->P|B_FLAG|U_FLAG);
				continue;
			case 0x68:
				pla(cpu);
				continue;
			case 0x28:
				plp(cpu);
				continue;
			case 0x2a:
				rol_a(cpu);
				continue;
			case 0x6a:
				ror_a(cpu);
				continue;
			case 0x40:
				rti(cpu);
				continue;
			case 0x60:
				rts(cpu);
				continue;
			case 0xe9:
			case 0xeb:
				/* SBC (immediate) */
				add(cpu, operand ^ 0xff);
				cpu->PC++;
				continue;
			case 0xcb:
				sbx(cpu, operand);
				continue;
			case 0xaa:
				transfer(cpu->A, &cpu->X);
				continue;
			case 0xa8:
				transfer(cpu->A, &cpu->Y);
				continue;
			case 0xba:
				transfer(cpu->S, &cpu->X);
				continue;
			case 0x8a:
				transfer(cpu->X, &cpu->A);
				continue;
			case 0x98:
				transfer(cpu->Y, &cpu->A);
				continue;

			case 0x01:
			case 0x03:
			case 0x21:
			case 0x23:
			case 0x41:
			case 0x43:
			case 0x61:
			case 0x63:
			case 0x81:
			case 0x83:
			case 0xa1:
			case 0xa3:
			case 0xc1:
			case 0xc3:
			case 0xe1:
			case 0xe3:
				idx_indir_addr(cpu, operand);
				break;

			case 0x11:
			case 0x31:
			case 0x51:
			case 0x71:
			case 0xb1:
			case 0xb3:
			case 0xd1:
			case 0xf1:
				indir_idx_addr_read(cpu, operand);
				break;

			case 0x13:
			case 0x33:
			case 0x53:
			case 0x73:
			case 0x91:
			case 0x93:
			case 0xd3:
			case 0xf3:
				indir_idx_addr_write(cpu, operand);
				break;

			case 0x6c:
				indir_addr(cpu, operand);
				break;

			case 0x1b:
			case 0x3b:
			case 0x5b:
			case 0x7b:
			case 0x99:
			case 0x9b:
			case 0x9e:
			case 0x9f:
			case 0xdb:
			case 0xfb:
				index = cpu->Y;
				goto calc_abs_idx_addr_write;
			case 0x1e:
			case 0x1f:
			case 0x3e:
			case 0x3f:
			case 0x5e:
			case 0x5f:
			case 0x7e:
			case 0x7f:
			case 0x9c:
			case 0x9d:
			case 0xde:
			case 0xdf:
			case 0xfe:
			case 0xff:
				index = cpu->X;
			calc_abs_idx_addr_write:
				abs_addr(cpu, operand);
				absaddr = cpu->address_bus;
				read_mem(cpu, (cpu->address_bus & 0xff00) |
					 ((cpu->address_bus + index) & 0xff));
				cpu->address_bus += index;
				break;

			case 0x19:
			case 0x39:
			case 0x59:
			case 0x79:
			case 0xb9:
			case 0xbb:
			case 0xbe:
			case 0xbf:
			case 0xd9:
			case 0xf9:
				index = cpu->Y;
				goto calc_abs_idx_addr_read;
			case 0x1c:
			case 0x1d:
			case 0x3c:
			case 0x3d:
			case 0x5c:
			case 0x5d:
			case 0x7c:
			case 0x7d:
			case 0xbc:
			case 0xbd:
			case 0xdc:
			case 0xdd:
			case 0xfc:
			case 0xfd:
				index = cpu->X;
			calc_abs_idx_addr_read:
				abs_addr(cpu, operand);
				absaddr = cpu->address_bus;
				if (((cpu->address_bus + index) ^ cpu->address_bus) & 0x100) {
					read_mem(cpu, (cpu->address_bus & 0xff00) |
						 ((cpu->address_bus + index) & 0xff));
				}
				cpu->address_bus += index;
				break;

			case 0x0c:
			case 0x0d:
			case 0x0e:
			case 0x0f:
			case 0x2c:
			case 0x2d:
			case 0x2e:
			case 0x2f:
			case 0x4c:
			case 0x4d:
			case 0x4e:
			case 0x4f:
			case 0x6d:
			case 0x6e:
			case 0x6f:
			case 0x8c:
			case 0x8d:
			case 0x8e:
			case 0x8f:
			case 0xac:
			case 0xad:
			case 0xae:
			case 0xaf:
			case 0xcc:
			case 0xcd:
			case 0xce:
			case 0xcf:
			case 0xec:
			case 0xed:
			case 0xee:
			case 0xef:
				abs_addr(cpu, operand);
				absaddr = cpu->address_bus;
				break;

			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
			case 0x24:
			case 0x25:
			case 0x26:
			case 0x27:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x47:
			case 0x64:
			case 0x65:
			case 0x66:
			case 0x67:
			case 0x84:
			case 0x85:
			case 0x86:
			case 0x87:
			case 0xa4:
			case 0xa5:
			case 0xa6:
			case 0xa7:
			case 0xc4:
			case 0xc5:
			case 0xc6:
			case 0xc7:
			case 0xe4:
			case 0xe5:
			case 0xe6:
			case 0xe7:
				/* Zero Page */
				cpu->address_bus = operand;
				cpu->PC++;
				break;

			case 0x96:
			case 0x97:
			case 0xb6:
			case 0xb7:
				index = cpu->Y;
				goto calc_zp_idx_addr;
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
			case 0x34:
			case 0x35:
			case 0x36:
			case 0x37:
			case 0x54:
			case 0x55:
			case 0x56:
			case 0x57:
			case 0x74:
			case 0x75:
			case 0x76:
			case 0x77:
			case 0x94:
			case 0x95:
			case 0xb4:
			case 0xb5:
			case 0xd4:
			case 0xd5:
			case 0xd6:
			case 0xd7:
			case 0xf4:
			case 0xf5:
			case 0xf6:
			case 0xf7:
				index = cpu->X;
			calc_zp_idx_addr:
				cpu->address_bus = operand;
				cpu->PC++;
				read_mem(cpu, cpu->address_bus);
				cpu->address_bus = (cpu->address_bus + index) & 0xff;
				break;
			}

			switch (opcode) {
			case 0x61:
			case 0x65:
			case 0x6d:
			case 0x71:
			case 0x75:
			case 0x79:
			case 0x7d:
				/* ADC */
				read_mem(cpu, cpu->address_bus);
				add(cpu, cpu->data_bus);
				break;

			case 0x21:
			case 0x25:
			case 0x2d:
			case 0x31:
			case 0x35:
			case 0x39:
			case 0x3d:
				read_mem(cpu, cpu->address_bus);
				cpu->A &= cpu->data_bus;
				set_zn_flags(cpu->A);
				break;

			case 0x06:
			case 0x0e:
			case 0x16:
			case 0x1e:
				asl(cpu);
				break;

			case 0x24:
			case 0x2c:
				bit(cpu);
				break;

			case 0xc1:
			case 0xc5:
			case 0xcd:
			case 0xd1:
			case 0xd5:
			case 0xd9:
			case 0xdd:
				read_mem(cpu, cpu->address_bus);
				compare(cpu, cpu->data_bus, cpu->A);
				break;

			case 0xe4:
			case 0xec:
				read_mem(cpu, cpu->address_bus);
				compare(cpu, cpu->data_bus, cpu->X);
				break;

			case 0xc4:
			case 0xcc:
				read_mem(cpu, cpu->address_bus);
				compare(cpu, cpu->data_bus, cpu->Y);
				break;

			case 0xc3:
			case 0xc7:
			case 0xcf:
			case 0xd3:
			case 0xd7:
			case 0xdb:
			case 0xdf:
				dcp(cpu);
				break;

			case 0xc6:
			case 0xce:
			case 0xd6:
			case 0xde:
				dec(cpu);
				break;

			case 0x41:
			case 0x45:
			case 0x4d:
			case 0x51:
			case 0x55:
			case 0x59:
			case 0x5d:
				eor(cpu);
				break;

			case 0xe6:
			case 0xee:
			case 0xf6:
			case 0xfe:
				inc(cpu);
				break;

			case 0xe3:
			case 0xe7:
			case 0xef:
			case 0xf3:
			case 0xf7:
			case 0xfb:
			case 0xff:
				isb(cpu);
				break;

			case 0x4c:
			case 0x6c:
				/* JMP */
				cpu->PC = cpu->address_bus;
				break;

			case 0xbb:
				las(cpu);
				break;

			case 0xa3:
			case 0xa7:
			case 0xaf:
			case 0xb3:
			case 0xb7:
			case 0xbf:
				/* LAX */
				load(cpu, &cpu->A);
				cpu->X = cpu->A;
				break;

			case 0xa1:
			case 0xa5:
			case 0xad:
			case 0xb1:
			case 0xb5:
			case 0xb9:
			case 0xbd:
				load(cpu, &cpu->A);
				break;

			case 0xa6:
			case 0xae:
			case 0xb6:
			case 0xbe:
				load(cpu, &cpu->X);
				break;

			case 0xa4:
			case 0xac:
			case 0xb4:
			case 0xbc:
				load(cpu, &cpu->Y);
				break;

			case 0x46:
			case 0x4e:
			case 0x56:
			case 0x5e:
				lsr(cpu);
				break;

			case 0x01:
			case 0x05:
			case 0x0d:
			case 0x11:
			case 0x15:
			case 0x19:
			case 0x1d:
				ora(cpu);
				break;

			case 0x04:
			case 0x0c:
			case 0x14:
			case 0x1c:
			case 0x34:
			case 0x3c:
			case 0x44:
			case 0x54:
			case 0x5c:
			case 0x64:
			case 0x74:
			case 0x7c:
			case 0xd4:
			case 0xdc:
			case 0xf4:
			case 0xfc:
				read_mem(cpu, cpu->address_bus);
				break;

			case 0x23:
			case 0x27:
			case 0x2f:
			case 0x33:
			case 0x37:
			case 0x3b:
			case 0x3f:
				rla(cpu);
				break;

			case 0x26:
			case 0x2e:
			case 0x36:
			case 0x3e:
				rol(cpu);
				break;

			case 0x66:
			case 0x6e:
			case 0x76:
			case 0x7e:
				ror(cpu);
				break;

			case 0x63:
			case 0x67:
			case 0x6f:
			case 0x73:
			case 0x77:
			case 0x7b:
			case 0x7f:
				rra(cpu);
				break;

			case 0x83:
			case 0x87:
			case 0x8f:
			case 0x97:
				/* SAX */
				write_mem(cpu, cpu->address_bus, cpu->A & cpu->X);
				break;

			case 0xe1:
			case 0xe5:
			case 0xed:
			case 0xf1:
			case 0xf5:
			case 0xf9:
			case 0xfd:
				/* SBC */
				read_mem(cpu, cpu->address_bus);
				add(cpu, cpu->data_bus ^ 0xff);
				break;

			case 0x9f:
				sha(cpu, absaddr);
				break;

			case 0x93:
				sha(cpu, cpu->address_bus);
				break;

			case 0x9b:
				shs(cpu, absaddr);
				break;
			case 0x9e:
				sxa(cpu, absaddr);
				break;
			case 0x9c:
				sya(cpu, absaddr);
				break;

			case 0x03:
			case 0x07:
			case 0x0f:
			case 0x13:
			case 0x17:
			case 0x1b:
			case 0x1f:
				slo(cpu);
				break;

			case 0x43:
			case 0x47:
			case 0x4f:
			case 0x53:
			case 0x57:
			case 0x5b:
			case 0x5f:
				sre(cpu);
				break;

			case 0x81:
			case 0x85:
			case 0x91:
			case 0x95:
			case 0x99:
			case 0x9d:
			case 0x8d:
				write_mem(cpu, cpu->address_bus, cpu->A);
				break;

			case 0x86:
			case 0x8e:
			case 0x96:
				write_mem(cpu, cpu->address_bus, cpu->X);
				break;

			case 0x84:
			case 0x8c:
			case 0x94:
				write_mem(cpu, cpu->address_bus, cpu->Y);
				break;
			}
		}

		if (cpu->cycles >= cpu->board_run_timestamp) {
			cpu->board_run_timestamp = ~0;
			board_run(cpu->emu->board, cpu->cycles);
		}

		update_interrupt_status(cpu);
	}
end_of_frame:

	return cpu->cycles;
}

void cpu_end_frame(struct cpu_state *cpu, uint32_t frame_cycles)
{
	int i;

	update_interrupt_status(cpu);
	for (i = 0; i < IRQ_MAX + 1; i++) {
		if (cpu->interrupt_times[i] != ~0) {
			if (cpu->interrupt_times[i] >= frame_cycles)
				cpu->interrupt_times[i] -= frame_cycles;
			else {
				/* This generally shouldn't happen; the one exception
				   would be if sprite DMA was being split across a
				   frame boundary and there were IRQs pending during
				   DMA.  Setting them to zero should be safe though;
				   there are still several cycles left in the DMA
				   process, and as long as the IRQs are handled when
				   they're supposed to be it doesn't matter what the
				   stored timstamp is.
				*/
				/* printf("irq time for %d was < frame cycles\n", */
				/*        i); */
                              cpu->interrupt_times[i] = 0;
			}
		}
	}

	if (cpu->dmc_dma_timestamp != ~0 && cpu->dmc_dma_timestamp >= frame_cycles)
		cpu->dmc_dma_timestamp -= frame_cycles;

	if (cpu->oam_dma_timestamp != ~0 && cpu->oam_dma_timestamp >= frame_cycles)
		cpu->oam_dma_timestamp -= frame_cycles;

	if (cpu->dma_timestamp != ~0 && cpu->dma_timestamp >= frame_cycles)
		cpu->dma_timestamp -= frame_cycles;

	cpu->cycles -= frame_cycles;
}

int cpu_get_pc(struct cpu_state *cpu)
{
	return cpu->PC;
}

int cpu_get_stack_pointer(struct cpu_state *cpu)
{
	return cpu->S;
}

void cpu_set_dmc_dma_timestamp(struct cpu_state *cpu, uint32_t cycles, int addr,
			   int immediate)
{
	cpu->dmc_dma_timestamp = cycles;
	cpu->dmc_dma_addr = addr;

	if (cycles <= cpu->oam_dma_timestamp)
		cpu->dma_timestamp = cycles;

	if (immediate)
		cpu->dmc_dma_step = DMC_DMA_STEP_DUMMY;
}

void cpu_oam_dma(struct cpu_state *cpu, int addr, int odd)
{
	cpu->oam_dma_addr = addr;
	if (cpu->oam_dma_step < 256)
		return;

	update_interrupt_status(cpu);
	cpu->polled_interrupts = 1;

	if (!odd)
		cpu->oam_dma_step = -2;
	else
		cpu->oam_dma_step = -1;

}

static int cpu_do_oam_dma(struct cpu_state *cpu)
{
	int i;
	int value;
	int addr;
	int max;

	while(cpu->oam_dma_step < 0) {
		read_mem(cpu, 0x4014); /* FIXME should be address_bus? */
		cpu->oam_dma_step++;
	}

	addr = cpu->oam_dma_addr + cpu->oam_dma_step;

	/* OAM DMA normally happens pretty early in vblank, but it's
	   possible to start the process late enough to make it run
	   past the end of the frame. This is not a problem per se,
	   but it requires saving a fair bit of extra PPU state
	   (background and sprite tile buffers, among other things).

	   If emulation is stopped early enough in scanline -1 (the
	   pre-render scanline), none of that state needs to be saved,
	   since no tiles are rendered for scanline -1 and no sprites
	   can appear on scanline 0.  The exact point at which
	   emulation is stopped doesn't matter as long as it happens
	   before the PPU starts to pull tiles for scanline 0.

	   This code doesn't let emulation go more than a handful of
	   CPU cycles beyond cpu->step_cycles, which also prevents it
	   from going too far beyond the end of the frame. The wait
	   cycles are always executed, as are the last two DMA
	   transfers.
	*/

	max = (cpu->frame_cycles - cpu->cycles) / cpu->cpu_clock_divider + 2;
	max += cpu->oam_dma_step * 2;
	if (max > 254 * 2)
		max = 254 * 2;

	for (i = cpu->oam_dma_step * 2; i < max; i+= 2) {
		read_mem(cpu, addr);
		value = cpu->data_bus;
		write_mem(cpu, 0x2004, value);
		addr++;
		cpu->oam_dma_step++;
	}


	if (i < 508)
		return 1;

	//cpu->dmc_dma_step = DMC_DMA_STEP_ALIGN;
	read_mem(cpu, addr);
	value = cpu->data_bus;
	addr++;
	//cpu->dmc_dma_step = DMC_DMA_STEP_XFER;
	write_mem(cpu, 0x2004, value);
	cpu->oam_dma_step++;
	//cpu->dmc_dma_step = DMC_DMA_STEP_ALIGN;
	read_mem(cpu, addr);
	value = cpu->data_bus;
	addr++;
	//cpu->dmc_dma_step = DMC_DMA_STEP_DUMMY;
	write_mem(cpu, 0x2004, value);
	cpu->oam_dma_step++;
	//cpu->dmc_dma_step = DMC_DMA_STEP_NONE;

	return 0;
}

int cpu_save_state(struct cpu_state *cpu, struct save_state *state)
{
	uint8_t *buf;
	size_t size;
	int rc;

	size = pack_state(cpu, cpu_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	pack_state(cpu, cpu_state_items, buf);

	rc = save_state_add_chunk(state, "CPU ", buf, size);
	free(buf);

	if (rc < 0)
		return -1;

	return 0;
}

int cpu_load_state(struct cpu_state *cpu, struct save_state *state)
{
	uint8_t *buf;
	size_t size;

	if (save_state_find_chunk(state, "CPU ", &buf, &size) < 0)
		return -1;

	unpack_state(cpu, cpu_state_items, buf);

	return 0;
}

void cpu_poke(struct cpu_state *cpu, int addr, uint8_t data)
{
	if (cpu->write_pagetable[addr >> CPU_PAGE_SHIFT]) {
		cpu->write_pagetable[addr >> CPU_PAGE_SHIFT][addr] = data;
	}
}

void cpu_set_pc(struct cpu_state *cpu, int addr)
{
	cpu->PC = addr & 0xffff;
}

void cpu_set_accumulator(struct cpu_state *cpu, uint8_t value)
{
	cpu->A = value;
	set_zn_flags(value);
}

void cpu_set_x_register(struct cpu_state *cpu, uint8_t value)
{
	cpu->X = value;
	set_zn_flags(value);
}

uint8_t cpu_get_accumulator(struct cpu_state *cpu)
{
	return cpu->A;
}

int cpu_is_opcode_fetch(struct cpu_state *cpu)
{
	return cpu->is_opcode_fetch;
}
