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

#include <libgen.h>

#include "emu.h"
#include "file_io.h"
#include "fds.h"
#include "crc32.h"
#include "sha1.h"

#define FDS_MAGIC "\x01*NINTENDO-HVC*"
#define FDS_MAGIC_LENGTH 15
#define FDS_MIN_PREGAP_LENGTH 26150
#define FDS_PREGAP_LENGTH 28296
#define FDS_MIN_GAP_LENGTH 480
#define FDS_GAP_LENGTH 968

static void fix_vc_bios(uint8_t *bios)
{

	if ((bios[0x239] == 0x42) && (bios[0x406] == 0x42) &&
	    (bios[0x73e] == 0x4c) && (bios[0x73f] == 0x43) &&
	    (bios[0x740] == 0xe7) && (bios[0x7a4] == 0x42) &&
	    (bios[0xef4] == 0x42)) {
		log_info("VC FDS bios detected; fixing\n");

		bios[0x239] = 0x85;
		bios[0x406] = 0x85;
		bios[0x73e] = 0xa2;
		bios[0x73f] = 0xb2;
		bios[0x740] = 0xca;
		bios[0x7a4] = 0x4c;
		bios[0xef4] = 0xa5;
	}
}

struct fds_manufacturer_code {
	uint8_t code;
	const char *name;
	const char *jpname;
};

static struct fds_manufacturer_code manufacturers[] = {
	{ 0x00, "<unlicensed>", "<非公認>" },
	{ 0x01, "Nintendo", "任天堂" },
	{ 0x08, "Capcom", "カプコン" },
	{ 0x0A, "Jaleco", "ジャレコ" },
	{ 0x18, "Hudson Soft", "ハドソン" },
	{ 0x49, "Irem", "アイレム" },
	{ 0x4A, "Gakken", "学習研究社" },
	{ 0x8B, "BulletProof Software (BPS)", "BPS" },
	{ 0x99, "Pack-In-Video", "パックインビデオ" },
	{ 0x9B, "Tecmo", "テクモ" },
	{ 0x9C, "Imagineer", "イマジニア" },
	{ 0xA2, "Scorpion Soft", "スコーピオンソフト" },
	{ 0xA4, "Konami", "コナミ" },
	{ 0xA6, "Kawada Co., Ltd.", "河田" },
	{ 0xA7, "Takara", "タカラ" },
	{ 0xA8, "Royal Industries", "ロイヤル工業" },
	{ 0xAC, "Toei Animation", "東映動画" },
	{ 0xAF, "Namco", "ナムコ" },
	{ 0xB1, "ASCII Corporation", "アスキー" },
	{ 0xB2, "Bandai", "バンダイ" },
	{ 0xB3, "Soft Pro Inc.", "ソフトプロ" },
	{ 0xB6, "HAL Laboratory", "HAL研究所" },
	{ 0xBB, "Sunsoft", "サンソフト" },
	{ 0xBC, "Toshiba EMI", "東芝EMI" },
	{ 0xC0, "Taito", "タイトー" },
	{ 0xC1, "Sunsoft / Ask Co., Ltd.", "サンソフト アスク講談社" },
	{ 0xC2, "Kemco", "ケムコ" },
	{ 0xC3, "Square", "スクウェア" },
	{ 0xC4, "Tokuma Shoten", "徳間書店" },
	{ 0xC5, "Data East", "データイースト" },
	{ 0xC6, "Tonkin House/Tokyo Shoseki", "トンキンハウス" },
	{ 0xC7, "East Cube", "イーストキューブ" },
	{ 0xCA, "Konami / Ultra / Palcom", "コナミ" },
	{ 0XCB, "NTVIC / VAP", "バップ" },
	{ 0xCC, "Use Co., Ltd.", "ユース" },
	{ 0xCE, "Pony Canyon / FCI", "ポニーキャニオン" },
	{ 0xD1, "Sofel", "ソフエル" },
	{ 0xD2, "Bothtec, Inc.", "ボーステック" },
	{ 0xDB, "Hiro Co., Ltd.", "ヒロ" },
	{ 0xE7, "Athena", "アテナ" },
	{ 0xEB, "Atlus", "アトラス " },
};

#define NUM_MANUFACTURERS (sizeof(manufacturers)/sizeof(manufacturers[0]))

const uint16_t fds_crc_table[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78,
};

static int fds_calculate_fds_crc(uint8_t *data, size_t size, int add_start_mark);

