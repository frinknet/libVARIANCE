/* (c) 2026 FRINKnet & Friends - MIT Licence */
#ifndef VARIANCE_H
#define VARIANCE_H

/* libVARIANCE - Vibecode Attention Records for Internal Analysis Notes and Code Exegesis */
/* Header-only: #include "variance.h" and you're done — no .c to compile. */
/* Requires: libPEANUTS (peanuts.h) */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "peanuts.h"

extern FILE *vcore_out;
extern FILE *vcore_err;

/* Core Configuration */
#define X(mode, name, ctype, value) extern ctype v##mode##_##name;
#include "../x/prompts.x"
#undef X

// populate a nutmeg context from act values
#define V_NUTMEG(ctx, act) do { \
	(ctx)->model     = v##act##_model    ? v##act##_model    : vcore_model; \
	(ctx)->endpoint  = v##act##_apiurl   ? v##act##_apiurl   : vcore_apiurl; \
	(ctx)->gatekey   = v##act##_apikey   ? v##act##_apikey   : vcore_apikey; \
	(ctx)->timeout   = v##act##_timeout  ? v##act##_timeout  : vcore_timeout; \
	(ctx)->tokens    = v##act##_tokens   ? v##act##_tokens   : vcore_tokens; \
	(ctx)->tries     = v##act##_tries    ? v##act##_tries    : vcore_tries; \
	(ctx)->pause     = v##act##_pause    ? v##act##_pause    : vcore_pause; \
	(ctx)->temp      = v##act##_temp     ? v##act##_temp     : vcore_temp; \
} while(0);

#define VCALL_NUTJOB(TYPE, nut, rtn) do { \
	nutmeg_t _ctx = {0}; \
	V##TYPE##_NUTMEG(&_ctx); \
	(rtn) = nutjob(&_ctx, &nut); \
} while(0);

#define VRETURN_NUTJOB(TYPE, nut) do { \
	char *_rtn; \
	VCALL_NUTJOB(TYPE, nut, _rtn); \
	free((void*)nut.persona); \
	free((void*)nut.evidence); \
	free((void*)nut.analysis); \
	free((void*)nut.nudging); \
	free((void*)nut.updates); \
	free((void*)nut.turnout); \
	return _rtn; \
} while(0)

#ifndef VINFO_NUTMEG
#define VINFO_NUTMEG(ctx) V_NUTMEG(ctx, info)
#endif

#ifndef VMARK_NUTMEG
#define VMARK_NUTMEG(ctx) V_NUTMEG(ctx, mark)
#endif

#ifndef VCODE_NUTMEG
#define VCODE_NUTMEG(ctx) V_NUTMEG(ctx, code)
#endif

#ifndef VPLAN_NUTMEG
#define VPLAN_NUTMEG(ctx) V_NUTMEG(ctx, plan)
#endif

// print formatted output to vcore_out
static inline void v_out(const char *fmt, ...) {
	char outmsg[1024];
	va_list args;

	if (vcore_out == NULL) return;

	va_start(args, fmt);
	vsnprintf(outmsg, 1024, fmt, args);
	fprintf(vcore_out, "%s\n\n", outmsg);
	va_end(args);
}

// print formatted output to vcore_err
static inline void v_log(const char *fmt, va_list args) {
	char errmsg[1024];

	if (vcore_err == NULL) return;

	vsnprintf(errmsg, 1024, fmt, args);
	fprintf(vcore_err, "%s\n", errmsg);

	errno = 0;
}

// print a formatted error message
static inline void *v_err(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	v_log(fmt, args);
	va_end(args);

	return NULL;
}

static inline void v_die(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	v_log(fmt, args);
	va_end(args);

	exit(1);
}

/* Buffer Operations */
static inline size_t v_bufwrite(char **buf, size_t *len, const char *s, size_t *max) {
	if (!*buf) {
		*len = 0;
		*max = 128;
		*buf = malloc(*max);

		if (!*buf) v_die("Out of memory.");
	}

	size_t beg = *len;
	const char *p = s;

	while (*p) {
		if (*len >= *max - 2) {
			*max *= 2;
			*buf = (char *)realloc(*buf, *max);

			if (!*buf) v_die("Out of memory.");
		}

		(*buf)[(*len)++] = *p++;
	}

	(*buf)[*len] = '\0';

	return (*len) - beg;
}

