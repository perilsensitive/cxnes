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

#include "board_private.h"
#include "input.h"
#include "actions.h"
#include "patch.h"
#include "file_io.h"
#include "fds.h"
#include "fds_audio.h"
#include "m2_timer.h"

#define FDS_BYTE_READ_CYCLES 150

#define _output_reg board->data[0]
#define _drive_status_reg board->data[1]
#define _read_buffer board->data[2]
#define _write_buffer board->data[3]
#define _control_reg board->data[4]
#define _status_reg board->data[5]
#define _flip_disk_counter board->data[6]
#define _bios_patch_enabled board->data[8]
#define _gap_covered board->data[9]
#define _previous_crc board->data[15]
#define _next_clock board->timestamps[0]
#define _next_disk_interrupt board->timestamps[1]
#define _offset board->timestamps[2]
#define _disk_offset board->timestamps[3]
#define _crc board->timestamps[4]

#define _auto_eject_state   board->data[10]
#define _auto_eject_counter board->data[11]
#define _auto_eject_counter_max board->data[12]
//#define board->emu->config->fds_auto_disk_change_enabled   board->data[12]
#define _diskio_enabled     board->data[13]
#define _dirty_flag board->data[14]

#define FDS_DRIVE_BATTERY 0x80

#define FDS_DISK_INSERTED  0x01
#define FDS_DISK_READY     0x02
#define FDS_DISK_PROTECTED 0x04

#define FDS_CTRL_MOTOR     0x01
#define FDS_CTRL_SCAN      0x02
#define FDS_CTRL_READ      0x04
#define FDS_CTRL_MIRRORING 0x08
#define FDS_CTRL_CRC       0x10
#define FDS_CTRL_XFER      0x40
#define FDS_CTRL_IRQ       0x80

#define FDS_STATUS_IRQ   0x01
#define FDS_STATUS_XFER  0x02
#define FDS_STATUS_CRC   0x10
#define FDS_STATUS_EOF   0x40

#define AUTO_EJECT_INSERTED 0
#define AUTO_EJECT_EJECTED  1
#define AUTO_EJECT_WAITING  2
#define AUTO_EJECT_DISABLED 3

static int fds_init(struct board *board);
static void fds_cleanup(struct board *board);
static void fds_reset(struct board *board, int);
static void fds_end_frame(struct board *board, uint32_t);

static void fds_run(struct board *board, uint32_t cycles);
static int fds_load_state(struct board *board, struct save_state *state);
static int fds_save_state(struct board *board, struct save_state *state);
static int fds_input_handler(void *, uint32_t event, uint32_t value);

static int fds_disk_set_inserted(struct board *board, int state);
static int fds_disk_set_side(struct board *board, int side);

static int fds_read_byte(struct board *board, int do_high_level, uint32_t cycles);
static int fds_write_byte(struct board *board);

static CPU_WRITE_HANDLER(fds_write_handler);
static CPU_READ_HANDLER(fds_read_handler);
static CPU_READ_HANDLER(fds_bios_misc);
static CPU_READ_HANDLER(fds_bios_read_write_byte);

struct auto_eject_timer_setup {
	uint8_t game_id[4];
	uint8_t manufacturer;
	uint8_t revision;
	int count;
};

/* some games don't work well with the standard auto disk eject
   counter value; look them up by game id, manufacturer and revision
   here and use the alternative value provided.
*/
static struct auto_eject_timer_setup eject_timer_settings[] = {
	{{0x4c, 0x54, 0x44, 0x20}, 0xe7, 0x00, 60 }, /* Lutter */
	{{0x4e, 0x45, 0x55, 0x20}, 0xb3, 0x00, 85 }, /* 19 */
	{{0x46, 0x59, 0x54, 0x20}, 0xb3, 0x00, 50 }, /* Fairytale */
	{{0, 0, 0, 0}, 0, 0, 0 },
};

static struct input_event_handler fds_handlers[] = {
	{ ACTION_FDS_EJECT, fds_input_handler},
	{ ACTION_FDS_SELECT, fds_input_handler},
	{ ACTION_FDS_FLIP, fds_input_handler},
	{ ACTION_NONE },
};

static struct board_funcs fds_funcs = {
	.init = fds_init,
	.reset = fds_reset,
	.cleanup = fds_cleanup,
	.end_frame = fds_end_frame,
	.run = fds_run,
	.load_state = fds_load_state,
	.save_state = fds_save_state,
};

static struct bank fds_init_prg[] = {
	{0, 0, SIZE_32K, 0x6000, MAP_PERM_READWRITE, MAP_TYPE_RAM0},
	{-1, 0, SIZE_8K, 0xe000, MAP_PERM_READ, MAP_TYPE_BIOS},
	{.type = MAP_TYPE_END},
};

static struct board_write_handler fds_write_handlers[] = {
	{fds_write_handler, 0x4020, 16, 0},
	{NULL},
};

static struct board_read_handler fds_read_handlers[] = {
	{fds_read_handler, 0x4030, 7, 0},
	{NULL},
};

struct board_info board_fds = {
	.board_type = BOARD_TYPE_FDS,
	.name = "Famicom Disk System",
	.funcs = &fds_funcs,
	.mirroring_shift = 3,
	.mirroring_values = std_mirroring_vh,
	.init_prg = fds_init_prg,
	.init_chr0 = std_chr_8k,
	.read_handlers = fds_read_handlers,
	.write_handlers = fds_write_handlers,
	.max_prg_rom_size = SIZE_8K + SIZE_64K * 8,
	.max_chr_rom_size = 0,
	.min_wram_size = {SIZE_32K, 0},
	.max_wram_size = {SIZE_32K, 0},
	.min_vram_size = {SIZE_8K, 0},
	.max_vram_size = {SIZE_8K, 0},
	.flags = BOARD_INFO_FLAG_MIRROR_M|
	         BOARD_INFO_FLAG_PRG_IPS|
		 BOARD_INFO_FLAG_M2_TIMER,
};

