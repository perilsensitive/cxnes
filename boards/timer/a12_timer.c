/*
  cxNES - NES/Famicom Emulator
  Copyright (C) 2011-2015 Ryan Jackson

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
#include "a12_timer.h"

#define SPRITE_SIZE_MASK (1 << 5)
#define SPRITE_PATTERN_TABLE (1 << 3)
#define BG_PATTERN_TABLE (1 << 4)
#define PATTERN_TABLE_MASK (BG_PATTERN_TABLE|SPRITE_PATTERN_TABLE)
#define CTRL_MASK (SPRITE_SIZE_MASK|SPRITE_PATTERN_TABLE|BG_PATTERN_TABLE)
#define RENDERING_MASK (3 << 3)

#define LARGE_SPRITES() (timer->ppu_ctrl_reg & SPRITE_SIZE_MASK)
#define SPRITE_MODE() (timer->ppu_ctrl_reg & SPRITE_PATTERN_TABLE)
#define BG_MODE() (timer->ppu_ctrl_reg & BG_PATTERN_TABLE)
#define SPRITE_TABLE(x, y) (timer->sprite_a12_table[(x) + 1] & (1 << ((((y - 1) & ~3) - 260) / 8)))

struct a12_timer;

extern CPU_WRITE_HANDLER(ppu_ctrl_reg_write_handler);
extern CPU_WRITE_HANDLER(ppu_mask_reg_write_handler);
extern CPU_WRITE_HANDLER(ppu_oam_addr_reg_write_handler);
extern CPU_WRITE_HANDLER(ppu_oam_data_reg_write_handler);
extern CPU_WRITE_HANDLER(oam_dma);

int a12_timer_init(struct emu *emu, int variant);
void a12_timer_reset(struct a12_timer *timer, int);
void a12_timer_cleanup(struct emu *emu);
void a12_timer_end_frame(struct a12_timer *timer, uint32_t cycles);
void a12_timer_hook(struct emu *emu, int address, int scanline,
			   int scanline_cycle, int rendering);

void a12_timer_run(struct a12_timer *timer, uint32_t cycle_count);
static void a12_timer_schedule_irq(struct a12_timer *timer);
static void update_sprite_a12_rises(struct a12_timer *timer);

static CPU_WRITE_HANDLER(a12_timer_ctrl_handler);
static CPU_WRITE_HANDLER(a12_timer_mask_handler);
static CPU_WRITE_HANDLER(a12_timer_oam_addr_handler);
static CPU_WRITE_HANDLER(a12_timer_oam_data_handler);
static CPU_WRITE_HANDLER(a12_timer_oam_dma_handler);

struct a12_timer {
	unsigned ppu_ctrl_reg;
	unsigned ppu_mask_reg;
	unsigned ppu_oam_addr_reg;
	uint8_t *ppu_oam;
	int in_oam_dma;
	uint8_t sprite_a12_table[241];	/* XXX should this be static? */
	int sprite_mode;
	uint8_t sprite_mask;
	uint32_t next_clock;
	uint32_t next_irq;
	uint32_t prev_a12;
	uint32_t counter;
	uint32_t reload;
	uint32_t timestamp;
	int scanline;
	int cycle;
	int delay;
	int vblank_scanlines;
	int force_reload_delay;
	int alt;
	int has_short_frame;
	int variant;
	int a12_rise_delta;
	int irq_enabled;
	int counter_enabled;
	int reload_flag;
	int frame_start_cpu_cycles;
	int flags;
	int prescaler_mask;
	int prescaler_size;
	int prescaler;

	struct emu *emu;
};

static struct state_item a12_timer_state_items[] = {
	STATE_8BIT(a12_timer, ppu_ctrl_reg),
	STATE_8BIT(a12_timer, ppu_mask_reg),
	STATE_8BIT(a12_timer, ppu_oam_addr_reg),
	STATE_8BIT(a12_timer, a12_rise_delta),
	STATE_8BIT(a12_timer, in_oam_dma), /* BOOLEAN */

	/* FIXME should be able to recalculate this on state load */
	STATE_8BIT_ARRAY(a12_timer, sprite_a12_table),
	STATE_8BIT(a12_timer, sprite_mode),
	STATE_8BIT(a12_timer, sprite_mask),
	STATE_16BIT(a12_timer, scanline),
	STATE_16BIT(a12_timer, cycle),
	STATE_32BIT(a12_timer, timestamp),
	STATE_32BIT(a12_timer, next_clock),
	STATE_32BIT(a12_timer, next_irq),
	STATE_32BIT(a12_timer, prev_a12),
	STATE_32BIT(a12_timer, flags),
	STATE_16BIT(a12_timer, prescaler),
	STATE_16BIT(a12_timer, counter),
	STATE_16BIT(a12_timer, reload),
	STATE_8BIT(a12_timer, prescaler_size),
	STATE_32BIT(a12_timer, delay), /* FIXME check type */
	STATE_8BIT(a12_timer, reload_flag), /* BOOLEAN */
	STATE_8BIT(a12_timer, counter_enabled), /* BOOLEAN */
	STATE_8BIT(a12_timer, irq_enabled), /* BOOLEAN */
	STATE_8BIT(a12_timer, reload_flag), /* BOOLEAN */
	STATE_ITEM_END(),
};

