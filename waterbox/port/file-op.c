/* file-op.c - QUASI88's file layer (src/osdepend/file-op.h) for the Chimera
 * core, modelled on upstream's src/osdepend/dummy/file-op.c.
 *
 * A core sees only the files the frontend mounted, by name, read-only: the
 * firmware under its id and the project's disks under their names. Each file
 * QUASI88 opens is read whole into guest memory the first time, and served
 * from there, so what the machine writes to a disk is written in memory: it
 * is part of every savestate, and it leaves as save data
 * (q88-driver.c), never onto the host's disk. Only ROMs and disks are
 * opened at all: QUASI88's configuration files, state files, snapshots,
 * tapes, printer and serial files are refused, as if they did not exist.
 *
 * Built natively too (run-native.c), where the folder the run is in plays
 * the part of the mounts. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quasi88.h"
#include "initval.h"
#include "file-op.h"
#include "q8tk.h"
#include "chimera-port.h"

/* ------------------------------------------------------------ the files */

#define MAX_FILES 64

typedef struct {
	char *name;
	uint8_t *data;
	long size, cap;
	int written;
} vfile;

static vfile g_files[MAX_FILES];
static int g_fileCount;

/* names that are not opened even when mounted (chimera_file_hide) */
#define MAX_HIDDEN 12
static char g_hidden[MAX_HIDDEN][32];
static int g_hiddenCount;

void chimera_file_hide(const char *name)
{
	if (g_hiddenCount < MAX_HIDDEN) {
		strncpy(g_hidden[g_hiddenCount], name, sizeof g_hidden[0] - 1);
		g_hiddenCount++;
	}
}

/* the file of that name, read in now if it is mounted; -1 if it is not */
static int vfile_find(const char *name)
{
	int i;
	FILE *f;
	long size;
	vfile *v;
	for (i = 0; i < g_hiddenCount; i++)
		if (strcmp(g_hidden[i], name) == 0) return -1;
	for (i = 0; i < g_fileCount; i++)
		if (strcmp(g_files[i].name, name) == 0) return i;
	if (g_fileCount >= MAX_FILES || !name[0]) return -1;
	f = fopen(name, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 0) { fclose(f); return -1; }
	v = &g_files[g_fileCount];
	v->name = (char *)malloc(strlen(name) + 1);
	v->data = (uint8_t *)malloc((size_t)(size > 0 ? size : 1));
	if (!v->name || !v->data) {
		free(v->name); free(v->data);
		fclose(f);
		return -1;
	}
	strcpy(v->name, name);
	if (size > 0 && fread(v->data, 1, (size_t)size, f) != (size_t)size) {
		free(v->name); free(v->data);
		fclose(f);
		return -1;
	}
	fclose(f);
	v->size = v->cap = size;
	v->written = 0;
	return g_fileCount++;
}

int chimera_file_exists(const char *name) { return vfile_find(name) >= 0; }
int chimera_file_count(void) { return g_fileCount; }
const char *chimera_file_name(int i) { return i >= 0 && i < g_fileCount ? g_files[i].name : NULL; }
int chimera_file_written(int i) { return i >= 0 && i < g_fileCount && g_files[i].written; }
int64_t chimera_file_size(int i) { return i >= 0 && i < g_fileCount ? g_files[i].size : 0; }
const uint8_t *chimera_file_data(int i) { return i >= 0 && i < g_fileCount ? g_files[i].data : NULL; }

/* ------------------------------------------------------------ settings */

int osd_file_config_init(void) { return TRUE; }
void osd_file_config_exit(void) {}

/* Every folder is the one the mounts are in. */
const char *osd_dir_cwd(void) { return ""; }
const char *osd_dir_rom(void) { return ""; }
const char *osd_dir_disk(void) { return ""; }
const char *osd_dir_tape(void) { return ""; }
const char *osd_dir_snap(void) { return ""; }
const char *osd_dir_state(void) { return ""; }
const char *osd_dir_gcfg(void) { return ""; }
const char *osd_dir_lcfg(void) { return ""; }
int osd_set_dir_cwd(const char *d) { (void)d; return FALSE; }
int osd_set_dir_rom(const char *d) { (void)d; return FALSE; }
int osd_set_dir_disk(const char *d) { (void)d; return FALSE; }
int osd_set_dir_tape(const char *d) { (void)d; return FALSE; }
int osd_set_dir_snap(const char *d) { (void)d; return FALSE; }
int osd_set_dir_state(const char *d) { (void)d; return FALSE; }
int osd_set_dir_gcfg(const char *d) { (void)d; return FALSE; }
int osd_set_dir_lcfg(const char *d) { (void)d; return FALSE; }

int osd_kanji_code(void) { return Q8TK_KANJI_ANK; }

/* ------------------------------------------------------------ streams */

struct OSD_FILE_STRUCT {
	int used;
	int file;		/* index in g_files */
	int type;
	int writable;
	long pos;
	char mode[4];
};

#define MAX_STREAM 8
static OSD_FILE osd_stream[MAX_STREAM];