static void fds_init_modified_ranges(struct board *board)
{
	struct fds_block_list *block_list;
	struct range_list *range;
	struct range_list *dirty_blocks;
	off_t offset;
	size_t side_size;
	int previous_side;
	int i;

	dirty_blocks = NULL;

	side_size = board->emu->rom->disk_side_size;

	fds_validate_image(board->emu->rom, &block_list, 0);
	if (!block_list)
		return;

	previous_side = -1;
	for (i = 0; i < block_list->total_entries; i++) {
		struct fds_block_list_entry *entry;
		int current_side;

		entry = &block_list->entries[i];

		current_side = entry->offset / side_size;

		if (previous_side != current_side) {
			offset = current_side * side_size;
			offset += board->emu->rom->offset;
		}

		entry->new_offset = offset;
		offset += entry->size;
		previous_side = block_list->entries[i].offset /
		                side_size;
	}
				
	for (range = board->modified_ranges; range;
	     range = range->next) {
		for (i = 0; i < block_list->total_entries; i++) {
			struct fds_block_list_entry *entry;
			size_t new_size;
			off_t new_offset;

			entry = &block_list->entries[i];

			if (range->offset + range->length <=
			    entry->new_offset) {
				continue;
			}

			if (entry->new_offset + entry->size <=
			    range->offset) {
				continue;
			}

			if (i) {
				new_offset = block_list->entries[i - 1].offset +
				block_list->entries[i - 1].size + 2;
			} else {
				new_offset = entry->offset;
			}

			/* We want to mark not only the block data, but also
			   gap, start mark and CRC.
			*/

			new_size = entry->offset + entry->size + 2;
			new_size -= new_offset;

			add_range(&dirty_blocks, new_offset, new_size);
		}
	}

	free_range_list(&board->modified_ranges);
	board->modified_ranges = dirty_blocks;
	fds_block_list_free(block_list);
}

static void fds_write_save(struct board *board)
{
	struct fds_block_list *block_list;
	struct range_list *range, *new_range;
	struct range_list *dirty_blocks;
	size_t side_size;
	struct rom fds_image;
	off_t offset;
	int previous_side;
	int i;

	dirty_blocks = NULL;

	side_size = board->emu->rom->disk_side_size;

	/* printf("flushing dirty disk data\n"); */

	if (!board->emu->config->fds_use_patch_for_saves) {
		size_t buffer_size;
		uint8_t *buffer;
		char *save_file;

		buffer_size = board->emu->rom->buffer_size;
		buffer = malloc(buffer_size);

		if (!buffer)
			return;

		memcpy(buffer, board->emu->rom->buffer, buffer_size);
		fds_image.buffer = buffer;
		fds_image.offset = board->emu->rom->offset;
		fds_image.buffer_size = buffer_size;
		fds_image.disk_side_size = board->emu->rom->disk_side_size;

		if (fds_convert_to_fds(&fds_image))
			return;

		buffer_size /= 65500;
		buffer_size *= 65500;
		buffer_size += fds_image.offset;

		save_file = config_get_path(board->emu->config,
						CONFIG_DATA_DIR_FDS_SAVE,
						board->emu->save_file, 1);

		if (!save_file)
			return;

		if (buffer_size && writefile(save_file, buffer, buffer_size)) {
			log_err("failed to write FDS save file \"%s\"\n",
				save_file);
		}

		if (save_file)
			free(save_file);

		free(buffer);

		return;

	}

	fds_validate_image(board->emu->rom, &block_list, 0);
	if (!block_list)
		return;


	previous_side = -1;
	for (i = 0; i < block_list->total_entries; i++) {
		struct fds_block_list_entry *entry;
		int current_side;
		
		entry = &block_list->entries[i];

		current_side = entry->offset / side_size;

		if (previous_side != current_side) {
			offset = current_side * side_size;
			offset += board->emu->rom->offset;
		}

		entry->new_offset = offset;
		offset += entry->size;
		previous_side = block_list->entries[i].offset /
		                side_size;
	}
				
	for (range = board->modified_ranges; range;
	     range = range->next) {
		for (i = 0; i < block_list->total_entries; i++) {
			struct fds_block_list_entry *entry;
			entry = &block_list->entries[i];
				
			if (range->offset + range->length <=
			    entry->offset) {
				continue;
			}

			if (entry->offset + entry->size <=
			    range->offset) {
				continue;
			}

			/* printf("adding range at %jd (moved to %jd) of size %zu\n", */
			/*        (intmax_t)entry->offset, */
			/*        (intmax_t)entry->new_offset, entry->size); */

			new_range = add_range(&dirty_blocks, entry->offset,
					       entry->size);
			new_range->output_offset = entry->new_offset;
		}
	}

	board_write_ips_save(board, dirty_blocks);
	/* board_write_ips_save(board, board->modified_ranges); */
	free_range_list(&dirty_blocks);
	fds_block_list_free(block_list);
}

static void fds_set_eof(struct board *board)
{
	_offset = board->emu->rom->disk_side_size;
	_drive_status_reg &= ~FDS_DISK_READY;
	_gap_covered = 0;
	_crc = 0;

	/* FIXME should these be handled by schedule_disk_interrupt? */
	_next_clock = ~0;
	_next_disk_interrupt = ~0;
	cpu_interrupt_cancel(board->emu->cpu, IRQ_DISK);
	cpu_set_overclock_allowed(board->emu->cpu, 1);

	if (_dirty_flag) {
		fds_write_save(board);
		_dirty_flag = 0;
	}

	_bios_patch_enabled = board->emu->config->fds_bios_patch_enabled;
}