static uint32_t calculate_next_clock(struct a12_timer *timer, int adjustment, uint32_t start, int offset)
{
	uint32_t next_clock;

	next_clock = start - timer->frame_start_cpu_cycles;
	next_clock += (offset + adjustment) * timer->emu->ppu_clock_divider;
	next_clock /= timer->emu->cpu_clock_divider;
	next_clock += timer->a12_rise_delta;
	next_clock *= timer->emu->cpu_clock_divider;
	next_clock += timer->frame_start_cpu_cycles;

	return next_clock;
}

void a12_timer_end_frame(struct a12_timer *timer, uint32_t cycles)
{
	int scanline, cycle, odd_frame, short_frame;
	/* printf("end_frame: %d %d\n", starting_cycles, timer->timestamp - cycles); */

	timer->frame_start_cpu_cycles = cpu_get_cycles(timer->emu->cpu);
	ppu_get_cycles(timer->emu->ppu, &scanline, &cycle, &odd_frame,
		       &short_frame);
	//timer->timestamp = starting_cycles;
	timer->timestamp -= cycles;

	/* printf("timestamp: %d scanline: %d cycles: %d\n", timer->timestamp, */
	/*        timer->scanline, timer->cycle); */

	/* printf("starting: %d start: %d scanline: %d cycle: %d\n", starting_cycles, */
	/*        timer->frame_start_cpu_cycles, scanline, cycle); */
	

	if (timer->next_clock >= cycles)
		timer->next_clock -= cycles;
	else
		timer->next_clock = 0;

	if (!timer->counter_enabled)
		return;

	if (timer->next_irq != ~0) {
		if (timer->next_irq >= cycles)
			timer->next_irq -= cycles;
		else
			timer->next_irq = ~0;
	}
	//a12_timer_run(timer, cycles);
//      ppu_run(cycles);
//      if (board->board_state.irq_control && next_irq == -1) {
//      printf("calling next_irq() from end_frame(): count = %d\n", board->board_state.irq_counter);
//              a12_timer_schedule_irq(board);
//      }
}

int a12_timer_init(struct emu *emu, int variant)
{
	int addr;
	struct a12_timer *timer;

	timer = malloc(sizeof(*timer));
	if (!timer)
		return 1;

	memset(timer, 0, sizeof(*timer));

	timer->emu = emu;
	emu->a12_timer = timer;
//	variant = A12_TIMER_VARIANT_ACCLAIM_MC_ACC;
	timer->variant = variant;

	switch (variant) {
	case A12_TIMER_VARIANT_MMC3_STD:
		timer->delay = 0;
		timer->force_reload_delay = 0;
		timer->alt = 0;
		timer->a12_rise_delta = 4;
		break;
	case A12_TIMER_VARIANT_MMC3_ALT:
		timer->delay = 0;
		timer->force_reload_delay = 0;
		timer->alt = 1;
		timer->a12_rise_delta = 4;
		break;
	case A12_TIMER_VARIANT_RAMBO1:
		timer->delay = 0;
		timer->force_reload_delay = 0;
		timer->alt = 0;
		timer->a12_rise_delta = 4;
		break;
	case A12_TIMER_VARIANT_TAITO_TC0190FMC:
		timer->delay = 12;
		timer->force_reload_delay = 0;
		timer->alt = 0;
		timer->a12_rise_delta = 4;
		break;
	case A12_TIMER_VARIANT_ACCLAIM_MC_ACC:
		timer->delay = 4;
		timer->force_reload_delay = 0;
		timer->alt = 0;
		/* I'm sure this is wrong, but it's the minimum value
		   that works for Mickey's Safari in Letterland.  I'm
		   99% sure that the rise delta for the MC-ACC *is*
		   longer than the MMC3's, but not sure exactly how
		   much.
		*/
		timer->a12_rise_delta = 11;
		break;
	}

	timer->flags = A12_TIMER_FLAG_DELAYED_RELOAD;

	for (addr = 0x2000; addr < 0x4000; addr += 8) {
		cpu_set_write_handler(emu->cpu, addr, 1, 0,
				      a12_timer_ctrl_handler);
		cpu_set_write_handler(emu->cpu, addr + 1, 1, 0,
				      a12_timer_mask_handler);
		cpu_set_write_handler(emu->cpu, addr + 3, 1, 0,
				      a12_timer_oam_addr_handler);
		cpu_set_write_handler(emu->cpu, addr + 4, 1, 0,
				      a12_timer_oam_data_handler);
	}

	ppu_enable_a12_timer(emu->ppu, 1);
	cpu_set_write_handler(emu->cpu, 0x4014, 1, 0,
			      a12_timer_oam_dma_handler);
	timer->ppu_oam = ppu_get_oam_ptr(emu->ppu);

	return 0;
}

void a12_timer_cleanup(struct emu *emu)
{
	if (emu->a12_timer)
		free(emu->a12_timer);

	emu->a12_timer = NULL;
}

void a12_timer_reset(struct a12_timer *timer, int hard)
{
	int ppu_type;
	int odd_frame;
	int short_frame;

	if (hard) {
		timer->next_clock = 0;
		timer->prev_a12 = 0;
		timer->vblank_scanlines = 20;
		timer->has_short_frame = 0;
		timer->frame_start_cpu_cycles = 0;
		timer->force_reload_delay = 0;
		timer->reload = 0;
		timer->counter = 0;
		timer->prescaler = 0;
		timer->reload_flag = 0;
		timer->counter_enabled = 1;
		timer->irq_enabled = 0;
		ppu_type = ppu_get_type(timer->emu->ppu);
		if ((ppu_type == PPU_TYPE_RP2C07) || (ppu_type == PPU_TYPE_DENDY))
			timer->vblank_scanlines = 70;
		else if (ppu_type == PPU_TYPE_RP2C02)
			timer->has_short_frame = 1;
		timer->delay *= timer->emu->ppu_clock_divider;
	}

	timer->timestamp = ppu_get_cycles(timer->emu->ppu, &timer->scanline, &timer->cycle, &odd_frame,
				       &short_frame);
}

