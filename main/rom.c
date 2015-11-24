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

#if defined __unix__
#include <glob.h>
#include <unistd.h>
#include <libgen.h>
#elif defined _WIN32
#include <tchar.h>
#include <libgen.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if ZIP_ENABLED
#include <zlib.h>
#include <unzip.h>
#endif

#include "config.h"
#include "emu.h"
#include "db.h"
#include "patch.h"
#include "file_io.h"
#include "ines.h"
#include "fds.h"
#include "nsf.h"
#include "unif.h"
#include "crc32.h"
#include "sha1.h"

int rom_apply_patches(struct rom *rom, int count,
		      char **patchfiles, int from_zip);

static int rom_load_file_data(struct emu *emu, const char *filename,
			      struct rom **romptr, uint8_t *buffer,
			      size_t size)
{
	struct rom *rom;
	int rc;
	
	rom = rom_alloc(filename, buffer, size);

	if (!rom) {
		return -1;
	}

	rc = db_rom_load(emu, rom);
	if (rc < 0)
		rc = ines_load(emu, rom);
	if (rc < 0)
		rc = unif_load(emu, rom);
	if (rc < 0)
		rc = fds_load(emu, rom);
	if (rc < 0)
		rc = nsf_load(emu, rom);

	if (rom->info.board_type == BOARD_TYPE_UNKNOWN)
		rc = -1;

	if (rc >= 0) {
		uint8_t *buffer = rom->buffer;
		size_t size = rom->buffer_size;
		size_t orig_size;
		uint8_t *tmp;

		orig_size = size;

		if (rom->offset < INES_HEADER_SIZE) {
			size += INES_HEADER_SIZE - rom->offset;
			tmp = realloc(buffer, size);
			if (!tmp) {
				rom_free(rom);
				return -1;
			}

			rom->buffer = tmp;
			rom->buffer_size = size;
			buffer = tmp;
		}

		if (rom->offset != INES_HEADER_SIZE) {
			memmove(buffer + INES_HEADER_SIZE, buffer + rom->offset,
				orig_size - rom->offset);
				
		}

		memset(buffer, 0, INES_HEADER_SIZE);
		rom->offset = INES_HEADER_SIZE;

		*romptr = rom;
	} else {
		rom_free(rom);
	}

	return rc;
}

int rom_load_single_file(struct emu *emu, const char *filename, struct rom **romptr)
{
	size_t file_size;
	uint8_t *buffer;
	char *filename_inzip;
	int filename_inzip_size;
#if ZIP_ENABLED
	unzFile zip;
	unz_file_info file_info;
	unz_global_info global_info;
#endif

	if (!romptr)
		return -1;

	*romptr = NULL;

	if (!filename)
		return -1;

	if (!check_file_exists(filename))
		return -1;

#if ZIP_ENABLED
	filename_inzip = NULL;
	filename_inzip_size = 0;
	zip = unzOpen(filename);
	if (zip != NULL) {
		int err, i;
		int rc;
		char *ext;

		buffer = NULL;
		rc = -1;

		err = unzGetGlobalInfo(zip, &global_info);
		if (err != UNZ_OK)
			return -1;

		for (i = 0; i < global_info.number_entry; i++) {
			err = unzGetCurrentFileInfo(zip, &file_info, filename_inzip,
						    filename_inzip_size,
						    NULL, 0, NULL, 0);
			if (err != UNZ_OK)
				break;

			if (file_info.size_filename >= filename_inzip_size) {
				char *tmp;
				filename_inzip_size = file_info.size_filename + 1;
				tmp = realloc(filename_inzip, filename_inzip_size);
				if (!tmp)
					break;

				filename_inzip = tmp;

				err = unzGetCurrentFileInfo(zip, &file_info, filename_inzip,
							    filename_inzip_size,
							    NULL, 0, NULL, 0);
				if (err != UNZ_OK)
					break;
			
			}

			printf("filename_inzip: %s\n", filename_inzip);

			ext = strrchr(filename_inzip, '.');
			if (!ext || (strcasecmp(ext, ".nes") &&
				     strcasecmp(ext, ".unf") && 
				     strcasecmp(ext, ".unif") && 
				     strcasecmp(ext, ".fds") && 
				     strcasecmp(ext, ".nsf"))) {
				err = unzGoToNextFile(zip);
				if (err != UNZ_OK)
					break;
				else
					continue;
			}

			file_size = file_info.uncompressed_size;
			if (file_size == 0)
				continue;

			err = unzOpenCurrentFile(zip);
			if (err != UNZ_OK)
				break;

			buffer = malloc(file_size + 16);
			if (!buffer)
				break;

			err = unzReadCurrentFile(zip, buffer, file_size);

			if (err != file_size) {
				free(buffer);
				buffer = NULL;
				break;
			}

			if (rom_load_file_data(emu, filename, romptr,
					       buffer, file_size) >= 0) {
				err = unzCloseCurrentFile(zip);
				if (err != UNZ_OK)
					break;

				rc = 0;
				break;
			}

			err = unzCloseCurrentFile(zip);
			if (err != UNZ_OK)
				break;

			err = unzGoToNextFile(zip);
			if (err != UNZ_OK)
				break;
		}

		if (rc < 0) {

			unzCloseCurrentFile(zip);
		}

		if (filename_inzip)
			free(filename_inzip);

		unzClose(zip);

		return rc;
	}
#endif

	file_size = get_file_size(filename);
	if ((int)file_size < 0)
		return -1;

	/* FIXME enforce maximum size */
	buffer = malloc(file_size + 16);
	if (!buffer)
		return -1;

	if (readfile(filename, buffer, file_size)) {
		free(buffer);
		return -1;
	}

	if (rom_load_file_data(emu, filename, romptr,
			       buffer, file_size) < 0) {
		return -1;
	}

	return 0;
}