static int fds_init(struct board *board)
{
	struct emu *emu;
	int i;

	emu = board->emu;

	int read_write_addrs[] = {
		/* Reads */
		0xe357, 0xe366, 0xe37f, 0xe382, 0xe38a, 0xe396,
		0xe399, 0xe3a9, 0xe3ae, 0xe44e, 0xe473, 0xe47a,
		0xe489, 0xe4a0, 0xe4a3, 0xe4e7, 0xe509,
		0xe533, 0xe563, 0xe57a, 0xe57d, 0xe5f9,	0xe628,
		0xe6f7, 0xe706, 0xe71b, 0xe761,	0xe770,

		/* Xfer1stByte */
		0xe4fb, 0xe6a3, 0xe6cc,

		/* Writes */
		0xe499, 0xe5b9, 0xe5cb, 0xe60c, 0xe643,
		0xe6d1,	0xe729,
	};

	_disk_offset = 0;

	fds_audio_init(emu);
	fds_audio_install_handlers(emu, 0);

	cpu_set_read_handler(emu->cpu, 0xe29a, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe4a6, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xefaf, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xef44, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe682, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe53c, 1, 0,
			     fds_bios_misc);

		
	cpu_set_read_handler(emu->cpu, 0xe478, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe4e5, 1, 0,
			     fds_bios_misc);
	/* Calls to MilSecTimer */
	cpu_set_read_handler(emu->cpu, 0xe652, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe655, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe65d, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe691, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe6e8, 1, 0,
			     fds_bios_misc);
	cpu_set_read_handler(emu->cpu, 0xe6ed, 1, 0,
			     fds_bios_misc);
		
	for (i = 0; i < sizeof(read_write_addrs) /
		     sizeof(read_write_addrs[0]); i++) {
		cpu_set_read_handler(emu->cpu, read_write_addrs[i], 1, 0,
				     fds_bios_read_write_byte);
	}

	fds_convert_to_raw(emu->rom);
	board->prg_rom.data = emu->rom->buffer + emu->rom->offset;
	board->prg_rom.size = emu->rom->buffer_size - emu->rom->offset;
	fds_init_modified_ranges(emu->board);

	cpu_set_read_handler(emu->cpu, 0xe445, 1, 0, fds_read_handler);

	input_connect_handlers(fds_handlers, emu);

	return 0;
}

static void fds_cleanup(struct board *board)
{
	if (_dirty_flag) {
		fds_write_save(board);
	}
	free_range_list(&board->modified_ranges);
	input_disconnect_handlers(fds_handlers);
	fds_audio_cleanup(board->emu);
}

void fds_reset(struct board *board, int hard)
{
	_bios_patch_enabled = board->emu->config->fds_bios_patch_enabled;

	if (hard) {
		m2_timer_set_flags(board->emu->m2_timer,
				   M2_TIMER_FLAG_ONE_SHOT |
				   M2_TIMER_FLAG_AUTO_IRQ_DISABLE, 0);
		m2_timer_set_enabled(board->emu->m2_timer, 0, 0);
	}

	if (hard || _bios_patch_enabled) {
		_drive_status_reg = FDS_DISK_INSERTED;
	}

	if (!(_drive_status_reg & FDS_DISK_INSERTED))
		_drive_status_reg = FDS_DISK_PROTECTED;
	else
		_drive_status_reg = FDS_DISK_INSERTED;

	if (board->emu->config->fds_auto_disk_change_enabled)
		_auto_eject_state = AUTO_EJECT_WAITING;
	else
		_auto_eject_state = AUTO_EJECT_DISABLED;
		
	_flip_disk_counter = ~0;
	_next_clock = ~0;
	_next_disk_interrupt = ~0;
	_offset = board->emu->rom->disk_side_size;
	_crc = 0;

	fds_audio_reset(board->emu->fds_audio, hard);
}

/*
 * FDS Timer IRQ Operation
 *
 * It appears (based on the NESdev wiki, which is incomplete), other
 * emulators' source code, and experimentation that the timer IRQ
 * is implemented as follows:
 *
 * Write-Only registers:
 * $4020 is the low byte of the timer reload register
 * $4021 is the high byte of the timer reload register
 * $4022 is the timer control register
 *  Bit 1==1: timer enabled
 *  Bit 1==0: timer disabled
 *
 *  When enabling timer, reload register is copied to counter, then
 *  reload register is set to 0.
 *
 *  Writing any of these registers acks the IRQ.
 *
 * Read-Only register:
 * $4030 bit 0 is the timer irq status (1 = irq fired).
 *
 *  Reading $4030 acknowledges the timer IRQ.
 *
 * The timer is clocked on every falling edge of M2.  When it reaches 0,
 * the IRQ fires.  Timer IRQs are then disabled.
 */

static int find_start_of_next_block(struct board *board)
{
	uint8_t *disk;
	size_t side_size;
	int i;

	side_size = board->emu->rom->disk_side_size;

	disk = board->prg_rom.data + _disk_offset;

	i = 0;
	while (_offset + i < side_size) {
		if (disk[_offset + i])
			break;

		i++;
	}

	if (_offset + i == side_size)
		return -1;

	return i;
}

static void calc_next_clock(struct board *board, int is_first, uint32_t cycles)
{
	struct emu *emu;
	int cyc;

	emu = board->emu;
	cyc = (_next_clock - cycles) / emu->cpu_clock_divider;
	cyc -= 2; /* Number of cycles XferByte takes */
	if (is_first)
		cyc -= 31; /* Number of additional cycles Xfer1stByte takes */
	cyc = 3 - (cyc % 3); /* Number of cycles taken by infinite loop after
				IRQ is asserted.
			     */
	cyc += 43; /* Number of cycles disk tranfer IRQ handler takes */
	_next_clock = cycles + (FDS_BYTE_READ_CYCLES - cyc) *
		emu->cpu_clock_divider;
}

static void schedule_disk_interrupt(struct board *board, uint32_t cycles)
{
	int irq = ~0;

	if ((_control_reg & FDS_CTRL_MOTOR) &&
	    (_control_reg & FDS_CTRL_SCAN)) {
		if (_next_clock == ~0) {
			_next_clock = FDS_BYTE_READ_CYCLES;
			_next_clock *= board->emu->cpu_clock_divider;
			_next_clock += cycles;
		}
	} else {
		if ((_next_disk_interrupt != ~0) &&
		    (_control_reg & FDS_CTRL_IRQ)) {
			cpu_interrupt_cancel(board->emu->cpu, IRQ_DISK);
		}

		_next_clock = ~0;
		_next_disk_interrupt = ~0;

		return;
	}

	if (_control_reg & FDS_CTRL_READ) {
		if (_control_reg & FDS_CTRL_XFER) {
			if (_gap_covered) {
				irq = _next_clock;
			} else {
				int clocks = find_start_of_next_block(board) + 1;
				if (clocks >= 0) {
					irq = FDS_BYTE_READ_CYCLES * clocks;
					irq *= board->emu->cpu_clock_divider;
					irq += _next_clock;
				}
			}
		}
	} else {
		if ((_control_reg & FDS_CTRL_XFER) &&
		    !(_control_reg & FDS_CTRL_CRC)) {
			irq = _next_clock;
		}
	}

	if ((irq != _next_disk_interrupt) && (_control_reg & FDS_CTRL_IRQ)) {
		cpu_interrupt_cancel(board->emu->cpu, IRQ_DISK);
		if (irq != ~0)
			cpu_interrupt_schedule(board->emu->cpu, IRQ_DISK, irq);

		_next_disk_interrupt = irq;
	}
}