void a12_timer_set_reload(struct a12_timer *timer, int reload, uint32_t cycles)
{
	if (timer->counter_enabled)
		ppu_run(timer->emu->ppu, cycles);

	timer->reload = reload;
//	printf("loading irq reload with %d at %d, %d\n", value, cycles, timer->counter);

	if (timer->counter_enabled && timer->irq_enabled)
		a12_timer_schedule_irq(timer);
}

CPU_WRITE_HANDLER(a12_timer_irq_latch)
{
	struct a12_timer *timer = emu->a12_timer;

	a12_timer_set_reload(timer, value, cycles);
}

void a12_timer_force_reload(struct a12_timer *timer, uint32_t cycles)
{
	ppu_run(timer->emu->ppu, cycles);

	if (timer->flags & A12_TIMER_FLAG_DELAYED_RELOAD) {
		timer->reload_flag = 1;
	} else {
		a12_timer_set_counter(timer, timer->reload +
				     timer->force_reload_delay, cycles);
	}

	if (timer->counter_enabled && timer->irq_enabled)
		a12_timer_schedule_irq(timer);
}

CPU_WRITE_HANDLER(a12_timer_irq_reload)
{
	struct a12_timer *timer = emu->a12_timer;

	a12_timer_force_reload(timer, cycles);
}

void a12_timer_set_irq_enabled(struct a12_timer *timer, int enabled, uint32_t cycles)
{
	if ((enabled && timer->irq_enabled) ||
	    (!enabled && !timer->irq_enabled)) {
		return;
	}

	ppu_run(timer->emu->ppu, cycles);

	if (!enabled) {
		cpu_interrupt_ack(timer->emu->cpu, IRQ_A12_TIMER);
		cpu_interrupt_cancel(timer->emu->cpu, IRQ_A12_TIMER);
	}

	timer->irq_enabled = enabled;

	if (timer->counter_enabled)
		a12_timer_schedule_irq(timer);
}

CPU_WRITE_HANDLER(a12_timer_irq_enable)
{
	struct a12_timer *timer = emu->a12_timer;

	a12_timer_set_irq_enabled(timer, 1, cycles);
}

CPU_WRITE_HANDLER(a12_timer_irq_disable)
{
	struct a12_timer *timer = emu->a12_timer;

	a12_timer_set_irq_enabled(timer, 0, cycles);
}

static void update_sprite_a12_rises(struct a12_timer *timer)
{
	int scanline;
	int sprite;

	timer->sprite_a12_table[0] = 0xff;
	timer->sprite_a12_table[1] = 0xff;
	timer->sprite_a12_table[240] = 0xff;
	timer->sprite_mask = 0xff;
	timer->sprite_mode = 1;

	/* scanlines -1, 0 and 239 are always 8 rises each */
	/* FIXME this does not handle the sprite 0 overwrite
	   case (the one that allows Huge Insect to work).
	*/
	for (scanline = 1; scanline <= 239; scanline++) {
		timer->sprite_a12_table[scanline + 1] = 0xff;
		int count = 0;
		for (sprite = 0; sprite < 64; sprite++) {
			int sl = timer->ppu_oam[sprite << 2];
			uint8_t addr = timer->ppu_oam[(sprite << 2) + 1];

			if (scanline < sl || scanline > sl + 15)
				continue;

			if (count < 8 && !(addr & 1)) {
				timer->sprite_a12_table[scanline + 1] &=
				    ~(1 << count);
			}

			count++;
		}

		timer->sprite_mask &= timer->sprite_a12_table[scanline + 1];

		if (timer->sprite_a12_table[scanline + 1] != 0xff) {
//			printf("scanline %d: %02x\n", scanline, timer->sprite_a12_table[scanline + 1]);
			timer->sprite_mode = 2;
		}
	}
}

#define FAST_IRQ 1
#if FAST_IRQ
static int a12_timer_fast_calculate_irq(struct a12_timer *timer, uint32_t *cyclesptr,
					int count, int *scanlineptr, int *cycleptr)
{
	uint32_t cycles;
	int scanline;
	int cycle;

	if ((timer->a12_rise_delta < 4)  || (timer->a12_rise_delta > 23) ||
	    (timer->prescaler_size != 0) || (timer->sprite_mode == 2 &&
					     !(timer->sprite_mask & 0x80)) ||
	    (!BG_MODE() && !timer->sprite_mode)) {
		return 0;
	}

	cycles = *cyclesptr;
	scanline = *scanlineptr;
	cycle = *cycleptr;

	/* FIXME doesn't take skipped cycle into account on NTSC */

	while (count > 0) {
		int new_scanline;

		new_scanline = scanline + count - 1;

		if (new_scanline > 239)
			new_scanline = 239;

		if (!BG_MODE() && timer->sprite_mode) {
			cycle = 261;
		} else if (BG_MODE() && !timer->sprite_mode) {
			if (scanline == -1 && cycle == 5) {
				count--;
				new_scanline--;
				if (!count)
					break;
			}

			cycle = 325;
		} else if (BG_MODE() && timer->sprite_mode) {
			/* Only clocks on scanline -1, cycle 5 */
			if (scanline > -1) {
				cycles += ((240 + timer->vblank_scanlines + 1) - scanline) * 341;
				scanline = -1;
			}

			cycles += ((240 + timer->vblank_scanlines + 2) * 341) * count;
			break;
		}

		count -= new_scanline - scanline + 1;
		cycles += (new_scanline - scanline) * 341;
		scanline = new_scanline;
		cycles += cycle - 5;

		if ((scanline == 239) && count) {
			cycles += (341 - cycle) + 5;
			cycles += (timer->vblank_scanlines + 1) * 341;
			scanline = -1;
			cycle = 5;
		}
	}

	*cyclesptr = cycles;
	*scanlineptr = scanline;
	*cycleptr = cycle;

	return 1;
}
#endif
		