struct rom *rom_load_file(struct emu *emu, const char *filename)
{
	struct rom *rom;

	rom_load_single_file(emu, filename, &rom);
	if (!rom) {
		err_message("Unrecognized ROM format\n");
		return NULL;
	}

	if (rom) {
		/* Board type may require fix-ups depending on other information
		   in the rom struct, so handle that here.
		*/
		int is_nv = rom->info.flags & (ROM_FLAGS_NV);
		if (board_set_rom_board_type(rom, rom->info.board_type, is_nv)) {
			rom_free(rom);
			return NULL;
		}
	}

	/* Generate iNES 2 header from current rom metadata.
	   Needed for applying patches for iNES files to roms
	   not loaded from iNES formats, since they may change
	   the iNES header.
	*/
	if ((rom->info.board_type != BOARD_TYPE_FDS) &&
	    (rom->info.board_type != BOARD_TYPE_NSF)) {
		ines_generate_header(rom, 2);
	}

	return rom;
}

struct rom *rom_reload_file(struct emu *emu, struct rom *rom)
{
	struct rom *patched_rom = NULL;
	int rc;

	patched_rom = rom_alloc(rom->filename, rom->buffer,
	                        rom->buffer_size);
	if (!patched_rom) {
		rom_free(rom);
		return NULL;
	}

	rc = ines_load(emu, patched_rom);
	if (rc < -1) {
		free(patched_rom->filename);
		free(patched_rom);
		rom_free(rom);
		return NULL;
	}

	/* 
	   Copy metadata from the old rom struct that (probably)
	   still applies, such as default input settings.
	*/
	patched_rom->info.auto_device_id[0] = rom->info.auto_device_id[0];
	patched_rom->info.auto_device_id[1] = rom->info.auto_device_id[1];
	patched_rom->info.auto_device_id[2] = rom->info.auto_device_id[2];
	patched_rom->info.auto_device_id[3] = rom->info.auto_device_id[3];
	patched_rom->info.auto_device_id[4] = rom->info.auto_device_id[4];
	patched_rom->info.vs_controller_mode = rom->info.vs_controller_mode;
	patched_rom->info.four_player_mode = rom->info.four_player_mode;

	free(patched_rom->filename);
	patched_rom->filename = rom->filename;
	memcpy(rom, patched_rom, sizeof(*rom));
	free(patched_rom);