static void fds_run(struct board *board, uint32_t cycles)
{
	if (board->emu->overclocking)
		return;

	fds_audio_run(board->emu->fds_audio, cycles);

	if (_next_clock == ~0)
		return;

	while (_next_clock < cycles) {
		if (_control_reg & FDS_CTRL_READ) {
			fds_read_byte(board, 0, _next_clock);
		}
		else {
			fds_write_byte(board);
		}

		if (_next_clock != ~0) {
			_next_clock += FDS_BYTE_READ_CYCLES *
				board->emu->cpu_clock_divider;
		}
	}
}

static void fds_end_frame(struct board *board, uint32_t cycles)
{
	if (_next_clock != ~0)
		_next_clock -= cycles;

	if ((board->emu->config->fds_auto_disk_change_enabled) &&
	    (_auto_eject_state < AUTO_EJECT_WAITING)) {
		if (_auto_eject_counter) {
			_auto_eject_counter--;
		} else {
			_auto_eject_counter = _auto_eject_counter_max;
			_auto_eject_state ^= 1;
		}
	}

	if ((int)_flip_disk_counter > 0) {
		_flip_disk_counter--;
		if (!_flip_disk_counter) {
			_flip_disk_counter = ~0;
			fds_disk_set_inserted(board, 1);
		}
	}

	fds_audio_end_frame(board->emu->fds_audio, cycles);
}

static int fds_find_start_of_next_block(struct board *board, uint8_t *tmp, int offset)
{
	int i;
	size_t side_size;

	side_size = board->emu->rom->disk_side_size;

	for (i = offset; i < side_size; i++) {
		if (tmp[i])
			break;
	}

	if (i == side_size)
		return i;

	if (tmp[i] == 0x80)
		i++;

	return i;
}

static int fds_check_file_list(struct board *board, uint8_t *disk, uint8_t *file_list)
{
	int file_count;
	int f, l;
	int off;
	int matches;
	size_t side_size;

	if (!disk || !file_list || (file_list[0] == 0xff))
		return -1;

	side_size = board->emu->rom->disk_side_size;

	off = 0;
	/* Find end of header block */
	off = fds_find_start_of_next_block(board, disk, off) + 58;
	
	if (off >= side_size || (disk[off - 58] != 0x01))
		return -1;

	/* Get file count */
	off = fds_find_start_of_next_block(board, disk, off) + 1;
	
	if (off >= side_size || (disk[off - 1] != 0x02))
		return -1;

	file_count = disk[off];

	off += 3;

	if (off >= side_size)
		return -1;

	matches = 0;
	/* Check each file's id byte against the file list */
	for (f = 0; f < file_count; f++) {
		size_t file_size;
		int id;


		off = fds_find_start_of_next_block(board, disk, off) + 18;

		if (off >= side_size || (disk[off - 18] != 0x03)) {
			return -1;
		}

		file_size  = disk[off - 5];
		file_size |= disk[off - 4] << 8;
		id = disk[off - 16];

		for (l = 0; l < 20; l++) {
			if (file_list[l] == 0xff)
				break;

			if (file_list[l] == id) {
				matches++;
				break;
			}
		}

		off = fds_find_start_of_next_block(board, disk, off);
		if (off >= side_size || (disk[off] != 0x04))
			return -1;

		off += file_size + 2 + 1;

		if (off >= side_size)
			return -1;

	}

	return matches;
}

/*
  Auto disk select is triggered by reads of $E445, which is the start
  of the disk header check routine.  It uses cpu_peek() to get
  the address of the disk header struct, and then uses it again to
  read the struct itself.
*/
static void fds_auto_disk_select(struct board *board)
{
	uint8_t request[10];
	int request_addr;
	int file_list_addr;
	int ret_addr;
	uint8_t *tmp, *first_header_match, *header;
	uint8_t *first_list_match;
	uint8_t *new_side;
	int header_matches;
	int list_matches;
	int i;
	int sp;
	size_t side_size;

	side_size = board->emu->rom->disk_side_size;

	request_addr = cpu_peek(board->emu->cpu, 0x0000);
	request_addr |= cpu_peek(board->emu->cpu, 0x0001) << 8;
	file_list_addr = cpu_peek(board->emu->cpu, 0x0002);
	file_list_addr |= cpu_peek(board->emu->cpu, 0x0003) << 8;

	sp = cpu_get_stack_pointer(board->emu->cpu);

	ret_addr = (cpu_peek(board->emu->cpu, 0x100 + (sp + 2)) << 8) |
		cpu_peek(board->emu->cpu, 0x100 + (sp + 1));
	ret_addr++;

	for (i = 0; i < sizeof(request); i++) {
		request[i] = cpu_peek(board->emu->cpu, request_addr + i);
//		printf("request[%d]: %02x\n", i, request[i]);
	}

	/* Check each side's header, starting with the current one,
	   to see if it matches the request.  Each byte of the request
	   must either match the corresponding byte in the header, or
	   else be "$FF" to match anything.

	   Most games will only have one side that matches.  Some
	   unlicensed games pass a request that matches multiple
	   sides.  In this case, we leave the current side selected.

	   Otherwise, select the correct side and allow ChkDiskHdr to
	   continue.  Note that the user may still manually select a
	   side before re-inserting the disk, but this will override
	   that selection if it doesn't match the request.  In the
	   multiple-match case, the user's selection will be honored
	   since we can't determine the correct side to select.
	 */

	header_matches = 0;
	list_matches = 0;
	first_header_match = NULL;
	first_list_match = NULL;
	tmp = board->prg_rom.data + _disk_offset;
	do {
		int i = fds_find_start_of_next_block(board, tmp, 0);

		if (i == side_size)
			break;

		header = tmp + i;

		if (!_auto_eject_counter_max) {
			struct auto_eject_timer_setup *setup;
			setup = &eject_timer_settings[0];

			for (i = 0; setup->count; i++) {
				/* printf("{{0x%02x, 0x%02x, 0x%02x, 0x%02x}, 0x%02x, 0x%02x, 0x%02x}\n", */
				/*        header[16], */
				/*        header[17], */
				/*        header[18], */
				/*        header[19], */
				/*        header[15], */
				/*        header[20], 0); */
				if ((header[15] == setup->manufacturer) &&
				    (header[16] == setup->game_id[0]) &&
				    (header[17] == setup->game_id[1]) &&
				    (header[18] == setup->game_id[2]) &&
				    (header[19] == setup->game_id[3]) &&
				    (header[20] == setup->revision)) {
					_auto_eject_counter_max = setup->count;
					break;
				}
				setup++;
			}

			if (!eject_timer_settings[i].count)
				_auto_eject_counter_max = 68;
		}

		for (i = 0; i < sizeof(request); i++) {
			if ((request[i] != header[15 + i]) && (request[i] != 0xff)) {
				break;
			}
		}


		if (i == sizeof(request)) {
			int file_matches;
			uint8_t file_list[20];
			int i;

			header_matches++;
			if (first_header_match == NULL)
				first_header_match = tmp;

			if (ret_addr == 0xe21d) {
				for (i = 0; i < 20; i++) {
					file_list[i] = cpu_peek(board->emu->cpu,
								file_list_addr + i);

//					printf("list[%d]: %02x\n", i, file_list[i]);
					if (file_list[i] == 0xff)
						break;
				}

				file_matches = fds_check_file_list(board, tmp, file_list);
				if (file_matches > 0) {
					list_matches++;
					if (first_list_match == NULL)
						first_list_match = tmp;
				}
			}
		}

		tmp += side_size;
		if (tmp >= board->prg_rom.data + (board->prg_rom.size / side_size) *
		    side_size)
			tmp = board->prg_rom.data;
	} while (tmp != board->prg_rom.data + _disk_offset);

	if (header_matches == 1)
		new_side = first_header_match;
	else if (list_matches == 1)
		new_side = first_list_match;
	else
		new_side = NULL;

	if (new_side && (board->prg_rom.data + _disk_offset != new_side)) {
		_disk_offset = new_side - board->prg_rom.data;
		_flip_disk_counter = ~0;
		osdprintf("Disk %u, side %c auto-selected\n",
			  (_disk_offset) / side_size / 2 + 1,
			  (_disk_offset) / side_size % 2 ? 'B' : 'A');
		_auto_eject_state = AUTO_EJECT_WAITING;
	} else if (!new_side && (request_addr != 0xeff5)) {
		/* If the game can't use auto-select, it can't use
		   auto-eject either.  This is not enforced for the
		   LoadFiles call that the BIOS uses to boot the disk.
		   Some images have multiple disk sides that are 'disk
		   1, side A' and all of them are supposed to be
		   bootable.  Multiple matches are OK in this case.
		*/
//		printf("disabled\n");
		_auto_eject_state = AUTO_EJECT_DISABLED;
	}
}