static void a12_timer_schedule_irq(struct a12_timer *timer)
{
	struct emu *emu;
	uint32_t cycles;
	int count;
	int reload_flag;

	uint32_t starting_cycles;
	int scanline, cycle;
	int short_frame, odd_frame;
	int next_clock;

#if FAST_IRQ	
	int tested_fast_irq_method;
#endif

	emu = timer->emu;

	starting_cycles =
	    ppu_get_cycles(emu->ppu, &scanline, &cycle, &odd_frame,
			   &short_frame);

	if (!timer->irq_enabled || !(timer->ppu_mask_reg & RENDERING_MASK) ||
	    (!(timer->ppu_ctrl_reg & BG_PATTERN_TABLE) &&
	     (timer->sprite_mode == 0))) {
		cycles = ~0;
		goto done;
	}

	cycles = 0;
	count = timer->counter;
	reload_flag = timer->reload_flag;

	if (timer->flags & A12_TIMER_FLAG_COUNT_UP) {
		count = 0xff - timer->counter;
	} else {
		count = timer->counter;
	}

	if (timer->flags & A12_TIMER_FLAG_IRQ_ON_WRAP)
	count++;
	
	if (timer->prescaler_size) {
		int remaining = timer->prescaler & timer->prescaler_mask;
		int prescaler_clocks = 0;
		int tmp = count;

		if (remaining != timer->prescaler_mask) {
			prescaler_clocks += remaining;
			tmp--;
		}

		prescaler_clocks += tmp * (1 << timer->prescaler_size);

		/* printf("count: %d prescaler_cycles: %d size: %d\n", */
		/*        count, prescaler_clocks, timer->prescaler_size); */
		count = prescaler_clocks;
	}

	next_clock = timer->next_clock;

	if (scanline < 240) {
		if (cycle <= 5) {
			cycles += 5 - cycle;
			cycle = 5;
		} else {
			int new_cycle;
			int remainder;

			new_cycle = (cycle - 5) / 8 * 8;
			remainder = (cycle - 5) % 8;

			if (remainder == 1 || remainder == 2)
				new_cycle += 2;
			else if (remainder != 0)
				new_cycle += 8;

			new_cycle += 5;

			if (new_cycle > 335) {
				cycles += 341 - cycle + 5;
				cycle = 5;
				scanline++;
			} else {
				cycles += new_cycle - cycle;
				cycle = new_cycle;
			}
		}

		if (!((cycle - 1) & 2) && (next_clock < 0)) {
			next_clock = calculate_next_clock(timer, -(cycles & ~0x01),
							  starting_cycles,
							  cycles);
		}
	}

#if FAST_IRQ
	tested_fast_irq_method = 0;
#endif

	while (count >= 0) {
		int rise;

		if (scanline >= 240) {
			int frame_scanlines = 241 + timer->vblank_scanlines;

			cycles += 341 - cycle;
			cycles += 341 * (frame_scanlines -
					 scanline - 1);
			scanline = -1;
			cycles += 5;
			cycle = 5;
			if (BG_MODE()) {
				calculate_next_clock(timer, -4, starting_cycles, cycles);
			}
			continue;
		}

#if FAST_IRQ
		if (!tested_fast_irq_method &&
		    ((starting_cycles + (cycles * emu->ppu_clock_divider) > next_clock) &&
		     (cycle == 5))) {
			int c = count;
			if (!c || reload_flag)
				c = timer->reload + 1;

			int rc = a12_timer_fast_calculate_irq(timer, &cycles, c,
							      &scanline, &cycle);

			if (rc) {
//				printf("used fast method\n");
				//printf("fast calculated irq at %d,%d\n", new_scanline, new_cycle);
				break;
			} else {
//				printf("failed to use fast method\n");
			}

			tested_fast_irq_method = 1;
		} else if (!tested_fast_irq_method) {
		}
#endif

		rise = 0;

		if (((cycle >= 325 && cycle < 336) && BG_MODE()) ||
		    ((cycle >= 261 && cycle < 320) &&
		     ((LARGE_SPRITES() && SPRITE_TABLE(scanline, cycle)) ||
		      SPRITE_MODE())) ||
		    ((cycle >= 5 && cycle < 256) && BG_MODE())) {
			rise = 1;

		}

		if (rise) {
			int increment = ((cycle - 1) & 2) ? 2 : 4;
			if (starting_cycles +
			    (cycles * emu->ppu_clock_divider) > next_clock) {

				if (!count || reload_flag) {
					if (timer->alt && !timer->reload && !reload_flag) {
						cycles = ~0;
						goto done;
					}

					if (timer->flags & A12_TIMER_FLAG_WRAP)
						count = 0xff;
					else
						count = timer->reload;

					if (reload_flag)
						count += timer->force_reload_delay;

					reload_flag = 0;
				} else {
					count--;
				}

				/* printf("predicted clock (%d) at %d,%d\n", count, */
				/*        scanline, cycle); */

				if (!count) {
					break;
				}

			}

			/* if (timer->a12_rise_delta >= 4) { */
			/* 	if ((cycle >= 5) && (cycle < 253)) { */
			/* 		cycles += 253 - cycle; */
			/* 		cycle = 253; */
			/* 	} else if ((cycle >= 261) && (cycle < 317)) { */
			/* 		cycles += 317 - cycle; */
			/* 		cycle = 317; */
			/* 	} else if ((cycle >= 325) && (cycle < 333)) { */
			/* 		cycles += 333 - cycle; */
			/* 		cycle = 333; */
			/* 	} */
			/* } */
			next_clock = calculate_next_clock(timer, increment,
							  starting_cycles,
							  cycles);
		} else if (next_clock < 0) {
			next_clock = calculate_next_clock(timer, 0,
							  starting_cycles,
							  cycles);
		}

		if (cycle < 333) {
			int increment = ((cycle - 1) & 2) ? 6 : 8;
			cycles += increment;
			cycle += increment;
		} else {
			cycles += 341 - cycle + 5;
			cycle = 5;
			scanline++;

		}

	}

	/* Convert cycles to master cycles, add them to the starting
	 * cycle count */
	cycles *= timer->emu->ppu_clock_divider;
	if (timer->variant == A12_TIMER_VARIANT_RAMBO1) {
		int remainder;
		/* RAMBO-1 checks the counter status on the falling edge of M2,
		   and if the counter is now 0 it does not assert IRQ until the
		   next falling edge.  Since we started our calculations from
		   the start of a CPU cycle, we can just round down to the
		   nearest CPU cycle and add two more to get the correct time.

		   Note that this does assume a particular CPU-PPU alignment,
		   but handling other alignments would only require a small
		   adjustment dependent on the alignment used.
		 */
		cycles += starting_cycles;
		cycles -= timer->frame_start_cpu_cycles;
		remainder = cycles % timer->emu->cpu_clock_divider;
		cycles /= timer->emu->cpu_clock_divider;
		if (remainder)
			cycles += 2;
		else
			cycles++;
		cycles *= timer->emu->cpu_clock_divider;
		cycles += timer->frame_start_cpu_cycles;
	} else {
		cycles += starting_cycles;
	}

 done:
	if (cycles != timer->next_irq) {
		if (timer->next_irq != ~0) {
			cpu_interrupt_cancel(timer->emu->cpu, IRQ_A12_TIMER);
		}

		timer->next_irq = cycles;

		if (timer->next_irq != ~0) {
			/* printf("scheduling irq for %d (%d,%d) count=%d reload=%d\n", timer->next_irq, scanline, cycle, timer->counter, timer->reload); */
 			cpu_interrupt_schedule(timer->emu->cpu, IRQ_A12_TIMER,
					       timer->next_irq + timer->delay);
		}
	}
}

