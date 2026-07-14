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
#include "peanuts.h"

/* ========================================================================
 * Configuration Hooks
 * ======================================================================== */

#ifndef VINFO_NUTMEG
#define VINFO_NUTMEG(ctx) do { \
    (ctx)->model = "openrouter/free"; \
    (ctx)->endpoint = "https://openrouter.ai/api/v1/chat/completions"; \
    (ctx)->gatekey = NULL; \
    (ctx)->timeout = 9000; \
    (ctx)->tokens = 4000; \
    (ctx)->tries = 10; \
    (ctx)->pause = 3; \
    (ctx)->temp = 0.3; \
} while(0)
#endif

#ifndef VMARK_NUTMEG
#define VMARK_NUTMEG(ctx) do { \
    (ctx)->model = "openrouter/free"; \
    (ctx)->endpoint = "https://openrouter.ai/api/v1/chat/completions"; \
    (ctx)->gatekey = NULL; \
    (ctx)->timeout = 9000; \
    (ctx)->tokens = 4000; \
    (ctx)->tries = 10; \
    (ctx)->pause = 3; \
    (ctx)->temp = 0.3; \
} while(0)
#endif

#ifndef VCODE_NUTMEG
#define VCODE_NUTMEG(ctx) do { \
    (ctx)->model = "openrouter/free"; \
    (ctx)->endpoint = "https://openrouter.ai/api/v1/chat/completions"; \
    (ctx)->gatekey = NULL; \
    (ctx)->timeout = 9000; \
    (ctx)->tokens = 8000; \
    (ctx)->tries = 10; \
    (ctx)->pause = 3; \
    (ctx)->temp = 0.1; \
} while(0)
#endif

/* ========================================================================
 * Error Handling
 * ======================================================================== */

static inline void v_errout(const char *fmt, va_list args) {
    char errmsg[1024];
    vsnprintf(errmsg, 1024, fmt, args);
    fprintf(stderr, "%s\n", errmsg);
    errno = 0;
}

static inline void *v_errfmt(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    v_errout(fmt, args);
    va_end(args);
    return NULL;
}

static inline void v_errdie(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    v_errout(fmt, args);
    va_end(args);
    exit(1);
}

/* ========================================================================
 * File type detection
 * ======================================================================== */

typedef enum {
    #define X(lang, exts, mark) V_FTYPE_##lang,
    #include "filetypes.x"
    #undef X
    V_FTYPE_NONE
} v_ftype;

static inline v_ftype v_filetype(const char *filename) {
    const char *ext = strrchr(filename, '.');
    const char *extstr = ext ? ext : "";
    #define X(lang, exts, mark) if ((*extstr && strstr(exts, extstr)) || strstr(exts, filename)) return V_FTYPE_##lang;
    #include "filetypes.x"
    #undef X
    return V_FTYPE_NONE;
}

static inline const char *v_filelang(const char *filename) {
    v_ftype t = v_filetype(filename);
    switch(t) {
        #define X(lang, exts, mark) case V_FTYPE_##lang: return #lang;
        #include "filetypes.x"
        #undef X
    default: return "text";
    }
}

static inline const char *v_filemark(const char *filename) {
    v_ftype t = v_filetype(filename);
    switch(t) {
        #define X(lang, exts, mark) case V_FTYPE_##lang: return mark;
        #include "filetypes.x"
        #undef X
    default: return "";
    }
}

/* ========================================================================
 * Buffer operations
 * ======================================================================== */

static inline size_t v_bufwrite(char **buf, size_t *len, const char *s, size_t *max) {
    if (!*buf) {
        *len = 0;
        *max = 128;
        *buf = malloc(*max);
        if (!*buf) v_errdie("Out of memory.");
    }
    size_t beg = *len;
    const char *p = s;
    while (*p) {
        if (*len >= *max - 2) {
            *max *= 2;
            *buf = (char *)realloc(*buf, *max);
            if (!*buf) v_errdie("Out of memory.");
        }
        (*buf)[(*len)++] = *p++;
    }
    (*buf)[*len] = '\0';
    return (*len) - beg;
}

static inline size_t v_bufstream(char **buf, size_t *len, FILE *s, size_t *max) {
    if (!*buf) {
        *len = 0;
        *max = 128;
        *buf = malloc(*max);
        if (!*buf) v_errdie("Out of memory.");
    }
    size_t beg = *len;
    int c;
    while ((c = getc(s)) != EOF) {
        (*buf)[(*len)++] = (char)c;
        if (*len >= *max - 2) {
            *max *= 2;
            *buf = (char *)realloc(*buf, *max);
            if (!*buf) v_errdie("Out of memory.");
        }
    }
    (*buf)[*len] = '\0';
    return (*len) - beg;
}

