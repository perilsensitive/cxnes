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

#include "a12_timer.h"

#define DO_INLINES 1

#define PPU_RAM_SIZE 0x4000
#define PPU_RAM_MAX 0x3fff
#define PPU_PALETTE_OFFSET 0x3f00
#define PPU_PALETTE_MAX 0x3f1f
#define PPU_NAMETABLE_OFFSET 0x2000
#define PPU_NAMETABLE_MAX 0x2fff
#define PPU_ATTRTABLE_OFFSET 0x23c0
#define PPU_PAGE_SHIFT 10
#define PPU_PAGE_MASK 0x3ff
#define PAGEMAP_ENTRIES 16
#define PAGE_SIZE 1024

#define DECAY_COUNTER_START_NTSC 36
#define DECAY_COUNTER_START_PAL 30

#define OAM_SIZE 256
#define SECONDARY_OAM_SIZE 32
#define PALETTE_SIZE 32
#define NAME_TABLE_SIZE 1024
#define NAMETABLE_INDEX_MASK 0xfff
#define SPRITE_PALETTE_OFFSET 0x10

#define PIXEL_BG_PRIORITY 0x80
#define PIXEL_SPRITE_ZERO 0x40

/* States for sprite evaluation state machine */
#define SPRITE_EVAL_STOPPED 0
#define SPRITE_EVAL_SCANLINE_CHECK 1
#define SPRITE_EVAL_COPY_INDEX 2
#define SPRITE_EVAL_COPY_ATTRIBUTES 3
#define SPRITE_EVAL_COPY_X_OFFSET 4
#define SPRITE_EVAL_LOOP 5
#define SPRITE_EVAL_CLEAR 6

/* Bits for control register ($2000) */
#define CTRL_REG_NAMETABLE     (3 << 0)
#define CTRL_REG_ADDR_INCR     (1 << 2)
#define CTRL_REG_SPRITE_TABLE  (1 << 3)
#define CTRL_REG_BG_TABLE      (1 << 4)
#define CTRL_REG_SPRITE_HEIGHT (1 << 5)
#define CTRL_REG_NMI_ENABLE    (1 << 7)

/* Bits for mask register ($2001) */
#define MASK_REG_GRAYSCALE       (1 << 0)
#define MASK_REG_BG_NO_CLIP      (1 << 1)
#define MASK_REG_SPRITE_NO_CLIP  (1 << 2)
#define MASK_REG_BG_ENABLED      (1 << 3)
#define MASK_REG_SPRITES_ENABLED (1 << 4)
#define MASK_REG_EMPHASIS_RED    (1 << 5)
#define MASK_REG_EMPHASIS_GREEN  (1 << 6)
#define MASK_REG_EMPHASIS_BLUE   (1 << 7)

#define SPRITE_HEIGHT() (ppu->ctrl_reg & CTRL_REG_SPRITE_HEIGHT ? 16 : 8)
#define RENDERING_ENABLED() (ppu->mask_reg & (MASK_REG_SPRITES_ENABLED|MASK_REG_BG_ENABLED))

/* Bits for status register ($2002) */
#define STATUS_REG_SPRITE_OVERFLOW (1 << 5)
#define STATUS_REG_SPRITE_ZERO     (1 << 6)
#define STATUS_REG_VBLANK_FLAG     (1 << 7)

/* Scroll address mask */
#define SCROLL_X                       0x001f
#define SCROLL_Y                       0x03e0
#define SCROLL_Y_FINE                  0x7000
#define SCROLL_NAMETABLE_LOW           0x0400
#define SCROLL_NAMETABLE_HIGH          0x0800
#define SCROLL_NAMETABLE               (SCROLL_NAMETABLE_HIGH|\
					SCROLL_NAMETABLE_LOW)
#define SCROLL_VERTICAL (SCROLL_NAMETABLE_HIGH|SCROLL_Y|\
			 SCROLL_Y_FINE)
#define SCROLL_HORIZONTAL (SCROLL_NAMETABLE_LOW|SCROLL_X)

#define X_FLIP (1 << 6)
#define Y_FLIP (1 << 7)

#define EXRAM_MODE_NAMETABLE 0
#define EXRAM_MODE_EXATTR    1
#define EXRAM_MODE_WRAM      2
#define EXRAM_MODE_WRAM_RO   3
#define EXRAM_MODE_DISABLED  4
#define add_cycle_debug() {			     \
	        printf("add_cycle: %d %d,%d (%d)\n", \
		       ppu->odd_frameppu->scanline,ppu->scanline_cycle, ppu->cycles);	\
		ppu->cycles++;	\
		ppu->scanline_cycle++;	\
}

#define add_cycle_nodebug() {		\
		ppu->cycles++;	\
		ppu->scanline_cycle++;	\
}

#define add_cycle() add_cycle_nodebug()

#define return_if_done() { \
		if (ppu->cycles >= cycles)  {	\
			ppu->catching_up = 0; \
			return 1; \
		} \
}

#define update_address_bus_a12(x) { \
	if (ppu->a12_timer_enabled && ((ppu->address_bus ^ (x)) & 0x1000)) {	\
		a12_timer_hook(ppu->emu, (x) & 0x1000, ppu->scanline, ppu->scanline_cycle, ppu->rendering); \
	}\
	ppu->address_bus = (x); \
}

#define update_address_bus(x) {						\
	ppu->address_bus = (x); \
}

#define start_nametable_fetch(ppu) update_address_bus(get_nametable_entry_addr(ppu))

#define start_attribute_fetch(ppu) update_address_bus(get_attrtable_entry_addr(ppu))

#define start_left_bg_tile_fetch() update_address_bus(ppu->nmt_latch << 4 | \
						      ppu->scroll_address >> 12 | \
						      bg_pattern_table_address())
#define start_right_bg_tile_fetch() update_address_bus((ppu->nmt_latch << 4 | \
							ppu->scroll_address >> 12 | \
							bg_pattern_table_address()) + 8)

#define finish_left_spr_tile_fetch() (ppu->left_tile_latch = read_spr(ppu, ppu->address_bus))
#define finish_right_spr_tile_fetch() (ppu->right_tile_latch = read_spr(ppu, ppu->address_bus))

#if DO_INLINES
#define INLINE inline
#define ALWAYS_INLINE __attribute((always_inline))
#else
#define INLINE
#define ALWAYS_INLINE
#endif

static const uint8_t rp2c04_0001_lookup[64] = {
	53, 35, 22, 34, 28,  9, 29, 21, 32,  0, 39,  5,  4, 40,  8, 32,
	33, 62, 31, 41, 60, 50, 54, 18, 63, 59, 46, 30, 61, 45, 36,  1,
	14, 49, 51, 42, 44, 12, 27, 20, 46,  7, 52,  6, 19,  2, 38, 15,
	15, 25, 16, 10, 57,  3, 55, 23, 47, 17, 26, 13, 56, 37, 24, 58,
};

static const uint8_t rp2c04_0002_lookup[64] = {
	46, 39, 24, 57, 58, 37, 28, 49, 22, 19, 56, 52, 32, 35, 60, 26,
	47, 33,  6, 61, 27, 41, 30, 34, 29, 36, 14, 59, 50,  8, 46,  3,
	4, 54, 38, 51, 17, 31, 16,  2, 20, 63,  0,  9, 18, 15, 40, 32,
	62, 13, 42, 23, 12,  1, 21, 25, 15, 44,  7, 55, 53,  5, 10, 45,
};

static const uint8_t rp2c04_0003_lookup[64] = {
	20, 37, 58, 16, 26, 32, 49,  9,  1, 46, 54,  8, 21, 61, 62, 60,
	34, 28,  5, 18, 25, 24, 23, 27,  0,  3, 46,  2, 22,  6, 52, 53,
	35, 47, 14, 55, 13, 39, 38, 32, 41,  4, 33, 36, 17, 45, 15, 31,
	44, 30, 57, 51,  7, 42, 40, 29, 10, 15, 50, 56, 19, 59, 63, 12,
};

static const uint8_t rp2c04_0004_lookup[64] = {
	24,  3, 28, 40, 46, 53,  1, 23, 16, 31, 42, 14, 54, 55, 26, 57,
	37, 30, 18, 52, 46, 29,  6, 38, 62, 27, 34, 25,  4, 15, 58, 33,
	5, 10,  7,  2, 19, 20,  0, 21, 12, 61, 17, 47, 13, 56, 45, 36,
	51, 32,  8, 22, 63, 59, 32, 60, 15, 39, 35, 49, 41, 50, 44,  9,
};

static const uint8_t rp2c04_yuv_lookup[64] = {
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 46, 12, 45, 1, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 8, 7,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 47, 44, 45, 46, 9,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 54, 55,
};