void a12_timer_reset_hook(struct emu *emu, uint32_t cycles)
{
	struct a12_timer *timer = emu->a12_timer;

	/* FIXME not sure if this can ever be false (it shouldn't ever
	 * be though) */
	if (timer->next_clock > cycles)
		timer->next_clock -= cycles;

	if ((timer->next_irq != ~0) && (timer->next_irq > cycles))
		timer->next_irq -= cycles;
}

void a12_timer_hook(struct emu *emu, int address, int scanline,
			   int scanline_cycle, int rendering)
{
	struct a12_timer *timer;
	uint32_t cycles;
	int odd_frame, short_frame;
	int old_counter;
	int prev_a12;
	int prescaler_wrapped;
	int counter_limit;
	int counter_delta;

	if (rendering) {
		printf("called while rendering: %d %d\n", scanline, scanline_cycle);
		return;
	}

	timer = emu->a12_timer;
	prev_a12 = timer->prev_a12;

	cycles = ppu_get_cycles(emu->ppu, &scanline, &scanline_cycle,
				&odd_frame, &short_frame);
	/* printf("hook: %d vs %d (%d,%d vs %d,%d)\n", cycles, timer->timestamp, scanline, scanline_cycle, timer->scanline, timer->cycle); */
//	a12_timer_run(timer, cycles);
	timer->prev_a12 = address;
	timer->timestamp = cycles;
	timer->scanline = scanline;
	timer->cycle = scanline_cycle;

	if (timer->flags & A12_TIMER_FLAG_COUNT_UP) {
		counter_delta = 1;
		counter_limit = 0xff;
	} else {
		counter_delta = -1;
		counter_limit = 0;
	}

	if (prev_a12 && !address) {
		timer->next_clock = (cycles - timer->frame_start_cpu_cycles);
		timer->next_clock /= emu->cpu_clock_divider;
		timer->next_clock += timer->a12_rise_delta;
		timer->next_clock *= emu->cpu_clock_divider;
		timer->next_clock += timer->frame_start_cpu_cycles;
		return;
	} else if ((prev_a12 && address) || (!prev_a12 && !address)) {
		return;
	}

	if (cycles <= timer->next_clock) {
		timer->next_clock = ~0;
		return;
	}

	if (!timer->counter_enabled)
		return;
//		goto not_connected;

	old_counter = timer->counter;

	prescaler_wrapped = 0;
	if (timer->prescaler_size) {
		int tmp = timer->prescaler & timer->prescaler_mask;
		tmp--;

		if (tmp & (1 << timer->prescaler_size)) {
			prescaler_wrapped = 1;
		}
		tmp &= timer->prescaler_mask;
		timer->prescaler &= ~timer->prescaler_mask;
		timer->prescaler |= tmp;

		if (!prescaler_wrapped)
			return;
	} else {
		prescaler_wrapped = 1;
	}

	if ((timer->counter == counter_limit) || timer->reload_flag) {
		int reload;

		if (timer->flags & A12_TIMER_FLAG_WRAP) {
			reload = (~counter_limit) & 0xff;
		} else {
			reload = timer->reload;
		}

		timer->counter = reload;

		if (timer->reload_flag) {
			if (!(timer->flags & A12_TIMER_FLAG_COUNT_UP))
				timer->counter += timer->force_reload_delay;
			else
				timer->counter -= timer->force_reload_delay;
		}
	} else {
		timer->counter += counter_delta;
	}
	/* printf("timer clock (%d) at %d,%d\n", timer->counter, */
	/*        scanline, scanline_cycle); */

	if (rendering || !timer->irq_enabled ||
	    (timer->alt && !old_counter && !timer->reload_flag)) {
		timer->reload_flag = 0;
		return;
	}

	/* address bus update caused by write to $2006 or read/write of $2007 */
	timer->reload_flag = 0;
	if (!timer->counter) {
		int tmp;
		cpu_interrupt_cancel(emu->cpu, IRQ_A12_TIMER);

		if (timer->variant == A12_TIMER_VARIANT_RAMBO1) {
			int remainder;
			/* Round down to the nearest CPU cycle
			   boundary and add two CPU cycles.  This
			   assumes the "standard" CPU-PPU alignment.
			*/
			int cpu_cycles = cpu_get_cycles(emu->cpu);
			tmp = cycles - cpu_cycles;
			remainder = tmp % emu->cpu_clock_divider;
			tmp /= emu->cpu_clock_divider;
			if (remainder)
				tmp += 2;
			else
				tmp++;
			tmp *= emu->cpu_clock_divider;
			tmp += cpu_cycles;
		} else {
			tmp = cycles + timer->delay;
		}

		cpu_interrupt_schedule(emu->cpu, IRQ_A12_TIMER, tmp);
	} else {
		a12_timer_schedule_irq(timer);
	}

	timer->reload_flag = 0;
//not_connected:
	//timer->next_clock = cycles + timer->a12_rise_delta * timer->emu->ppu_clock_divider;

      /* if (!timer->counter && timer->irq_enabled) { */
      /*        printf("irq would fire at %d (%d,%d)\n", cycles, scanline, scanline_cycle); */
      /* } */

}