static inline size_t v_buflines(char **buf, size_t *len, const char *s, size_t *max) {
    if (!*buf) {
        *len = 0;
        *max = 128;
        *buf = malloc(*max);
        if (!*buf) v_errdie("Out of memory.");
    }
    size_t beg = *len;
    const char *p = s;
    int lineno = 1;
    char num[16];
    int n = snprintf(num, sizeof(num), "%d ", lineno);
    v_bufwrite(buf, len, num, max);
    while (*p) {
        if (*len >= *max - 20) {
            *max *= 2;
            *buf = (char *)realloc(*buf, *max);
            if (!*buf) v_errdie("Out of memory.");
        }
        (*buf)[(*len)++] = *p;
        if (*p == '\n') {
            n = snprintf(num, sizeof(num), "%d ", ++lineno);
            memcpy(*buf + *len, num, n);
            *len += n;
        }
        p++;
    }
    (*buf)[*len] = '\0';
    return (*len) - beg;
}

static inline char *v_tagfill(char* tpl, const char* tag, const char* val) {
    if (!val) val = "";
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

static inline int v_isfenced(const char *buf) {
    const char *p = buf;
    while (*p && isspace(*p)) p++;
    if (strncmp(p, "```", 3) != 0) return 0;
    const char *last = buf + strlen(buf) - 1;
    while (last > buf && isspace(*last)) last--;
    if (last - buf < 3) return 0;
    if (strncmp(last - 2, "```", 3) == 0) return 1;
    return 0;
}

static inline char *v_unfenced(char *buf) {
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

/* ========================================================================
 * File I/O and Walking
 * ======================================================================== */

static inline int v_filelist(const char *src, char files[][256], int n) {
    int i = 0;
    const char *p = src;
    while (*p && i < n) {
        const char *start = p;
        while (*p && *p != '\n' && *p != '|') p++;
        if (p > start) {
            int len = p - start;
            if (len >= 256) len = 255;
            memcpy(files[i], start, len);
            files[i][len] = '\0';
            i++;
        }
        if (*p == '|' || *p == '\n') p++;
    }
    return i;
}

static inline char *v_fileload(const char *filename) {
    FILE *f;
    if (strcmp(filename, "-") == 0) f = stdin;
    else f = fopen(filename, "r");

    if (!f) {
        v_errfmt("Cannot find `%s`\n", filename);
        return NULL;
    }

    char *buf = NULL;
    size_t cap = 128;
    size_t len = 0;

    v_bufstream(&buf, &len, f, &cap);

    if (ferror(f)) {
        v_errfmt("Cannot read `%s`\n", filename);
        if (f != stdin) fclose(f);
        free(buf);
        return NULL;
    }

    if (f != stdin) fclose(f);
    return buf;
}

static inline size_t v_filesave(const char *filename, char *str) {
    FILE *f = strcmp(filename, "-") ? fopen(filename, "w") : stdout;
    if (!f) {
        perror(filename);
        return (size_t)-1;
    }

    size_t len = strlen(str), n = fwrite(str, 1, len, f);
    if (n != len || ferror(f)) {
        if (f != stdout) fclose(f);
        return (size_t)-1;
    }
    if (f != stdout) fclose(f);
    return n;
}

static inline char *v_fileinst(const char *filename, const char **kw, const char *inst) {
    char *language = strdup(v_filelang(filename));
    char *instruct = strdup(inst);
    char keywords[1024];
    char *p = keywords;

    if (!instruct) return NULL;

    while (*kw) {
        const char *word = *kw;
        int len = strlen(word);
        int need = len + (p != keywords ? 2 : 0);
        if (p + need > keywords + sizeof(keywords) - 1) break;
        if (p != keywords) {
            *p++ = ',';
            *p++ = ' ';
        }
        memcpy(p, word, len);
        p += len;
        kw++;
    }
    *p = '\0';

    for (p = language; *p; p++) *p = toupper((unsigned char)*p);

    instruct = v_tagfill(instruct, "{{language}}", language);
    instruct = v_tagfill(instruct, "{{keywords}}", keywords);

    free(language);
    return instruct;
}

// Recursive directory walk (replaces 'find')
static inline void _v_walk_dir(const char *path, char **buf, size_t *len, size_t *max, int depth) {
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
            if (entry->d_name[0] == '.') continue; // Skip hidden dirs
            if (depth > 3) continue; // Max depth
            _v_walk_dir(fullpath, buf, len, max, depth + 1);
        } else {
            if (entry->d_name[0] == '.') continue; // Skip hidden files
            v_bufwrite(buf, len, fullpath, max);
            v_bufwrite(buf, len, "\n", max);
        }
    }
    closedir(dir);
}