static CPU_WRITE_HANDLER(fds_write_handler)
{
	struct board *board = emu->board;
	int flags;

	fds_run(board, cycles);

	switch (addr) {
	case 0x4020:
		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_reload_lo(emu->m2_timer, value, cycles);
		break;
	case 0x4021:
		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_reload_hi(emu->m2_timer, value, cycles);
		break;
	case 0x4022:
		if (value & 0x01)
			flags = M2_TIMER_FLAG_RELOAD;
		else
			flags = M2_TIMER_FLAG_ONE_SHOT |
				M2_TIMER_FLAG_AUTO_IRQ_DISABLE;

		m2_timer_ack(emu->m2_timer, cycles);
		m2_timer_set_flags(emu->m2_timer, flags, cycles);

		board->irq_control = value & 0x03;

		if (value & 0x02)
			m2_timer_force_reload(emu->m2_timer, cycles);
		if ((value & 0x03) == 0x02)
			m2_timer_set_reload(emu->m2_timer, 0, cycles);
		    

		m2_timer_set_enabled(emu->m2_timer, value & 0x02,
				     cycles);
		break;
	case 0x4023:
		_diskio_enabled = value & 0x01;
		fds_audio_enable(emu->fds_audio, cycles, value & 0x02);
		break;
	case 0x4024:
		if (!_diskio_enabled)
			break;

		_status_reg &= ~FDS_STATUS_XFER;
		if (board->emu->config->fds_auto_disk_change_enabled && (_auto_eject_state != AUTO_EJECT_DISABLED)) {
			_auto_eject_state = AUTO_EJECT_WAITING;
			_flip_disk_counter = ~0;
		}
		cpu_interrupt_ack(emu->cpu, IRQ_DISK);
		schedule_disk_interrupt(board, cycles);
		_write_buffer = value;
		break;
	case 0x4025:
		if (emu->overclocking) {
			if ((value & 0x03) == 0x01) {
				cpu_set_pc(emu->cpu, cpu_get_opcode_address(emu->cpu));
				cpu_force_overclock_end(emu->cpu);
				break;
			}
		} else {
			if ((value & 0x03) == 0x01) {
				cpu_set_overclock_allowed(emu->cpu, 0);
			} else {
				cpu_set_overclock_allowed(emu->cpu, 1);
			}
		}
		standard_mirroring_handler(emu, 0, value, cycles);

		if (!_diskio_enabled)
			break;

		/* invert the sense of the SCAN bit so that
		   we can treat it as if it were active when
		   set (instead of reset). */
		value ^= 0x02;

		/* FIXME is this correct? */
		cpu_interrupt_ack(emu->cpu, IRQ_DISK);
		_status_reg &= ~FDS_STATUS_XFER;

		/* FIXME not sure if this is correct, but it seems to work */
		if (!(value & FDS_CTRL_MOTOR) || !(value & FDS_CTRL_SCAN)) {
			fds_set_eof(board);
			value &=
			    ~(FDS_CTRL_SCAN | FDS_CTRL_XFER | FDS_CTRL_READ |
			      FDS_CTRL_IRQ | FDS_CTRL_MOTOR);
			_control_reg = value;
			return;
		}

		if ((value ^ _control_reg) & FDS_CTRL_SCAN) {
			if (value & FDS_CTRL_SCAN) {
				_offset = 0;
				_drive_status_reg |= FDS_DISK_READY;
			}
		}

		if ((value ^ _control_reg) & FDS_CTRL_XFER) {
			_gap_covered = 0;
		}

		if ((value ^ _control_reg) & FDS_CTRL_IRQ) {
			if (!(value & FDS_CTRL_IRQ))
				cpu_interrupt_cancel(emu->cpu, IRQ_DISK);
		}

		_control_reg = value;

		schedule_disk_interrupt(board, cycles);

		break;
	case 0x4026:
		if (!_diskio_enabled)
			break;

		_output_reg = value;
		break;
	}
}