	print_rom_info(rom);

	return rom;
}

#if defined __unix__
static char *escape_path(const char *path)
{
	char *buffer;
	int length;
	int metachars;
	int i, j;

	length = 0;
	metachars = 0;

	for (i = 0; path[i]; i++) {
		switch (path[i]) {
		case '?':
		case '*':
		case '[':
		case ']':
			metachars++;
			break;
		}

		length++;
	}

	buffer = malloc(length + metachars + 1);
	if (!buffer)
		return NULL;

	j = 0;
	for (i = 0; path[i]; i++) {
		switch (path[i]) {
		case '?':
		case '*':
		case '[':
		case ']':
			buffer[j++] = '\\';
			break;
		}

		buffer[j++] = path[i];
	}

	buffer[j] = '\0';

	return buffer;
}

/* Returns the number of patches applied */
static char **rom_posix_find_autopatches(const char *patch_path,
					 const char *rom_base,
					 const char *rom_ext,
					 struct rom *rom)
{
	char *buffer;
	char *escaped_path;
	char **path_list;
	char *paths;
	long path_max;
	int length;
	int i;
	
	glob_t patch_glob;

	path_list = NULL;

	path_max = pathconf(patch_path, _PC_PATH_MAX);
	length = path_max + 1;
	buffer = malloc(length);
	if (!buffer)
		return NULL;

	snprintf(buffer, path_max + 1, "%s/%s", patch_path, rom_base);

	escaped_path = escape_path(buffer);

	/* First look for patches that don't include the ROM's
	   extension in the filename.
	 */
	snprintf(buffer, path_max + 1, "%s-*.[biuBIU][pP][sS]", escaped_path);
	patch_glob.gl_offs = 0;
	glob(buffer, 0, NULL, &patch_glob);
	if (patch_glob.gl_pathc == 0) {
		globfree(&patch_glob);
		snprintf(buffer, length, "%s.[biuBIU][pP][sS]", escaped_path);
		glob(buffer, 0, NULL, &patch_glob);
	}

	/* If we didn't find any patches, try for patches that have
	   the extension in the filename (only if the ROM had an
	   extension, of course).
	 */
	if (patch_glob.gl_pathc == 0 && rom_ext) {
		globfree(&patch_glob);
		snprintf(buffer, length, "%s.%s-*.[biuBIU][pP][sS]",
			 escaped_path, rom_ext);
		glob(buffer, 0, NULL, &patch_glob);
		if (patch_glob.gl_pathc == 0) {
			globfree(&patch_glob);
			snprintf(buffer, length, "%s.%s.[biuBIU][pP][sS]",
				 escaped_path, rom_ext);
			glob(buffer, 0, NULL, &patch_glob);
		}
	}

	if (escaped_path)
		free(escaped_path);

	if (!patch_glob.gl_pathc)
		goto done;

	length = (patch_glob.gl_pathc + 1) * sizeof(char **);
	for (i = 0; i < patch_glob.gl_pathc; i ++) {
		length += strlen(patch_glob.gl_pathv[i]) + 1;
	}

	path_list = malloc(length);
	if (!path_list)
		goto done;

	paths = (char *)(path_list + patch_glob.gl_pathc + 1);
	for (i = 0; i < patch_glob.gl_pathc; i++) {
		int len = strlen(patch_glob.gl_pathv[i]) + 1;
		strncpy(paths, patch_glob.gl_pathv[i], len);
		path_list[i] = paths;
		paths += len;
	}
	path_list[i] = NULL;

done:
	globfree(&patch_glob);
	free(buffer);

	return path_list;
}

#elif defined _WIN32

static int cmpstring(const void *p1, const void *p2)
{
	return strcmp(*(char * const *)p1, *(char * const *)p2);
}