static int block_list_add_block(struct fds_block_list *block_list, int type, off_t offset,
				size_t size, int crc, int calculated_crc)
{
	int index;

	if (block_list->available_entries == 0) {
		struct fds_block_list_entry *tmp;

		tmp = realloc(block_list->entries,
			      (block_list->total_entries + 1) *
			      sizeof(*tmp));

		if (!tmp)
			return -1;

		block_list->total_entries++;
		block_list->available_entries++;
		block_list->entries = tmp;
	}

	index = block_list->total_entries -
		block_list->available_entries;

	block_list->entries[index].type = type;
	block_list->entries[index].offset = offset;
	block_list->entries[index].size = size;
	block_list->entries[index].crc = crc;
	block_list->entries[index].calculated_crc = calculated_crc;
	block_list->available_entries--;

	return 0;
}

struct fds_block_list *fds_block_list_new(void)
{
	struct fds_block_list *list;

	list = malloc(sizeof(*list));
	if (list) {
		memset(list, 0, sizeof(*list));
		list->entries = NULL;
	}

	return list;
}

void fds_block_list_free(struct fds_block_list *list)
{
	if (!list)
		return;

	if (list->entries)
		free(list->entries);

	free(list);
}

void fds_block_list_print(struct fds_block_list *list)
{
	int i;

	if (!list)
		return;

	for (i = 0; i < list->total_entries; i++) {
		printf("%d: size=%zu, offset=%jd crc=%04x calc_crc=%04x\n",
		       list->entries[i].type,
		       list->entries[i].size,
		       (intmax_t)list->entries[i].offset, list->entries[i].crc,
		       list->entries[i].calculated_crc);
	}
}

static int fds_validate_image_real(struct rom *rom, struct fds_block_list *block_list, int do_crc, int skip_crcs)
{
	uint8_t *data;
	size_t size;
	int side_count, side;
	off_t offset;

	data = rom->buffer + 16;
	size = rom->buffer_size - 16;

	side_count = size / rom->disk_side_size;

	/* printf("skipping crcs: %d\n", skip_crcs); */

	offset = 0;
	for (side = 0; side < side_count; side++) {
		int prev_block_type = 0;
		size_t data_size = 0;
//		int expected_file_count = -1;
		int file_count = 0;
		off_t side_max_offset;
		sha1nfo sha1;

		side_max_offset = (side + 1) * rom->disk_side_size;

		while (offset < side_max_offset) {
			off_t gap_offset;
			int start_mark_present;
			size_t block_size;
			int calculated_crc;
			int crc;
			int type;

			gap_offset = offset;
			start_mark_present = 0;
			type = 5;
			crc = 0;
			calculated_crc = 0;

			while (!data[offset] && (offset < side_max_offset))
				offset++;

			if (offset == side_max_offset)
				break; /* FIXME side is done */

			if (data[offset] == 0x80) {
				if (offset + 1 < side_max_offset) {
					start_mark_present = 1;
					offset++;
				}
			}

			switch (data[offset]) {
			case 1:
				if ((prev_block_type == 0) &&
				    ((side_max_offset - offset) >= FDS_MAGIC_LENGTH) &&
				    (!memcmp(data + offset, FDS_MAGIC,
					     FDS_MAGIC_LENGTH))) {
					type = 1;
					block_size = 56;
					prev_block_type = 1;
				}
				break;
			case 2:
				if (prev_block_type == 1) {
					type = 2;
					block_size = 2;
//					expected_file_count = data[offset + 1];
					prev_block_type = 2;
					if ((side == 0) && (offset == 58)) {
						/* QD format images are exactly
						   64K per side, whereas raw and
						   FDS images are 65500 bytes per
						   side.

						   Block 2 is only at offset 58 if
						   this is a QD image; it's at 56
						   for FDS images and is at a much
						   higher offset for raw images.
						*/
						side_max_offset = 65536;
					}
				}
				break;
			case 3:
				if (((prev_block_type == 2) && (file_count == 0)) ||
				    (prev_block_type == 4)) {
					type = 3;
					block_size = 16;
					data_size = data[offset + 13] | (data[offset + 14] << 8);
					prev_block_type = 3;
				}
				break;
			case 4:
				if (prev_block_type == 3) {
					file_count++;
					type = 4;
					block_size = data_size + 1;
					prev_block_type = 4;
					block_list->entries[block_list->total_entries - 1].crc32 =
						crc32buf(&data[offset + 1], block_size - 1, NULL /*&crc32_tmp*/);
					sha1_init(&sha1);
					sha1_write(&sha1,
						   (const char *)&data[offset + 1], block_size - 1);
					memcpy(block_list->entries[block_list->total_entries - 1].sha1,
					       sha1_result(&sha1), 20);
				}
				break;
			}

			if ((type == 5) || (block_size + offset > side_max_offset) ||
			    ((start_mark_present || !skip_crcs) && (offset + block_size + 2 > side_max_offset))) {
				offset = gap_offset;
				block_size = side_max_offset - offset;
				type = 5;
				start_mark_present = 0;
			}

			if (start_mark_present || !skip_crcs) {
				crc = data[offset + block_size];
				crc |= data[offset + block_size + 1] << 8;
				if (type == 4)
					block_list->entries[block_list->total_entries - 1].data_crc = crc;
			}

			if (type != 5 && do_crc) {
				calculated_crc =
					fds_calculate_fds_crc(data + offset, block_size, 1);
				if (type == 4)
					block_list->entries[block_list->total_entries - 1].calculated_data_crc = calculated_crc;
			}

			if (block_list_add_block(block_list, type, offset,
						 block_size, crc, calculated_crc) < 0) {
				return -1;
			}

			offset += block_size;
			if ((type == 5) && (side_max_offset == 65536)) {
				block_list->entries[block_list->total_entries - 1].size -= 36;
			}

			if (start_mark_present || !skip_crcs)
				offset += 2;
		}
	}

	return 0;
}