static CPU_READ_HANDLER(fds_read_handler)
{
	struct board *board = emu->board;

	fds_run(board, cycles);

	switch (addr) {
	case 0x4030:
		value = _status_reg;
		if (m2_timer_get_irq_status(emu->m2_timer, cycles)) {
			value |= FDS_STATUS_IRQ;
		}

		if (_offset == board->emu->rom->disk_side_size)
			value |= FDS_STATUS_EOF;

		if (_crc) {
			value |= FDS_STATUS_CRC;
		}

		cpu_interrupt_ack(emu->cpu, IRQ_DISK);
		m2_timer_ack(emu->m2_timer, cycles);
		if ((board->irq_control & 0x03) == 0x03)
			m2_timer_schedule_irq(emu->m2_timer, cycles);
		/* Mask out the IRQ flags */
		_status_reg &= ~(FDS_STATUS_IRQ | FDS_STATUS_XFER);

		schedule_disk_interrupt(board, cycles);
		break;
	case 0x4031:
		if (!_diskio_enabled)
			break;

		if (board->emu->config->fds_auto_disk_change_enabled && (_auto_eject_state != AUTO_EJECT_DISABLED)) {
			_auto_eject_state = AUTO_EJECT_WAITING;
			_flip_disk_counter = ~0;
		}
		value = _read_buffer;

		cpu_interrupt_ack(emu->cpu, IRQ_DISK);
		/* Mask out the byte transfer flag since we've acked
		   the interrupt */
		_status_reg &= ~FDS_STATUS_XFER;

		schedule_disk_interrupt(board, cycles);
		break;
	case 0x4032:
		if (!_diskio_enabled)
			break;

		/* Upper 5 bits are open-bus */
		value = 0x40 | (_drive_status_reg ^ 0x03);
		/* invert the sense of the first two bits (
		   and disk not ready) so that we can treat them as
		   'disk inserted' and 'disk ready' internally */
		if (board->emu->config->fds_auto_disk_change_enabled && cpu_get_pc(emu->cpu) < 0xe000) {
			switch(_auto_eject_state) {
			case AUTO_EJECT_INSERTED:
				value &= 0xfe; /* inserted */
				break;
			case AUTO_EJECT_EJECTED:
				value |= 0x05; /* ejected */
				break;
			case AUTO_EJECT_WAITING:
				_flip_disk_counter = ~0;
				_auto_eject_state = 0;
				_auto_eject_counter = 9;
				break;
			case AUTO_EJECT_DISABLED:
				break;
			}
		}
		break;
	case 0x4033:
		value = _output_reg & FDS_DRIVE_BATTERY;
		break;
	case 0xe445:
		if (board->emu->config->fds_auto_disk_change_enabled &&
		    cpu_is_opcode_fetch(board->emu->cpu)) {
			fds_auto_disk_select(board);
		}
		break;
	}

	return value;
}

static int fds_input_handler(void *data, uint32_t event, uint32_t value)
{
	struct emu *emu = data;
	struct board *board = emu->board;
	int side;

	if (!event)
		return 1;

	switch (value) {
	case ACTION_FDS_EJECT:
		_flip_disk_counter = ~0;
		if (fds_disk_set_inserted(board, -1))
			osdprintf("Disk Inserted\n");
		else
			osdprintf("Disk Ejected\n");
		break;
	case ACTION_FDS_FLIP:
		fds_disk_set_inserted(board, 0);
		_flip_disk_counter = 200;
		side = fds_disk_set_side(board, -1);
		osdprintf("Disk %d, side %c selected\n",
			  side / 2 + 1, side % 2 ? 'B' : 'A');
		break;
	case ACTION_FDS_SELECT:
		_flip_disk_counter = ~0;
		side = fds_disk_set_side(board, -1);
		osdprintf("Disk %d, side %c selected\n",
			  side / 2 + 1, side % 2 ? 'B' : 'A');
		break;
	}

	return 1;
}

static int fds_disk_set_inserted(struct board *board, int state)
{
	if (state < 0) {
		if (_drive_status_reg & FDS_DISK_INSERTED)
			state = 0;
		else
			state = 1;
	}

	if (!state) {
		_drive_status_reg = FDS_DISK_PROTECTED;
		_auto_eject_state = AUTO_EJECT_DISABLED;
	} else {
		_drive_status_reg = FDS_DISK_INSERTED;
		_auto_eject_state = AUTO_EJECT_WAITING;
	}

	return state;
}

static int fds_disk_set_side(struct board *board, int side)
{
	size_t side_size;
	int max_side;

	if (_drive_status_reg & FDS_DISK_INSERTED)
		return -1;

	side_size = board->emu->rom->disk_side_size;

	max_side = board->prg_rom.size / side_size;

	if (side < 0)
		side = (_disk_offset / side_size) + 1;

	side %= max_side;

	_disk_offset = side * side_size;

	return side;
}

static int fds_load_state(struct board *board, struct save_state *state)
{
	return fds_audio_load_state(board->emu, state);
}

static int fds_save_state(struct board *board, struct save_state *state)
{
	return fds_audio_save_state(board->emu, state);
}

static void update_crc(struct board *board, int data)
{
	if (_previous_crc)
		return;

	/* printf("adding %02x to _crc at _offset: %d\n", data, _offset); */
	if (!_previous_crc) {
		_crc  = (_crc >> 8) ^ fds_crc_table[_crc & 0xff];
		_crc ^= (data << 8);
	}

	/* for (i = 0x01; i <= 0x80; i <<= 1) { */
	/* 	if (data & i) */
	/* 		_crc |= 0x10000; */
	/* 	if (_crc & 1) */
	/* 		_crc ^= 0x10810; */
	/* 	_crc >>= 1; */
	/* 	_crc &= 0xffff; */
	/* } */

	/* printf("_crc is now %04x\n", _crc); */
}