static char **find_patches(const char *path, const char *pattern)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	char *buffer, *ext, *tmp;
	char **path_list;
	char *paths;
	size_t buffer_size;
	int patchc, i;
	int path_list_size;

	buffer = NULL;
	path_list = NULL;
	buffer_size = 0;
	patchc = 0;

	hFind = FindFirstFile(pattern, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
		return NULL;

	do {
		size_t new_size;
		int len;

		ext = strrchr(FindFileData.cFileName, '.');
		if (!ext)
			continue;

		if (strcasecmp(ext, ".bps") && strcasecmp(ext, ".ips") && 
		    strcasecmp(ext, ".ups")) {
			continue;
		}

		len = strlen(FindFileData.cFileName) + 1;

		new_size = buffer_size + len;
		tmp = realloc(buffer, new_size * sizeof(*buffer));
		if (!tmp) {
			patchc = -1;
			break;
		}
		buffer = tmp;

		strncpy(buffer + buffer_size, FindFileData.cFileName, len);
		buffer_size = new_size;
		patchc++;
	} while (FindNextFile(hFind, &FindFileData) != 0);

	FindClose(hFind);

	if (!patchc)
		return NULL;

	path_list_size = buffer_size + patchc * (strlen(path) + 1);
	path_list_size += (patchc + 1) * sizeof(*path_list);

	path_list = malloc(path_list_size);
	if (!path_list) {
		if (buffer)
			free(buffer);

		return NULL;
	}

	paths = (char *)(path_list + patchc + 1);
	tmp = paths;
	for (i = 0; i < patchc; i++) {
		sprintf(tmp, "%s\\%s", path, buffer);
		path_list[i] = tmp;
		tmp += strlen(tmp) + 1;
	}
	path_list[i] = NULL;
	free(buffer);

	qsort(&path_list[0], patchc, sizeof(char *), cmpstring);

	return path_list;
}

static char **rom_win32_find_autopatches(const char *patch_path,
					 const char *rom_base,
					 const char *rom_ext,
					 struct rom *rom)
{
	char pattern[MAX_PATH];
	char **patch_list;

	snprintf(pattern, sizeof(pattern), "%s\\%s-*.?ps",
	         patch_path, rom_base);
	patch_list = find_patches(patch_path, pattern);

	if (!patch_list) {
		snprintf(pattern, sizeof(pattern), "%s\\%s.?ps",
	        	patch_path, rom_base);
		patch_list = find_patches(patch_path, pattern);
	}

	if (!patch_list && rom_ext) {
		snprintf(pattern, sizeof(pattern), "%s\\%s.%s-*.?ps",
	        	patch_path, rom_base, rom_ext);
		patch_list = find_patches(patch_path, pattern);
	}

	if (!patch_list && rom_ext) {
		snprintf(pattern, sizeof(pattern), "%s\\%s.%s.?ps",
	        	patch_path, rom_base, rom_ext);
		patch_list = find_patches(patch_path, pattern);
	}

	return patch_list;
}

#endif

static int rom_apply_patch_data(struct rom *rom, uint8_t *buffer, size_t size,
				uint32_t rom_buffer_crc)
{
	size_t input_size, output_size;
	off_t data_offset, target_offset;
	int rc;

	data_offset = 0;
	target_offset = 0;


	if (rom->info.board_type == BOARD_TYPE_FDS) {
		int input_size_rc;
		int output_size_rc;

		input_size_rc = patch_get_input_size(buffer, size,
						     &input_size);

		output_size_rc = patch_get_output_size(buffer, size,
						       &output_size);

		if (!input_size_rc && !(input_size % 65500))
			data_offset = 16;

		if (!output_size_rc && !(output_size % 65500))
			target_offset = 16;

		if (patch_is_ips(buffer, size)) {
			uint8_t *tmp, *orig;
			size_t orig_size;

			orig = rom->buffer;
			orig_size = rom->buffer_size;

			tmp = malloc(orig_size);
			if (tmp) {
				memcpy(tmp, rom->buffer, rom->buffer_size);

				rc = patch_apply(&tmp, &rom->buffer_size, 0, 0,
						 rom_buffer_crc, buffer, size);

				rom->buffer = tmp;
				if (fds_disk_sanity_checks(rom) > 0) {
					data_offset = 16;
					target_offset = 16;
				}

				free(tmp);

				rom->buffer = orig;
				rom->buffer_size = orig_size;
			}
		}
	}

	if (data_offset) {
		rom_buffer_crc = crc32buf(rom->buffer + data_offset,
					  rom->buffer_size -
					  data_offset,
					  NULL);
	}

	rc = patch_apply(&rom->buffer, &rom->buffer_size, data_offset,
			 target_offset, rom_buffer_crc, buffer, size);

	if (rc) {
		return -1;
	}

	return 0;
}