/* Supported formats for FDS images

   - fwNES FDS format (with or without 16-byte fwNES header)
     This is the most common format; no gaps, no block start mark and no
     crcs. Sides are 65500 bytes each.

   - "Raw"
     This has all gaps of standard length ("standard" being what the
     BIOS/hardware would expect to have present when reading or would
     create when writing), block start marks are present as are
     CRCs. Each side is at least 65500 bytes in size, but may be larger
     if required (all sides concatenated in the same file must be the
     same size however).  This is the format used internally by cxNES.

   - QD format (Nintendo's own format used by its emulators)
     Similar to FDS format but with no 16-byte header. CRCs are
     present but not gaps or block start marks. Sides are 65536 bytes
     each; this is odd as the QuickDisks Nintendo used only held 65500
     bytes.  Note that at least one official Nintendo disk image in
     this format doesn't have correct CRCs; all CRC bytes are set to
     0x00.  The game does nothing special with the CRC data (i.e., no
     tricky copy protection or anything), so this is likely due to an
     oversight when preparing the QD image.

     Currently, cxnes does not support multiple files per disk image
     (one side per file), All disk sides required for a game must be
     concatenated in the correct order in one file.  Any of the three
     above formats are acceptable (all sides must be in the same
     format however, and with only one fwNES header).

     cxNES converts all formats to headered FDS in memory for
     patching, then back to raw afterward when actually booting the
     image.  This is for compatibility with the majority of FDS
     patches which assume a headered FDS image.  If you have a patch
     that requires a different format, you'll probably need to apply
     it directly to the image using a separate application.

     cxNES saves writes to the disk image in IPS format (with a .sav
     extension however); this allows the original disk image to remain
     unmodified.  The patch includes all bytes written to disk, not
     just the differences between the unmodified image and the
     modified one.  This makes it possible for the patch to be applied
     to any disk image of the same game, even if that image already
     has modified save data on it.

     The save file assumes the image is in headered FDS format
     (currently the most common format) so that it could be applied
     directly as an IPS patch to the most common type of disk image.
     This is true even for disk images in another supported format;
     cxNES can still correctly apply these save patches to a raw or QD
     format image.
*/

int fds_validate_image(struct rom *rom, struct fds_block_list **block_listp, int do_crc)
{
	struct fds_block_list *with_crcs, *without_crcs;
	int with_rc, without_rc;
	int with_block_count, without_block_count;
	int final_rc;

	final_rc = 0;

	*block_listp = NULL;

	without_crcs = fds_block_list_new();
	with_crcs = fds_block_list_new();

	without_rc = fds_validate_image_real(rom, without_crcs, do_crc, 1);
	with_rc = fds_validate_image_real(rom, with_crcs, do_crc, 0);

	without_block_count = without_crcs->total_entries;
	with_block_count = with_crcs->total_entries;

	if (((without_rc != 0) && (with_rc == 0)) ||
	    (with_block_count >= without_block_count)) {
		*block_listp = with_crcs;
		with_crcs = NULL;
	} else if (((with_rc != 0) && (without_rc == 0)) ||
		   (without_block_count > with_block_count)) {
		*block_listp = without_crcs;
		without_crcs = NULL;
	} else if ((without_rc != 0) && (with_rc != 0)) {
		final_rc = -1;
	}

	if (with_crcs)
		fds_block_list_free(with_crcs);

	if (without_crcs)
		fds_block_list_free(without_crcs);

	return final_rc;
}