static const uint8_t xflip_lookup[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

struct ppu_state {
	int overclocking;
	int overclock_start_timestamp;
	int overclock_end_timestamp;
	int overclock_mode;
	int overclock_scanlines;
	int cycles;
	int frame_cycles;
	int visible_cycles;
	int vblank_timestamp;
	int ppu_clock_divider;
	int scanline;
	unsigned int scanline_cycle;

	/* holds the bg tiles as they're being rendered
	   (one pixel per byte) */
	uint8_t bg_pixels[16];
	uint8_t left_tile_latch;
	uint8_t right_tile_latch;
	uint8_t attribute_latch;

	/* Instead of using shift registers, latches, and counters
	   for sprite data, just load it here. Lower 4 bits
	   are palette index (incl. attribute data).  Upper
	   4 bits are 0 if sprite in front of BG, 0xff if
	   in back of bg.  Empty pixels are 0x0.
	 */
	uint8_t sprite_tile_buffer[256];

	/* PPU registers (CPU 0x2000 - 0x2007) */
	uint8_t ctrl_reg;
	uint8_t mask_reg;
	uint8_t status_reg;
	uint8_t oam_addr_reg;

	/* a.k.a. loopy_v */
	uint16_t scroll_address;
	uint16_t scroll_address_latch;
	int scroll_address_toggle;
	int sprite_zero_present;
	int odd_frame;
	int suppress_vblank_flag;

	/* holds current value of address bus */
	uint16_t address_bus;
	uint8_t nmt_latch;

	uint8_t fine_x_scroll;

	/* Sprite (object) RAM */
	uint8_t oam[OAM_SIZE + 8];
	int oam_address;
	int oam_address_latch;
	int scanline_sprite_count;
	int scanline_extended_sprite_count;
	uint8_t oam_latch;

	/* Sprite data for current scanline */
	uint8_t secondary_oam[OAM_SIZE + 8];
	unsigned int secondary_oam_index;
	int sprite_eval_state;

	uint8_t palette[PALETTE_SIZE * 2];
	uint8_t *translated_palette;
	int emphasis;

	uint8_t io_latch;
	uint8_t io_latch_decay;
	uint8_t io_buffer;

	uint8_t bg_mask;
	uint8_t sprite_mask;
	uint8_t bg_left8_mask;
	uint8_t sprite_left8_mask;
	uint8_t bg_all_mask;
	uint8_t sprite_all_mask;
	uint8_t palette_mask;
	int rendering;

	int no_sprite_limit;
	int allow_sprite_hiding;
	int hide_sprites;
	int hide_bg;

	uint32_t *buf;
	uint32_t *nes_pixel_buf;

	/* Set when ppu_run() is called and cleared when it's done
	   to make sure we don't nest ppu_run() calls.
	 */
	int catching_up;

	int exram_mode;
	int split_screen_start;
	int split_screen_end;
	int split_screen_scroll;
	int split_screen_enabled;
	int split_screen_bank;
	int first_frame_flag;

	int ppu_type;
	int reset_connected;
	unsigned int vblank_scanlines;
	unsigned int post_render_scanlines;

	int decay_counter_start;
	int use_scanline_renderer;

	uint8_t *read_pagemap0[PAGEMAP_ENTRIES];
	uint8_t *write_pagemap0[PAGEMAP_ENTRIES];
	uint8_t *read_pagemap1[PAGEMAP_ENTRIES];
	uint8_t *write_pagemap1[PAGEMAP_ENTRIES];

	uint8_t **read_bg_pagemap;
	uint8_t **write_bg_pagemap;
	uint8_t **read_spr_pagemap;
	uint8_t **write_spr_pagemap;

	uint8_t *exram;
	uint8_t *chr_rom;
	size_t chr_rom_size;

	int burst_phase;

/* Some VS. Unisystem PPUs return a magic word in the low 5-6
   bits of $2002.  If the wrong word is returned, the game
   won't boot.  If this value is non-zero, then it is ORed
   with the status we would normally return.  Note that Mighty
   Bomb Jack expects 0x3d, which means that it always looks
   like the sprite overflow bit is set.
*/
	int vs_ppu_magic;
	struct emu *emu;
	int do_palette_lookup;
	uint8_t palette_lookup[64];

	int a12_timer_enabled;

	int wrote_2006;
	int sprite_overflow_flag;
};

static struct state_item ppu_state_items[] = {
	STATE_32BIT(ppu_state, cycles),
	STATE_32BIT(ppu_state, frame_cycles),
	STATE_32BIT(ppu_state, vblank_timestamp),
	STATE_16BIT(ppu_state, scanline),
	STATE_16BIT(ppu_state, scanline_cycle),
	STATE_8BIT(ppu_state, left_tile_latch),
	STATE_8BIT(ppu_state, right_tile_latch),
	STATE_8BIT(ppu_state, attribute_latch),
	STATE_8BIT(ppu_state, ctrl_reg),
	STATE_8BIT(ppu_state, mask_reg),
	STATE_8BIT(ppu_state, status_reg),
	STATE_8BIT(ppu_state, oam_addr_reg),
	STATE_16BIT(ppu_state, scroll_address),
	STATE_16BIT(ppu_state, scroll_address_latch),
	STATE_8BIT(ppu_state, scroll_address_toggle), /* BOOLEAN */
	STATE_8BIT(ppu_state, sprite_zero_present), /* BOOLEAN */
	STATE_8BIT(ppu_state, odd_frame), /* BOOLEAN */
	STATE_8BIT(ppu_state, suppress_vblank_flag), /* BOOLEAN */

	STATE_16BIT(ppu_state, address_bus),
	STATE_8BIT(ppu_state, fine_x_scroll),

	STATE_16BIT(ppu_state, oam_address),
	STATE_16BIT(ppu_state, oam_address_latch),
	STATE_8BIT(ppu_state, scanline_sprite_count),
	STATE_8BIT(ppu_state, scanline_extended_sprite_count),
	STATE_8BIT(ppu_state, oam_latch),

	STATE_8BIT(ppu_state, sprite_eval_state),

	STATE_8BIT_ARRAY(ppu_state, palette),

	STATE_8BIT(ppu_state, io_latch),
	STATE_8BIT(ppu_state, io_latch_decay),
	STATE_8BIT(ppu_state, io_buffer),

	STATE_8BIT(ppu_state, catching_up), /* BOOLEAN */
	STATE_8BIT(ppu_state, first_frame_flag), /* BOOLEAN */

	/* MMC5 stuff (check types) */
	STATE_32BIT(ppu_state, exram_mode),
	STATE_32BIT(ppu_state, split_screen_start),
	STATE_32BIT(ppu_state, split_screen_end),
	STATE_32BIT(ppu_state, split_screen_scroll),
	STATE_32BIT(ppu_state, split_screen_enabled),
	STATE_32BIT(ppu_state, split_screen_bank),

	STATE_8BIT(ppu_state, burst_phase),

	STATE_16BIT(ppu_state, wrote_2006),
	
	STATE_ITEM_END(),
};

static uint8_t power_up_palette[] = {
	0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D,
	0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
	0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14,
	0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08,
};

/* static void print_state(void) */
/* { */
/* 	printf("ppu state\n"); */
/* 	printf("---------\n"); */
/* 	printf("cycles: %d\n", ppu->cycles); */
/* 	printf("scanline: %d\n", ppu->scanline); */
/* 	printf("scanline_cycle: %d\n", ppu->scanline_cycle); */
/* 	printf("scroll_address: %x\n", ppu->scroll_address); */
/* 	printf("\n"); */
/* } */

//#define read_mem(x) (read_pagemap[(x) >> PPU_PAGE_SHIFT][(x) & PPU_PAGE_MASK])
static void (*ppu_read_hook) (struct board *, int);

void ppu_set_read_hook(void (*hook) (struct board *, int))
{
	ppu_read_hook = hook;
}

void ppu_enable_a12_timer(struct ppu_state *ppu, int enabled)
{
	ppu->a12_timer_enabled = enabled;
}

static INLINE void sprite_read(struct ppu_state *ppu) ALWAYS_INLINE;
static INLINE void sprite_read(struct ppu_state *ppu)
{
	ppu->status_reg |= ppu->sprite_overflow_flag;
	ppu->oam_latch = ppu->oam[ppu->oam_address & 0xff];
}

static INLINE uint8_t read_nmt(struct ppu_state *ppu,
			       uint16_t addr) ALWAYS_INLINE;
static INLINE uint8_t read_nmt(struct ppu_state *ppu, uint16_t addr)
{
	uint8_t data;
	int page;
	int offset;

	page = (addr >> PPU_PAGE_SHIFT);
	offset = addr & PPU_PAGE_MASK;

	/* Open bus */
	//data = ppu->io_latch; /* FIXME is this right? */
	data = 0xff;

	if (ppu->read_bg_pagemap[page])
		data = ppu->read_bg_pagemap[page][offset];

	if (ppu_read_hook)
		ppu_read_hook(ppu->emu->board, addr);

	return data;
}

static INLINE uint8_t read_bg(struct ppu_state *ppu,
			      uint16_t addr) ALWAYS_INLINE;
static INLINE uint8_t read_bg(struct ppu_state *ppu, uint16_t addr)
{
	uint8_t data;
	int page;
	int offset;

	page = addr >> PPU_PAGE_SHIFT;
	offset = addr & PPU_PAGE_MASK;

	/* Open bus */
	//data = ppu->io_latch; /* FIXME is this right? */
	data = 0xff;

	if (ppu->read_bg_pagemap[page])
		data = ppu->read_bg_pagemap[page][offset];

	if (ppu_read_hook)
		ppu_read_hook(ppu->emu->board, addr);

	return data;
}

static INLINE uint8_t read_spr(struct ppu_state *ppu,
			       uint16_t addr) ALWAYS_INLINE;
static INLINE uint8_t read_spr(struct ppu_state *ppu, uint16_t addr)
{
	uint8_t data;
	int page;
	int offset;

	page = addr >> PPU_PAGE_SHIFT;
	offset = addr & PPU_PAGE_MASK;

	/* Open bus */
	//data = ppu->io_latch; /* FIXME is this right? */
	data = 0xff;

	if (ppu->read_spr_pagemap[page])
		data = ppu->read_spr_pagemap[page][offset];

	if (ppu_read_hook)
		ppu_read_hook(ppu->emu->board, addr);

	return data;
}

CPU_WRITE_HANDLER(ppu_ctrl_reg_write_handler);
CPU_WRITE_HANDLER(ppu_mask_reg_write_handler);
CPU_WRITE_HANDLER(ppu_oam_addr_reg_write_handler);
CPU_WRITE_HANDLER(ppu_oam_data_reg_write_handler);
static CPU_WRITE_HANDLER(write_scroll_reg);
static CPU_WRITE_HANDLER(write_address_reg);
static CPU_WRITE_HANDLER(write_data_reg);
static CPU_WRITE_HANDLER(write_2xxx);
static CPU_READ_HANDLER(read_status_reg);
static CPU_READ_HANDLER(read_oam_data_reg);
static CPU_READ_HANDLER(read_data_reg);
static CPU_READ_HANDLER(read_2xxx);
int ppu_run(struct ppu_state *ppu, int cycles);
void ppu_reset(struct ppu_state *, int hard);
uint8_t *ppu_get_oam_ptr(struct ppu_state *ppu);

uint8_t *ppu_get_oam_ptr(struct ppu_state *ppu)
{
	return ppu->oam;
}

void ppu_set_reset_connected(struct ppu_state *ppu, int connected)
{
	ppu->reset_connected = connected;
}

int ppu_get_reset_connected(struct ppu_state *ppu)
{
	return ppu->reset_connected;
}

void ppu_use_exram(struct ppu_state *ppu, int mode, uint32_t cycles)
{
	ppu_run(ppu, cycles);
	ppu->exram_mode = mode;
}

void ppu_map_nametable(struct ppu_state *ppu, int nametable, uint8_t * data,
		       int rw, uint32_t cycles)
{
	if (nametable < 0 || nametable > 3)
		return;

	ppu_run(ppu, cycles);

	if (rw & 1) {
		ppu->read_pagemap0[8 + nametable] = data;
		ppu->read_pagemap1[8 + nametable] = data;
		ppu->read_pagemap0[8 + nametable + 4] = data;
		ppu->read_pagemap1[8 + nametable + 4] = data;
	}

	if (rw & 2) {
		ppu->write_pagemap0[8 + nametable] = data;
		ppu->write_pagemap1[8 + nametable] = data;
		ppu->write_pagemap0[8 + nametable + 4] = data;
		ppu->write_pagemap1[8 + nametable + 4] = data;
	}
}

void ppu_select_bg_pagemap(struct ppu_state *ppu, int map, uint32_t cycles)
{
	if (map < 0 || map > 1)
		return;

	ppu_run(ppu, cycles);

	if (map) {
		ppu->read_bg_pagemap = ppu->read_pagemap1;
		ppu->write_bg_pagemap = ppu->write_pagemap1;
	} else {
		ppu->read_bg_pagemap = ppu->read_pagemap0;
		ppu->write_bg_pagemap = ppu->write_pagemap0;
	}
}

void ppu_select_spr_pagemap(struct ppu_state *ppu, int map, uint32_t cycles)
{
	if (map < 0 || map > 1)
		return;

	ppu_run(ppu, cycles);

	if (map) {
		ppu->read_spr_pagemap = ppu->read_pagemap1;
		ppu->write_spr_pagemap = ppu->write_pagemap1;
	} else {
		ppu->read_spr_pagemap = ppu->read_pagemap0;
		ppu->write_spr_pagemap = ppu->write_pagemap0;
	}
}

/* FIXME Ick.  This needs to be thought out more. */
void ppu_set_pagemap_entry(struct ppu_state *ppu, int map, int addr,
			   int size, uint8_t * data, int rw, uint32_t cycles)
{
	int i;
	int page;
	uint8_t **r_ptr, **w_ptr;

	addr &= 0x3fff;

	if (addr >= 0x3000)
		addr &= 0x2fff;

	/* FIXME what to do here? */
	if (addr > 0x2c00)
		return;

	if (addr + size > PPU_NAMETABLE_OFFSET + 1)
		return;

	if (map) {
		r_ptr = ppu->read_pagemap1;
		w_ptr = ppu->write_pagemap1;
	} else {
		r_ptr = ppu->read_pagemap0;
		w_ptr = ppu->write_pagemap0;
	}

	if (size < PAGE_SIZE) {
		size = PAGE_SIZE;
		data = NULL;
	}

	for (i = 0; i * PAGE_SIZE < size; i++) {
		uint8_t *old;
		uint8_t *new;

		new = data;
		if (data)
			new += i * PAGE_SIZE;

		page = (addr >> PPU_PAGE_SHIFT) + i;
		if (rw & 1) {
			old = r_ptr[page];
			if (old != new) {
				ppu_run(ppu, cycles);
				r_ptr[page] = new;
			}
		}
		if (rw & 2) {
			old = w_ptr[page];
			if (old != new) {
				ppu_run(ppu, cycles);
				w_ptr[page] = new;
			}
		}
	}
}

int ppu_get_type(struct ppu_state *ppu)
{
	return ppu->ppu_type;
}

void ppu_set_type(struct ppu_state *ppu, int type)
{
	const char *ppu_type_string;
	int addr;

	ppu->ppu_type = type;
	ppu->vblank_scanlines = 20;
	ppu->post_render_scanlines = 1;
	ppu->vs_ppu_magic = 0;
	ppu->decay_counter_start = DECAY_COUNTER_START_NTSC;
	ppu->ppu_clock_divider = 4;

	switch (type) {
	case PPU_TYPE_RP2C03B:
		ppu_type_string = "RP2C03B";
		break;
	case PPU_TYPE_RP2C03G:
		ppu_type_string = "RP2C03G";
		break;
	case PPU_TYPE_RP2C04_0001:
		ppu_type_string = "RP2C04-0001";
		break;
	case PPU_TYPE_RP2C04_0002:
		ppu_type_string = "RP2C04-0002";
		break;
	case PPU_TYPE_RP2C04_0003:
		ppu_type_string = "RP2C04-0003";
		break;
	case PPU_TYPE_RP2C04_0004:
		ppu_type_string = "RP2C04-0004";
		break;
	case PPU_TYPE_RC2C03B:
		ppu_type_string = "RC2C03B";
		break;
	case PPU_TYPE_RC2C03C:
		ppu_type_string = "RC2C03C";
		break;
	case PPU_TYPE_RC2C05_01:
		ppu_type_string = "RC2C05-01";
		ppu->vs_ppu_magic = 0x1b;
		break;
	case PPU_TYPE_RC2C05_02:
		ppu_type_string = "RC2C05-02";
		ppu->vs_ppu_magic = 0x3d;
		break;
	case PPU_TYPE_RC2C05_03:
		ppu_type_string = "RC2C05-03";
		ppu->vs_ppu_magic = 0x1c;
		break;
	case PPU_TYPE_RC2C05_04:
		ppu_type_string = "RC2C05-04";
		ppu->vs_ppu_magic = 0x1b;
		break;
	case PPU_TYPE_RC2C05_05:
		ppu_type_string = "RC2C05-05";
		break;
	case PPU_TYPE_RP2C02:
		ppu_type_string = "RP2C02";
		break;
	case PPU_TYPE_RP2C07:
		ppu_type_string = "RP2C07";
		ppu->vblank_scanlines = 70;
		ppu->ppu_clock_divider = 5;
		ppu->decay_counter_start = DECAY_COUNTER_START_PAL;
		break;
	case PPU_TYPE_DENDY:
		ppu_type_string = "DENDY";
		ppu->post_render_scanlines = 51;
		ppu->ppu_clock_divider = 5;
		break;
	default:
		ppu_type_string = "unknown";
		break;
	}

	ppu->emu->ppu_clock_divider = ppu->ppu_clock_divider;

	log_dbg("PPU: %s (%s timing)\n", ppu_type_string,
		(type == PPU_TYPE_RP2C07 || type == PPU_TYPE_DENDY) ? "PAL" : "NTSC");

	for (addr = 0x2000; addr < 0x4000; addr += 8) {
		cpu_write_handler_t *old_write_handler;

		cpu_set_read_handler(ppu->emu->cpu, addr, 1, 0, read_2xxx);

		cpu_set_read_handler(ppu->emu->cpu, addr + 1, 1, 0, read_2xxx);

		if (!ppu->vs_ppu_magic) {
			old_write_handler = cpu_get_write_handler(ppu->emu->cpu, addr);
			if (!old_write_handler ||
			    (old_write_handler == ppu_mask_reg_write_handler)) {
				cpu_set_write_handler(ppu->emu->cpu, addr, 1, 0,
						      ppu_ctrl_reg_write_handler);
			}

			old_write_handler = cpu_get_write_handler(ppu->emu->cpu, addr + 1);
			if (!old_write_handler ||
			    (old_write_handler == ppu_ctrl_reg_write_handler)) {
				cpu_set_write_handler(ppu->emu->cpu, addr + 1, 1, 0,
						      ppu_mask_reg_write_handler);
			}
		} else {
			cpu_set_write_handler(ppu->emu->cpu, addr, 1, 0,
					      ppu_mask_reg_write_handler);
			cpu_set_write_handler(ppu->emu->cpu, addr + 1, 1, 0,
					      ppu_ctrl_reg_write_handler);
		}
	}

}

int ppu_init(struct emu *emu)
{
	uint16_t addr;
	struct ppu_state *ppu;
	int i;

	ppu = malloc(sizeof(*ppu));
	if (!ppu)
		return 0;
	memset(ppu, 0, sizeof(*ppu));

	emu->ppu = ppu;
	ppu->emu = emu;
	ppu->ppu_type = -1;

	for (i = 0; i < PAGEMAP_ENTRIES; i++) {
		ppu->read_pagemap0[i] = NULL;
		ppu->write_pagemap0[i] = NULL;
		ppu->read_pagemap1[i] = NULL;
		ppu->write_pagemap1[i] = NULL;
	}

	ppu_read_hook = NULL;

	/* Defaults */
	ppu->use_scanline_renderer = 1;
	ppu->reset_connected = 1;
	ppu->no_sprite_limit = 0;
	ppu->allow_sprite_hiding = 0;
	ppu->hide_sprites = 0;
	ppu->hide_bg = 0;

	for (addr = 0x2000; addr < 0x4000; addr += 8) {
		cpu_set_read_handler(emu->cpu, addr + 2, 1, 0, read_status_reg);
		cpu_set_write_handler(emu->cpu, addr + 2, 1, 0, write_2xxx);

		cpu_set_read_handler(emu->cpu, addr + 3, 1, 0, read_2xxx);
		cpu_set_write_handler(emu->cpu, addr + 3, 1, 0,
				      ppu_oam_addr_reg_write_handler);

		cpu_set_read_handler(emu->cpu, addr + 4, 1, 0,
				     read_oam_data_reg);
		cpu_set_write_handler(emu->cpu, addr + 4, 1, 0,
				      ppu_oam_data_reg_write_handler);

		cpu_set_read_handler(emu->cpu, addr + 5, 1, 0, read_2xxx);
		cpu_set_write_handler(emu->cpu, addr + 5, 1, 0,
				      write_scroll_reg);

		cpu_set_read_handler(emu->cpu, addr + 6, 1, 0, read_2xxx);
		cpu_set_write_handler(emu->cpu, addr + 6, 1, 0,
				      write_address_reg);

		cpu_set_read_handler(emu->cpu, addr + 7, 1, 0, read_data_reg);
		cpu_set_write_handler(emu->cpu, addr + 7, 1, 0, write_data_reg);
	}

	for (addr = 0x2000; addr < 0x4000; addr += 8) {
		cpu_set_read_handler(emu->cpu, addr, 1, 0, read_2xxx);

		cpu_set_read_handler(emu->cpu, addr + 1, 1, 0, read_2xxx);

		cpu_set_write_handler(emu->cpu, addr, 1, 0,
				      ppu_ctrl_reg_write_handler);
		cpu_set_write_handler(emu->cpu, addr + 1, 1, 0,
				      ppu_mask_reg_write_handler);
	}

	return 0;
}

void ppu_cleanup(struct ppu_state *ppu)
{
	ppu->emu->ppu = NULL;
	free(ppu);
}

int ppu_apply_config(struct ppu_state *ppu)
{
	const char *sprite_limit_mode;

	sprite_limit_mode = ppu->emu->config->sprite_limit_mode;

	if (strcasecmp(sprite_limit_mode, "disabled") == 0) {
		ppu->no_sprite_limit = 0;
		ppu->allow_sprite_hiding = 0;
	} else if (strcasecmp(sprite_limit_mode, "enabled") == 0) {
		ppu->no_sprite_limit = 1;
		ppu->allow_sprite_hiding = 0;
	} else if (strcasecmp(sprite_limit_mode, "auto") == 0) {
		ppu->no_sprite_limit = 1;
		ppu->allow_sprite_hiding = 1;
	}
	
	ppu->use_scanline_renderer =
		ppu->emu->config->scanline_renderer_enabled;

	ppu->translated_palette = ppu->palette;
	ppu->do_palette_lookup = 0;

	switch (ppu->ppu_type) {
	case PPU_TYPE_RP2C03B:
	case PPU_TYPE_RP2C03G:
	case PPU_TYPE_RC2C03B:
	case PPU_TYPE_RC2C03C:
	case PPU_TYPE_RC2C05_01:
	case PPU_TYPE_RC2C05_02:
	case PPU_TYPE_RC2C05_03:
	case PPU_TYPE_RC2C05_04:
	case PPU_TYPE_RC2C05_05:
		break;
	case PPU_TYPE_RP2C04_0001:
		ppu->translated_palette = ppu->palette + PALETTE_SIZE;
		memcpy(ppu->palette_lookup, rp2c04_0001_lookup,
		       sizeof(ppu->palette_lookup));
		ppu->do_palette_lookup = 1;
		break;
	case PPU_TYPE_RP2C04_0002:
		ppu->translated_palette = ppu->palette + PALETTE_SIZE;
		memcpy(ppu->palette_lookup, rp2c04_0002_lookup,
		       sizeof(ppu->palette_lookup));
		ppu->do_palette_lookup = 1;
		break;
	case PPU_TYPE_RP2C04_0003:
		ppu->translated_palette = ppu->palette + PALETTE_SIZE;
		memcpy(ppu->palette_lookup, rp2c04_0003_lookup,
		       sizeof(ppu->palette_lookup));
		ppu->do_palette_lookup = 1;
		break;
	case PPU_TYPE_RP2C04_0004:
		ppu->translated_palette = ppu->palette + PALETTE_SIZE;
		memcpy(ppu->palette_lookup, rp2c04_0004_lookup,
		       sizeof(ppu->palette_lookup));
		ppu->do_palette_lookup = 1;
		break;
	case PPU_TYPE_RP2C02:
		break;
	case PPU_TYPE_RP2C07:
		break;
	case PPU_TYPE_DENDY:
		break;
	default:
		break;
	}

	if (ppu->do_palette_lookup && !strcasecmp(ppu->emu->config->palette,
					     "yiq")) {
		int i;

		for (i = 0; i < 64; i++) {
			ppu->palette_lookup[i] =
				rp2c04_yuv_lookup[ppu->palette_lookup[i]];
		}
	}


	return 0;
}

void ppu_reset(struct ppu_state *ppu, int hard)
{
	size_t exram_size;

	if (!hard && ppu->a12_timer_enabled) {
		a12_timer_reset_hook(ppu->emu, ppu->cycles);
	}

	ppu->overclock_mode = OVERCLOCK_MODE_NONE;
	ppu->overclocking = 0;

	if (!hard && !ppu->reset_connected) {
		/* If the reset line isn't connected, the PPU just
		   continues running from whatever state it's in.
		   Any pending NMIs will still fire at the expected
		   time due to the CPU core rescheduling all pending
		   interrupts at reset.
		*/
		   
		if (ppu->vblank_timestamp >= ppu->cycles) {
			ppu->vblank_timestamp -= ppu->cycles;
		}

		/* Reset the cycle count to 0 (so that it's in sync with the
		   CPU), and then update the number of cycles to run in this
		   frame.  This frame will be shorter than normal since it
		   should still end at the same time it normally would.
		*/
		ppu->cycles = 0;
		ppu->frame_cycles = (241 + ppu->post_render_scanlines +
				     ppu->vblank_scanlines -
				     (ppu->scanline + 1)) * 341 -
		                    ppu->scanline_cycle;
		ppu->visible_cycles = ppu->frame_cycles -
		                      (ppu->vblank_scanlines) * 341;
		cpu_set_frame_cycles(ppu->emu->cpu,
		                     ppu->visible_cycles * ppu->ppu_clock_divider,
				     ppu->frame_cycles * ppu->ppu_clock_divider);
		return;
	}

	if (hard) {
		memset(ppu->oam, 0xff, sizeof(ppu->oam));
		memcpy(ppu->palette, power_up_palette, sizeof(ppu->palette));
		if (ppu->do_palette_lookup) {
			int i;

			for (i = 0; i < PALETTE_SIZE; i++) {
				ppu->translated_palette[i] =
					ppu->palette_lookup[ppu->palette[i]];
			}
		}

		ppu->io_buffer = 0xff;

		ppu->status_reg = 0;
		ppu->sprite_overflow_flag = 0;
		ppu->oam_addr_reg = 0;
		ppu->address_bus = 0;
		ppu->oam_address = 0;
		ppu->scroll_address = 0;
		ppu->scroll_address_latch = 0;
		ppu->write_bg_pagemap = ppu->write_pagemap0;
		ppu->write_spr_pagemap = ppu->write_pagemap0;
		ppu->read_bg_pagemap = ppu->read_pagemap0;
		ppu->read_spr_pagemap = ppu->read_pagemap0;
		ppu->burst_phase = 0;

	} else {
		ppu->first_frame_flag = 1;
	}

	/*
	ppu->frame_cycles = (241 + ppu->post_render_scanlines +
			     ppu->vblank_scanlines - 1) * 341 - 1;
	ppu->visible_cycles = ppu->frame_cycles -
	                      (ppu->vblank_scanlines) * 341;
			      */
	ppu->frame_cycles = 240 * 341 - 1;
	ppu->visible_cycles = ppu->frame_cycles;
	cpu_set_frame_cycles(ppu->emu->cpu,
	                     ppu->visible_cycles * ppu->ppu_clock_divider,
			     ppu->frame_cycles * ppu->ppu_clock_divider);

	board_get_mapper_ram(ppu->emu->board, &ppu->exram, &exram_size);
	board_get_chr_rom(ppu->emu->board, &ppu->chr_rom, &ppu->chr_rom_size);
	ppu->exram_mode = EXRAM_MODE_DISABLED;

	ppu->fine_x_scroll = 0;
	ppu->mask_reg = 0;
	ppu->ctrl_reg = 0;
	ppu->rendering = 0;
	ppu->cycles = 0;
	ppu->scanline = 0;
	ppu->scanline_cycle = 1;
	ppu->left_tile_latch = 0;
	ppu->right_tile_latch = 0;
	ppu->attribute_latch = 0;
	ppu->sprite_eval_state = SPRITE_EVAL_STOPPED;
	ppu->scanline_sprite_count = 0;
	ppu->scanline_extended_sprite_count = 0;
	ppu->secondary_oam_index = 0;
	ppu->scroll_address_toggle = 0;
	ppu->scroll_address_latch &= 0x3fff;
	ppu->odd_frame = 0;
	ppu->suppress_vblank_flag = 0;
	ppu->bg_mask = 0x00;
	ppu->sprite_mask = 0x00;
	ppu->bg_left8_mask = 0x00;
	ppu->sprite_left8_mask = 0x00;
	ppu->bg_all_mask = 0x00;
	ppu->sprite_all_mask = 0x00;
	ppu->palette_mask = 0xff;
	ppu->io_latch = 0;
	ppu->io_buffer = 0;
	ppu->sprite_zero_present = 0;
	ppu->vblank_timestamp = ((240 + ppu->post_render_scanlines) * 341) *
		ppu->ppu_clock_divider;
	cpu_interrupt_cancel(ppu->emu->cpu, IRQ_NMI);
}

void ppu_begin_frame(struct ppu_state *ppu, uint32_t * buf, uint32_t *nes_pixel_buf)
{
	ppu->buf = buf;
	ppu->nes_pixel_buf = nes_pixel_buf;
}

uint32_t ppu_end_frame(struct ppu_state *ppu, uint32_t cycles)
{
	uint32_t tmp;

	ppu_run(ppu, cycles);

	ppu->cycles -= ppu->frame_cycles;
	if (ppu->ctrl_reg & CTRL_REG_NMI_ENABLE) {
		cpu_interrupt_schedule(ppu->emu->cpu, IRQ_NMI,
				       ppu->vblank_timestamp);
	}
	ppu->vblank_timestamp -= ppu->frame_cycles * ppu->ppu_clock_divider;

	ppu->io_latch_decay--;
	if (ppu->io_latch_decay == 0)
		ppu->io_latch = 0;
	else if (ppu->io_latch_decay < 0)
		ppu->io_latch_decay = -1;

	tmp = ppu->frame_cycles * ppu->ppu_clock_divider;
	ppu->frame_cycles = (241 + ppu->post_render_scanlines +
			     ppu->vblank_scanlines) * 341;
	ppu->visible_cycles = ppu->frame_cycles -
	                      (ppu->vblank_scanlines) * 341;
	cpu_set_frame_cycles(ppu->emu->cpu,
	                     ppu->visible_cycles * ppu->ppu_clock_divider,
			     ppu->frame_cycles * ppu->ppu_clock_divider);

	return tmp;
}

static INLINE void plot_pixel(struct ppu_state *ppu, int bg_pixel,
			      int x) ALWAYS_INLINE;
static INLINE void plot_pixel(struct ppu_state *ppu, int bg_pixel, int x)
{
	int index;
	int sprite_pixel;
	int pixel = 0;

	bg_pixel &= ppu->bg_mask;

	if (!(bg_pixel & 3))
		bg_pixel = 0;

	if (!ppu->hide_bg)
		pixel = bg_pixel;

	sprite_pixel = ppu->sprite_tile_buffer[x] & ppu->sprite_mask;
	ppu->sprite_tile_buffer[x] = 0;

	if (sprite_pixel) {
		if ((sprite_pixel & PIXEL_SPRITE_ZERO) && bg_pixel)
			ppu->status_reg |= STATUS_REG_SPRITE_ZERO;

		if (!pixel || !(sprite_pixel & PIXEL_BG_PRIORITY)) {
			if (!ppu->hide_sprites)
				pixel = sprite_pixel & 0x1f;
		}
	}

	index = ppu->translated_palette[pixel] & ppu->palette_mask;
	index |= ppu->emphasis;

	ppu->nes_pixel_buf[(ppu->scanline * 256) + x] = index;
}

static INLINE void render_disabled_pixels(struct ppu_state *ppu, int x, int count)
{
	int pixel;
	int index;

	if (ppu->scanline < 0) {
		while (count) {
			ppu->sprite_tile_buffer[x] = 0;
			x++;
			count--;
		}

		return;
	}

	if ((ppu->scroll_address & PPU_RAM_MAX) >= PPU_PALETTE_OFFSET) {
		pixel = ppu->scroll_address & 0x1f;
		if ((pixel & 0x1c) == pixel)
			pixel &= 0xc;
	} else {
		pixel = 0;
	}

	while (count) {
		ppu->bg_pixels[x & 0xf] = 0; /* FIXME */
		ppu->sprite_tile_buffer[x] = 0;

		index = ppu->translated_palette[pixel] & ppu->palette_mask;
		index |= ppu->emphasis;

		ppu->nes_pixel_buf[(ppu->scanline * 256) + x] = index;
		x++;
		count--;
	}
}

static INLINE void render_pixel(struct ppu_state *ppu)
{
	uint8_t bg_pixel;

	if (ppu->scanline == -1) {
		ppu->sprite_tile_buffer[ppu->scanline_cycle - 2] = 0;
		return;
	}

	bg_pixel =
	    ppu->bg_pixels[(ppu->scanline_cycle - 2 + ppu->fine_x_scroll) & 0x0f];

	plot_pixel(ppu, bg_pixel, ppu->scanline_cycle - 2);
}

static INLINE void load_bg_shift_register(struct ppu_state *ppu)
{
	uint32_t attributes;
	unsigned int even, odd;
	int i;

	attributes = (ppu->attribute_latch & 3) << 2;

	/* interleave bits for even pixels */
	even = ppu->left_tile_latch & 0x55;
	even |= (ppu->right_tile_latch << 1) & 0xaa;

	/* interleave bits for odd pixels */
	odd = (ppu->left_tile_latch >> 1) & 0x55;
	odd |= ppu->right_tile_latch & 0xaa;

	i = ((ppu->scanline_cycle - 1) & 0x0f) ^ 0x08;
	ppu->bg_pixels[i + 0] = attributes | ((odd & 0xc0) >> 6);
	ppu->bg_pixels[i + 1] = attributes | ((even & 0xc0) >> 6);
	ppu->bg_pixels[i + 2] = attributes | ((odd & 0x30) >> 4);
	ppu->bg_pixels[i + 3] = attributes | ((even & 0x30) >> 4);
	ppu->bg_pixels[i + 4] = attributes | ((odd & 0x0c) >> 2);
	ppu->bg_pixels[i + 5] = attributes | ((even & 0x0c) >> 2);
	ppu->bg_pixels[i + 6] = attributes | (odd & 0x03);
	ppu->bg_pixels[i + 7] = attributes | (even & 0x03);
}

static INLINE void load_extended_sprite_tiles(struct ppu_state *ppu)
{
	int temp;
	int attributes, a;
	int i;
	int index;
	int x_pos;

	uint8_t left_tile;
	uint8_t right_tile;

	for (index = ppu->scanline_sprite_count * 4;
	     index < ppu->scanline_extended_sprite_count * 4; index += 4) {
		int line, height;
		uint16_t addr, ptbl;

		addr = ppu->secondary_oam[index + 1];
		attributes = ppu->secondary_oam[index + 2];
		x_pos = ppu->secondary_oam[index + 3];

		line = ppu->scanline - ppu->secondary_oam[index];

		height = SPRITE_HEIGHT();
		ptbl = (ppu->ctrl_reg & CTRL_REG_SPRITE_TABLE) ? 0x1000 : 0;

		if (height == 16) {
			ptbl = (addr & 1) << 12;
			addr &= 0xfe;
		}

		addr <<= 4;

		if (attributes & Y_FLIP)
			line = height - 1 - line;

		addr += ptbl;
		addr += line;

		if (height == 16 && line >= 8)
			addr += 8;

		left_tile = read_spr(ppu, addr);
		right_tile = read_spr(ppu, addr + 8);

		if (attributes & X_FLIP) {
			left_tile = xflip_lookup[left_tile];
			right_tile = xflip_lookup[right_tile];
		}

		a = (attributes & 3) << 2;

		for (i = 0; i < 8; i++) {
			temp = (left_tile >> 7) & 0x01;
			temp |= (right_tile >> 6) & 0x02;
			temp |= a;
			temp |= 0x10;

			if ((attributes & (1 << 5)) && (temp & 0x03)) {
				temp |= PIXEL_BG_PRIORITY;
			}

			if (x_pos <= 255) {
				if (!(ppu->sprite_tile_buffer[x_pos] & 0x03)) {
					if (temp & 0x03)
						ppu->sprite_tile_buffer[x_pos] =
						    temp;
				}
			}

			left_tile <<= 1;
			right_tile <<= 1;
			x_pos++;
		}
	}

}

static INLINE void load_sprite_tile(struct ppu_state *ppu)
{
	int temp;
	int attributes, a;
	int i;
	int index;
	int x_pos;
	int scanline;

	index = (ppu->scanline_cycle - 261) / 2;
	scanline = ppu->secondary_oam[index - 1];
	attributes = ppu->secondary_oam[index + 1];
	x_pos = ppu->secondary_oam[index + 2];

	if ((ppu->scanline < 0) || (scanline > ppu->scanline) ||
	    (ppu->scanline - scanline >= SPRITE_HEIGHT())) {
		ppu->left_tile_latch = 0;
		ppu->right_tile_latch = 0;
	}

	if (attributes & X_FLIP) {
		ppu->left_tile_latch = xflip_lookup[ppu->left_tile_latch];
		ppu->right_tile_latch = xflip_lookup[ppu->right_tile_latch];
	}

	a = (attributes & 3) << 2;

	/* FIXME this probably isn't necessary */
	if (index < 4 && ppu->sprite_zero_present && x_pos == 255) 
		ppu->sprite_zero_present = 0;

	for (i = 0; i < 8; i++) {
		temp = (ppu->left_tile_latch >> 7) & 0x01;
		temp |= (ppu->right_tile_latch >> 6) & 0x02;
		if ((temp & 0x03) == 0) {
			temp = 0;
		} else {

			temp |= a | 0x10;

			if ((attributes & (1 << 5)) && (temp & 0x03)) {
				temp |= PIXEL_BG_PRIORITY;
			}

			if (index < 4 && (temp & 0x03)
			    && ppu->sprite_zero_present && x_pos < 255) {
				temp |= PIXEL_SPRITE_ZERO;
			}

			if (x_pos <= 255) {
				if (!(ppu->sprite_tile_buffer[x_pos] & 0x03)) {
					ppu->sprite_tile_buffer[x_pos] = temp;
				}
			}
		}

		ppu->left_tile_latch <<= 1;
		ppu->right_tile_latch <<= 1;
		x_pos++;
	}
}

static INLINE uint16_t get_nametable_entry_addr(struct ppu_state *ppu)
{
	uint16_t addr;

	addr = PPU_NAMETABLE_OFFSET;
	addr |= ppu->scroll_address & (SCROLL_NAMETABLE | SCROLL_X | SCROLL_Y);

	return addr;
}

static INLINE uint16_t get_attrtable_entry_addr(struct ppu_state *ppu)
{
	uint16_t addr;

	addr = PPU_ATTRTABLE_OFFSET;
	addr |= ppu->scroll_address & (SCROLL_NAMETABLE);

	/* "row" in attribute table * 8 */
	addr |= ppu->scroll_address >> 4 & 0x38;

	/* "column" in attribute table */
	addr |= ppu->scroll_address >> 2 & 0x07;

	return addr;
}

#define bg_pattern_table_address() ((ppu->ctrl_reg & CTRL_REG_BG_TABLE) << 8)
#define sprite_pattern_table_address() ((ppu->ctrl_reg & CTRL_REG_SPRITE_TABLE) << 9)

static INLINE void load_sprite_address(struct ppu_state *ppu, int half)
{
	int height, line;
	uint16_t addr, ptbl;
	int index;

	index = (ppu->scanline_cycle - (half << 1) - 261) >> 1;

	line = ppu->scanline - ppu->secondary_oam[index];
	height = SPRITE_HEIGHT();
	ptbl = (ppu->ctrl_reg & CTRL_REG_SPRITE_TABLE) ? 0x1000 : 0;
	addr = ppu->secondary_oam[index + 1];

	if (height == 16) {
		ptbl = (addr & 1) << 12;
		addr &= 0xfe;
	}

	addr <<= 4;

	if (ppu->scanline == -1 || ppu->secondary_oam[index] >= 0xef) {
		addr = (height == 16 || ptbl ? 0x1ff0 : 0x0ff0);
		update_address_bus(addr);
		return;
	}

	if (ppu->secondary_oam[index + 2] & Y_FLIP)
		line = height - 1 - line;

	addr += ptbl;
	addr += line;

	if (height == 16 && line >= 8)
		addr += 8;

	if (half)
		addr += 8;

	update_address_bus(addr);
}

static INLINE void finish_nametable_fetch(struct ppu_state *ppu)
{
	uint16_t addr;
	uint8_t value = 0;
	int column, row;
	int nmt_index;

	nmt_index = ppu->address_bus & 0x3ff;
	column = ((ppu->scanline_cycle - 1)/ 8) + 2;
	row = (column / 42 + ppu->scanline + ppu->split_screen_scroll) / 8;
	column %= 42;

	addr = ppu->address_bus & 0x3fff;
	if (ppu->exram_mode < EXRAM_MODE_WRAM &&
	    ppu->split_screen_enabled &&
	    column >= ppu->split_screen_start &&
	    column <= ppu->split_screen_end) {

		row &= 0x1f;
		/* FIXME not sure if this is correct.  If the "row" we would display
		   is in the attribute table area, wrap around to row 0, but only
		   if the scroll wasn't explicitly set to a value >= 240 */
		if (ppu->split_screen_scroll < 240 && row > 29)
			row = 0;

		nmt_index = ((row << 5) | column) & 0x3ff;
		value = ppu->exram[nmt_index];
	} else if (ppu->read_bg_pagemap[addr >> PPU_PAGE_SHIFT]) {
		value =
		    ppu->read_bg_pagemap[addr >> PPU_PAGE_SHIFT][nmt_index];
	}
	/* FIXME what to do if open bus? */

	ppu->nmt_latch = value;
}

static INLINE void increment_x_scroll(struct ppu_state *ppu)
{
	if ((ppu->scroll_address & SCROLL_X) == SCROLL_X) {
		ppu->scroll_address ^= SCROLL_NAMETABLE_LOW | SCROLL_X;
	} else {
		ppu->scroll_address++;
	}
}

static INLINE void reset_x_scroll(struct ppu_state *ppu)
{
	ppu->scroll_address = ppu->scroll_address &
		(SCROLL_Y | SCROLL_Y_FINE | SCROLL_NAMETABLE_HIGH);
	ppu->scroll_address |= ppu->scroll_address_latch &
		(SCROLL_X | SCROLL_NAMETABLE_LOW);
}

static INLINE void increment_y_scroll(struct ppu_state *ppu)
{
	if ((ppu->scroll_address & SCROLL_Y_FINE) != SCROLL_Y_FINE) {
		ppu->scroll_address += (1 << 12);	/* increment fine scroll */
	} else if ((ppu->scroll_address & SCROLL_Y) == (29 << 5)) {
		ppu->scroll_address ^= SCROLL_NAMETABLE_HIGH;
		ppu->scroll_address &= SCROLL_NAMETABLE | SCROLL_X;
	} else if ((ppu->scroll_address & SCROLL_Y) == (31 << 5)) {
		ppu->scroll_address &= SCROLL_NAMETABLE | SCROLL_X;
	} else {
		ppu->scroll_address &= SCROLL_Y | SCROLL_X | SCROLL_NAMETABLE;
		ppu->scroll_address += (1 << 5);
	}
}

static INLINE void load_extended_oam(struct ppu_state *ppu, int oam_addr)
{
	int addr;
	int count, x_coord_count;

	ppu->scanline_extended_sprite_count = ppu->scanline_sprite_count;

	/* Heuristic to check for exploitation of the 8-sprite limit
	   to intentionally hide sprites.  Some games that do this and
	   where the effect is visible are listed below:

	   - The Legend of Zelda: hide Link as he travels through
	     doors at top and bottom of the screen
	   - Castlevania 2: Simon's Quest: hide Simon's legs when
	     he's standing in the swamp
	   - Majou Densetsu 2: hide Populon when he enters doors
	   - Ninja Gaiden: hide sprites in cutscenes
	   - Gremlins 2: hide some sprites during cutscenes
	   - Operation Wolf: hide parachuter behind caption during
	     intro
	   - Gimmick!: Hide Yumetaro/Mr. Gimmick before he pops out of
             the "teleport" at the start of a level


	   All of these games put the first 8 sprites of the relevant
	   scanlines at the same X and Y coordinates.  Ninja Gaiden
	   and Operation Wolf both have at least one occasion where
	   the first of these sprites isn't the same as the others,
	   but the last 7 are completely identical.  Gremlins 2 has
	   all 8 identical except for their x-coordinate. TODO: see if
	   this check can be refined a bit.  It's possible to have
	   8 identical sprites on the same scanline that are visible
	   and intentionally spread out. :-/

	   The heuristic, for now, is to require either the last 7
	   sprites of the first 8 to be identical (covers everything
	   except Gremlins 2), or have all 8 be identical aside from
	   the x-coordinate if none of the x-coordinates match (for
	   Gremlins 2).

	   If, for some reason, this heuristic incorrectly determines
	   that sprite hiding was intentional, then only the first 8
	   sprites will be drawn for this scanline (as on an actual
	   NES or Famicom).
	 */
	if (ppu->allow_sprite_hiding && ppu->scanline_sprite_count == 8) {
		count = 1;
		x_coord_count = 1;
		for (addr = 0; addr < 28; addr += 4) {
			/* FIXME This probably isn't optimal */
			if (memcmp(&ppu->secondary_oam[addr],
				   &ppu->secondary_oam[28], 3) == 0) {
				count++;
			}

			if (ppu->secondary_oam[addr + 3] == ppu->secondary_oam[31]) {
				x_coord_count++;
			}

		}

		if (((count >= 7) && (x_coord_count >= count)) ||
		    ((count == 8) && (x_coord_count == 0))) {
			return;
		}
	}

	addr = oam_addr;
	while (addr > 0 && addr < sizeof(ppu->oam)) {
		if ((ppu->scanline >= ppu->oam[addr]) &&
		    (ppu->scanline - ppu->oam[addr] < SPRITE_HEIGHT())) {
			ppu->secondary_oam[ppu->secondary_oam_index++] =
			    ppu->oam[addr++];
			ppu->secondary_oam[ppu->secondary_oam_index++] =
			    ppu->oam[addr++];
			ppu->secondary_oam[ppu->secondary_oam_index++] =
			    ppu->oam[addr++];
			ppu->secondary_oam[ppu->secondary_oam_index++] =
			    ppu->oam[addr++];
			ppu->scanline_extended_sprite_count++;
		} else {
			addr += 4;
		}
	}
}

static INLINE void sprite_eval(struct ppu_state *ppu)
{
	if (ppu->scanline < 0)
		return;

	switch (ppu->sprite_eval_state) {
	case SPRITE_EVAL_CLEAR:
		ppu->secondary_oam[(ppu->scanline_cycle - 1)/ 2] = 0xff;
		break;
	case SPRITE_EVAL_STOPPED:
		break;
	case SPRITE_EVAL_SCANLINE_CHECK:
		if (ppu->scanline < ppu->oam_latch ||
		    ppu->scanline - ppu->oam_latch >= SPRITE_HEIGHT()) {
			ppu->oam_address += 4;

			if ((ppu->oam_address & 0xfc) == 0)
				ppu->sprite_eval_state = SPRITE_EVAL_LOOP;
			else if (ppu->scanline_sprite_count == 8) {
				ppu->oam_address = (ppu->oam_address & 0xfc) +
				    ((ppu->oam_address + 1) & 0x03);
			}
		} else {
			if (ppu->scanline_sprite_count < 8) {
				/* NOTE: copying the Y-coordinate should happen
				   even if the sprite is out of range, but
				   since I don't (yet) do range checking when
				   loading the sprite tile data, this results
				   in garbage appearing in the rightmost
				   column.  So I avoid that by only copying the
				   Y-coordinate if it's in range.

				   This is effectively the same behavior as the
				   real PPU anyway, and it should still be "correct"
				   according to current understanding of how the
				   PPU works.
				 */
				ppu->secondary_oam[ppu->secondary_oam_index] =
				    ppu->oam_latch;
				ppu->secondary_oam_index++;
			} else if (ppu->scanline_sprite_count == 8) {
				ppu->sprite_overflow_flag = STATUS_REG_SPRITE_OVERFLOW;
			}

			ppu->sprite_eval_state = SPRITE_EVAL_COPY_INDEX;
			ppu->oam_address++;
		}
		break;
	case SPRITE_EVAL_COPY_INDEX:
		if (ppu->scanline_sprite_count < 8) {
			ppu->secondary_oam[ppu->secondary_oam_index] =
			    ppu->oam_latch;
			ppu->secondary_oam_index++;
		}
		ppu->oam_address++;
		ppu->sprite_eval_state = SPRITE_EVAL_COPY_ATTRIBUTES;
		break;
	case SPRITE_EVAL_COPY_ATTRIBUTES:
		if (ppu->scanline_sprite_count < 8) {
			ppu->secondary_oam[ppu->secondary_oam_index] =
			    ppu->oam_latch;
			ppu->secondary_oam_index++;
		}
		ppu->oam_address++;
		ppu->sprite_eval_state = SPRITE_EVAL_COPY_X_OFFSET;
		break;
	case SPRITE_EVAL_COPY_X_OFFSET:
		ppu->oam_address++;
		if (ppu->scanline_sprite_count < 8) {
			ppu->secondary_oam[ppu->secondary_oam_index] =
			    ppu->oam_latch;
			ppu->secondary_oam_index++;
			ppu->scanline_sprite_count++;

			if ((ppu->scanline_sprite_count == 8) &&
			    ppu->no_sprite_limit) {
				load_extended_oam(ppu, ppu->oam_address);
			}

			if (ppu->oam_address == ppu->oam_address_latch + 4) {
				ppu->sprite_zero_present = 1;
			}

			if ((ppu->oam_address & 0xff) == 0)
				ppu->sprite_eval_state = SPRITE_EVAL_LOOP;
			else
				ppu->sprite_eval_state =
				    SPRITE_EVAL_SCANLINE_CHECK;
		} else {
			if ((ppu->oam_address & 3) == 3)
				ppu->oam_address++;

			ppu->oam_address &= 0xfc;
			ppu->sprite_eval_state = SPRITE_EVAL_LOOP;
		}
		break;
	case SPRITE_EVAL_LOOP:
		ppu->oam_address = (ppu->oam_address + 4) & 0xff;
		/* HACK */
		if (ppu->scanline_sprite_count < 8) {
			ppu->secondary_oam[ppu->secondary_oam_index] = 0xff;
		}
		break;
	}
}

static INLINE uint8_t do_attribute_fetch(struct ppu_state *ppu) ALWAYS_INLINE;
static INLINE uint8_t do_attribute_fetch(struct ppu_state *ppu)
{
	int column;
	uint32_t addr;
	uint8_t *ptr;
	uint8_t data;
	int index;
	int exattr;

	exattr = 0;
	addr = ppu->address_bus & 0x3fff;
	index = addr & PPU_PAGE_MASK;
	ptr = ppu->read_bg_pagemap[(addr >> PPU_PAGE_SHIFT)];

	if (ppu->exram_mode < EXRAM_MODE_WRAM) {
		column = (((ppu->scanline_cycle - 1) / 8) + 2) % 42;

		if (ppu->split_screen_enabled &&
		    column >= ppu->split_screen_start &&
		    column <= ppu->split_screen_end) {
			ptr = ppu->exram;
		} else if (ppu->exram_mode == EXRAM_MODE_EXATTR) {
			index = ppu->scroll_address & 0x3ff;
			ptr = ppu->exram;
			exattr = 1;
		}
	}

	data = 0xff;
	if (ptr)
		data = ptr[index];

	if (exattr)
		data >>= 6;
	else
		data >>= ((ppu->scroll_address & 0x02) |
			  (ppu->scroll_address >> 4 & 0x04));

	return data;
}

static INLINE uint8_t do_bg_tile_fetch(struct ppu_state *ppu) ALWAYS_INLINE;
static INLINE uint8_t do_bg_tile_fetch(struct ppu_state *ppu)
{
	int column, row;
	uint32_t addr;
	uint8_t *ptr;
	int offset;
	uint8_t data;
	int fine_y_scroll;
	int index;

	offset = 0;
	addr = ppu->address_bus;
	index = addr & 0x3f8;
	ptr = ppu->read_bg_pagemap[addr >> PPU_PAGE_SHIFT];
	fine_y_scroll = addr & 0x07;

	if (ppu->exram_mode < EXRAM_MODE_WRAM) {
		column = (((ppu->scanline_cycle - 1)/ 8) + 2);
		row = column / 42 + ppu->scanline + ppu->split_screen_scroll;
		column %= 42;

		if (ppu->split_screen_enabled &&
		    column >= ppu->split_screen_start &&
		    column <= ppu->split_screen_end) {
			ptr = ppu->chr_rom;
			offset = ppu->split_screen_bank << 12;
			offset %= ppu->chr_rom_size;
			fine_y_scroll = row & 0x07;
			index = addr & 0xff8;
		} else if (ppu->exram_mode == EXRAM_MODE_EXATTR) {
			ptr = ppu->chr_rom;
			/* FIXME need upper two bits */
			offset = (ppu->exram[ppu->scroll_address & 0x3ff] & 0x3f) << 12;
			offset %= ppu->chr_rom_size;
			index = addr & 0xff8;
		}
	}

	data = 0xff;
	if (ptr)
		data = ptr[offset | index | fine_y_scroll];

	if (ppu_read_hook)
		ppu_read_hook(ppu->emu->board, ppu->address_bus);

	return data;
}

static INLINE void sprite_zero_overwrite(struct ppu_state *ppu) ALWAYS_INLINE;
static INLINE void sprite_zero_overwrite(struct ppu_state *ppu)
{
	int addr;
	int i;

	addr = ppu->oam_addr_reg & 0xf8;

	/* Replace the first two sprites with the ones at addr.  This
	   is necessary for Huge Insect to work.

	   The two overwritten sprites are first copied to the two
	   extra slots at the end of OAM.  These slots are ignored
	   during normal sprite evaluation.  If the scanline limit has
	   been disabled though, load_extended_oam() will see them and
	   copy them to secondary_oam if they are in range.  Without
	   this, games that exploit this bug will still have a small
	   amount of flicker even with the sprite limit disabled.

	   Bubble Bobble is one game affected by this.
	*/

	for (i = 0; i < 8; i++) {
		ppu->oam[256 + i] = ppu->oam[i];
		ppu->oam[i] = ppu->oam[addr + i];
	}
}

static int do_disabled_scanline(struct ppu_state *ppu, int cycles)
{
	int needed;
	int start_cycle;

	start_cycle = ppu->scanline_cycle;
	needed = 341 - ppu->scanline_cycle;
	if (needed > cycles - ppu->cycles)
		needed = cycles - ppu->cycles;

	ppu->cycles += needed;
	ppu->scanline_cycle += needed;

	if (ppu->scanline == -1) {
		if (ppu->scanline_cycle > 1)
			ppu->status_reg &= STATUS_REG_VBLANK_FLAG;

		if (ppu->scanline_cycle > 2)
			ppu->status_reg &= ~STATUS_REG_VBLANK_FLAG;
	}

	if (start_cycle < 258) {
		int end_cycle = ppu->scanline_cycle;

		if (end_cycle > 258)
			end_cycle = 258;

		if (ppu->scanline_cycle > 1 && start_cycle < 2)
			start_cycle = 2;

		render_disabled_pixels(ppu, start_cycle - 2,
				       end_cycle - start_cycle);
	}

	if (ppu->scanline_cycle > 321) {
		ppu->oam_latch = ppu->oam[0];
		ppu->sprite_eval_state = SPRITE_EVAL_STOPPED;
	}

	if (ppu->scanline == -1 && ppu->scanline_cycle == 341) {
		ppu->burst_phase = (ppu->burst_phase + 1) % 3;
//		printf("burst_phase = %d (disabled)\n", ppu->burst_phase);
	}

	if (ppu->scanline_cycle == 341)
		return 0;
	else
		return 1;
}

static int do_partial_scanline(struct ppu_state *ppu, int cycles)
{
	/* Tiles take 8 cycles to render:
	   2 for nametable fetch
	   2 for attribute table fetch
	   2 for left tile fetch
	   2 for right tile fetch
	 */
	while (ppu->scanline_cycle < 341) {
		switch (ppu->scanline_cycle) {
		case 0:
			add_cycle();
			return_if_done();
		case   1: case   9: case  17: case  25:
		case  33: case  41: case  49: case  57:
		case  65: case  73: case  81: case  89:
		case  97: case 105: case 113: case 121:
		case 129: case 137: case 145: case 153:
		case 161: case 169: case 177: case 185:
		case 193: case 201: case 209: case 217:
		case 225: case 233: case 241: case 249:
			if (ppu->scanline_cycle == 1) {
				if (ppu->scanline < 0)
					ppu->status_reg &= STATUS_REG_VBLANK_FLAG;
				ppu->secondary_oam_index = 0;
				ppu->sprite_eval_state = SPRITE_EVAL_CLEAR;
			} else {
				render_pixel(ppu);
				load_bg_shift_register(ppu);
				if (ppu->scanline_cycle == 9) {
					ppu->bg_mask = ppu->bg_all_mask;
					ppu->sprite_mask = ppu->sprite_all_mask;
				}else if (ppu->scanline_cycle == 65) {
					ppu->sprite_eval_state =
						SPRITE_EVAL_SCANLINE_CHECK;
					/* FIXME do I need to set this here? */
					ppu->oam_address_latch = ppu->oam_addr_reg;
					ppu->oam_address = ppu->oam_address_latch;
					ppu->oam_latch = 0xff;
				}
			}
			start_nametable_fetch(ppu);
			sprite_read(ppu);
			add_cycle();
			return_if_done();
		case   2: case  10: case  18: case  26:
		case  34: case  42: case  50: case  58:
		case  66: case  74: case  82: case  90:
		case  98: case 106: case 114: case 122:
		case 130: case 138: case 146: case 154:
		case 162: case 170: case 178: case 186:
		case 194: case 202: case 210: case 218:
		case 226: case 234: case 242: case 250:
			if (ppu->scanline_cycle == 2) {
				if (ppu->scanline < 0)
					ppu->status_reg &= ~STATUS_REG_VBLANK_FLAG;
				ppu->bg_mask = ppu->bg_left8_mask;
				ppu->sprite_mask = ppu->sprite_left8_mask;
			}
			render_pixel(ppu);
			finish_nametable_fetch(ppu);
			sprite_eval(ppu);
			add_cycle();
			return_if_done();
		case   3: case  11: case  19: case  27:
		case  35: case  43: case  51: case  59:
		case  67: case  75: case  83: case  91:
		case  99: case 107: case 115: case 123:
		case 131: case 139: case 147: case 155:
		case 163: case 171: case 179: case 187:
		case 195: case 203: case 211: case 219:
		case 227: case 235: case 243: case 251:
			render_pixel(ppu);
			start_attribute_fetch(ppu);
			sprite_read(ppu);
			add_cycle();
			return_if_done();
		case   4: case  12: case  20: case  28:
		case  36: case  44: case  52: case  60:
		case  68: case  76: case  84: case  92:
		case 100: case 108: case 116: case 124:
		case 132: case 140: case 148: case 156:
		case 164: case 172: case 180: case 188:
		case 196: case 204: case 212: case 220:
		case 228: case 236: case 244: case 252:
			render_pixel(ppu);
			ppu->attribute_latch = do_attribute_fetch(ppu);
			sprite_eval(ppu);
			add_cycle();
			return_if_done();
		case   5: case  13: case  21: case  29:
		case  37: case  45: case  53: case  61:
		case  69: case  77: case  85: case  93:
		case 101: case 109: case 117: case 125:
		case 133: case 141: case 149: case 157:
		case 165: case 173: case 181: case 189:
		case 197: case 205: case 213: case 221:
		case 229: case 237: case 245: case 253:
			render_pixel(ppu);
			start_left_bg_tile_fetch();
			sprite_read(ppu);
			add_cycle();
			return_if_done();
		case   6: case  14: case  22: case  30:
		case  38: case  46: case  54: case  62:
		case  70: case  78: case  86: case  94:
		case 102: case 110: case 118: case 126:
		case 134: case 142: case 150: case 158:
		case 166: case 174: case 182: case 190:
		case 198: case 206: case 214: case 222:
		case 230: case 238: case 246: case 254:
			render_pixel(ppu);
			ppu->left_tile_latch = do_bg_tile_fetch(ppu);
			sprite_eval(ppu);
			add_cycle();
			return_if_done();
		case   7: case  15: case  23: case  31:
		case  39: case  47: case  55: case  63:
		case  71: case  79: case  87: case  95:
		case 103: case 111: case 119: case 127:
		case 135: case 143: case 151: case 159:
		case 167: case 175: case 183: case 191:
		case 199: case 207: case 215: case 223:
		case 231: case 239: case 247: case 255:
			render_pixel(ppu);
			start_right_bg_tile_fetch();
			sprite_read(ppu);
			add_cycle();
			return_if_done();
		case   8: case  16: case  24: case  32:
		case  40: case  48: case  56: case  64:
		case  72: case  80: case  88: case  96:
		case 104: case 112: case 120: case 128:
		case 136: case 144: case 152: case 160:
		case 168: case 176: case 184: case 192:
		case 200: case 208: case 216: case 224:
		case 232: case 240: case 248: case 256:
			render_pixel(ppu);
			ppu->right_tile_latch = do_bg_tile_fetch(ppu);
			sprite_eval(ppu);
			if (ppu->scanline_cycle == 256) {
				if ((ppu->wrote_2006 < 255) || (ppu->wrote_2006 > 256)) {
					/*The PPU does some odd things if you
					* complete the second $2006 write
					* exactly at the start of cycle 256.
					*
					* The fine vertical scroll value is
					* kept from what was written to $2006,
					* but the rest seems unpredictable and
					* appears to depend on the value that
					* was written to the scroll address.
					* I haven't tested this on actual
					* hardware, but this is what happens
					* in Visual 2C02.  Here, the scroll
					* increment is skipped; while not quite
					* accurate this appears to work for
					* the games affected by this behavior.
					*
					* Two games, to my knowledge, require
					* this to be emulated to avoid the
					*"shaky status bar syndrome":
					*
					* - Cosmic Wars (J)
					* - The Simpsons: Bart vs. the Space
					*   Mutants (U)
					*/
					increment_y_scroll(ppu);
					increment_x_scroll(ppu);
				}

				/* If there are fewer than 8 sprites on this
				   scanline, then the overwritten sprite zero
				   data wasn't handled by loading extended
				   OAM in sprite_eval().  Force them to be
				   evaluated here if their y-coordinates
				   aren't 0xff. */
				if (((ppu->oam[256] != 0xff) ||
				    (ppu->oam[260] != 0xff)) &&
				    ppu->no_sprite_limit &&
				    ppu->scanline_sprite_count < 8) {
					load_extended_oam(ppu, 256);
				}
			} else {
				increment_x_scroll(ppu);
			}
			add_cycle();
			return_if_done();
			if (ppu->scanline_cycle < 257)
				continue;
		case 257: case 265: case 273: case 281:
		case 289: case 297: case 305: case 313:
			if (ppu->scanline_cycle == 257) {
				ppu->status_reg |= ppu->sprite_overflow_flag;
				render_pixel(ppu);
				ppu->oam_latch = 0xff;
				reset_x_scroll(ppu);
			}
			start_nametable_fetch(ppu);	/* garbage */
			if (ppu->scanline == -1) {
				if (ppu->scanline_cycle == 257) {
					if (ppu->oam_addr_reg & 0xf8) {
						sprite_zero_overwrite(ppu);
					} else {
						ppu->oam[256] = 0xff;
						ppu->oam[260] = 0xff;
					}
				}
			}
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
		case 258: case 266: case 274: case 282:
		case 290: case 298: case 306: case 314:
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
		case 259: case 267: case 275: case 283:
		case 291: case 299: case 307: case 315:
			start_nametable_fetch(ppu);	/* garbage */
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
		case 260: case 268: case 276: case 284:
		case 292: case 300: case 308: case 316:
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
		case 261: case 269: case 277: case 285:
		case 293: case 301: case 309: case 317:
			load_sprite_address(ppu, 0);
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
		case 262: case 270: case 278: case 286:
		case 294: case 302: case 310: case 318:
			finish_left_spr_tile_fetch();
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
		case 263: case 271: case 279: case 287:
		case 295: case 303: case 311: case 319:
			load_sprite_address(ppu, 1);
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
		case 264: case 272: case 280: case 288:
		case 296: case 304: case 312: case 320:
			finish_right_spr_tile_fetch();
			load_sprite_tile(ppu);
			if (ppu->scanline_cycle == 320)
				if (ppu->no_sprite_limit)
					load_extended_sprite_tiles(ppu);
			if (ppu->scanline == -1 && ppu->scanline_cycle == 304) {
				/* This also happens on cycles 280-303, but that
				   is handled at the end of the function. */
				ppu->scroll_address &= SCROLL_HORIZONTAL;
				ppu->scroll_address |= ppu->scroll_address_latch &
					SCROLL_VERTICAL;
			}
			ppu->oam_addr_reg = 0;
			add_cycle();
			return_if_done();
			if (ppu->scanline_cycle < 321)
				continue;
		case 321:
		case 329:
		case 337:
			if (ppu->scanline_cycle == 321) {
				ppu->oam_latch = ppu->oam[0];
				ppu->sprite_eval_state = SPRITE_EVAL_STOPPED;
			} else {
				load_bg_shift_register(ppu);
			}
			start_nametable_fetch(ppu);
			add_cycle();
			return_if_done();
		case 322:
		case 330:
		case 338:
			finish_nametable_fetch(ppu);
			add_cycle();
			return_if_done();
		case 323:
		case 331:
		case 339:
			start_attribute_fetch(ppu);
			add_cycle();
			return_if_done();
		case 324:
		case 332:
		case 340:
			ppu->attribute_latch = do_attribute_fetch(ppu);
			if (ppu->scanline_cycle == 340) {
				if ((ppu->scanline == -1) && ppu->odd_frame) {
					ppu->frame_cycles--;
					ppu->visible_cycles--;
					cpu_set_frame_cycles(ppu->emu->cpu,
							     ppu->visible_cycles *
							     ppu->ppu_clock_divider,
							     ppu->frame_cycles *
							     ppu->ppu_clock_divider);
					ppu->burst_phase = (ppu->burst_phase + 2) % 3;
//					printf("burst_phase = %d (short)\n", ppu->burst_phase);
				} else {
					if (ppu->scanline == -1) {
						ppu->burst_phase = (ppu->burst_phase + 1) % 3;
//						printf("burst_phase = %d (long)\n", ppu->burst_phase);
					}
					add_cycle();
				}
				return 0;
			}
			add_cycle();
			return_if_done();
		case 325:
		case 333:
			start_left_bg_tile_fetch();
			add_cycle();
			return_if_done();
		case 326:
		case 334:
			ppu->left_tile_latch = do_bg_tile_fetch(ppu);
			add_cycle();
			return_if_done();
		case 327:
		case 335:
			start_right_bg_tile_fetch();
			add_cycle();
			return_if_done();
		case 328:
		case 336:
			ppu->right_tile_latch = do_bg_tile_fetch(ppu);
			increment_x_scroll(ppu);
			add_cycle();
			return_if_done();
		}
	}

	/* The vertical scroll portion of vramaddr_v gets copied from
	   vramaddr_t on the prerender scanline, on each of the cycles
	   280 - 304 inclusive.  Cycle 304 implements this above, but
	   it is done again here in case we stop execution before
	   then.
	*/
	if (ppu->scanline == -1 && ppu->scanline_cycle >= 281 &&
	    ppu->scanline_cycle <= 304) {
		ppu->scroll_address &= SCROLL_HORIZONTAL;
		ppu->scroll_address |= ppu->scroll_address_latch &
			SCROLL_VERTICAL;
	}

	return 0;
}

static INLINE void sprite_eval_scanline(struct ppu_state *ppu) ALWAYS_INLINE;
static INLINE void sprite_eval_scanline(struct ppu_state *ppu)
{
	int extended_oam_start;
	int i;

	extended_oam_start = 0;

	memset(ppu->secondary_oam, 0xff, 32);
	ppu->secondary_oam_index = 0;
	ppu->oam_address_latch = ppu->oam_addr_reg;
	ppu->oam_address = ppu->oam_addr_reg;

	ppu->scanline_sprite_count = 0;
	ppu->scanline_extended_sprite_count = 0;

	/* FIXME make sure sprite eval state is set properly in case
	   next line is a partial one?
	*/

	for (i = 0; i < 64; i++) {
		int sprite_scanline;
		int addr;
		int x_coord;
		int attrib;

		sprite_scanline = ppu->oam[ppu->oam_address & 0xff];

		if ((ppu->scanline < sprite_scanline) ||
		    (ppu->scanline - sprite_scanline >= SPRITE_HEIGHT())) {
			if (ppu->scanline_sprite_count == 8) {
				ppu->oam_address =
				    ((ppu->oam_address + 4) & 0x1fc) +
				    ((ppu->oam_address + 1) & 3);
				if (ppu->oam_address & 0x100)
					break;
				else
					continue;
			}
			goto skip;
		} else {
			if (ppu->scanline_sprite_count == 8) {
				ppu->status_reg |= STATUS_REG_SPRITE_OVERFLOW;
				break;
			}
			ppu->scanline_sprite_count++;
		}

		addr = ppu->oam[(ppu->oam_address + 1) & 0xff];
		attrib = ppu->oam[(ppu->oam_address + 2) & 0xff];
		x_coord = ppu->oam[(ppu->oam_address + 3) & 0xff];

		ppu->secondary_oam[ppu->secondary_oam_index++] =
		    sprite_scanline;
		ppu->secondary_oam[ppu->secondary_oam_index++] = addr;
		ppu->secondary_oam[ppu->secondary_oam_index++] = attrib;
		ppu->secondary_oam[ppu->secondary_oam_index++] = x_coord;
		if (i == 0) {
//			printf("d: sprite_zero_present on %d\n", ppu->scanline);
			ppu->sprite_zero_present = 1;
		}

		if (ppu->scanline_sprite_count == 8
		    && ppu->no_sprite_limit) {
			extended_oam_start = ppu->oam_address + 4;
		}

	skip:
		ppu->oam_address += 4;
		if (ppu->oam_address & 0x100)
			break;
	}

	if (ppu->scanline_sprite_count < 8) {
		extended_oam_start = 256;
	}

	if (ppu->no_sprite_limit) {
		load_extended_oam(ppu, extended_oam_start);
	}
}

static int do_whole_scanline(struct ppu_state *ppu)
{
	int x_coord, i;

	/* Render bg and sprites for this scanline */
	ppu->bg_mask = ppu->bg_left8_mask;
	ppu->sprite_mask = ppu->sprite_left8_mask;

	
	x_coord = 0;
	i = ppu->fine_x_scroll;
	while (x_coord < 8) {
		plot_pixel(ppu, ppu->bg_pixels[i], x_coord);
		x_coord++;
		i++;
	}

	ppu->bg_mask = ppu->bg_all_mask;
	ppu->sprite_mask = ppu->sprite_all_mask;

	while (i < 16) {
		plot_pixel(ppu, ppu->bg_pixels[i], x_coord);
		x_coord++;
		i++;
	}

	ppu->scanline_cycle = 1;
	while (ppu->scanline_cycle < 257) {
		int left, right, attr;
		int even, odd;
		int pixel;
		int i;

		start_nametable_fetch(ppu);
		finish_nametable_fetch(ppu);

		start_attribute_fetch(ppu);
		attr = do_attribute_fetch(ppu);
		attr = (attr & 3) << 2;

		start_left_bg_tile_fetch();
		left = do_bg_tile_fetch(ppu);

		start_right_bg_tile_fetch();
		right = do_bg_tile_fetch(ppu);


		ppu->scanline_cycle += 8;

		increment_x_scroll(ppu);

		if (x_coord > 255)
			continue;

		even = left & 0x55;
		even |= (right << 1) & 0xaa;

		odd = (left >> 1) & 0x55;
		odd |= right & 0xaa;

		for (i = 0; i < 8 && x_coord < 256; i += 2) {
			pixel = attr | ((odd & 0xc0) >> 6);
			plot_pixel(ppu, pixel, x_coord);
			x_coord++;
			if (x_coord > 255)
				break;

			pixel = attr | ((even & 0xc0) >> 6);
			plot_pixel(ppu, pixel, x_coord);
			x_coord++;
			even <<= 2;
			odd <<= 2;
		}
	}

	sprite_eval_scanline(ppu);
	ppu->status_reg |= ppu->sprite_overflow_flag;

	increment_y_scroll(ppu);

	/* Load sprite tiles for next scanline */
	ppu->oam_latch = 0xff;
	reset_x_scroll(ppu);

	ppu->scanline_cycle = 257;
	while (ppu->scanline_cycle < 321) {
		ppu->scanline_cycle += 4;

		load_sprite_address(ppu, 0);
		finish_left_spr_tile_fetch();
		ppu->scanline_cycle += 2;

		load_sprite_address(ppu, 1);
		ppu->scanline_cycle += 1;

		finish_right_spr_tile_fetch();
		load_sprite_tile(ppu);
		ppu->scanline_cycle++;
	}

	if (ppu->no_sprite_limit &&
	    (ppu->scanline_extended_sprite_count > ppu->scanline_sprite_count)) {
		load_extended_sprite_tiles(ppu);
	}

	/* Cycle 321 */

	ppu->oam_latch = ppu->oam[0];
	ppu->sprite_eval_state = SPRITE_EVAL_STOPPED;

	/* Load first two background tiles of next scanline */
	/* Optimizing this doesn't make a noticible difference,
	   so leave it as-is. */
	while (ppu->scanline_cycle < 341) {
		start_nametable_fetch(ppu);
		finish_nametable_fetch(ppu);

		start_attribute_fetch(ppu);
		ppu->attribute_latch = do_attribute_fetch(ppu);

		ppu->scanline_cycle += 4;
		if (ppu->scanline_cycle == 341)
			break;

		start_left_bg_tile_fetch();
		ppu->left_tile_latch = do_bg_tile_fetch(ppu);

		start_right_bg_tile_fetch();
		ppu->right_tile_latch = do_bg_tile_fetch(ppu);

		ppu->scanline_cycle += 3;
		increment_x_scroll(ppu);
		ppu->scanline_cycle++;
		load_bg_shift_register(ppu);
	}

	ppu->cycles += 341;

	return 0;
}

int ppu_run(struct ppu_state *ppu, int cycles)
{
	if (emu_resetting(ppu->emu) || ppu->catching_up || ppu->overclocking)
		return ppu->cycles * ppu->ppu_clock_divider;

	ppu->catching_up = 1;
	cycles /= ppu->ppu_clock_divider;

	return_if_done();

	/* printf("here: %d,%d\n", ppu->scanline, ppu->scanline_cycle); */

	while (1) {
		/* This includes the dummy scanline */
		while (ppu->scanline < 240) {
			if (ppu->scanline_cycle == 0) {
				ppu->scanline_extended_sprite_count = 0;
				ppu->sprite_zero_present = 0;
				ppu->secondary_oam_index = 0;
				ppu->scanline_sprite_count = 0;
				ppu->wrote_2006 = 0;
				ppu->sprite_overflow_flag = 0;
				memset(ppu->secondary_oam + 32, 0xff,
				       sizeof(ppu->secondary_oam) - 32);
			}

			if (!RENDERING_ENABLED()) {
				do_disabled_scanline(ppu, cycles);
				if (ppu->cycles >= cycles)
					goto end;
			} else if (ppu->use_scanline_renderer &&
				   ppu->scanline >= 0 &&
				   ppu->scanline_cycle == 0 &&
				   ((cycles - ppu->cycles) >= 341)) {
				do_whole_scanline(ppu);
			} else if (do_partial_scanline(ppu, cycles)) {
				if (ppu->cycles >= cycles)
					goto end;
			}

			ppu->scanline++;
			ppu->scanline_cycle = 0;

			if (ppu->cycles >= cycles)
				goto end;
		}

		/* idle scanline */
		while ((ppu->scanline >= 240) &&
		       (ppu->scanline < 240 + ppu->post_render_scanlines)) {
			int needed;
			int left_in_line;

			if ((ppu->scanline == 240) && (ppu->scanline_cycle == 0)) {
				ppu->rendering = 0;
				ppu->sprite_overflow_flag = 0;
				update_address_bus_a12(ppu->scroll_address & 0x3fff);
			}

			needed = cycles - ppu->cycles;
			left_in_line = 341;
			left_in_line -= ppu->scanline_cycle;

			if (left_in_line < needed)
				needed = left_in_line;

			ppu->cycles += needed;
			ppu->scanline_cycle += needed;
			ppu->scanline += ppu->scanline_cycle / 341;
			ppu->scanline_cycle %= 341;

			if ((ppu->overclock_mode == OVERCLOCK_MODE_POST_RENDER) &&
			    (ppu->scanline == 240 + ppu->post_render_scanlines) &&
			    (ppu->scanline_cycle == 0)) {
				ppu->overclock_start_timestamp =
				    ppu->cycles * ppu->ppu_clock_divider;
				ppu->overclock_end_timestamp = ppu->overclock_start_timestamp + (ppu->overclock_scanlines * 341 * ppu->ppu_clock_divider);
				ppu->overclocking = 1;
				ppu->scanline--;
				ppu->scanline_cycle = 340;
				goto end;
			}

			if (ppu->cycles >= cycles)
				goto end;
		}

		/* first vblank cycle */
		if (ppu->scanline == 240 + ppu->post_render_scanlines) {
			if (ppu->scanline_cycle == 0) {
				add_cycle();
				if (ppu->cycles >= cycles)
					goto end;
			}

			if (ppu->scanline_cycle == 1) {
				ppu->status_reg |= STATUS_REG_VBLANK_FLAG;
				add_cycle();
				if (ppu->cycles >= cycles)
					goto end;
			}
		}

		/* Rest of frame */
		if (ppu->scanline >= 240 + ppu->post_render_scanlines) {
			int needed;
			int left_in_frame;

			needed = cycles - ppu->cycles;

			left_in_frame = (ppu->vblank_scanlines -
					 (ppu->scanline -
					  (240 + ppu->post_render_scanlines))) * 341;
			left_in_frame -= ppu->scanline_cycle;

			if (left_in_frame < needed)
				needed = left_in_frame;

			ppu->cycles += needed;
			ppu->scanline_cycle += needed;
			ppu->scanline += ppu->scanline_cycle / 341;
			ppu->scanline_cycle %= 341;

			if (ppu->scanline == 240 + ppu->post_render_scanlines +
			    ppu->vblank_scanlines) {
				if (ppu->overclock_mode == OVERCLOCK_MODE_VBLANK) {
					ppu->overclock_start_timestamp =
					    ppu->frame_cycles *
					    ppu->ppu_clock_divider;
					ppu->overclock_end_timestamp =
					    ppu->overclock_start_timestamp +
					    (ppu->overclock_scanlines * 341 *
					     ppu->ppu_clock_divider);
					ppu->overclocking = 1;
					ppu->scanline--;
					ppu->scanline_cycle = 340;
					goto end;
				}

				ppu->scanline = -1;
				if (RENDERING_ENABLED())
					ppu->rendering = 1;

				if (ppu->ppu_type == PPU_TYPE_RP2C02)
					ppu->odd_frame ^= 1;

				ppu->first_frame_flag = 0;
				ppu->vblank_timestamp = 341 *
					(241 + ppu->post_render_scanlines) + 1;
				if (ppu->odd_frame && RENDERING_ENABLED())
					ppu->vblank_timestamp--;
				ppu->vblank_timestamp *=
					ppu->ppu_clock_divider;
				ppu->vblank_timestamp += ppu->cycles *
					ppu->ppu_clock_divider;

				/* if (ppu->a12_timer_enabled) */
				/* 	a12_timer_run(ppu->emu->a12_timer, ppu->cycles); */
			}

			if (ppu->cycles >= cycles)
				goto end;
		}


	}

end:
	if (ppu->a12_timer_enabled)
		a12_timer_run(ppu->emu->a12_timer, ppu->cycles);

	ppu->catching_up = 0;

	return ppu->cycles * ppu->ppu_clock_divider;
}

/* register handlers */

/* register $2000 */
CPU_WRITE_HANDLER(ppu_ctrl_reg_write_handler)
{
	uint8_t old_reg;
	struct ppu_state *ppu = emu->ppu;
	int ppu_status;

	if (ppu->first_frame_flag)
		return;

	ppu_run(ppu, cycles);

	ppu_status = ppu->status_reg;
	ppu->scroll_address_latch = (ppu->scroll_address_latch &
				     (SCROLL_X | SCROLL_Y | SCROLL_Y_FINE));

	old_reg = ppu->ctrl_reg & 0x80;
	ppu->scroll_address_latch |= (value & 0x3) << 10;
	ppu->io_latch = value;
	ppu->io_latch_decay = ppu->decay_counter_start;
	ppu->ctrl_reg = ppu->io_latch;

	if (ppu->cycles * ppu->ppu_clock_divider <= ppu->vblank_timestamp) {
		if (!(old_reg & CTRL_REG_NMI_ENABLE) &&
		    (value & CTRL_REG_NMI_ENABLE)) {
			cpu_interrupt_schedule(ppu->emu->cpu, IRQ_NMI,
					       ppu->vblank_timestamp);
		} else if ((old_reg & CTRL_REG_NMI_ENABLE) &&
			   !(value & CTRL_REG_NMI_ENABLE)) {
			cpu_interrupt_cancel(ppu->emu->cpu, IRQ_NMI);
		}
	}

	/* Technically the VBLANK flag gets cleared one cycle
	   later than the others, but when using the preferred
	   ppu-cpu alignment it appears to get cleared at the
	   same time.
	*/
	if (ppu->scanline == -1 && ppu->scanline_cycle == 2)
		ppu_status = 0;

	if (ppu_status & STATUS_REG_VBLANK_FLAG) {
		if (!(old_reg & CTRL_REG_NMI_ENABLE) &&
		    (value & CTRL_REG_NMI_ENABLE)) {
			cpu_interrupt_schedule(ppu->emu->cpu, IRQ_NMI_IMMEDIATE,
					       cycles);
		} else if (ppu->scanline == (240 + ppu->post_render_scanlines) &&
			   ppu->scanline_cycle > 0 && ppu->scanline_cycle <= 4) {
			/* When using preferred alignment, disabling NMI on the
			   cycle VBLANK is set or either of the next two
			   cycles causes NMI to not happen.
			*/
			cpu_interrupt_cancel(ppu->emu->cpu, IRQ_NMI);
		}
	}
}

/* $2001 */
CPU_WRITE_HANDLER(ppu_mask_reg_write_handler)
{
	struct ppu_state *ppu = emu->ppu;
	int old_rendering_state, new_rendering_state;
	if (ppu->first_frame_flag)
		return;

	ppu_run(ppu, cycles + ppu->ppu_clock_divider);

	old_rendering_state = RENDERING_ENABLED();

	if (!(value & MASK_REG_BG_NO_CLIP)) {
		ppu->bg_left8_mask = 0x00;
	} else {
		ppu->bg_left8_mask = 0xff;
	}

	if (!(value & MASK_REG_BG_ENABLED)) {
		ppu->bg_all_mask = 0x00;
		ppu->bg_left8_mask = 0x00;
	} else {
		ppu->bg_all_mask = 0xff;
	}

	if (!(value & MASK_REG_SPRITE_NO_CLIP)) {
		ppu->sprite_left8_mask = 0x00;
	} else {
		ppu->sprite_left8_mask = 0xff;
	}

	if (!(value & MASK_REG_SPRITES_ENABLED)) {
		ppu->sprite_all_mask = 0x00;
		ppu->sprite_left8_mask = 0x00;
	} else {
		ppu->sprite_all_mask = 0xff;
	}

	if (value & MASK_REG_GRAYSCALE)
		ppu->palette_mask = 0x30;
	else
		ppu->palette_mask = 0xff;

	if (ppu->scanline_cycle >= 9) {
		ppu->bg_mask = ppu->bg_all_mask;
		ppu->sprite_mask = ppu->sprite_all_mask;
	} else {
		ppu->bg_mask = ppu->bg_left8_mask;
		ppu->sprite_mask = ppu->sprite_left8_mask;
	}

	ppu->emphasis  = value & 0x80;
	if (ppu->ppu_type == PPU_TYPE_RP2C07) {
		ppu->emphasis |= (value & 0x40) >> 1;
		ppu->emphasis |= (value & 0x20) << 1;
	} else {
		ppu->emphasis |= (value & 0x40);
		ppu->emphasis |= (value & 0x20);
	}
	ppu->emphasis <<= 1;
	ppu->io_latch = value;
	ppu->io_latch_decay = ppu->decay_counter_start;

	ppu->mask_reg = ppu->io_latch;

	ppu->rendering = 0;
	new_rendering_state = RENDERING_ENABLED();
	if (new_rendering_state && ppu->scanline < 240) {
		ppu->rendering = 1;
	}

	if (ppu->odd_frame && (ppu->scanline == -1) &&
	    (new_rendering_state != old_rendering_state)) {
		if (new_rendering_state)
			ppu->vblank_timestamp -= ppu->ppu_clock_divider;
		else
			ppu->vblank_timestamp += ppu->ppu_clock_divider;

		if (ppu->ctrl_reg & CTRL_REG_NMI_ENABLE) {
			cpu_interrupt_cancel(ppu->emu->cpu, IRQ_NMI);
			cpu_interrupt_schedule(ppu->emu->cpu, IRQ_NMI,
					       ppu->vblank_timestamp);
		}
	}

	if (old_rendering_state && !new_rendering_state) {
		update_address_bus_a12(ppu->scroll_address & 0x3fff);
	}
}

/* $2002 */
static CPU_READ_HANDLER(read_status_reg)
{
	struct ppu_state *ppu = emu->ppu;
	uint8_t status;

	ppu_run(ppu, cycles);
	status = ppu->status_reg;

	if (ppu->scanline == (240 + ppu->post_render_scanlines) &&
	    ppu->scanline_cycle && ppu->scanline_cycle <= 4) {
		switch(ppu->scanline_cycle) {
		case 2:
			status &= ~STATUS_REG_VBLANK_FLAG;
		case 3:
		case 4:
			cpu_interrupt_cancel(emu->cpu, IRQ_NMI);
			break;
		}
	}

	ppu->status_reg &= ~STATUS_REG_VBLANK_FLAG;
	if (ppu->vs_ppu_magic)
		ppu->io_latch = status | ppu->vs_ppu_magic;
	else
		ppu->io_latch = (ppu->io_latch & 0x1f) | status;

	ppu->scroll_address_toggle = 0;

	return ppu->io_latch;
}

/* $2003 */
CPU_WRITE_HANDLER(ppu_oam_addr_reg_write_handler)
{
	struct ppu_state *ppu = emu->ppu;
	ppu_run(ppu, cycles);

	ppu->oam_addr_reg = value;
	ppu->io_latch = value;
	ppu->io_latch_decay = ppu->decay_counter_start;
}

/* $2004 */
static CPU_READ_HANDLER(read_oam_data_reg)
{
	struct ppu_state *ppu = emu->ppu;
	ppu_run(ppu, cycles);

	if (!(ppu->mask_reg & (MASK_REG_BG_ENABLED | MASK_REG_SPRITES_ENABLED))
	    || ppu->scanline >= 240) {
		ppu->io_latch = ppu->oam[ppu->oam_addr_reg];
	} else {
		ppu->io_latch = ppu->oam_latch;
	}

	return ppu->io_latch;
}

CPU_WRITE_HANDLER(ppu_oam_data_reg_write_handler)
{
	uint8_t mask;
	struct ppu_state *ppu = emu->ppu;
	ppu_run(ppu, cycles);

	mask = 0xff;
	if (!(ppu->mask_reg & (MASK_REG_BG_ENABLED | MASK_REG_SPRITES_ENABLED))
	    || ppu->scanline >= 240) {
		if ((ppu->oam_addr_reg & 0x03) == 0x02)
			mask = 0xe3;
		/* } else { */
		/*      value = 0xff; */
	}

	ppu->oam[ppu->oam_addr_reg] = value & mask;
	ppu->io_latch = value & 0x7f;
	ppu->io_latch_decay = ppu->decay_counter_start;
	ppu->oam_addr_reg++;
}

/* $2005 */
static CPU_WRITE_HANDLER(write_scroll_reg)
{
	struct ppu_state *ppu = emu->ppu;
	if (ppu->first_frame_flag)
		return;

	ppu_run(ppu, cycles);

	ppu->io_latch = value;
	ppu->io_latch_decay = ppu->decay_counter_start;

	if (ppu->scroll_address_toggle ^= 1) {
		/* Set new x scroll and fine x scroll */
		ppu->scroll_address_latch &= (SCROLL_NAMETABLE |
					      SCROLL_Y_FINE | SCROLL_Y);

		/* x scroll */
		ppu->scroll_address_latch |= ((value >> 3) & 0x1f);

		/* fine x scroll */
		ppu->fine_x_scroll = value & 0x7;
	} else {
		ppu->scroll_address_latch &= (SCROLL_NAMETABLE | SCROLL_X);

		/* y scroll */
		ppu->scroll_address_latch |= (value & 0xf8) << 2;

		/* Fine y scroll */
		ppu->scroll_address_latch |= (value & 0x07) << 12;
	}
}

/* $2006 */
static CPU_WRITE_HANDLER(write_address_reg)
{
	struct ppu_state *ppu = emu->ppu;
	if (ppu->first_frame_flag)
		return;

	ppu_run(ppu, cycles);

	ppu->io_latch = value;
	ppu->io_latch_decay = ppu->decay_counter_start;

	if (ppu->scroll_address_toggle ^= 1) {
		ppu->scroll_address_latch =
		    (ppu->scroll_address_latch & 0xff) | (value & 0x3f) << 8;
	} else {
		ppu->wrote_2006 = ppu->scanline_cycle;
		ppu->scroll_address_latch =
		    (ppu->scroll_address_latch & 0x7f00) | value;
		ppu->scroll_address = ppu->scroll_address_latch;


		if (!ppu->rendering) {
			update_address_bus_a12(ppu->scroll_address & 0x3fff);
		}
	}

}

static void increment_scroll_address(struct ppu_state *ppu)
{
	if (ppu->rendering) {
		increment_y_scroll(ppu);
		increment_x_scroll(ppu);
	} else {
		ppu->scroll_address =
		    (ppu->scroll_address +
		     (((ppu->
			ctrl_reg & CTRL_REG_ADDR_INCR) ? 32 : 1))) & 0x7fff;
	}
}

/* $2007 */
static CPU_READ_HANDLER(read_data_reg)
{
	struct ppu_state *ppu = emu->ppu;
	ppu_run(ppu, cycles);

	addr = ppu->scroll_address & 0x3fff;
	increment_scroll_address(ppu);
	if (!ppu->rendering)
		update_address_bus_a12(ppu->scroll_address & 0x3fff);

	if (addr >= PPU_PALETTE_OFFSET) {
		ppu->io_buffer = read_nmt(ppu, addr);
		addr &= 0x1f;
		ppu->io_latch &= 0xc0;
		ppu->io_latch |= ppu->palette[addr] & ppu->palette_mask;
	} else if (addr >= PPU_NAMETABLE_OFFSET) {
		ppu->io_latch = ppu->io_buffer;
		ppu->io_buffer = read_nmt(ppu, addr);
	} else if (ppu->rendering) {
		ppu->io_latch = ppu->io_buffer;
		if (ppu->scanline < 240 && ppu->scanline_cycle >= 257 &&
		    ppu->scanline_cycle < 321) {
			ppu->io_buffer = read_spr(ppu, addr);
		} else {
			ppu->io_buffer = read_bg(ppu, addr);
		}
	} else {
		ppu->io_latch = ppu->io_buffer;
		ppu->io_buffer = read_spr(ppu, addr);
	}

	return ppu->io_latch;
}

uint8_t ppu_peek(struct ppu_state *ppu, int addr)
{
	int cycles;
	uint8_t value;

	cycles = cpu_get_cycles(ppu->emu->cpu);

	ppu_run(ppu, cycles);

	if (addr >= PPU_PALETTE_OFFSET) {
		value = ppu->palette[addr & 0x1f] & ppu->palette_mask;
	} else if (addr >= PPU_NAMETABLE_OFFSET) {
		value = read_nmt(ppu, addr);
	} else if (ppu->rendering) {
		if (ppu->scanline < 240 && ppu->scanline_cycle >= 257 &&
		    ppu->scanline_cycle < 321) {
			value = read_spr(ppu, addr);
		} else {
			value = read_bg(ppu, addr);
		}
	} else {
		value = read_spr(ppu, addr);
	}

	return value;
}

void ppu_poke(struct ppu_state *ppu, int address, uint8_t value)
{
	int page;
	int offset;
	int cycles;

	cycles = cpu_get_cycles(ppu->emu->cpu);

	ppu_run(ppu, cycles);

	page = address >> PPU_PAGE_SHIFT;
	offset = address & PPU_PAGE_MASK;

	if (address >= PPU_PALETTE_OFFSET) {
		address &= 0x1f;
		ppu->palette[address] = value & 0x3f;

		if (!(address & 0x03))
			ppu->palette[address ^ 0x10] = value & 0x3f;

		if (ppu->do_palette_lookup) {
			ppu->translated_palette[address] =
				ppu->palette_lookup[ppu->palette[address]];

			if (!(address & 0x03)) {
				ppu->translated_palette[address ^ 0x10] =
					ppu->palette_lookup[
						ppu->palette[address]];
			}
		}
		return;
	} else if (address >= PPU_NAMETABLE_OFFSET) {
		if (ppu->write_bg_pagemap[page])
			ppu->write_bg_pagemap[page][offset] = value;
	} else if (ppu->rendering && ppu->scanline_cycle >= 257 &&
		   ppu->scanline_cycle < 321) {
		if (ppu->write_spr_pagemap[page])
			ppu->write_spr_pagemap[page][offset] = value;
	} else {
		if (ppu->write_bg_pagemap[page])
			ppu->write_bg_pagemap[page][offset] = value;
	}
}

static CPU_WRITE_HANDLER(write_data_reg)
{
	uint16_t address;
	int page;
	int offset;
	struct ppu_state *ppu = emu->ppu;

	ppu_run(ppu, cycles);

	address = ppu->scroll_address & 0x3fff;
	increment_scroll_address(ppu);
	if (!ppu->rendering)
		update_address_bus_a12(ppu->scroll_address & 0x3fff);

	ppu->io_latch = value;
	ppu->io_latch_decay = ppu->decay_counter_start;

	page = address >> PPU_PAGE_SHIFT;
	offset = address & PPU_PAGE_MASK;

	if (ppu->rendering)
		return;

	if (address >= PPU_PALETTE_OFFSET) {
		address &= 0x1f;
		ppu->palette[address] = value & 0x3f;

		if (!(address & 0x03))
			ppu->palette[address ^ 0x10] = value & 0x3f;

		if (ppu->do_palette_lookup) {
			ppu->translated_palette[address] =
				ppu->palette_lookup[ppu->palette[address]];

			if (!(address & 0x03)) {
				ppu->translated_palette[address ^ 0x10] =
					ppu->palette_lookup[
						ppu->palette[address]];
			}
		}
		return;
	} else if (address >= PPU_NAMETABLE_OFFSET) {
		if (ppu->write_bg_pagemap[page])
			ppu->write_bg_pagemap[page][offset] = value;
	} else if (ppu->rendering && ppu->scanline_cycle >= 257 &&
		   ppu->scanline_cycle < 321) {
		if (ppu->write_spr_pagemap[page])
			ppu->write_spr_pagemap[page][offset] = value;
	} else {
		if (ppu->write_bg_pagemap[page])
			ppu->write_bg_pagemap[page][offset] = value;
	}
}

/* $2xxx */
static CPU_READ_HANDLER(read_2xxx)
{
	struct ppu_state *ppu = emu->ppu;
	ppu_run(ppu, cycles);
	return ppu->io_latch;
}

static CPU_WRITE_HANDLER(write_2xxx)
{
	struct ppu_state *ppu = emu->ppu;
	ppu_run(ppu, cycles);
	ppu->io_latch = value;
	ppu->io_latch_decay = ppu->decay_counter_start;
}

uint32_t ppu_get_cycles(struct ppu_state *ppu, int *scanline, int *cycle,
			int *odd_frame, int *short_frame)
{
	*scanline = ppu->scanline;
	if (ppu->scanline == (240 + ppu->post_render_scanlines) +
	    ppu->vblank_scanlines) {
		*scanline = -1;
	}

	*cycle = ppu->scanline_cycle;
	*odd_frame = ppu->odd_frame;
	return ppu->cycles * ppu->ppu_clock_divider;
}

uint8_t ppu_get_register(struct ppu_state * ppu, int reg)
{
	uint8_t val = 0;

	switch (reg) {
	case 0:
		val = ppu->ctrl_reg;
		break;
	}

	return val;
}

void ppu_set_split_screen(struct ppu_state *ppu, int enabled, int right_side,
			  int startstop, uint32_t cycles)
{
	ppu_run(ppu, cycles);
	ppu->split_screen_enabled = enabled;
	if (right_side) {
		ppu->split_screen_start = startstop;
		ppu->split_screen_end = 0x1f;
	} else {
		ppu->split_screen_start = 0;
		ppu->split_screen_end = startstop;
	}
}

void ppu_set_split_screen_scroll(struct ppu_state *ppu, int value,
				 uint32_t cycles)
{
	ppu_run(ppu, cycles);
	ppu->split_screen_scroll = value;
}

void ppu_set_split_screen_bank(struct ppu_state *ppu, int value,
			       uint32_t cycles)
{
	ppu_run(ppu, cycles);
	ppu->split_screen_bank = value;
}

void ppu_set_sprite_limit(struct ppu_state *ppu, int enabled)
{
	if (enabled < 0)
		enabled = !ppu->no_sprite_limit;

	ppu->no_sprite_limit = enabled;
	osdprintf("Sprite limit %sabled",
		  ppu->no_sprite_limit ? "dis" : "en");
}

void ppu_set_sprite_hiding(struct ppu_state *ppu, int enabled)
{
	if (enabled < 0)
		enabled = !ppu->allow_sprite_hiding;

	ppu->allow_sprite_hiding = enabled;
}

void ppu_toggle_bg(struct ppu_state *ppu)
{
	ppu->hide_bg ^= 1;
	osdprintf("Background %sabled", ppu->hide_bg ? "dis" : "en");
}

void ppu_toggle_sprites(struct ppu_state *ppu)
{
	ppu->hide_sprites ^= 1;
	osdprintf("Sprites %sabled", ppu->hide_sprites ? "dis" : "en");
}

void ppu_set_scanline_renderer(struct ppu_state *ppu, int enabled)
{
	if (enabled < 0)
		enabled = !ppu->use_scanline_renderer;

	ppu->use_scanline_renderer = enabled;
	osdprintf("Scanline renderer %sabled",
		  ppu->use_scanline_renderer ? "en" : "dis");
}

int ppu_save_state(struct ppu_state *ppu, struct save_state *state)
{
	uint8_t *buf;
	size_t size;
	int rc;

	size = pack_state(ppu, ppu_state_items, NULL);

	buf = malloc(size);
	if (!buf)
		return -1;

	/* printf("save: scanline is now %d\n", ppu->scanline); */
	pack_state(ppu, ppu_state_items, buf);

	rc = save_state_add_chunk(state, "PPU ", buf, size);
	free(buf);

	if (rc < 0)
		return -1;

	rc = save_state_add_chunk(state,"OAM ", ppu->oam,
				  sizeof(ppu->oam));

	if (rc < 0)
		return -1;

	return 0;
}

int ppu_load_state(struct ppu_state *ppu, struct save_state *state)
{
	uint8_t *buf;
	size_t size;

	if (save_state_find_chunk(state, "PPU ", &buf, &size) < 0)
		return -1;

	unpack_state(ppu, ppu_state_items, buf);

	if (save_state_find_chunk(state, "OAM ", &buf, &size) < 0)
		return -1;

	if (size > sizeof(ppu->oam))
		return -1;

	memcpy(ppu->oam, buf, size);

	/* Ick :-P */
	if (ppu->scanline == 65535)
		ppu->scanline = -1;

	ppu->bg_all_mask = ppu->bg_left8_mask = 0;
	ppu->sprite_all_mask = ppu->sprite_left8_mask = 0;
	ppu->rendering = 0;
	ppu->palette_mask = 0xff;

	if (ppu->mask_reg & MASK_REG_SPRITES_ENABLED)
		ppu->sprite_all_mask = 0xff;

	if (ppu->mask_reg & MASK_REG_SPRITE_NO_CLIP)
		ppu->sprite_left8_mask = 0xff;

	if (ppu->mask_reg & MASK_REG_BG_ENABLED)
		ppu->bg_all_mask = 0xff;

	if (ppu->mask_reg & MASK_REG_BG_NO_CLIP)
		ppu->bg_left8_mask = 0xff;

	if (ppu->mask_reg & MASK_REG_GRAYSCALE)
		ppu->palette_mask = 0x30;

	ppu->emphasis = ppu->mask_reg & 0x80;
	if (ppu->ppu_type == PPU_TYPE_RP2C07) {
		ppu->emphasis |= (ppu->mask_reg & 0x40) >> 1;
		ppu->emphasis |= (ppu->mask_reg & 0x20) << 1;
	} else {
		ppu->emphasis |= (ppu->mask_reg & 0x40);
		ppu->emphasis |= (ppu->mask_reg & 0x20);
	}
	ppu->emphasis <<= 1;

	if (ppu->scanline_cycle < 9) {
		ppu->bg_mask = ppu->bg_left8_mask;
		ppu->sprite_mask = ppu->sprite_left8_mask;
	} else {
		ppu->bg_mask = ppu->bg_all_mask;
		ppu->sprite_mask = ppu->sprite_all_mask;
	}

	if (RENDERING_ENABLED() && (ppu->scanline < 240))
		ppu->rendering = 1;

	if (ppu->do_palette_lookup) {
		int i;

		for (i = 0; i < PALETTE_SIZE; i++) {
			ppu->translated_palette[i] =
				ppu->palette_lookup[ppu->palette[i]];
		}
	}

	ppu->visible_cycles = ppu->frame_cycles -
	                      (ppu->vblank_scanlines) * 341;
	cpu_set_frame_cycles(ppu->emu->cpu,
	                     ppu->visible_cycles * ppu->ppu_clock_divider,
			     ppu->frame_cycles * ppu->ppu_clock_divider);
	return 0;
}

int ppu_get_burst_phase(struct ppu_state *ppu)
{
	int phase;
	if (ppu->ppu_type == PPU_TYPE_RP2C02)
		phase = ppu->burst_phase;
	else
		phase = 0;

	return phase;
}

void ppu_end_overclock(struct ppu_state *ppu, int cycles)
{
	if (!ppu->overclocking)
		return;

	if (cycles < ppu->overclock_end_timestamp)
		return;

	ppu->overclocking = 0;
	ppu->scanline++;
	ppu->scanline_cycle = 0;
	cycles -= ppu->overclock_end_timestamp;
	cycles += ppu->cycles * ppu->ppu_clock_divider;
	ppu->overclock_mode = OVERCLOCK_MODE_NONE;
	ppu_run(ppu, cycles);
}

void ppu_set_overclock_mode(struct ppu_state *ppu, int mode, int scanlines)
{
	ppu->overclocking = 0;
	ppu->overclock_mode = mode;
	ppu->overclock_scanlines = scanlines;
}