static inline char *v_walk(const char *path) {
    char *buf = NULL;
    size_t len = 0;
    size_t max = 128;
    _v_walk_dir(path ? path : ".", &buf, &len, &max, 0);
    if (buf) buf[len] = '\0';
    return buf;
}

// Internal search (replaces 'grep')
static inline char *v_search(const char *path, const char *term) {
    char *content = v_fileload(path);
    if (!content) return NULL;

    char *buf = NULL;
    size_t len = 0;
    size_t max = 128;
    char *line = content;
    char *next;
    int found = 0;

    while ((next = strchr(line, '\n')) != NULL) {
        *next = '\0';
        if (strstr(line, term)) {
            if (!found) {
                v_bufwrite(&buf, &len, path, &max);
                v_bufwrite(&buf, &len, ":\n", &max);
                found = 1;
            }
            v_bufwrite(&buf, &len, line, &max);
            v_bufwrite(&buf, &len, "\n", &max);
        }
        line = next + 1;
    }
    // Check last line if no trailing newline
    if (found && strlen(line) > 0 && strstr(line, term)) {
         v_bufwrite(&buf, &len, line, &max);
         v_bufwrite(&buf, &len, "\n", &max);
    }

    free(content);
    if (!found) {
        free(buf);
        return NULL;
    }
    return buf;
}

/* ========================================================================
 * TODO/Mark structures
 * ======================================================================== */

typedef struct {
    int   line;
    char  name[128];
    char  type[28];
    char  note[512];
} v_todo_t;

typedef struct {
    int   line;
    char  path[256];
    char  type[16];
    char  note[1024];
} v_mark_t;

static inline const char* v_todoline(const char* line, const char* kw[], const char* mk) {
    const char *p = line;
    char st[16];
    int ml = 0;
    while (mk[ml] && mk[ml] != ' ' && ml < 15) ml++;
    strncpy(st, mk, ml);
    st[ml] = '\0';
    if (!ml) return NULL;

    while (*p && isblank(*p)) p++;
    if (strncmp(p, st, ml) == 0) {
        p += ml;
        while (*p && isblank(*p)) p++;
        for (int k = 0; kw[k] != NULL; k++) {
            const char *key = kw[k];
            int kl = strlen(key);
            if (strncmp(p, key, kl) == 0 && p[kl] == ':') return p;
        }
    }
    return NULL;
}

static inline int v_todofind(const char *src, const char *kw[], const char *mk, char **out) {
    char *buf = NULL;
    size_t cap = 128;
    size_t len = 0;
    const char *line = src;
    const char *next;
    const char *todo;
    int cnt = 0;

    v_bufwrite(&buf, &len, "I found these problems:\n\n", &cap);

    while (*line) {
        todo = v_todoline(line, kw, mk);
        next = strchr(line, '\n');
        if (!next) next = line + strlen(line);

        if (todo) {
            char tmp[1024];
            cnt++;
            snprintf(tmp, sizeof(tmp), "%d. %.*s\n", cnt, (int)(next - todo), todo);
            v_bufwrite(&buf, &len, tmp, &cap);
        }
        if (!*next) break;
        line = next + 1;
    }

    if (cnt == 0) {
        free(buf);
        return 0;
    }

    v_bufwrite(&buf, &len, "\nWant me to fix them?", &cap);

    if (out) {
        free(*out);
        *out = buf;
    } else {
        free(buf);
    }
    return cnt;
}

static inline int v_todoscan(const char *str, v_todo_t *todos, size_t max) {
    const char *p = str, *end = str + strlen(str);
    int cnt = 0;
    int adv = 0;

    while (cnt < max && p < end) {
        v_todo_t *e = &todos[cnt];
        int n = sscanf(p, "%d|%127[^ |]|%27[A-Z]|%511[^\n]%n", &e->line, e->name, e->type, e->note, &adv);
        if (n != 4) return 0;
        cnt++;
        p += adv;
        if (!*p) break;
        p++;
    }
    return cnt;
}