/*   BIOS images

     The FDS BIOS is required for FDS support.  While cxNES does do
     some high-level emulation of disk I/O, it does so by intercepting
     reads of key locations in the BIOS ROM.  If it can reasonably do
     the requested operation in high-level code, it does so and tweaks
     system state to make it appear as if the underlying machine code
     finished normally.  If not, it continues executing the machine
     code as-is and the operation will be completed using the
     low-level I/O emulation.

     No attempt is made to emulate any of the other support routines
     in the BIOS, and most games require these routines to function.
     Even the routines that do have high-level emulation are only
     emulated in part; the portions that aren't I/O bottlenecks are
     always handled by the original BIOS machine code.  This is done
     to keep the code simple and the high-level emulation as
     compatible as possible.

     There are supposedly three versions of the BIOS, although I have
     only encoutered two:

     1. The original version of the BIOS which doesn't have the easter egg.
        This is the one I have not personally seen, and so can't guarantee
	it will work with cxNES.  As long as none of the code cxNES expects
	to intercept execution of has changed, this version should still
	work (if it really does exist).

     2. The second revision of the BIOS which does include the easter egg.
        This is the most common one, and is generally the one that emulators
	expect you to use.  This works with cxNES.

     3. The Sharp Twin Famicom version; the code for this version is exactly
        the same as that of #2, but the graphics data has changed so that
	the title screen logo says "Famicom" instead of "Nintendo".  This
	also works with cxNES.

     There is also a variant of #2 that was patched by Nintendo to
     work with their NES emulator(s) (used in Animal Crossing for
     Gamecube, Virtual Console titles, etc).  The changes here replace
     several opcodes in disk I/O routines with opcodes that would
     cause the CPU to hang on real hardware, but their emulator traps
     these opcodes and determines what high-level disk I/O operation
     to do (probably) based on the current value of the program
     counter.  There is also one timed delay that is patched out
     because their high-level disk I/O emulation eliminates the need
     for it.

     This BIOS will *not* work with any other existing emulator
     without patching it first.  cxNES can detect this version of the
     BIOS and will patch it at runtime so that it is identical to the
     standard FDS BIOS.  (There are actually only 7 bytes that need to
     be changed, so it isn't a huge patch).

     In terms of supported BIOS image formats, the preferred format is
     an 8KiB binary blob of the BIOS.  By default cxNES will look for this
     in ~/.cxnes/disksys.rom (on Linux) or disksys.rom in the folder where
     you extracted cxNES (on Windows), but you can override this in the
     configuration.

     Alternatively, a version of the BIOS that has been packaged as a
     regular ROM (in, say, iNES format) will also work provided cxNES
     supports that ROM format and the mapper/board type is NROM
     (mapper 0 in iNES).  cxNES assumes that the BIOS is located in the
     last 8 KiB of PRG data in the ROM, but since the BIOS needs to be
     mapped to the last 8 KiB of address space to function this makes
     sense.  You can store this as disksys.rom (as for the binary blob
     version) or specify the location in the config file.
 */

