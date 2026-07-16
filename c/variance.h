/* (c) 2026 FRINKnet & Friends - MIT Licence */
#ifndef VARIANCE_H
#define VARIANCE_H

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

#ifndef VCONFIGURE
#define VCONFIGURE "../x/configure.x"
#endif /* VCONFIGURE */

#include VCONFIGURE

#define VMODE(mode, MODE) \
	X(mode, keywords,  const char**, V##MODE##_KEYWORDS) \
	X(mode, endpoint,    const char*,  V##MODE##_ENDPOINT) \
	X(mode, apikey,    const char*,  V##MODE##_APIKEY) \
	X(mode, model,     const char*,  V##MODE##_MODEL) \
	X(mode, instruct,  const char*,  V##MODE##_INSTRUCT) \
	X(mode, timeout,   int,          V##MODE##_TIMEOUT) \
	X(mode, tokens,    int,          V##MODE##_TOKENS) \
	X(mode, tries,     int,          V##MODE##_TRIES) \
	X(mode, pause,     int,          V##MODE##_PAUSE) \
	X(mode, temp,      double,       V##MODE##_TEMP) \

#define VMODEX \
	VMODE(core, CORE) \
	VMODE(info, INFO) \
	VMODE(mark, MARK) \
	VMODE(code, CODE) \
	VMODE(plan, PLAN)

/* Core Configuration */
#define X(mode, name, ctype, value) extern ctype v##mode##_##name;
VMODEX
#undef X

// populate a nutmeg context from act values
#define V_NUTMEG(ctx, act) do { \
	(ctx)->model     = v##act##_model     ? v##act##_model     : vcore_model; \
	(ctx)->endpoint  = v##act##_endpoint  ? v##act##_endpoint  : vcore_endpoint; \
	(ctx)->apikey    = v##act##_apikey    ? v##act##_apikey    : vcore_apikey; \
	(ctx)->timeout   = v##act##_timeout   ? v##act##_timeout   : vcore_timeout; \
	(ctx)->tokens    = v##act##_tokens    ? v##act##_tokens    : vcore_tokens; \
	(ctx)->tries     = v##act##_tries     ? v##act##_tries     : vcore_tries; \
	(ctx)->pause     = v##act##_pause     ? v##act##_pause     : vcore_pause; \
	(ctx)->temp      = v##act##_temp      ? v##act##_temp      : vcore_temp; \
} while(0);

#define VCALL_NUTJOB(TYPE, nut, rtn) do { \
	nutmeg_t _ctx = {0}; \
	V##TYPE##_NUTMEG(&_ctx); \
	(rtn) = nutjob(&_ctx, &nut); \
} while(0);

#define VWIPE_NUTJOB(nut) do { \
	free(nut.persona); \
	free(nut.evidence); \
	free(nut.analysis); \
	free(nut.nudging); \
	free(nut.updates); \
	free(nut.turnout); \
} while(0);

#define VRETURN_NUTJOB(TYPE, nut) do { \
	char *_rtn; \
	VCALL_NUTJOB(TYPE, nut, _rtn); \
	VWIPE_NUTJOB(nut); \
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

		if (!*buf) v_die(VFILE_NOTICE_DEAD);
	}

	size_t beg = *len;
	const char *p = s;

	while (*p) {
		if (*len >= *max - 2) {
			*max *= 2;
			*buf = (char *)realloc(*buf, *max);

			if (!*buf) v_die(VFILE_NOTICE_DEAD);
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
		if (!*buf) v_die(VFILE_NOTICE_DEAD);
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

				if (!*buf) v_die(VFILE_NOTICE_DEAD);
			}

			memcpy(*buf + *len, num, n);

			*len += n;
			newline = 0;
		}

		if (*len >= *max - 2) {
			*max *= 2;
			*buf = (char *)realloc(*buf, *max);

			if (!*buf) v_die(VFILE_NOTICE_DEAD);
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


/* Base Types */
typedef enum {
	V_FTYPE_NONE,
	#define X(lang, exts, mark) V_FTYPE_##lang,
	#include "../x/filetypes.x"
	#undef X
} vfiletype_t;

typedef enum {
	VINFO_NODATA,
	VINFO_SEARCH,
	VINFO_REVIEW,
	VINFO_DIGEST,
	VINFO_MEMORY,
	VINFO_FORGET,
	VINFO_GENIUS,
	VINFO_ANSWER
} vinfotype_t;

typedef struct {
	int   line;
	char  name[128];
	char  type[28];
	char  note[512];
} vtodo_t;

typedef struct {
	int   line;
	char  path[256];
	char  type[16];
	char  note[1024];
} vmark_t;

typedef struct {
    int done;
    char *task;
    char **files;
    int count;
} vstep_t;

typedef struct {
    char *title;
    char *goals;
    char *notes;
		vstep_t **steps;
    int count;
} vplan_t;

/* File IO */
vfiletype_t v_filetype(const char *filename);
const char *v_filelang(const char *filename);
const char *v_filemark(const char *filename);
int v_filelist(const char *src, char files[][256], int n);
char *v_fileload(const char *filename);
size_t v_filesave(const char *filename, const char *str);
char *v_fileinst(const char *filename, const char **kw, const char *inst);

/* Todo Handling */
const char* v_todoline(const char* line, const char* kw[], const char* mk);
int v_todofind(const char *src, const char *kw[], const char *mk, char **out);
int v_todoscan(const char *str, vtodo_t *todos, size_t max);
char *v_todomark(const char *orig, vtodo_t *todos, const char *mk, size_t cnt);

/* Infromation Subsystem */
char *v_infolist(const char **files, int n, const char *note);
char *v_infoshow(const char **files, int n, const char *note);
char *v_infoscan(const char **files, int n, const char *note);
char *v_infofind(const char *query);

/* File Marking */
int v_markscan(const char *src, const char *kw[], const char *mk, char **list);
char *v_marklist(const char **files, int n, const char **kw);
char *v_markeach(const char **files, int n, const char *goal);

/* Code Generation */
char *v_codefind(const char *quest);
char *v_codemove(const char *quest);
int v_codefile(const char *filename, const char *goal);

/* tTask Planning */
void v_planwipe(void);
void v_planfree(vplan_t *plan);
vplan_t *v_planload(void);
char *v_plantext(vplan_t *plan);
int v_plansave(vplan_t *plan);
int v_plannext(vplan_t *plan);
int v_planexec(vplan_t *plan);
char *v_planmore(const char *goal);
char *v_plantask(const char **files, int n, const char *goal);
char *v_plantodo(const char **files, int n, const char *goal);

/* Task Delegation */
extern char *v_taskinfo(const char **files, int n, const char *goal);
extern char *v_taskmark(const char **files, int n, const char *goal);
extern char *v_taskcode(const char **files, int n, const char *goal);
extern char *v_taskplan(const char **files, int n, const char *goal);
extern char *v_tasktodo(const char **files, int n, const char *goal);
extern char *v_taskauto(const char **files, int n, const char *goal);
extern char *v_tasknext(const char **files, int n, const char *goal);
extern char *v_taskdone(const char **files, int n, const char *goal);
extern char *v_tasktest(const char **files, int n, const char *goal);
extern char *v_taskedit(const char **files, int n, const char *goal);

#endif /* VARIANCE_H */