static int fds_read_byte(struct board *board, int do_high_level, uint32_t cycles)
{
	uint8_t tmp;

	if (_offset >= board->emu->rom->disk_side_size)
		return -1;

	tmp = board->prg_rom.data[_disk_offset + _offset];
	/* printf("read %x from _offset %d\n", tmp, _disk_offset + _offset); */
	/* printf("_gap_covered = %d, %x\n", _gap_covered, _control_reg & FDS_CTRL_XFER); */

	if (_control_reg & FDS_CTRL_XFER) {
		if (!_gap_covered) {
			if (do_high_level) {
				int bytes = find_start_of_next_block(board);
				if (bytes >= 0) {
					_offset += bytes + 1; /* FIXME */
					_crc = 0;
					_previous_crc = (_control_reg & FDS_CTRL_CRC);
					update_crc(board, 0x80);

					tmp = board->prg_rom.data[_disk_offset + _offset];
					_gap_covered = 1;
				}
			} else if (tmp) {
				_gap_covered = 1;
				_crc = 0;
			}
		}

		if (_gap_covered) {
			_read_buffer = tmp;
			_status_reg |= FDS_STATUS_XFER;
			update_crc(board, _read_buffer);
		}


		/* if (_control_reg & FDS_CTRL_CRC) { */
		/* 	if (!_previous_crc) { */
		/* 		printf("(post) calculated _crc: %04x\n", _crc); */
		/* 	} */
		/* } */
	}

	_previous_crc = (_control_reg & FDS_CTRL_CRC);

	_offset++;
	if (_offset >= board->emu->rom->disk_side_size)
		fds_set_eof(board);

	return 0;
}

static int fds_write_byte(struct board *board)
{
	if (_offset >= board->emu->rom->disk_side_size)
		return -1;

	if (!(_control_reg & FDS_CTRL_XFER))
		_write_buffer = 0;

	if (_control_reg & FDS_CTRL_CRC) {
		if (!_previous_crc) {
			update_crc(board, 0);
			update_crc(board, 0);
			/* printf("final _crc is now %x\n", _crc); */
		}

		_write_buffer = _crc & 0xff;
		_crc >>= 8;
	} else {
		update_crc(board, _write_buffer);
		/* printf("write: _crc is now %04x\n", _crc); */
	}

	/* printf("writing byte at _offset %x; old=%x, new=%x\n", */
	/*        _offset, board->prg_rom.data[_offset], _write_buffer); */
	board->prg_rom.data[_disk_offset + _offset] =
		_write_buffer;
	_dirty_flag = 1;
	add_range(&board->modified_ranges, _disk_offset + _offset +
	          board->emu->rom->offset, 1);
	_read_buffer = board->prg_rom.data[_disk_offset + _offset];

	_previous_crc = (_control_reg & FDS_CTRL_CRC);

	_offset++;
	if (_offset >= board->emu->rom->disk_side_size)
		fds_set_eof(board);

	return 0;
}

static uint8_t fds_bios_load_cpu_data(struct emu *emu, uint8_t opcode, uint32_t cycles)
{
	int dest_addr;
	int count;
	int dummy;
	int i;
	struct board *board;

	board = emu->board;

	dest_addr  = cpu_peek(emu->cpu, 0x000a);
	dest_addr |= cpu_peek(emu->cpu, 0x000b) << 8;
	count  = cpu_peek(emu->cpu, 0x000c);
	count |= cpu_peek(emu->cpu, 0x000d) << 8;
	count++; /* BIOS already decremented this once */
	dummy = cpu_peek(emu->cpu, 0x0009);

	/* Fall back to native code for loads that would touch $2000 -
	   $5FFF
	*/
	if (!dummy && (dest_addr >= 0x2000) && (dest_addr < 0x6000)) {
		return opcode;
	}

	if (!dummy && (dest_addr < 0x2000) && (dest_addr + count > 0x2000)) {
		return opcode;
	}

	for (i = 0; i < count; i++) {
		if (fds_read_byte(board, 1, cycles) < 0)
			break;

		if (!dummy)
			cpu_poke(emu->cpu, dest_addr + i, _read_buffer);
	}

	if (_next_clock != ~0) {
		_next_clock = cycles + FDS_BYTE_READ_CYCLES * board->emu->cpu_clock_divider;
		schedule_disk_interrupt(board, cycles);
	}

	if (board->emu->config->fds_auto_disk_change_enabled && (_auto_eject_state != AUTO_EJECT_DISABLED)) {
		_auto_eject_state = AUTO_EJECT_WAITING;
	}

	cpu_set_pc(emu->cpu, 0xe572);
	return cpu_peek(emu->cpu, 0xe572);
}

static uint8_t fds_bios_load_ppu_data(struct emu *emu, uint8_t opcode, uint32_t cycles)
{
	int dest_addr;
	int count;
	int dummy;
	int i;
	struct board *board;

	board = emu->board;

	dest_addr  = cpu_peek(emu->cpu, 0x000a);
	dest_addr |= cpu_peek(emu->cpu, 0x000b) << 8;
	count  = cpu_peek(emu->cpu, 0x000c);
	count |= cpu_peek(emu->cpu, 0x000d) << 8;
	count++; /* BIOS already decremented this once */
	dummy = cpu_peek(emu->cpu, 0x0009);

	dest_addr &= 0x3fff;

	/* Fall back to native code for loads that would touch $3F00 -
	   $3FFF.
	*/
	if (!dummy && (dest_addr >= 0x3f00)) 
		return opcode;

	if (!dummy && (dest_addr < 0x3f00) && (dest_addr + count > 0x3f00))
		return opcode;

	for (i = 0; i < count; i++) {
		if (fds_read_byte(board, 1, cycles) < 0)
			break;

		if (!dummy)
			ppu_poke(emu->ppu, dest_addr + i, _read_buffer);
	}

	if (_next_clock != ~0) {
		_next_clock = cycles + FDS_BYTE_READ_CYCLES * board->emu->cpu_clock_divider;
		schedule_disk_interrupt(board, cycles);
	}

	if (board->emu->config->fds_auto_disk_change_enabled && (_auto_eject_state != AUTO_EJECT_DISABLED))
		_auto_eject_state = AUTO_EJECT_WAITING;

	cpu_set_pc(emu->cpu, 0xe572);
	return cpu_peek(emu->cpu, 0xe572);
}