static inline char *v_todomark(const char *orig, v_todo_t *todos, const char *mk, size_t cnt) {
    if (!orig || !todos || !mk || cnt == 0) return NULL;

    int len1 = 0;
    while (mk[len1] && mk[len1] != ' ') len1++;
    const char *mk2 = mk + len1;
    if (*mk2 == ' ') mk2++;

    char *buf = NULL;
    size_t cap = 128;
    size_t len = 0;
    const char *in = orig;
    int line = 1;
    size_t t = 0;

    while (*in) {
        while (t < cnt && todos[t].line == line) {
            char tmp[1024];
            int n = snprintf(tmp, sizeof(tmp), "%s %s: %s %s\n", mk, todos[t].type, todos[t].note, mk2);
            if (n > 0) v_bufwrite(&buf, &len, tmp, &cap);
            t++;
        }

        const char *eol = strchr(in, '\n');
        size_t linelen = eol ? (size_t)(eol - in) : strlen(in);

        // Bulk write line
        if (*len + linelen + 2 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        memcpy(buf + *len, in, linelen);
        *len += linelen;

        if (eol) {
            buf[(*len)++] = '\n';
            in = eol + 1;
            line++;
        } else {
            break;
        }
    }
    buf[*len] = '\0';

    while (t < cnt) {
        char tmp[1024];
        int n = snprintf(tmp, sizeof(tmp), "%s LINE %d - %s: %s %s\n", mk, todos[t].line, todos[t].type, todos[t].note, mk2);
        if (n > 0) v_bufwrite(&buf, &len, tmp, &cap);
        t++;
    }
    return buf;
}

/* ========================================================================
 * Private info helpers (internal: _vinfo_* prefix)
 * ======================================================================== */

static inline char *_vinfo_refresh(const char *code) {
    return strdup(code);
}

static inline int _vinfo_search(char **buf, size_t *len, size_t *max, const char *path, const char *terms) {
    char head[1024];
    snprintf(head, sizeof(head), "# LOCATE: `%s` in %s\n\n", terms, path);
    v_bufwrite(buf, len, head, max);

    char *result = v_search(path, terms);
    if (result) {
        v_bufwrite(buf, len, result, max);
        free(result);
    } else {
        v_bufwrite(buf, len, "(No matches found)\n", max);
    }
    v_bufwrite(buf, len, "\n", max);
    return 1;
}

static inline int _vinfo_review(char **buf, size_t *len, size_t *max, const char *path, const char *notes) {
    const char *lang = v_filelang(path);
    char head[1024];
    if (notes) snprintf(head, sizeof(head), "# REVIEW %s\n%s\n\n", path, notes);
    else snprintf(head, sizeof(head), "# REVIEW %s\n\n", path);

    v_bufwrite(buf, len, head, max);

    char *content = v_fileload(path);
    if (content) {
        v_bufwrite(buf, len, "```", max);
        v_bufwrite(buf, len, lang, max);
        v_bufwrite(buf, len, "\n", max);
        v_bufwrite(buf, len, content, max);
        v_bufwrite(buf, len, "\n```\n", max);
        free(content);
    }
    return 1;
}

static inline int _vinfo_digest(char **buf, size_t *len, size_t *max, const char *path, const char *notes) {
    char head[1024];
    snprintf(head, sizeof(head), "# SUMMARY %s\n%s\n\n", path, notes ? notes : "");
    v_bufwrite(buf, len, head, max);
    return 1;
}

static inline int _vinfo_memory(char **buf, size_t *len, size_t *max, const char *topic, const char *notes) {
    char path[256];
    snprintf(path, sizeof(path), "#%s", topic);
    return _vinfo_digest(buf, len, max, path, notes);
}

static inline int _vinfo_forget(char **buf, size_t *len, size_t *max, const char *topic, const char *notes) {
    char path[256];
    snprintf(path, sizeof(path), "#%s", topic);
    return _vinfo_digest(buf, len, max, path, NULL);
}

static inline int _vinfo_genius(char **buf, size_t *len, size_t *max, const char *source, const char *query) {
    char head[1024];
    snprintf(head, sizeof(head), "# GENIUS: %s\nQuery: %s\n\n", source, query);
    v_bufwrite(buf, len, head, max);
    v_bufwrite(buf, len, "(Genius requires external API - stubbed)\n", max);
    return 1;
}

static inline int _vinfo_answer(char **buf, size_t *len, size_t *max, const char *subject, const char *ideas) {
    v_bufwrite(buf, len, "# ", max);
    v_bufwrite(buf, len, subject, max);
    v_bufwrite(buf, len, "\n", max);
    v_bufwrite(buf, len, ideas, max);
    v_bufwrite(buf, len, "\n\n", max);
    return 1;
}

typedef enum {
    INFO_SEARCH,
    INFO_REVIEW,
    INFO_DIGEST,
    INFO_MEMORY,
    INFO_FORGET,
    INFO_GENIUS,
    INFO_ANSWER,
    INFO_INVALID
} v_infotype_t;

static inline v_infotype_t _vinfo_parser(const char **list, char term[256], char note[2048]) {
    char verb[16] = {0};
    const char *line = *list;
    term[0] = '\0';
    note[0] = '\0';

    int n = sscanf(line, "%15[^|\n]|%255[^|\n]|%2047[^\n]", verb, term, note);
    const char *eol = strchr(line, '\n');
    if (eol) *list = eol + 1;
    else *list = line + strlen(line);

    if (n < 3) return INFO_INVALID;
    if (strcmp(verb, "review") == 0)   return INFO_REVIEW;
    if (strcmp(verb, "search") == 0)   return INFO_SEARCH;
    if (strcmp(verb, "genius") == 0)   return INFO_GENIUS;
    if (strcmp(verb, "digest") == 0)   return INFO_DIGEST;
    if (strcmp(verb, "memory") == 0)   return INFO_MEMORY;
    if (strcmp(verb, "forget") == 0)   return INFO_FORGET;
    if (strcmp(verb, "answer") == 0)   return INFO_ANSWER;
    return INFO_INVALID;
}

static inline bool _vinfo_checks(peanuts_t *nut, char **res) {
    int actions = 0;
    int replies = 0;
    char term[256];
    char note[2048];
    size_t len  = 0;
    size_t max  = 1024;
    char *buf = malloc(max);
    const char *line = *res;

    while (*line) {
        switch(_vinfo_parser(&line, term, note)) {
            case INFO_REVIEW: actions += _vinfo_review(&buf, &len, &max, term, note); break;
            case INFO_SEARCH: actions += _vinfo_search(&buf, &len, &max, term, note); break;
            case INFO_GENIUS: actions += _vinfo_genius(&buf, &len, &max, term, note); break;
            case INFO_DIGEST: actions += _vinfo_digest(&buf, &len, &max, term, note); break;
            case INFO_MEMORY: actions += _vinfo_memory(&buf, &len, &max, term, note); break;
            case INFO_FORGET: actions += _vinfo_forget(&buf, &len, &max, term, note); break;
            case INFO_ANSWER: replies += _vinfo_answer(&buf, &len, &max, term, note); break;
            default: break;
        }
    }

    if (!actions && replies) {
        free(*res);
        *res = buf;
        return true;
    }

    free((void*)nut->analysis);
    free((void*)nut->nudging);
    free((void*)nut->updates);

    nut->evidence = _vinfo_refresh(nut->evidence);
    nut->analysis = *res;
    nut->nudging  = buf;
    nut->updates  = strdup("Okay that helps. But do I have enough info to answer? Let me think about it a bit more.");

    return false;
}

// running a nutjob for PEANUTS (Info)
static inline char *_vinfo_nutjob(peanuts_t *nut) {
    nutmeg_t ctx = {0};
    VINFO_NUTMEG(&ctx);
    return nutjob(&ctx, nut);
}

static inline char *v_infolist(const char **files, int n, const char *note) {
    char *buf = NULL;
    size_t len = 0;
    size_t max = 128;

    if (n == 0) {
        char *walked = v_walk(NULL);
        if (walked) {
            v_bufwrite(&buf, &len, walked, &max);
            free(walked);
        }
    } else {
        for (int i = 0; i < n; i++) {
            v_bufwrite(&buf, &len, files[i], &max);
            v_bufwrite(&buf, &len, "\n", &max);
        }
    }

    if (note) {
        v_bufwrite(&buf, &len, "\n", &max);
        v_bufwrite(&buf, &len, note, &max);
        v_bufwrite(&buf, &len, "\n", &max);
    }
    return buf;
}

static inline char *v_infoshow(const char **files, int n, const char *note) {
    char *buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    for (int i = 0; i < n; i++) {
        _vinfo_review(&buf, &len, &cap, files[i], NULL);
    }
    if (note) {
        v_bufwrite(&buf, &len, "\n", &cap);
        v_bufwrite(&buf, &len, note, &cap);
        v_bufwrite(&buf, &len, "\n", &cap);
    }
    return buf;
}

static inline char *v_infoscan(const char **files, int n, const char *note) {
    peanuts_t *nut  = {0};
    const char *list = v_infolist(files, n, note);
    const char *code = v_infoshow(files, n, note);

    nut->persona    = v_fileinst(files[0], (const char*[]){"TODO", "FIXME", NULL}, "You are an architect.");
    nut->evidence   = list;
    nut->analysis   = strdup("Okay. Show me the contents of these files...");
    nut->nudging    = code;
    nut->updates    = strdup("Okay, so I need to review files first to figure out how to summarize.");
    nut->turnout    = "Respond with `review|path|notes` or `search|path|terms`";
    nut->safety     = _vinfo_checks;

    char *rtn = _vinfo_nutjob(nut);

    free((void*)nut->persona);
    free((void*)nut->evidence);
    free((void*)nut->analysis);
    free((void*)nut->nudging);
    free((void*)nut->updates);
    free((void*)nut->turnout);
    return rtn;
}

static inline char *v_infofind(const char *query) {
    peanuts_t *nut  = {0};
    const char *list = v_infolist(NULL, 0, query);
    size_t len = strlen(query);
    char *quip = malloc(len + 56);
    char *need = malloc(len + 56);

    sprintf(quip, "Okay I see the files. But you want me to focus on:\n\n%s", query);
    sprintf(need, "Yes. What files do you need to answer this:\n\n%s", query);

    nut->persona    = v_fileinst(NULL, (const char*[]){"TODO", "FIXME", NULL}, "You are an architect.");
    nut->evidence   = list;
    nut->analysis   = quip;
    nut->nudging    = need;
    nut->updates    = strdup("Okay, so I need to search terms and review the code to figure this out.");
    nut->turnout    = "Respond with `review|path|notes` or `search|path|terms`";
    nut->safety     = _vinfo_checks;

    char *rtn = _vinfo_nutjob(nut);

    free((void*)nut->persona);
    free((void*)nut->evidence);
    free((void*)nut->analysis);
    free((void*)nut->nudging);
    free((void*)nut->updates);
    free((void*)nut->turnout);
    return rtn;
}

/* ========================================================================
 * Private mark helpers (internal: _vmark_* prefix)
 * ======================================================================== */

static inline int _vmark_number(char **buf, size_t *len, size_t *max, const char *path) {
    const char *lang = v_filelang(path);
    char *content = v_fileload(path);

    v_bufwrite(buf, len, path, max);
    v_bufwrite(buf, len, "\n```", max);
    v_bufwrite(buf, len, lang, max);
    v_bufwrite(buf, len, "\n", max);

    if (content) {
        v_buflines(buf, len, content, max);
        free(content);
    }

    v_bufwrite(buf, len, "\n```\n\n", max);
    return 1;
}

static inline char *_vmark_target(const char **files, int n, const char *goal) {
    char *buf = NULL;
    size_t max = 128;
    size_t len = 0;
    for (int i = 0; i < n; i++) {
        _vmark_number(&buf, &len, &max, files[i]);
    }
    v_bufwrite(&buf, &len, goal, &max);
    return buf;
}

static inline int _vmark_sorted(const void *a, const void *b) {
    const v_mark_t *x = a;
    const v_mark_t *y = b;
    int c = strcmp(x->path, y->path);
    if (c != 0) return c;
    return (x->line < y->line) ? -1 : (x->line > y->line);
}

static inline int _vmark_parser(const char *str, v_mark_t *marks, size_t max) {
    const char *p = str, *end = str + strlen(str);
    int cnt = 0;
    int adv = 0;

    while (cnt < max && p < end) {
        v_mark_t *e = &marks[cnt];
        int n = sscanf(p, "%d|%255[^ |]|%15[A-Z]|%1023[^\n]%n", &e->line, e->path, e->type, e->note, &adv);
        if (n != 4) return 0;
        cnt++;
        p += adv;
        if (!*p) break;
        p++;
    }
    return cnt;
}

static inline char *_vmark_source(const char *path, v_mark_t *marks, size_t cnt, const char *mk) {
    if (!path || !marks || !mk || cnt == 0) return NULL;

    int len1 = 0;
    while (mk[len1] && mk[len1] != ' ') len1++;
    const char *mk2 = mk + len1;
    if (*mk2 == ' ') mk2++;

    char *buf = NULL;
    size_t max = 128;
    size_t len = 0;
    const char *src = v_fileload(path);
    if (!src) return NULL;

    const char *in = src;
    int line = 1;
    size_t t = 0;

    while (*in) {
        // Skip marks not for this file
        if (t < cnt && strcmp(marks[t].path, path) != 0) {
            t++;
            continue;
        }

        while (t < cnt && marks[t].line == line) {
            char tmp[1024];
            int n = snprintf(tmp, sizeof(tmp), "%s %s: %s %s\n", mk, marks[t].type, marks[t].note, mk2);
            if (n > 0) v_bufwrite(&buf, &len, tmp, &max);
            t++;
        }

        const char *eol = strchr(in, '\n');
        size_t linelen = eol ? (size_t)(eol - in) : strlen(in);

        if (*len + linelen + 2 >= max) {
            max *= 2;
            buf = realloc(buf, max);
        }
        memcpy(buf + *len, in, linelen);
        *len += linelen;

        if (eol) {
            buf[(*len)++] = '\n';
            in = eol + 1;
            line++;
        } else {
            break;
        }
    }
    free(src);
    buf[*len] = '\0';

    while (t < cnt) {
        char tmp[1024];
        int n = snprintf(tmp, sizeof(tmp), "%s LINE %d - %s: %s %s\n", mk, marks[t].line, marks[t].type, marks[t].note, mk2);
        if (n > 0) v_bufwrite(&buf, &len, tmp, &max);
        t++;
    }
    return buf;
}

static inline bool _vmark_checks(peanuts_t *nut, char **res) {
    v_mark_t marks[128] = {0};
    int n = _vmark_parser(*res, marks, 128);
    const char *current_path = ""; // Fix segfault

    if (!n) {
        free((void*)nut->updates);
        nut->updates = *res;
        return false;
    }

    qsort(marks, n, sizeof(v_mark_t), _vmark_sorted);

    for (int i = 0; i < n; ++i) {
        v_mark_t *mark = &marks[i];
        if (strcmp(current_path, mark->path) != 0) {
            current_path = mark->path;
            const char *mk = v_filemark(current_path);
            char *src = _vmark_source(current_path, marks, n, mk);
            if (src) {
                v_filesave(current_path, src);
                free(src);
            }
        }
    }
    return true;
}

// running a nutjob for PEANUTS (Mark)
static inline char *_vmark_nutjob(peanuts_t *nut) {
    nutmeg_t ctx = {0};
    VMARK_NUTMEG(&ctx);
    return nutjob(&ctx, nut);
}

/* ========================================================================
 * Private code helpers (internal: _vcode_* prefix)
 * ======================================================================== */

static inline bool _vcode_writes(peanuts_t *nut, char **res) {
    (void)nut;
    (void)res;
    return strstr(*res, "```") != NULL;
}

static inline int _vcode_answer(char **buf, size_t *len, size_t *max, const char *topic, const char *notes) {
    v_bufwrite(buf, len, topic, max);
    v_bufwrite(buf, len, "|", max);
    v_bufwrite(buf, len, notes, max);
    v_bufwrite(buf, len, "\n", max);
    return 1;
}

static inline bool _vcode_search(peanuts_t *nut, char **res) {
    (void)nut;
    (void)res;
    return false;
}

static inline const char* _vcode_markup(const char* src, const char* kw[], const char* mk) {
    const char *p = src;
    char st[16];
    int ml = 0;
    while (mk[ml] && mk[ml] != ' ' && ml < 15) ml++;
    strncpy(st, mk, ml);
    st[ml] = '\0';
    if (!ml) return NULL;

    while (*p && isblank(*p)) p++;
    if (strncmp(p, st, ml) == 0) {
        p += ml;
        while (*p && isblank(*p)) p++;
        for (int k = 0; kw[k] != NULL; k++) {
            const char *key = kw[k];
            int kl = strlen(key);
            if (strncmp(p, key, kl) == 0 && p[kl] == ':') return p;
        }
    }
    return NULL;
}

static inline int v_codescan(const char *src, const char *kw[], const char *mk, char **list) {
    char *buf = NULL;
    size_t max = 128;
    size_t len = 0;
    const char *line = src;
    const char *next;
    const char *mark;
    int cnt = 0;

    while (*line) {
        mark = _vcode_markup(line, kw, mk);
        next = strchr(line, '\n');
        if (!next) next = line + strlen(line);

        if (mark) {
            char tmp[1024];
            cnt++;
            snprintf(tmp, sizeof(tmp), "%d. %.*s\n", cnt, (int)(next - mark), mark);
            v_bufwrite(&buf, &len, tmp, &max);
        }
        if (!*next) break;
        line = next + 1;
    }

    if (cnt == 0) {
        free(buf);
        return 0;
    }

    if (list) {
        free(*list);
        *list = buf;
    } else {
        free(buf);
    }
    return cnt;
}

static inline char *v_codefind(const char *quest) {
    peanuts_t *nut  = {0};
    const char *list = v_infolist(NULL, 0, quest);
    size_t len = strlen(quest);
    const char *quip = malloc(len + 54);
    const char *need = malloc(len + 128);

    sprintf((char*)quip, "Okay I see the files. But you want me to focus on:\n\n%s", quest);
    sprintf((char*)need, "Search the codebase to answer:\n\n%s\n\nThen list the files as `answer|filename|comment` after you do you research.\n", quest);

    nut->persona    = v_fileinst(NULL, (const char*[]){"TODO", "FIXME", NULL}, "You are an architect.");
    nut->evidence   = list;
    nut->analysis   = quip;
    nut->nudging    = need;
    nut->updates    = strdup("Okay, so I need to search code first to figure this out.");
    nut->turnout    = "Respond with `search|path|terms` or `answer|filename|comment`";
    nut->safety     = _vcode_search;

    char *rtn = _vinfo_nutjob(nut);

    free((void*)nut->persona);
    free((void*)nut->evidence);
    free((void*)nut->analysis);
    free((void*)nut->nudging);
    free((void*)nut->updates);
    free((void*)nut->turnout);
    return rtn;
}

static inline int v_codefile(const char *filename, const char *goal) {
    peanuts_t *nut  = {0};
    const char *mk   = v_filemark(filename);
    const char *code = v_fileload(filename);
    const char *inst = v_fileinst(filename, (const char*[]){"TODO", "FIXME", NULL}, "You are an architect.");
    const char *list;
    char *todo = malloc(1024);
    int len = 0;
    int max = 1024;
    int t = v_codescan(code, (const char*[]){"TODO", "FIXME", NULL}, mk, (char**)&list);

    if (!t && goal) {
        code = v_fileload(filename);
        t = v_codescan(code, (const char*[]){"TODO", "FIXME", NULL}, mk, (char**)&list);
    }

    if (!t) {
        v_errfmt("No comments found in `%s`", filename);
        free(todo);
        return 0;
    }

    v_bufwrite(&todo, &len, "Okay...\n\nI found these comments:\n\n", &max);
    v_bufwrite(&todo, &len, list, &max);
    free((void*)list);
    v_bufwrite(&todo, &len, "\n\nAre we ready to address the comments?", &max);

    nut->persona    = inst;
    nut->evidence   = goal;
    nut->analysis   = mk;
    nut->nudging    = code;
    nut->updates    = todo;
    nut->turnout    = inst;
    nut->safety     = _vcode_writes;

    char *src = _vinfo_nutjob(nut);

    if (!src) {
        v_errfmt("AI Error");
        free((void*)inst);
        free((void*)code);
        free(todo);
        return 0;
    }

    v_filesave(filename, src);

    free((void*)inst);
    free((void*)code);
    free(todo);
    free(src);
    return 1;
}

/* ========================================================================
 * Task Orchestration (v_task*) - From task.h
 * ======================================================================== */

static inline char *v_taskinfo(const char **files, int n, const char *goal) {
    char *info;
    if (goal) fprintf(stderr, "Searching Files: %s\n", goal);
    if (!n && !goal) info = v_infolist(NULL, 0, NULL);
    else if (!n) info = v_infofind(goal);
    else info = v_infoscan(files, n, goal);
    return info;
}

static inline int v_taskmark(const char **files, int n, const char *goal) {
    if (goal) fprintf(stderr, "Marking Files: %s\n", goal);
    // Simplified: full implementation would require vest config mapping
    // For now, we just return 1 to indicate success/stub
    (void)files; (void)n; (void)goal;
    return 1;
}

static inline int v_taskcode(const char **files, int n, const char *goal) {
    int i = 0;
    if (n > 1) v_taskmark(files, n, goal);
    while (i < n) v_codefile(files[i++], goal);
    return n;
}

static inline char *v_taskplan(const char **files, int n, const char *goal) {
    (void)files; (void)n; (void)goal;
    return strdup("Plan functionality pending.");
}

#endif /* VARIANCE_H */