static CPU_WRITE_HANDLER(a12_timer_ctrl_handler)
{
	struct a12_timer *timer = emu->a12_timer;
	int old, new;

	old = timer->ppu_ctrl_reg & CTRL_MASK;
	new = value & CTRL_MASK;

	ppu_ctrl_reg_write_handler(emu, addr, value, cycles);
	timer->ppu_ctrl_reg = value;
	if (old != new) {
		if (new & SPRITE_SIZE_MASK) {
			update_sprite_a12_rises(timer);
		} else {
			timer->sprite_mode = (new & SPRITE_PATTERN_TABLE) >> 3;
			timer->sprite_mask = timer->sprite_mode ? 0xff : 0x00;
		}

		if (timer->irq_enabled) {
//                      printf("calling next_irq() from ctrl_handler()\n");
			a12_timer_schedule_irq(timer);
		}
	}

}

static CPU_WRITE_HANDLER(a12_timer_mask_handler)
{
	struct a12_timer *timer = emu->a12_timer;
	int old, new;

	old = ! !(timer->ppu_mask_reg & RENDERING_MASK);
	new = ! !(value & RENDERING_MASK);

	ppu_mask_reg_write_handler(emu, addr, value, cycles);
	timer->ppu_mask_reg = value;
//      if (old != new) {
//              printf("rendering was %d, is now %d (%d)\n", old, new, cycles);
//      }
	if ((old != new) && timer->irq_enabled) {
		a12_timer_schedule_irq(timer);
	}

}

static CPU_WRITE_HANDLER(a12_timer_oam_addr_handler)
{
	struct a12_timer *timer = emu->a12_timer;

	timer->ppu_oam_addr_reg = value;
	ppu_oam_addr_reg_write_handler(emu, addr, value, cycles);
}

static CPU_WRITE_HANDLER(a12_timer_oam_data_handler)
{
	struct a12_timer *timer = emu->a12_timer;

	ppu_oam_data_reg_write_handler(emu, addr, value, cycles);

	if (timer->in_oam_dma)
		timer->in_oam_dma--;

	if (timer->in_oam_dma == 0 && (timer->ppu_ctrl_reg & SPRITE_SIZE_MASK)) {
		update_sprite_a12_rises(timer);
		if (timer->irq_enabled) {
//                      printf("calling from oam data handler\n");
			a12_timer_schedule_irq(timer);
		}
	}
}

static CPU_WRITE_HANDLER(a12_timer_oam_dma_handler)
{
	struct a12_timer *timer = emu->a12_timer;

	timer->in_oam_dma = 256;
	oam_dma(emu, addr, value, cycles);
}