/* Apply patches to rom
   - 'count' is the number of patch files to apply. If < 0, patchfiles
     is assumed to be a NULL-terminated list.
   - 'patchfiles' is the list of patch files
   - 'rom' is a pointer to the rom struct which points to the rom data
   - returns -1 on error, or the number of patches applied if successful
*/
int rom_apply_patches(struct rom *rom, int count, char **patchfiles, int from_zip)
{
	uint32_t buffer_crc;
	uint8_t *patch_buffer;
	int patch_buffer_size;
	int patches_applied;
	int i;
#if ZIP_ENABLED
	unzFile zip;
	unz_file_info file_info;
	int err;
#endif

	patch_buffer = NULL;
	zip = NULL;
	patch_buffer_size = 0;
	patches_applied = 0;

	buffer_crc = crc32buf(rom->buffer, rom->buffer_size, NULL);

#if ZIP_ENABLED
	if (from_zip) {
		zip = unzOpen(rom->filename);
		if (!zip)
			return -1;
	}
#endif

	for (i = 0; (i < count) || (patchfiles[i]); i++) {
		char *patch_file;
		int patch_size;

		patch_file = patchfiles[i];
		patch_size = 0;

		printf("Applying patch \"%s\"%s\n", patch_file,
		       from_zip ? " (from ZIP)" : "");

#if ZIP_ENABLED
		if (from_zip) {
			err = unzLocateFile(zip, patch_file, 0);
			if (err != UNZ_OK) {
				break;
			}

			err = unzGetCurrentFileInfo(zip, &file_info, NULL, 0,
						    NULL, 0, NULL, 0);
			if (err != UNZ_OK)
				break;

			patch_size = file_info.uncompressed_size;
		} else {
#endif
		patch_size = get_file_size(patch_file);
#if ZIP_ENABLED
		}
#endif

		if (patch_size < 0) {
			return -1;
		} else if (patch_size > patch_buffer_size) {

			uint8_t *tmp = realloc(patch_buffer, patch_size);
			if (!tmp) {
				err_message("rom_apply_patches: %s: failed to"
					    " allocate memory for patch\n",
					    patch_file);
				return -1;
			}

			patch_buffer = tmp;
			patch_buffer_size = patch_size;
		}

#if ZIP_ENABLED
		if (from_zip) {
			err = unzOpenCurrentFile(zip);
			if (err != UNZ_OK)
				break;

			err = unzReadCurrentFile(zip, patch_buffer, patch_size);
			unzCloseCurrentFile(zip);
			if (err != patch_size) {
				err_message("rom_apply_patches: %s: failed to load file\n",
					patch_file);
				break;
			}
		} else {
#endif
		if (readfile(patch_file, patch_buffer, patch_size)) {
			err_message("rom_apply_patches: %s: failed to load file\n",
				    patch_file);
			return -1;
		}
#if ZIP_ENABLED
		}
#endif

		if (rom_apply_patch_data(rom, patch_buffer, patch_size,
					 buffer_crc) == 0) {
			patches_applied++;
		} else {
			break;
		}
	}

#if ZIP_ENABLED
	if (from_zip) {
		unzClose(zip);
	}
#endif

	if (patch_buffer)
		free(patch_buffer);

	return patches_applied;
}

struct rom *rom_alloc(const char *filename, uint8_t *buffer, size_t size)
{
	struct rom *rom;

	rom = malloc(sizeof(*rom));
	if (!rom)
		return NULL;

	memset(rom, 0, sizeof(*rom));

