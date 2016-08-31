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

#ifndef _MMC3_H_
#define _MMC3_H_

#define _bank_select board->data[0]
#define _wram_protect board->data[1]
#define _mmc6_compat_hack board->data[3]
#define _first_bank_select board->data[4]
#define _bank_select_mask board->data[5]
#define _chr_mode_mask board->data[6]
#define _cpu_cycle_irq_enable board->data[8]

#define _ext_regs (board->data + 9)

CPU_WRITE_HANDLER(mmc3_wram_protect);
CPU_WRITE_HANDLER(mmc3_bank_select);
CPU_WRITE_HANDLER(mmc3_bank_data);

int mmc3_init(struct board *board);
void mmc3_cleanup(struct board *board);
void mmc3_reset(struct board *board, int);
void mmc3_end_frame(struct board *board, uint32_t cycles);
void mmc3_txsrom_mirroring(struct board *board);
int mmc3_save_state(struct board *board, struct save_state *state);
int mmc3_load_state(struct board *board, struct save_state *state);

struct board_funcs mmc3_funcs;
extern struct bank mmc3_init_prg[];
extern struct bank mmc3_init_chr0[];
extern struct board_write_handler mmc3_write_handlers[];

#endif /* _MMC3_H_ */