int a12_timer_save_state(struct emu *emu, struct save_state *state)
{
	struct a12_timer *timer;
	size_t size;
	uint8_t *buf;
	int rc;

	timer = emu->a12_timer;

	size = pack_state(timer, a12_timer_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	pack_state(timer, a12_timer_state_items, buf);

	rc = save_state_add_chunk(state, "A12 ", buf, size);
	free(buf);

	return rc;
}

int a12_timer_load_state(struct emu *emu, struct save_state *state)
{
	struct a12_timer *timer;
	size_t size;
	uint8_t *buf;

	timer = emu->a12_timer;

	if (save_state_find_chunk(state, "A12 ", &buf, &size) < 0)
		return -1;

	unpack_state(timer, a12_timer_state_items, buf);

	timer->prescaler_mask = (1 << timer->prescaler_size) - 1;

	return 0;
}

void a12_timer_set_counter_enabled(struct a12_timer *timer, int enabled, uint32_t cycles) {

	if (!!enabled == timer->counter_enabled)
		return;

	ppu_run(timer->emu->ppu, cycles);
	
	if (!enabled) {
		timer->counter_enabled = 0;
		timer->next_irq = ~0;
		cpu_interrupt_ack(timer->emu->cpu, IRQ_A12_TIMER);
		cpu_interrupt_cancel(timer->emu->cpu, IRQ_A12_TIMER);
	} else {
		timer->counter_enabled = 1;
		timer->reload_flag = 1;
		if (timer->irq_enabled)
			a12_timer_schedule_irq(timer);
	}
}

void a12_timer_set_delta(struct a12_timer *timer, int delta, uint32_t cycles)
{
	if (delta <= 0)
		return;
	ppu_run(timer->emu->ppu, cycles);

	timer->a12_rise_delta = delta;
	cpu_interrupt_ack(timer->emu->cpu, IRQ_A12_TIMER);
	cpu_interrupt_cancel(timer->emu->cpu, IRQ_A12_TIMER);
	a12_timer_schedule_irq(timer);
}

void a12_timer_set_counter(struct a12_timer *timer, int value, uint32_t cycles)
{
	cpu_interrupt_ack(timer->emu->cpu, IRQ_A12_TIMER);
	cpu_interrupt_cancel(timer->emu->cpu, IRQ_A12_TIMER);
	ppu_run(timer->emu->ppu, cycles);

	timer->counter = value & 0xff;
	a12_timer_schedule_irq(timer);
}

int a12_timer_get_counter(struct a12_timer *timer, uint32_t cycles)
{
	ppu_run(timer->emu->ppu, cycles);

	return timer->counter;
}

void a12_timer_set_flags(struct a12_timer *timer, int flags, uint32_t cycles)
{
	ppu_run(timer->emu->ppu, cycles);
	timer->flags = flags;
	a12_timer_schedule_irq(timer);
}

int a12_timer_get_flags(struct a12_timer *timer)
{
	return timer->flags;
}

void a12_timer_set_prescaler_size(struct a12_timer *timer, int size, uint32_t cycles)
{
	if (size > 8)
		size = 8;
	else if (size < 0)
		size = 0;

	ppu_run(timer->emu->ppu, cycles);
	timer->prescaler_size = size;
	timer->prescaler_mask = (1 << size) - 1;
	a12_timer_schedule_irq(timer);
}

int a12_timer_get_prescaler_size(struct a12_timer *timer)
{

	return timer->prescaler_size;
}

void a12_timer_set_prescaler(struct a12_timer *timer, int prescaler, uint32_t cycles)
{
	cpu_interrupt_ack(timer->emu->cpu, IRQ_A12_TIMER);
	cpu_interrupt_cancel(timer->emu->cpu, IRQ_A12_TIMER);
	ppu_run(timer->emu->ppu, cycles);
	timer->prescaler = prescaler;
	a12_timer_schedule_irq(timer);
}

int a12_timer_get_prescaler(struct a12_timer *timer)
{
	return timer->prescaler;
}

void a12_timer_set_force_reload_delay(struct a12_timer *timer, int value, uint32_t cycles)
{
	ppu_run(timer->emu->ppu, cycles);
	timer->force_reload_delay = value;
}

void a12_timer_run(struct a12_timer *timer, uint32_t cycle_count)
{
	struct emu *emu;
	int scanline, cycle;
	int ppu_scanline, ppu_cycle;
	uint32_t cycles, ppu_cycles;
	int odd_frame, short_frame;
	int prev_a12, address;
	int prescaler_wrapped;
	int counter_limit;
	int counter_delta;
	int rendering;
	int next_cycle;

	emu = timer->emu;
	prev_a12 = timer->prev_a12;
	scanline = timer->scanline;
	cycle = timer->cycle;
	cycles = timer->timestamp;
	rendering = timer->ppu_mask_reg & RENDERING_MASK;

	ppu_cycles = ppu_get_cycles(emu->ppu, &ppu_scanline, &ppu_cycle,
				    &odd_frame, &short_frame);

	/* printf("starting with count=%d, scanline=%d, cycle=%d (%d %d), (%d, %d)\n", */
	/*        timer->counter, scanline, cycle, ppu_scanline, ppu_cycle, cycles, ppu_cycles); */

	if (!timer->counter_enabled || !rendering) {
		/* printf("done!!!\n"); */
		timer->scanline = ppu_scanline;
		timer->cycle = ppu_cycle;
		timer->timestamp = ppu_cycles;
		return;
	}

	/* FIXME what about even cycles? is this the right way to handle them? */
	if (!(cycle & 1)) {
		cycle++;
		cycles += emu->ppu_clock_divider;

		if (cycle == 341) {
			cycle = 1;
			scanline++;
			cycles += emu->ppu_clock_divider;
			if (scanline == 241 + timer->vblank_scanlines) {
				scanline = -1;
			}
		}
	}

	if (timer->flags & A12_TIMER_FLAG_COUNT_UP) {
		counter_delta = 1;
		counter_limit = 0xff;
	} else {
		counter_delta = -1;
		counter_limit = 0;
	}

	prescaler_wrapped = 0;

	while (cycles < ppu_cycles) {
		int clock;

		if (scanline > 239) {
			int ppu_clocks_available = (ppu_cycles - cycles) /
				emu->ppu_clock_divider;
			int ppu_clocks_left = (240 + timer->vblank_scanlines + 1 - scanline) *
				341 - cycle;

			if (ppu_clocks_available < ppu_clocks_left)
				ppu_clocks_left = ppu_clocks_available;

			cycle += ppu_clocks_left;
			cycles += ppu_clocks_left * emu->ppu_clock_divider;
			scanline += cycle / 341;
			cycle %= 341;

			if (scanline >= 240 + 1 + timer->vblank_scanlines) {
				scanline -= 240 + 1 + timer->vblank_scanlines + 1;
			}

			if (scanline < 0) {
				cycle = 1;
				cycles += emu->ppu_clock_divider;
			}
	/* printf("ending with count=%d, scanline=%d, cycle=%d (%d %d), (%d, %d) (v)\n", */
	/*        timer->counter, scanline, cycle, ppu_scanline, ppu_cycle, cycles, ppu_cycles); */

			continue;
		}

		address = 0x0000;
		clock = 0;

		if (cycle & 4) {
			if (cycle < 256) {
				address = BG_MODE() << 8;
			} else if (cycle < 321) {
				if (!LARGE_SPRITES()) {
					address = SPRITE_MODE() << 9;
				} else if (SPRITE_TABLE(scanline, cycle)) {
					address = 0x1000;
				}
			} else if (cycle < 337) {
				address = BG_MODE() << 8;
			}
		}

		if (prev_a12 && !address) {
			timer->next_clock = (cycles - timer->frame_start_cpu_cycles);
			timer->next_clock /= emu->cpu_clock_divider;
			timer->next_clock += timer->a12_rise_delta;
			timer->next_clock *= emu->cpu_clock_divider;
			timer->next_clock += timer->frame_start_cpu_cycles;
		} else if (!prev_a12 && address) {
			if (cycles <= timer->next_clock)
				timer-> next_clock = ~0;
			else
				clock = 1;
		}

		if (clock) {
			if (timer->prescaler_size) {
				int tmp = timer->prescaler & timer->prescaler_mask;
				tmp--;

				if (tmp & (1 << timer->prescaler_size)) {
					prescaler_wrapped = 1;
				}
				tmp &= timer->prescaler_mask;
				timer->prescaler &= ~timer->prescaler_mask;
				timer->prescaler |= tmp;

				if (!prescaler_wrapped)
					continue;
			}

			if ((timer->counter == counter_limit) || timer->reload_flag) {
				int reload;

				timer->reload_flag = 0;
				if (timer->flags & A12_TIMER_FLAG_WRAP) {
					reload = (~counter_limit) & 0xff;
				} else {
					reload = timer->reload;
				}

				timer->counter = reload;

				if (timer->reload_flag) {
					if (!(timer->flags & A12_TIMER_FLAG_COUNT_UP))
						timer->counter += timer->force_reload_delay;
					else
						timer->counter -= timer->force_reload_delay;
				}
			} else {
				timer->counter += counter_delta;
			}
			/* printf("clock: %d,%d (%d) (%d %d %d %d) %x %x\n", scanline, cycle, timer->counter, cycles, ppu_cycles, ppu_scanline, ppu_cycle, prev_a12, address); */
		} else {
			/* printf("no clock: %d,%d %d\n", scanline, cycle, cycles); */
		}

		prev_a12 = address;
		prescaler_wrapped = 0;

		next_cycle = -1;

		if (cycle < 261 && !BG_MODE()) {
			next_cycle = 261;
		} else if ((cycle > 256) && (cycle < 325) &&
			 !timer->sprite_a12_table[scanline + 1]) {
			next_cycle = 325;
		} else if ((cycle > 324) && (cycle < 337) && !BG_MODE()) {
			next_cycle = 337;
		}

		if (next_cycle != -1) {
			cycles += (next_cycle - cycle) * emu->ppu_clock_divider;
			cycle = next_cycle;
		} else if (cycle % 4 == 1) {
			cycle += 4;
			cycles += 4 * emu->ppu_clock_divider;
		} else {
			cycle += 2;
			cycles += 2 * emu->ppu_clock_divider;
		}

		if (cycle == 341) {
			scanline++;
			cycles += emu->ppu_clock_divider;
			cycle = 1;
		}

		//printf("1. cycles: %d ppu_cycles: %d\n", cycles, ppu_cycles);
	}

	/* printf("ending with count=%d, scanline=%d, cycle=%d (%d,%d) (%d,%d)\n", */
	/*        timer->counter, scanline, cycle, cycles/ 4  / 341 - 1, cycles / 4 % 341, ppu_cycles / 4 / 341 -1, */
	/* 	  ppu_cycles / 4 % 341); */

	timer->prev_a12 = prev_a12;
	timer->scanline = scanline;
	timer->cycle = cycle;
	timer->timestamp = cycles;
}