	rom->buffer = buffer;
	rom->buffer_size = size;
	rom->info.system_type = EMU_SYSTEM_TYPE_NES;
	rom->info.mirroring = MIRROR_M;
	rom->info.four_player_mode = FOUR_PLAYER_MODE_NONE;
	rom->info.vs_controller_mode = VS_CONTROLLER_MODE_STANDARD;
	rom->info.auto_device_id[0] = IO_DEVICE_CONTROLLER_1;
	rom->info.auto_device_id[1] = IO_DEVICE_CONTROLLER_2;
	rom->info.auto_device_id[2] = IO_DEVICE_CONTROLLER_3;
	rom->info.auto_device_id[3] = IO_DEVICE_CONTROLLER_4;
	rom->info.auto_device_id[4] = IO_DEVICE_NONE;
	rom->filename = strdup(filename);
	if (!rom->filename) {
		rom_free(rom);
		return NULL;
	}

	return rom;
}

void rom_free(struct rom *rom)
{
	if (!rom)
		return;

	if (rom->filename)
		free(rom->filename);

	if (rom->buffer)
		free(rom->buffer);

	free(rom);
}

#if ZIP_ENABLED
static int cmpstringp(const void *p1, const void *p2)
{
	return strcmp(* (char * const *) p1, * (char * const *) p2);
}

char **rom_find_zip_autopatches(struct config *config, struct rom *rom)
{
	char **patch_list;
	char *buffer;
	unzFile zip;
	unz_file_info file_info;
	unz_global_info global_info;
	char filename[512];
	size_t buffer_size;
	int err;
	int count;
	int i;

	patch_list = NULL;
	buffer = NULL;
	buffer_size = 0;

	zip = unzOpen(rom->filename);
	if (!zip)
		return NULL;

	err = unzGetGlobalInfo(zip, &global_info);
	if (err != UNZ_OK) {
		unzClose(zip);
		return NULL;
	}

	count = 0;
	for (i = 0; i < global_info.number_entry; i++) {
		char *ext;
		err = unzGetCurrentFileInfo(zip, &file_info,
					    filename, sizeof(filename) - 1,
					    NULL, 0, NULL, 0);
		if (err != UNZ_OK) {
			unzClose(zip);
			break;
		}

		if (file_info.uncompressed_size == 0)
			continue;

		ext = strrchr(filename, '.');

		/* FIXME skip if no ext */

		if (ext && (!strcasecmp(ext, ".ips") ||
			    !strcasecmp(ext, ".ups") ||
			    !strcasecmp(ext, ".bps"))) {
			char *tmp;
			int offset;
			int length;

			/* Patches must be placed in the root
			   directory of the zip file.
			*/
			if (strchr(filename, '/'))
				continue;

			offset = buffer_size;
			length = strlen(filename) + 1;

			buffer_size += length;
			tmp = realloc(buffer, buffer_size);
			if (!tmp) {
				free(buffer);
				unzClose(zip);
				break;
			}

			buffer = tmp;
			memcpy(buffer + offset, filename, length);
			count++;
		}

		err = unzGoToNextFile(zip);
		if (err != UNZ_OK) {
			unzClose(zip);
			break;
		}
	}

	if (buffer) {
		int offset = sizeof(char *) * (count + 1);
		char *tmp = realloc(buffer, buffer_size + offset);
		int i;

		if (!tmp) {
			free(buffer);
			return NULL;
		}

		buffer = tmp;
		memmove(buffer + offset, buffer, buffer_size);
		patch_list = (char **)buffer;

		for (i = 0; i < count; i++) {
			patch_list[i] = buffer + offset;
			offset += strlen(patch_list[i]) + 1;
		}

		patch_list[i] = NULL;

		qsort(patch_list, count, sizeof(char *), cmpstringp);
	}

	return patch_list;
}
#endif