static inline size_t v_bufstream(char **buf, size_t *len, FILE *s, size_t *max, int lineno) {
	if (!*buf) {
		*len = 0;
		*max = 128;
		*buf = malloc(*max);
		if (!*buf) v_die("Out of memory.");
	}

	size_t beg = *len;
	int c;
	int newline = 1;
	char num[8];
	int n;

	while ((c = getc(s)) != EOF) {
		if (lineno && newline) {
			n = snprintf(num, sizeof(num), "%d ", lineno++);

			if (*len + n >= *max - 2) {
				*max = (*len + n + 16) * 2;
				*buf = (char *)realloc(*buf, *max);

				if (!*buf) v_die("Out of memory.");
			}

			memcpy(*buf + *len, num, n);

			*len += n;
			newline = 0;
		}

		if (*len >= *max - 2) {
			*max *= 2;
			*buf = (char *)realloc(*buf, *max);

			if (!*buf) v_die("Out of memory.");
		}

		(*buf)[(*len)++] = (char)c;

		if (c == '\n') newline = 1;
	}

	(*buf)[*len] = '\0';

	return (*len) - beg;
}

/* Tag Replacer */
static inline char *v_tagfill(char* tpl, const char* tag, const char* val) {
	size_t tag_len = strlen(tag);
	size_t val_len = strlen(val);
	char *rtn = tpl;
	char *pos;

	while ((pos = strstr(rtn, tag)) != NULL) {
		char *tmp = malloc(strlen(rtn) - tag_len + val_len + 1);

		if (!tmp) return NULL;

		size_t pre_len = pos - rtn;

		memcpy(tmp, rtn, pre_len);
		memcpy(tmp + pre_len, val, val_len);
		strcpy(tmp + pre_len + val_len, pos + tag_len);
		free(rtn);

		rtn = tmp;
	}

	return rtn;
}

/* Code Fences */
static inline int vfence_chk(const char *buf) {
	const char *p = buf;

	while (*p && isspace(*p)) p++;

	if (strncmp(p, "```", 3) != 0) return 0;

	const char *last = buf + strlen(buf) - 1;

	while (last > buf && isspace(*last)) last--;

	if (last - buf < 3) return 0;
	if (strncmp(last - 2, "```", 3) == 0) return 1;

	return 0;
}

static inline char *vfence_clr(char *buf) {
	char *p = buf;

	while (*p && isspace(*p)) p++;

	if (!*p || strncmp(p, "```", 3) != 0) return NULL;

	p = strchr(p, '\n');

	if (!p) return NULL;

	p++;

	size_t len = strlen(buf);
	char *q = buf + len;

	while (q > buf) {
		q--;

		if (*q == '\n') break;
	}

	if (q == buf && *q != '\n') return NULL;

	*q = '\0';

	return p;
}

/* File IO */
typedef enum {
	V_FTYPE_NONE,
	#define X(lang, exts, mark) V_FTYPE_##lang,
	#include "../x/filetypes.x"
	#undef X
} vfiletype_t;

vfiletype_t v_filetype(const char *filename);
const char *v_filelang(const char *filename);
const char *v_filemark(const char *filename);
int v_filelist(const char *src, char files[][256], int n);
char *v_fileload(const char *filename);
size_t v_filesave(const char *filename, const char *str);
char *v_fileinst(const char *filename, const char **kw, const char *inst);

/* Todo Handling */
typedef struct {
	int   line;
	char  name[128];
	char  type[28];
	char  note[512];
} vtodo_t;

const char* v_todoline(const char* line, const char* kw[], const char* mk);
int v_todofind(const char *src, const char *kw[], const char *mk, char **out);
int v_todoscan(const char *str, vtodo_t *todos, size_t max);
char *v_todomark(const char *orig, vtodo_t *todos, const char *mk, size_t cnt);

/* Infromation Subsystem */
typedef enum {
	VINFO_SEARCH,
	VINFO_REVIEW,
	VINFO_DIGEST,
	VINFO_MEMORY,
	VINFO_FORGET,
	VINFO_GENIUS,
	VINFO_ANSWER,
	VINFO_OTHERS
} vinfotype_t;