int fds_load_bios(struct emu *emu, struct rom *rom)
{
	uint8_t *tmp;
	char *bios_file;
	size_t new_size, old_size;
	size_t bios_size;
	int rc = 0;

	/* Look for the BIOS at the location which the
	   user configured, then try the cxnes data
	   directory.  If that doesn't work, try the
	   same directory as the rom, then try the
	   configured rom path, if any.
	*/
	bios_file = config_get_fds_bios(emu->config);
	if (!bios_file) {
		char *romfile_path, *buffer;
		int length;

		buffer = NULL;
		romfile_path = strdup(dirname(rom->filename));
		if (romfile_path) {
			length = strlen(romfile_path) + 1 +
				 strlen(DEFAULT_FDS_BIOS) + 1;
			buffer = malloc(length);

			if (buffer) {
				snprintf(buffer, length, "%s%s%s", romfile_path,
				         PATHSEP, DEFAULT_FDS_BIOS);
			}

			free(romfile_path);
		}

		if (!buffer || !check_file_exists(buffer)) {
			if (emu->config->rom_path) {
				char *t;
				length = strlen(emu->config->rom_path) + 1 +
					 strlen(DEFAULT_FDS_BIOS) + 1;
				t = realloc(buffer, length);
				if (!t) {
					if (buffer)
						free(buffer);
					buffer = NULL;
				} else {
					buffer = t;
					snprintf(buffer, length, "%s%s%s",
					         emu->config->rom_path,
						 PATHSEP, DEFAULT_FDS_BIOS);
				}
			}
		}

		if (!buffer || !check_file_exists(buffer)) {
			err_message("Unable to locate FDS BIOS\n");
			free(buffer);
			return 1;
		}

		bios_file = buffer;
	}

	old_size = rom->buffer_size;
	new_size = ((rom->buffer_size - 16) / SIZE_8K) + 1;

	if ((rom->buffer_size - 16) % SIZE_8K)
		new_size += 1;

	new_size *= SIZE_8K;
	new_size += 16;

	tmp = realloc(rom->buffer, new_size);
	if (!tmp) {
		free(bios_file);
		return -1;
	}

	rom->buffer = tmp;
	rom->buffer_size = new_size;
	rom->info.total_prg_size = new_size - 16;

	memset(rom->buffer + old_size, 0, new_size - old_size);

	bios_size = get_file_size(bios_file);

	if (bios_size != 8192) {
		/* Try loading the supplied file name as a regular rom,
		   then copy the last 8K of PRG as the BIOS. There is a dump
		   of the FDS BIOS in iNES format, and of course there is
		   always going to be someone who tries using that version
		   regardless of what documentation says.
		*/
		struct rom *fds_rom;
		fds_rom = rom_load_file(emu, bios_file);
		if (!fds_rom || rom->info.total_prg_size < SIZE_8K) {
			err_message("Invalid FDS BIOS\n");
			rc = 1;
		} else {
			off_t offset = fds_rom->info.total_prg_size - SIZE_8K;
			memcpy(rom->buffer + new_size - SIZE_8K,
			       fds_rom->buffer + fds_rom->offset + offset,
			       SIZE_8K);

			rom_free(fds_rom);
		}
	} else if (!rc && readfile(bios_file, rom->buffer + new_size - SIZE_8K,
				   SIZE_8K)) {
		rc = 1;
	}

	free(bios_file);

	fix_vc_bios(rom->buffer + new_size - SIZE_8K);

	return rc;
	
}

/*
  Returns -1 if this is not an FDS image, 0 if successful, and 1 if we failed for
  some other reason.
 */
int fds_load(struct emu *emu, struct rom *rom)
{
	uint8_t *tmp;
	size_t size;
	size_t disk_data_size;
	off_t offset;
	int i;

	size = rom->buffer_size;

	for (i = 0; i < size; i++) {
		if (rom->buffer[i])
			break;
	}
	tmp = rom->buffer + i;
	if (*tmp == 0x80)
		tmp++;

	if (memcmp(tmp, FDS_MAGIC, FDS_MAGIC_LENGTH) == 0) {
		offset = 0;
	} else if ((size >= 16 + FDS_MAGIC_LENGTH) &&
		 !memcmp(tmp + 16, FDS_MAGIC, FDS_MAGIC_LENGTH)) {
		offset = 16;
	} else {
		return -1;
	}

	/* Try to handle truncated files or files with
	   garbage on the end in a reasonable way. */
	disk_data_size = ((size - offset) / 65500) * 65500;
	if (disk_data_size == 0)
		return -1;

	/* FIXME assumes the mangled FDS image format: no gaps, no
	   start marks, and no checksums.
	*/
	if (!offset) {
		tmp = realloc(rom->buffer, disk_data_size + 16);
		if (!tmp)
			return -1;

		rom->buffer = tmp;

		memmove(rom->buffer + 16, rom->buffer,
			disk_data_size);
	} else {
		tmp = realloc(rom->buffer, 16 + disk_data_size);
		if (tmp)
			rom->buffer = tmp;
	}

	rom->buffer_size = 16 + disk_data_size;
	memset(rom->buffer, 0, 16);

	rom->info.total_prg_size = disk_data_size;
	rom->info.total_chr_size = 0;
	rom->offset = 16;
	rom->disk_side_size = 65500;
	rom->info.board_type = BOARD_TYPE_FDS;
	rom->info.system_type = EMU_SYSTEM_TYPE_FAMICOM;
	rom->info.wram_size[0] = SIZE_32K;
	rom->info.vram_size[0] = SIZE_8K;
	rom->info.wram_size[1] = 0;
	rom->info.vram_size[1] = 0;
	rom->info.flags = 0;
	rom->info.mirroring = MIRROR_M;

	rom->offset = 16;
	rom_calculate_checksum(rom);

	return 0;
}

