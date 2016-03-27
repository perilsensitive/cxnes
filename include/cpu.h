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

#ifndef __CPU_H__
#define __CPU_H__

#include "emu.h"

struct save_state;

#define CPU_TYPE_RP2A03 0
#define CPU_TYPE_RP2A07 1
#define CPU_TYPE_DENDY  2

#define CPU_PAGE_READ 1
#define CPU_PAGE_WRITE 2
#define CPU_PAGE_READWRITE 3
#define CPU_PAGE_SHIFT 10
#define CPU_PAGE_SIZE (1 << CPU_PAGE_SHIFT)

#define IRQ_RESET         0
#define IRQ_NMI           1
#define IRQ_NMI_IMMEDIATE 2
#define IRQ_M2_TIMER      3
#define IRQ_A12_TIMER     4
#define IRQ_MMC5_TIMER    5
#define IRQ_APU_FRAME     6
#define IRQ_APU_DMC       7
#define IRQ_DISK          8
#define IRQ_MMC5_PCM      9
#define IRQ_MAX           9
#define IRQ_NONE         32

#define IRQ_RESET_MASK (IRQ_FLAG(IRQ_RESET))
#define IRQ_NMI_MASK   (IRQ_FLAG(IRQ_NMI)|IRQ_FLAG(IRQ_NMI_IMMEDIATE))
#define IRQ_IRQ_MASK   (~(IRQ_NMI_MASK|IRQ_RESET_MASK))
#define IRQ_ALL_MASK   (IRQ_RESET_MASK|IRQ_NMI_MASK|IRQ_IRQ_MASK)

#define CPU_WRITE_HANDLER(_handler) void _handler(struct emu *emu, int addr, uint8_t value, uint32_t cycles)
#define CPU_READ_HANDLER(_handler) uint8_t _handler(struct emu *emu, int addr, uint8_t value, uint32_t cycles)

typedef void (cpu_write_handler_t) (struct emu *, int, uint8_t, uint32_t);
typedef uint8_t(cpu_read_handler_t) (struct emu *, int, uint8_t, uint32_t);

struct cpu_state;

int cpu_init(struct emu *);
void cpu_cleanup(struct cpu_state *);
void cpu_reset(struct cpu_state *, int);
int cpu_apply_config(struct cpu_state *);

int cpu_get_type(struct cpu_state *cpu);
void cpu_set_type(struct cpu_state *cpu, int);

uint32_t cpu_get_cycles(struct cpu_state *cpu);

uint32_t cpu_get_frame_cycles(struct cpu_state *cpu);
void cpu_set_frame_cycles(struct cpu_state *cpu, uint32_t);
uint32_t cpu_run(struct cpu_state *cpu);
void cpu_end_frame(struct cpu_state *cpu, uint32_t);

void cpu_board_run_schedule(struct cpu_state *cpu, uint32_t cycles);
void cpu_board_run_cancel(struct cpu_state *cpu);
void cpu_interrupt_schedule(struct cpu_state *cpu, unsigned int intr,
			    uint32_t cycles);
void cpu_interrupt_schedule_ack(struct cpu_state *cpu, unsigned int intr,
				uint32_t cycles);
int cpu_interrupt_ack(struct cpu_state *cpu, unsigned int intr);
void cpu_interrupt_cancel(struct cpu_state *cpu, unsigned int intr);

void cpu_set_pagetable_entry(struct cpu_state *cpu, int page,
			     int size, uint8_t * data, int rw);
uint8_t cpu_peek(struct cpu_state *cpu, int addr);

cpu_read_handler_t *cpu_get_read_handler(struct cpu_state *cpu, int addr);
void cpu_set_read_handler(struct cpu_state *cpu, int addr, size_t size,
			  int mask, cpu_read_handler_t * h);
void cpu_set_write_handler(struct cpu_state *cpu, int addr, size_t size,
			   int mask, cpu_write_handler_t * h);

void cpu_set_dma_timestamp(struct cpu_state *cpu, uint32_t cycles, int addr,
			   int immediate);
void cpu_oam_dma(struct cpu_state *cpu, int addr, int odd);
cpu_write_handler_t *cpu_get_write_handler(struct cpu_state *cpu, int addr);
int cpu_get_pc(struct cpu_state *cpu);
int cpu_get_stack_pointer(struct cpu_state *cpu);
void cpu_set_trace(struct cpu_state *cpu, int enabled);
int cpu_save_state(struct cpu_state *cpu, struct save_state *state);
int cpu_load_state(struct cpu_state *cpu, struct save_state *state);
void cpu_poke(struct cpu_state *cpu, int addr, uint8_t data);
void cpu_set_pc(struct cpu_state *cpu, int addr);
int cpu_is_opcode_fetch(struct cpu_state *cpu);
uint8_t cpu_get_accumulator(struct cpu_state *cpu);
void cpu_set_accumulator(struct cpu_state *cpu, uint8_t value);
void cpu_set_x_register(struct cpu_state *cpu, uint8_t value);
#endif				/* __CPU_H__ */