OSD_FILE *osd_fopen(int type, const char *path, const char *mode)
{
	int i, f, writable;
	OSD_FILE *st = NULL;

	switch (type) {
	case FTYPE_ROM:
	case FTYPE_DISK:
	case FTYPE_READ:
		break;
	default:
		/* configuration, state, snapshot, tape, printer, serial and key
		 * record files: the core has none */
		return NULL;
	}
	if (strchr(mode, 'w') || strchr(mode, 'a')) return NULL;
	writable = strchr(mode, '+') != NULL && type == FTYPE_DISK;

	/* a disk already open is shared, as upstream's file layer does it:
	 * two drives holding disks of one D88 file read one file */
	if (type == FTYPE_DISK) {
		for (i = 0; i < MAX_STREAM; i++)
			if (osd_stream[i].used && osd_stream[i].type == type &&
			    strcmp(g_files[osd_stream[i].file].name, path) == 0) {
				if (strcmp(osd_stream[i].mode, mode) == 0) return &osd_stream[i];
				return NULL;
			}
	}

	for (i = 0; i < MAX_STREAM; i++)
		if (!osd_stream[i].used) { st = &osd_stream[i]; break; }
	if (!st) return NULL;
	f = vfile_find(path);
	if (f < 0) return NULL;
	st->used = 1;
	st->file = f;
	st->type = type;
	st->writable = writable;
	st->pos = 0;
	strncpy(st->mode, mode, sizeof st->mode - 1);
	st->mode[sizeof st->mode - 1] = '\0';
	return st;
}

int osd_fclose(OSD_FILE *stream)
{
	if (!stream || !stream->used) return EOF;
	stream->used = 0;
	return 0;
}

int osd_fflush(OSD_FILE *stream) { (void)stream; return 0; }

int osd_fseek(OSD_FILE *stream, long offset, int whence)
{
	long base;
	if (!stream || !stream->used) return -1;
	switch (whence) {
	case SEEK_SET: base = 0; break;
	case SEEK_CUR: base = stream->pos; break;
	case SEEK_END: base = g_files[stream->file].size; break;
	default: return -1;
	}
	if (base + offset < 0) return -1;
	stream->pos = base + offset;
	return 0;
}

long osd_ftell(OSD_FILE *stream)
{
	return stream && stream->used ? stream->pos : -1;
}

void osd_rewind(OSD_FILE *stream)
{
	(void)osd_fseek(stream, 0L, SEEK_SET);
}

size_t osd_fread(void *ptr, size_t size, size_t nobj, OSD_FILE *stream)
{
	vfile *v;
	size_t want, have;
	if (!stream || !stream->used || size == 0) return 0;
	v = &g_files[stream->file];
	want = size * nobj;
	have = stream->pos < v->size ? (size_t)(v->size - stream->pos) : 0;
	if (want > have) want = have - have % size;
	memcpy(ptr, v->data + stream->pos, want);
	stream->pos += (long)want;
	return want / size;
}

size_t osd_fwrite(const void *ptr, size_t size, size_t nobj, OSD_FILE *stream)
{
	vfile *v;
	size_t n;
	long end;
	if (!stream || !stream->used || !stream->writable || size == 0) return 0;
	v = &g_files[stream->file];
	n = size * nobj;
	end = stream->pos + (long)n;
	if (end > v->cap) {
		long cap = v->cap ? v->cap : 4096;
		uint8_t *p;
		while (cap < end) cap *= 2;
		p = (uint8_t *)realloc(v->data, (size_t)cap);
		if (!p) return 0;
		memset(p + v->cap, 0, (size_t)(cap - v->cap));
		v->data = p;
		v->cap = cap;
	}
	if (stream->pos > v->size) memset(v->data + v->size, 0, (size_t)(stream->pos - v->size));
	memcpy(v->data + stream->pos, ptr, n);
	stream->pos = end;
	if (end > v->size) v->size = end;
	v->written = 1;
	return nobj;
}

int osd_fputc(int c, OSD_FILE *stream)
{
	unsigned char b = (unsigned char)c;
	return osd_fwrite(&b, 1, 1, stream) == 1 ? c : EOF;
}

int osd_fgetc(OSD_FILE *stream)
{
	unsigned char b;
	return osd_fread(&b, 1, 1, stream) == 1 ? b : EOF;
}

char *osd_fgets(char *str, int size, OSD_FILE *stream)
{
	int i = 0, c;
	if (size <= 0) return NULL;
	while (i < size - 1 && (c = osd_fgetc(stream)) != EOF) {
		str[i++] = (char)c;
		if (c == '\n') break;
	}
	if (i == 0) return NULL;
	str[i] = '\0';
	return str;
}

int osd_fputs(const char *str, OSD_FILE *stream)
{
	size_t n = strlen(str);
	return osd_fwrite(str, 1, n, stream) == n ? 0 : EOF;
}

/* ------------------------------------------------------------ the rest */

int osd_file_stat(const char *pathname)
{
	return vfile_find(pathname) >= 0 ? FILE_STAT_FILE : FILE_STAT_NOEXIST;
}

T_DIR_INFO *osd_opendir(const char *filename) { (void)filename; return NULL; }
T_DIR_ENTRY *osd_readdir(T_DIR_INFO *dirp) { (void)dirp; return NULL; }
void osd_closedir(T_DIR_INFO *dirp) { (void)dirp; }

int osd_path_normalize(const char *path, char resolved_path[], int size)
{
	if ((int)strlen(path) >= size) return FALSE;
	strcpy(resolved_path, path);
	return TRUE;
}

int osd_path_split(const char *path, char dir[], char file[], int size)
{
	if ((int)strlen(path) >= size) return FALSE;
	dir[0] = '\0';
	strcpy(file, path);
	return TRUE;
}

int osd_path_join(const char *dir, const char *file, char path[], int size)
{
	(void)dir;
	if ((int)strlen(file) >= size) return FALSE;
	strcpy(path, file);
	return TRUE;
}