static int fds_calculate_fds_crc(uint8_t *data, size_t size, int add_start_mark)
{
	int crc;
	int i;

	if (add_start_mark)
		crc = 0x8000;
	else
		crc = 0;

	for (i = 0; i < size; i++) {
		crc  = (crc >> 8) ^ fds_crc_table[crc & 0xff];
		crc ^= (data[i] << 8);
	}

	crc  = (crc >> 8) ^ fds_crc_table[crc & 0xff];
	crc  = (crc >> 8) ^ fds_crc_table[crc & 0xff];

	return crc;
}

/* static int fds_calculate_fds_crc(uint8_t *data, size_t size, int add_start_mark) */
/* { */
/* 	int i, j; */
/* 	int crc; */

/* 	crc = 0; */

/* 	if (add_start_mark) */
/* 		size++; */

/* 	for (i = 0; i < size; i++) { */
/* 		uint8_t d; */

/* 		if (add_start_mark) { */
/* 			if (i == 0) */
/* 				d = 0x80; */
/* 			else */
/* 				d = data[i - 1]; */
/* 		} else { */
/* 			d = data[i]; */
/* 		} */

/* 		for (j = 0x01; j <= 0x80; j <<= 1) { */

/* 			if (d & j) */
/* 				crc |= 0x10000; */
/* 			if (crc & 1) */
/* 				crc ^= 0x10810; */

/* 			crc >>= 1; */
/* 		} */
/* 	} */

/* 	for (i = 0; i < 16; i++) { */
/* 		if (crc & 1) */
/* 			crc ^= 0x10810; */

/* 		crc >>= 1; */
/* 	} */
	
/* 	return crc; */
/* } */

int fds_convert_to_raw(struct rom *rom)
{
	off_t current_offset;
	uint8_t *data, *tmp_buffer;
	int i;
	int previous_side, current_side;
	size_t current_size;
	size_t largest_size;
	size_t tmp_buffer_size;
	int side_count;
	int rc;

	struct fds_block_list *list;

	rc = -1;

	fds_validate_image(rom, &list, 1);
	if (!list)
		return -1;

	data = rom->buffer + 16;

	current_offset = 0;

	largest_size = 0;
	current_size = 0;
	side_count = 0;
	for (i = 0; i < list->total_entries; i++) {
		if (list->entries[i].type == 1) {
			if (current_size > largest_size) {
				largest_size = current_size;
			}
			current_size = 0;
			current_size += FDS_PREGAP_LENGTH / 8;
			current_size -= FDS_GAP_LENGTH / 8;
			side_count++;
		}

		current_size += list->entries[i].size;

		if (list->entries[i].type != 5) {
			current_size += FDS_GAP_LENGTH / 8;
			current_size += 3;
		}
	}

	if (largest_size < 65500)
		largest_size = 65500;

	previous_side = -1;
	current_side = -1;
	for (i = 0; i < list->total_entries; i++) {
		int gap_length;

		gap_length = 0;

		switch (list->entries[i].type) {
		case 1:
			gap_length = FDS_PREGAP_LENGTH / 8;
			current_side++;
			break;
		case 2:
		case 3:
		case 4:
			gap_length = FDS_GAP_LENGTH / 8;
			break;
		}

		if (previous_side != current_side)
			current_offset = current_side * largest_size;

		list->entries[i].new_offset = current_offset +
			gap_length;

		if (list->entries[i].type != 5)
			current_offset++;

		current_offset = list->entries[i].new_offset +
			list->entries[i].size;

		/* Allow room for checksum */
		if (list->entries[i].type != 5) {
			current_offset += 2;
		}

		previous_side = current_side;
	}

	tmp_buffer_size = (largest_size * side_count);
	if (tmp_buffer_size % 8192)
		tmp_buffer_size += 8192;
	tmp_buffer_size += 8192;
	tmp_buffer_size /= 8192;
	tmp_buffer_size *= 8192;
	tmp_buffer_size += 16;

	tmp_buffer = malloc(tmp_buffer_size);
	if (!tmp_buffer)
		goto done;

	memset(tmp_buffer, 0, tmp_buffer_size);

	for (i = 0; i < list->total_entries; i++) {
		uint8_t *src, *dest;
		size_t size;
		int crc;

		src = data + list->entries[i].offset;
		dest = tmp_buffer + list->entries[i].new_offset + 16;
		size = list->entries[i].size;
		crc = list->entries[i].calculated_crc;

		memcpy(dest, src, size);
		if (list->entries[i].type != 5) {
			dest[-1] = 0x80;
			dest[size] = crc & 0xff;
			dest[size + 1] = (crc >> 8) & 0xff;
		}
	}

	memcpy(tmp_buffer, rom->buffer, 16);
	/* memcpy(tmp_buffer + (side_count * largest_size + 16), */
	/*        data + (side_count * rom->disk_side_size), */
	/*        rom->buffer_size - (side_count * rom->disk_side_size + 16)); */
	memcpy(tmp_buffer + (tmp_buffer_size - 8192),
	       rom->buffer + (rom->buffer_size - 8192), 8192);

	free(rom->buffer);
	rom->buffer = tmp_buffer;
	rom->buffer_size = tmp_buffer_size;
	rom->disk_side_size = largest_size;
	rom->info.total_prg_size = rom->buffer_size - 16;

	rc = 0;
done:

	fds_block_list_free(list);

	return rc;
}