/* Returns a list of patches to be automatically applied to the rom. The
   list is a char ** array with a terminating NULL pointer, so no explicit
   patch count is necessary.

   The returned list must be freed by tha caller. Doing so will also free
   the values in the list, as they are stored in the same chunk of memory
   as the array itself.
*/
char **rom_find_autopatches(struct config *config, struct rom *rom)
{
	char *patch_path, *tmp, *tmp2;
	char *rom_path;
	char *rom_base;
	char *rom_ext;
	char **patch_list;
	int i;

	patch_list = rom_find_zip_autopatches(config, rom);
	i = 0;
	while (patch_list && patch_list[i]) {
		printf("patch %d: %s\n", i, patch_list[i]);
		i++;
	}

	if (patch_list)
		free(patch_list);

	patch_list = NULL;

	if (!config->autopatch_enabled)
		return NULL;

	tmp = strdup(rom->filename);
	if (!tmp)
		return NULL;

	tmp2 = strdup(rom->filename);
	if (!tmp2) {
		free(tmp);
		return NULL;
	}

	patch_path = config_get_path(config, CONFIG_DATA_DIR_PATCH, NULL, 1);
	if (!patch_path) {
		free(tmp);
		free(tmp2);
		return NULL;
	}

	rom_path = dirname(tmp);

	rom_base = basename(tmp2);
	rom_ext = strrchr(rom_base, '.');
	if (rom_ext) {
		*rom_ext = '\0';
		rom_ext++;
	}

#if defined __unix__
	patch_list = rom_posix_find_autopatches(rom_path, rom_base,
						rom_ext, rom);
	if (!patch_list) {
		patch_list = rom_posix_find_autopatches(patch_path,
							rom_base,
							rom_ext,
							rom);
	}
#elif defined _WIN32
	patch_list = rom_win32_find_autopatches(rom_path, rom_base,
						rom_ext, rom);
	if (!patch_list) {
		patch_list = rom_win32_find_autopatches(patch_path,
							rom_base,
							rom_ext,
							rom);
	}
#endif

	free(patch_path);
	free(tmp);
	free(tmp2);

	return patch_list;
}

int rom_calculate_checksum(struct rom *rom)
{
	uint8_t *result;
	sha1nfo sha1;
	uint32_t crc;
	uint8_t *data;
	size_t size;

	data = rom->buffer + rom->offset;
	size = rom->info.total_prg_size + rom->info.total_chr_size;

	sha1_init(&sha1);

	crc = crc32buf(data, size, NULL);
	sha1_write(&sha1, (char *)data, size);

	result = sha1_result(&sha1);
	memcpy(rom->info.combined_sha1, result, 20);

	rom->info.combined_crc = crc;

	return 0;
}