static CPU_READ_HANDLER(fds_bios_read_write_byte)
{
	struct board *board;
	uint8_t data;
	int is_first = 0;

	board = emu->board;

	fds_run(board, cycles);

	if (!_diskio_enabled || !_bios_patch_enabled ||
	    !cpu_is_opcode_fetch(emu->cpu)) {
		return value;
	}

	switch (addr) {
	case 0xe533:
		value = fds_bios_load_cpu_data(emu, value, cycles);
		if (value != 0x20)
			return value;
		break;
	case 0xe563:
		value = fds_bios_load_ppu_data(emu, value, cycles);
		if (value != 0x20)
			return value;
		break;
	case 0xe4fb:
	case 0xe6a3:
	case 0xe6cc:
		/* Xfer1stByte */
		is_first = 1;
		cpu_poke(emu->cpu, 0x0101, 0x40);
		data = cpu_peek(emu->cpu, 0xfa) | 0x80;
		cpu_poke(emu->cpu, 0xfa, data);
		fds_write_handler(emu, 0x4025, data, cycles);
		break;
	}

	if (_drive_status_reg & FDS_DISK_INSERTED) {
		if (_control_reg & FDS_CTRL_READ) {
			fds_read_byte(board, 1, cycles);
			cpu_set_x_register(emu->cpu, _read_buffer);
			cpu_set_accumulator(emu->cpu, _read_buffer);
		} else {
			fds_write_byte(board);
			_write_buffer = cpu_get_accumulator(emu->cpu);
		}

		if (board->emu->config->fds_auto_disk_change_enabled && (_auto_eject_state != AUTO_EJECT_DISABLED))
			_auto_eject_state = AUTO_EJECT_WAITING;

		if (_next_clock != ~0) {
			calc_next_clock(board, is_first, cycles);
			schedule_disk_interrupt(board, cycles);
		}
	}

	value = 0x0c; /* Undocumented NOP (Absolute) */

	return value;
}

static CPU_READ_HANDLER(fds_bios_misc)
{
	struct board *board;
	int i;

	board = emu->board;

	if (!_bios_patch_enabled || !cpu_is_opcode_fetch(emu->cpu))
		return value;

	fds_run(board, cycles);

	switch (addr) {
	case 0xe53c:
		/* Hack for games that circumvent the license
		   check.

		   If a byte was just written to $2000, jump
		   to an infinite loop and wait for NMI.
		*/
		if (((cpu_peek(emu->cpu, 0x000b) & 0xe0) == 0x20) &&
		    ((cpu_peek(emu->cpu, 0x000a) & 0x07) == 0x00)) {
			cpu_set_pc(emu->cpu, 0xe7a4);
			cpu_interrupt_cancel(emu->cpu, IRQ_DISK);
			value = cpu_peek(emu->cpu, 0xe7a4);
		}
		break;
	case 0xefaf:
		/* Skip display of copyright screen */
		if (emu->config->fds_hide_license_screen) {
			cpu_set_pc(emu->cpu, 0xefcd);
			value = cpu_peek(emu->cpu, 0xefcd);
		}
		break;
	case 0xef44:
		/* Disable the initial title screen check for disk
		   presence (will automatically insert disk at reset
		   (hard or soft) and select disk 1, side A).
		*/
		if (emu->config->fds_hide_bios_title_screen) {
			cpu_set_pc(emu->cpu, 0xef46);
			value = cpu_peek(emu->cpu, 0xef46);
		}
		break;
	case 0xe682:
		/* Assume that the disk is always ready if it is
		 * inserted */
		value = 0x0c; /* Undocumented NOP (Absolute) */
		break;
	case 0xe478:
		/* Instantly skip 30 bytes when reading the
		   disk info block. Very minor performance hack.
		*/
		for (i = 0; i < 30; i++)
			fds_read_byte(board, 1, cycles);
		cpu_set_pc(emu->cpu, 0xe480);
		value = cpu_peek(emu->cpu, 0xe480);
		if (_next_clock != ~0) {
			_next_clock = cycles + FDS_BYTE_READ_CYCLES *
				emu->cpu_clock_divider;
			schedule_disk_interrupt(board, cycles);
		}
		break;
	case 0xe4e5:
		/* Instantly skip 10 bytes when reading the
		   file header for each file in SkipFiles.
		   Very minor performance hack.
		*/
		for (i = 0; i < 10; i++)
			fds_read_byte(board, 1, cycles);
		cpu_set_pc(emu->cpu, 0xe4ed);
		value = cpu_peek(emu->cpu, 0xe4ed);
		if (_next_clock != ~0) {
			_next_clock = cycles + FDS_BYTE_READ_CYCLES *
				emu->cpu_clock_divider;
			schedule_disk_interrupt(board, cycles);
		}
		break;
	case 0xe4a6:
		/* Instantly skip 8 bytes in FileMatchTest.
		   This is just a minor performance hack to
		   trim a few cycles from each file header
		   block comparison.  The native code works
		   correctly without this hack.
		*/
		for (i = 0; i < 8; i++)
			fds_read_byte(board, 1, cycles);

		if (_next_clock != ~0) {
			_next_clock = cycles + (8 * FDS_BYTE_READ_CYCLES) *
				emu->cpu_clock_divider;
			schedule_disk_interrupt(board, cycles);
		}

		/* Set the loop counter to 0 */
		cpu_poke(emu->cpu, 0x0101, 0x00);
		cpu_set_pc(emu->cpu, 0xe4ac);
		value = cpu_peek(emu->cpu, 0xe4ac);
		break;
	case 0xe29a:
		/* Skip file verification */
		/* This is also a performance hack; file
		   verification works just fine in high-level I/O
		   mode, but it is unnecessary except perhaps for
		   debugging the handling of writes.  Skipping it
		   saves a bit of time during saves.
		*/
		cpu_set_pc(emu->cpu, 0xe2a7);
		value = cpu_peek(emu->cpu, 0xe2a7);
		break;
	case 0xe652:
	case 0xe655:
	case 0xe65d:
	case 0xe691:
	case 0xe6e8:
	case 0xe6ed:
		/* These are all calls to MilSecTimer. No point
		   to them when using high-level disk I/O.  There
		   are other calls to MilSecTimer, but they have
		   to do with waiting for the gap to be written
		   to disk; we don't want to change the gap length,
		   so they're left alone.  These calls are just
		   present to compensate for start-up delays of
		   the actual hardware.
		*/
		value = 0x0c; /* Undocumented NOP (Absolute) */
		break;
	}

	return value;
}