static void fix_name(char *dest, uint8_t *name)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (name[i] && name[i] != 0x12 && name[i] != 0xf0)
			dest[i] = name[i];
		else
			dest[i] = ' ';
	}

	dest[8] = 0x00;
}

int fds_get_disk_info(struct rom *rom, struct text_buffer *buffer)
{
	struct fds_block_list *list;
	int disk, side, file_list_header;
	int i, k;

	if (!rom || (rom->info.board_type != BOARD_TYPE_FDS))
		return 0;

	fds_validate_image(rom, &list, 1);
	if (!list)
		return -1;

	//fds_block_list_print(list);
	disk = 0;
	side = 0;
	file_list_header = 0;
	for (i = 0; i < list->total_entries; i++) {
		uint8_t *ptr = rom->buffer + rom->offset + list->entries[i].offset;
		int manufacturer, j;
		char *type;
		int addr, size;
		char name[33];

		switch (list->entries[i].type) {
		case 1:
			file_list_header = 0;
			text_buffer_append(buffer, "\n");

			text_buffer_append(buffer, "Disk %d, Side %c Information\n",
					   (disk / 2) + 1, (side & 1) ? 'B' : 'A');
			text_buffer_append(buffer, "--------------------------\n");
			disk++;
			side++;

			text_buffer_append(buffer, "Game ID: %c%c%c%c\n",
					   ptr[16] ? ptr[16] : ' ',
					   ptr[17] ? ptr[17] : ' ',
					   ptr[18] ? ptr[18] : ' ',
					   ptr[19] ? ptr[19] : ' ');
			text_buffer_append(buffer, "Revision: %d\n",
					   ptr[20]);
			text_buffer_append(buffer, "Disk #: %d\n",
					   ptr[22] + 1);
			text_buffer_append(buffer, "Disk side: %c\n",
					   ptr[21] ? 'B' : 'A');
			text_buffer_append(buffer, "Boot file code: 0x%02x\n",
					   ptr[25]);

			manufacturer = -1;
			for (j = 0; j < NUM_MANUFACTURERS; j++) {
				if (manufacturers[j].code == ptr[15]) {
					manufacturer = j;
					break;
				}
			}

			text_buffer_append(buffer, "Manufacturer: %s (0x%02x)\n",
					   (manufacturer >= 0) ?
					   manufacturers[manufacturer].name :
					   "<unknown>", manufacturer);

			break;
		case 2:
			text_buffer_append(buffer, "Non-hidden file count: %d\n",
					   ptr[1]);
			break;
		case 3:
			if (!file_list_header) {
				text_buffer_append(buffer, "\n");
				text_buffer_append(buffer,
						   " # ID Name     Addr   "
						   "Size (Hex)   Type CRC       CRC32    SHA1\n");
				text_buffer_append(buffer,
						   "--------------------------"
						   "--------------------------"
						   "--------------------------"
						   "---------------------\n");
				file_list_header = 1;
			}
			addr = ptr[11] | (ptr[12] << 8);
			size = ptr[13] | (ptr[14] << 8);
			switch (ptr[15]) {
			case 0:
				type = "PRG";
				break;
			case 1:
				type = "CHR";
				break;
			case 2:
				type = "NMT";
				break;
			default:
				type = "?";
				break;
			}
			fix_name(name, &ptr[3]);
			text_buffer_append(buffer,
			       "%2d %02x %s $%04X %5d ($%04X) %s  %04x %04x %08x ",
				ptr[1], ptr[2], name,
					   addr, size, size, type,
					   list->entries[i].type == 3 ? list->entries[i].data_crc : list->entries[i].crc,
					   list->entries[i].type == 3 ? list->entries[i].calculated_data_crc : list->entries[i].calculated_crc,
					   list->entries[i].crc32);

			for (k = 0; k < 20; k++)
				text_buffer_append(buffer, "%02x", list->entries[i].sha1[k]);
			text_buffer_append(buffer, "\n");
			break;
		case 4:
			break;
		}
	}

	fds_block_list_free(list);

	return 0;
}