void rom_get_info(struct rom *rom, struct text_buffer *tbuffer)
{
	int i;
	const char *mirror;
	size_t mapper_ram_size;
	uint32_t board_type;

	mirror = NULL;
	switch (rom->info.mirroring) {
	case MIRROR_H:
		mirror = "Horizontal";
		break;
	case MIRROR_V:
		mirror = "Vertical";
		break;
	case MIRROR_1A:
	case MIRROR_1B:
		mirror = "One-screen";
		break;
	case MIRROR_4:
		mirror = "Four-screen";
		break;
	case MIRROR_M:
		mirror = "Mapper-controlled";
		break;

	}

	board_type = rom->info.board_type;

	text_buffer_append(tbuffer, "ROM Information\n");
	text_buffer_append(tbuffer, "---------------\n");
	if (rom->filename)
		text_buffer_append(tbuffer, "Filename: %s\n", rom->filename);

	text_buffer_append(tbuffer, "Board Type: %s\n",
			   board_info_get_name(rom->board_info));

	if (BOARD_TYPE_IS_INES(rom->info.board_type)) {
		text_buffer_append(tbuffer, "iNES Mapper: %d\n",
		       BOARD_TYPE_TO_INES_MAPPER(rom->info.board_type));
		text_buffer_append(tbuffer, "NES 2.0 Submapper: %d\n",
		       BOARD_TYPE_TO_INES_SUBMAPPER(rom->info.board_type));
	}

	text_buffer_append(tbuffer, "System Type: %s\n",
			   emu_get_system_type_name(rom->info.system_type));

	if (board_type == BOARD_TYPE_NSF) {
		int expansion_sound;

		text_buffer_append(tbuffer, "\n");
		text_buffer_append(tbuffer, "NSF Information\n");
		text_buffer_append(tbuffer, "---------------\n");

		expansion_sound = nsf_get_expansion_sound(rom);

		text_buffer_append(tbuffer, "Title: %s\n",
				   nsf_get_title(rom));
		text_buffer_append(tbuffer, "Artist: %s\n",
				   nsf_get_artist(rom));
		text_buffer_append(tbuffer, "Copyright: %s\n",
				   nsf_get_copyright(rom));
		text_buffer_append(tbuffer, "Timing: %s\n",
				   nsf_get_region(rom));
		text_buffer_append(tbuffer, "Song count: %d\n",
				   nsf_get_song_count(rom));
		text_buffer_append(tbuffer, "First song: %d\n",
				   nsf_get_first_song(rom));

		if (expansion_sound) {
			int i;

			text_buffer_append(tbuffer, "Additional audio chips: ");
			for (i = 0; expansion_sound; i++) {
				if (expansion_sound & 1) {
					text_buffer_append(tbuffer, "%s%s",
					       nsf_expansion_audio_chips[i],
					       expansion_sound & 0x3e ?
					       ", " : "\n");
				}
				expansion_sound >>= 1;
			}
		}

		goto done;
	} else if (board_type == BOARD_TYPE_FDS) {
		fds_get_disk_info(rom, tbuffer);
		goto done;
	}

	if (mirror)
		text_buffer_append(tbuffer, "Mirroring: %s\n", mirror);

	text_buffer_append(tbuffer, "PRG Size: %d\n", rom->info.total_prg_size);

	if (rom->info.total_chr_size) {
		text_buffer_append(tbuffer, "CHR Size: %d\n",
				   rom->info.total_chr_size);
	}

	if (rom->info.wram_size[0]) {
		text_buffer_append(tbuffer, "WRAM 0 Size: %d%s\n",
				   rom->info.wram_size[0],
				   rom->info.flags & ROM_FLAG_WRAM0_NV ?
				   " (non-volatile)" : "");
	}

	if (rom->info.wram_size[1]) {
		text_buffer_append(tbuffer, "WRAM 1 Size: %d%s\n",
				   rom->info.wram_size[1],
				   rom->info.flags & ROM_FLAG_WRAM1_NV ?
				   " (non-volatile)" : "");
	}

	if (rom->info.vram_size[0]) {
		text_buffer_append(tbuffer, "VRAM 0 Size: %d%s\n",
				   rom->info.vram_size[0],
				   rom->info.flags & ROM_FLAG_VRAM0_NV ?
				   " (non-volatile)" : "");
	}

	if (rom->info.vram_size[1]) {
		text_buffer_append(tbuffer, "VRAM 1 Size: %d%s\n",
				   rom->info.vram_size[1],
				   rom->info.flags & ROM_FLAG_VRAM1_NV ?
				   " (non-volatile)" : "");
	}

	mapper_ram_size = board_info_get_mapper_ram_size(rom->board_info);
	if (mapper_ram_size) {
		text_buffer_append(tbuffer, "Mapper RAM Size: %lu%s\n",
				   mapper_ram_size,
				   rom->info.flags & ROM_FLAG_MAPPER_NV ?
				   " (non-volatile)" : "");
	}

	text_buffer_append(tbuffer, "CRC32: %08x\n", rom->info.combined_crc);
	text_buffer_append(tbuffer, "SHA-1: ");

	for (i = 0; i < 20; i++)
		text_buffer_append(tbuffer, "%02x", rom->info.combined_sha1[i]);
	text_buffer_append(tbuffer, "\n");

	text_buffer_append(tbuffer, "Present in ROM Database: %s\n",
			   (rom->info.flags & ROM_FLAG_IN_DB) ? "Yes" : "No");

done:
		
	printf("\n");
}

void print_rom_info(struct rom *rom)
{
	struct text_buffer *tbuffer;

	tbuffer = text_buffer_new();
	rom_get_info(rom, tbuffer);
	printf("%s", text_buffer_get_text(tbuffer));
	text_buffer_free(tbuffer);
}