#define INFO_TEMPLATES \
	"Respond with these lines and I'll help you:\n" \
	"\n" \
	"- `search|path|words`    Locate words in the code\n" \
	"- `review|path|query`    Inspect source from a file\n" \
	"- `digest|path|notes`    Write a digest of the file\n" \
	"- `memory|topic|thought` Save a memory for future you\n" \
	"- `forget|topic|thought` Remove a memory for future you\n" \
	"- `genius|source|query`   Get advice from top experts\n" \
	"- `answer|subject|ideas` Explain your thought to the user\n" \
	"\n" \
	"Lines must be this format or I'll discard them..."

char *v_infolist(const char **files, int n, const char *note);
char *v_infoshow(const char **files, int n, const char *note);
char *v_infoscan(const char **files, int n, const char *note);
char *v_infofind(const char *query);

/* File Marking */
#define MARK_MAX 128

typedef struct {
	int   line;
	char  path[256];
	char  type[16];
	char  note[1024];
} vmark_t;

char *v_marklist(const char **files, int n, const char **kw);
char *v_markeach(const char **files, int n, const char *goal);

/* Code Generation */
#define CODE_TEMPLATES \
	"Respond with as many of these lines as you need:\n" \
	"\n" \
	"`search|path|terms` - Search a portion of the codebase\n" \
	"`review|path|notes` - Show the source code of a file\n" \
	"`digest|path|notes` - Write a digest of the file\n" \
	"`memory|topic|notes` - Save a memory for future you\n" \
	"`forget|topic|notes` - Remove a memory for future you\n" \
	"`answer|path|notes` - Explain why the file fits\n" \
	"\n" \
	"Any other lines will be discarded..."

int v_codescan(const char *src, const char *kw[], const char *mk, char **list);
char *v_codefind(const char *quest);
char *v_codemove(const char *quest);
int v_codefile(const char *filename, const char *goal);

/* Task Delegation */
extern char *v_taskinfo(const char **files, int n, const char *goal);
extern char *v_taskmark(const char **files, int n, const char *goal);
extern char *v_taskcode(const char **files, int n, const char *goal);
extern char *v_taskplan(const char **files, int n, const char *goal);
extern char *v_tasktodo(const char **files, int n, const char *goal);
extern char *v_tasknext(const char **files, int n, const char *goal);
extern char *v_tasktest(const char **files, int n, const char *goal);
extern char *v_taskedit(const char **files, int n, const char *goal);

/* Internal Tools */
static inline void _vtool_dive(const char *path, char **buf, size_t *len, size_t *max, int depth) {
	DIR *dir;
	struct dirent *entry;
	struct stat statbuf;

	if (!(dir = opendir(path))) return;

	while ((entry = readdir(dir)) != NULL) {
		char fullpath[1024];

		snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

		if (lstat(fullpath, &statbuf) < 0) continue;
		if (S_ISDIR(statbuf.st_mode)) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			if (entry->d_name[0] == '.') continue; // skip hidden dirs
			if (depth > 3) continue; // max depth

			_vtool_dive(fullpath, buf, len, max, depth + 1);
		} else {
			if (entry->d_name[0] == '.') continue; // skip hidden files

			v_bufwrite(buf, len, fullpath, max);
			v_bufwrite(buf, len, "\n", max);
		}
	}

	closedir(dir);
}

static inline char *_vtool_find(const char *path) {
	char *buf = NULL;
	size_t len = 0;
	size_t max = 128;

	_vtool_dive(path ? path : ".", &buf, &len, &max, 0);

	if (buf) buf[len] = '\0';

	return buf;
}

static inline char *_vtool_grep(const char *term, const char *path) {
	char *files = _vtool_find(path);

	if (!files) return NULL;

	char *result = NULL;
	size_t rlen = 0;
	size_t rmax = 1024;
	int found_any = 0;
	char *fline = files;
	char *fnext;

	while ((fnext = strchr(fline, '\n')) != NULL) {
		*fnext = '\0';

		if (strlen(fline) > 0) {
			char *content = v_fileload(fline);

			if (content) {
				if (strstr(content, term)) {
					if (!found_any) found_any = 1;

					v_bufwrite(&result, &rlen, fline, &rmax);
					v_bufwrite(&result, &rlen, "\n", &rmax);
				}

				free(content);
			}
		}

		fline = fnext + 1;
	}

	free(files);

	if (!found_any) {
		free(result);

		return NULL;
	}

	return result;
}

#endif /* VARIANCE_H */