int fds_convert_to_fds(struct rom *rom)
{
	off_t current_offset;
	uint8_t *ptr;
	uint8_t *data;
	size_t slack_size;
	int current_side, previous_side;
	int i;

	struct fds_block_list *list;

	fds_validate_image(rom, &list, 1);
	if (!list)
		return -1;

//	fds_block_list_print(list);
//	goto done;

	data = rom->buffer + 16;

	current_side = 0;
	current_offset = 0;
	previous_side = -1;
	for (i = 0; i < list->total_entries; i++) {
		current_side = list->entries[i].offset / 65500;
		if (previous_side != current_side) {
			if (current_offset)
				current_offset = current_side * 65500;
		}

		list->entries[i].new_offset = current_offset;
		/* printf("offset %d: %x\n", i, current_offset); */

		current_offset = list->entries[i].new_offset +
			list->entries[i].size;
		previous_side = current_side;
	}

	previous_side = -1;
	for (i = 0; i < list->total_entries; i++) {
		uint8_t *src, *dest;
		size_t block_size;

		src = data + list->entries[i].offset;
		dest = data + list->entries[i].new_offset;
		block_size = list->entries[i].size;
		current_side = list->entries[i].offset / 65500;

		memmove(dest, src, block_size);

		if (i && (previous_side != current_side)) {
			ptr = data + (list->entries[i - 1].new_offset);
			ptr += list->entries[i - 1].size;

			slack_size = dest - ptr;
			memset(ptr, 0, slack_size);
		}

		previous_side = current_side;
	}

	i = list->total_entries - 1;
	ptr = data + list->entries[i].new_offset + list->entries[i].size;
	slack_size = 65500 - (list->entries[i].new_offset % 65500) -
		list->entries[i].size;
	memset(ptr, 0, slack_size);

//done:
	fds_block_list_free(list);

	/* Add FDS header in case patches account for it in their
	   checksums */
	memset(rom->buffer, 0, 16);
	rom->buffer[0] = 'F';
	rom->buffer[1] = 'D';
	rom->buffer[2] = 'S';
	rom->buffer[3] = 0x1a;
	rom->buffer[4] = current_side + 1;

	
	return 0;
}

int fds_disk_sanity_checks(struct rom *rom)
{
	struct fds_block_list *list;
	int rc;
	int i;
	int side;
	int file_count, actual_file_count;
	int size;

	rc = fds_validate_image(rom, &list, 0);

	if (rc < 0) {
		if (list)
			fds_block_list_free(list);

		return rc;
	}

	rc = 1;

	/* Ensure we have enough entries for at least the header and
	   file count blocks. */
	if (list->total_entries < 2)
		goto done;

	i = 0;
	for (side = 0; side < rom->buffer_size / 65500; side++) {
		/* Make sure we start with block type 1 */
		if (list->entries[i].type != 0x01)
			goto done;

		/* Should be at an offset that is a multiple of 65500 */
		if (list->entries[i].offset % 65500)
			goto done;

		if (list->entries[i + 1].type != 0x02)
			goto done;

		file_count = rom->buffer[rom->offset +
					 list->entries[i + 1].offset + 1];

		size = 0;
		size += list->entries[i].size;
		size += list->entries[i + 1].size;

		size += FDS_PREGAP_LENGTH / 8;
		size += FDS_GAP_LENGTH / 8;
		size += 6; /* CRCs and block start marks */

		actual_file_count = 0;

		i += 2;
		while (i < list->total_entries)	{
			if ((list->entries[i].type == 5) ||
			    (list->entries[i].type == 1)) {
				break;
			}

			if (list->entries[i].type != 3)
				goto done;

			size += list->entries[i].size + 3 + (FDS_GAP_LENGTH / 8);
			i++;

			if ((i >= list->total_entries) ||
			    (list->entries[i].type != 4)) {
				goto done;
			}

			size += list->entries[i].size + 3 + (FDS_GAP_LENGTH / 8);
			i++;

			actual_file_count++;
		}

		if (size > 65500) {
			goto done;
		}

		if (file_count > actual_file_count) {
			goto done;
		}

		while (i < list->total_entries) {
			if (list->entries[i].type == 1) {
				break;
			}

			i++;
		}
	}

	rc = 0;
done:

	fds_block_list_free(list);

	return rc;
}
