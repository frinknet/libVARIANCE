/* (c) 2026 FRINKnet & Friends - MIT Licence */
#include "variance.h"

FILE *vcore_out = NULL;
FILE *vcore_err = NULL;

/* Core Configuration */
#define X(mode, name, ctype, value) ctype v##mode##_##name = value;
VMODEX
#undef X

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

static inline char *_vtool_memory(const char *topic) {
	char *info = v_fileload(VFILENAME_INFO);

	if (!info) return NULL;

	char *buf = NULL;
	size_t len = 0;
	size_t max = 1024;
	char *line = info;
	char *next;
	char *pipe;

	while ((next = strchr(line, '\n')) != NULL) {
		*next = '\0';

		if (line[0] == '#' && (pipe = strchr(line, '|')) != NULL) {
			*pipe = '\0';

			if (!topic || strcmp(line + 1, topic) == 0) {
				v_bufwrite(&buf, &len, "# " VINFO_LABEL_MEMORIES " ", &max);
				v_bufwrite(&buf, &len, line + 1, &max);
				v_bufwrite(&buf, &len, "\n", &max);
				v_bufwrite(&buf, &len, pipe + 1, &max);
				v_bufwrite(&buf, &len, "\n\n", &max);
			}
		}

		line = next + 1;
	}

	free(info);

	return buf;
}

static inline char *_vtool_summary(const char **files, int n) {
	char *info = v_fileload(VFILENAME_INFO);

	if (!info) return NULL;

	char *buf = NULL;
	size_t len = 0;
	size_t max = 1024;

	for (int i = 0; i < n; i++) {
		char key[1024];

		snprintf(key, sizeof(key), "%s|", files[i]);

		char *p = strstr(info, key);
		if (p) {
			p += strlen(key);
			char *end = strchr(p, '\n');

			if (end) {
				*end = '\0';
				v_bufwrite(&buf, &len, files[i], &max);
				v_bufwrite(&buf, &len, " ⟵ ", &max);
				v_bufwrite(&buf, &len, p, &max);
				v_bufwrite(&buf, &len, "\n", &max);
			}
		} else {
			v_bufwrite(&buf, &len, files[i], &max);
			v_bufwrite(&buf, &len, "\n", &max);
		}
	}

	free(info);

	return buf;
}


// refresh infolist for proompt
static inline char *_vinfo_refresh(const char *code) {
	char local[1024][256];
	int n = v_filelist(code, local, 1024);
	const char **files = (const char **)local;

	return v_infolist(files, n, NULL);
}

// read a file to understand it
static inline int _vinfo_review(char **buf, size_t *len, size_t *cap, const char *path, const char *notes) {
	const char *lang = v_filelang(path);
	FILE *f = fopen(path, "r");
	char head[1024];

	if (notes) snprintf(head, 1024, "# %s %s\n%s\n\n", VINFO_REVIEW_DIRECTIVE, path, notes);

	v_bufwrite(buf, len, head, cap);
	v_out(VFILE_NOTICE_VIEW, path, notes);

	if (f) {
		v_bufwrite(buf, len, "\n```", cap);
		v_bufwrite(buf, len, lang, cap);
		v_bufwrite(buf, len, "\n", cap);
		v_bufstream(buf, len, f, cap, 0);
		fclose(f);
		v_bufwrite(buf, len, "\n```\n\n" VINFO_REVIEW_DIRECTIVE, cap);
	}

	return 1;
}

// summarize a file to remember
static inline int _vinfo_digest(char **buf, size_t *len, size_t *cap, const char *path, const char *notes) {
	char tmp[] = "._" VFILENAME_INFO;
	FILE *fin = fopen(VFILENAME_INFO, "r"), *fout = fopen(tmp, "w");
	char head[1024];

	if (!fout) return 0;

	if (fin) {
		char *line = NULL; size_t n = 0;

		while (getline(&line, &n, fin) > 0) {
			if (strncmp(line, path, strlen(path)) || line[strlen(path)] != '|') fputs(line, fout);
		}

		free(line);
		fclose(fin);
	}

	if (notes) fprintf(fout, "%s|%s\n", path, notes);

	fclose(fout);
	rename(tmp, VFILENAME_INFO);
	snprintf(head, 1024, " # %s %s\n%s\n\n", VINFO_LABEL_SUMMARY, path, notes);
	v_bufwrite(buf, len, head, cap);

	if (notes) v_out(VFILE_NOTICE_NOTE, (*path == '#') ? path - 1 : path, notes);

	return 1;
}

// search codebase for words
static inline int _vinfo_search(char **buf, size_t *len, size_t *cap, const char *path, const char *terms) {
	char head[1024];

	snprintf(head, sizeof(head), "# %s: `%s` in %s\n\n", VINFO_LABEL_LOCATE, terms, path);
	v_bufwrite(buf, len, head, cap);

	char *result = _vtool_grep(terms, path);

	if (result) {
		v_bufwrite(buf, len, result, cap);
		free(result);
	} else {
		v_bufwrite(buf, len, VINFO_SEARCH_NODATA "\n", cap);
	}

	v_bufwrite(buf, len, "\n", cap);

	return 1;
}

// remember something important
static inline int _vinfo_memory(char **buf, size_t *len, size_t *cap, const char *topic, const char *notes) {
	char *path = malloc(strlen(topic) + 1);

	sprintf(path, "#%s", topic);

	int rtn = _vinfo_digest(buf, len, cap, path, notes);

	free((void*)path);

	return rtn;
}

// TODO: giving an answer
static inline int _vinfo_genius(char **buf, size_t *len, size_t *cap, const char *topic, const char *notes) {
	char head[1024];

	snprintf(head, sizeof(head), "# %s (%s)\n\n", notes, topic);
	v_bufwrite(buf, len, head, cap);
	v_bufwrite(buf, len, VINFO_GENIUS_NODATA "\n", cap);

	return 1;
}

// giving an answer
static inline int _vinfo_answer(char **buf, size_t *len, size_t *cap, const char *topic, const char *notes) {
	v_bufwrite(buf, len, "# ", cap);
	v_bufwrite(buf, len, topic, cap);
	v_bufwrite(buf, len, "\n", cap);
	v_bufwrite(buf, len, notes, cap);
	v_bufwrite(buf, len, "\n\n", cap);

	return 1;
}

// parsing an answer
static inline vinfotype_t _vinfo_parser(char **list, char term[256], char note[2048]) {
	char verb[16] = {0};
	char *line = *list;

	term[0] = '\0';
	note[0] = '\0';

	int n = sscanf(line, "%15[^|\n]|%255[^|\n]|%2047[^\n]", verb, term, note);
	char *eol = strchr(line, '\n');

	if (eol) *list = eol + 1;
	else *list = line + strlen(line);

	if (n < 3) return VINFO_NODATA;
	if (strcmp(verb, "review") == 0)   return VINFO_REVIEW;
	if (strcmp(verb, "search") == 0)   return VINFO_SEARCH;
	if (strcmp(verb, "genius") == 0)   return VINFO_GENIUS;
	if (strcmp(verb, "digest") == 0)   return VINFO_DIGEST;
	if (strcmp(verb, "memory") == 0)   return VINFO_MEMORY;
	if (strcmp(verb, "forget") == 0)   return VINFO_FORGET;
	if (strcmp(verb, "answer") == 0)   return VINFO_ANSWER;

	return VINFO_NODATA;
}

// checking for completeness
static inline bool _vinfo_checks(peanuts_t *nut, char **res) {
	int actions = 0;
	int replies = 0;
	char term[256];
	char note[2048];
	size_t len  = 0;
	size_t cap  = 1024;
	char *buf = malloc(cap);
	char *line = *res;

	while (*line) {
		switch(_vinfo_parser(&line, term, note)) {
			case VINFO_REVIEW: actions += _vinfo_review(&buf, &len, &cap, term, note); break;
			case VINFO_SEARCH: actions += _vinfo_search(&buf, &len, &cap, term, note); break;
			case VINFO_GENIUS: actions += _vinfo_genius(&buf, &len, &cap, term, note); break;
			case VINFO_DIGEST: actions += _vinfo_digest(&buf, &len, &cap, term, note); break;
			case VINFO_MEMORY: actions += _vinfo_memory(&buf, &len, &cap, term, note); break;
			case VINFO_FORGET: actions += _vinfo_memory(&buf, &len, &cap, term, NULL); break;
			case VINFO_ANSWER: replies += _vinfo_answer(&buf, &len, &cap, term, note); break;
			default: break;
		}
	}

	if (!actions && replies) {
		free(*res);

		*res = buf;

		return true;
	}

	free(nut->analysis);
	free(nut->nudging);
	free(nut->updates);

	nut->evidence = _vinfo_refresh(nut->evidence);
	nut->analysis = *res;
	nut->nudging  = buf;
	nut->updates  = strdup(VINFO_REVIEW_CONTINUE);

	return false;
}




// render a file with line numbers into the buffer
static inline int _vmark_number(char **buf, size_t *len, size_t *cap, const char *path) {
	const char *lang = v_filelang(path);
	FILE *f = fopen(path, "r");
	char head[1024];

	v_bufwrite(buf, len, path, cap);
	v_out(VFILE_NOTICE_READ, path);

	if (f) {
		v_bufwrite(buf, len, "\n```", cap);
		v_bufwrite(buf, len, lang, cap);
		v_bufwrite(buf, len, "\n", cap);
		v_bufstream(buf, len, f, cap, 1);
		fclose(f);
		v_bufwrite(buf, len, "\n```\n\n", cap);
	} else {
		v_bufwrite(buf, len, path, cap);
	}

	return 1;
}

// render all files with line numbers and append the goal
static inline char *_vmark_target(const char **files, int n, const char *goal) {
	char *buf = NULL;
	size_t cap = 0;
	size_t len = 0;
	int i = 0;

	while(i < n) _vmark_number(&buf, &len, &cap, files[i]);

	v_bufwrite(&buf, &len, goal, &cap);

	return buf;
}

// qsort comparator: sort marks by path then by line number
static inline int _vmark_sorted(const void *a, const void *b) {
	const vmark_t *x = a;
	const vmark_t *y = b;
	int c;

	// First: sort by path
	c = strcmp(x->path, y->path);

	if (c != 0) return c;

	// Second: sort by line
	return (x->line < y->line) ? -1 : (x->line > y->line);
}

// parse a string into an array of vmark_t structs
static inline int _vmark_parser(char *str, vmark_t *marks, size_t cap) {
	char num[32];
	const char *p = str, *end = str + strlen(str);
	int cnt = 0;
	int adv = 0;

	//TODO:  sort string
	while (cnt < cap && p < end) {
		vmark_t *e = &marks[cnt];
		int n = sscanf(p, "%d|%255[^ |]|%15[A-Z]|%1023[^\n]%n", &e->line, e->path, e->type, e->note, &adv);

		if (n != 4) return 0;

		cnt++;
		p += adv;

		if (!*p) break;

		p++;
	}

	return cnt;
}

// insert mark comments into a source file at the specified lines
static inline char *_vmark_source(const char *path, vmark_t *marks, size_t cnt, const char *mk) {
	if (!path || !marks || !mk || cnt == 0) return NULL;

	int len1 = 0;

	while (mk[len1] && mk[len1] != ' ' && len1 < 15) len1++;

	const char *mk2 = mk + len1;

	if (*mk2 == ' ') mk2++;

	char *buf = NULL;
	size_t cap = 128, len = 0;
	char *src = v_fileload(path);
	char *in = src;
	int line = 1;

	while (*in) {
		for (size_t t = 0; t < cnt; t++) {
			if (strcmp(marks[t].path, path) == 0 && marks[t].line == line) {
				char tmp[1024];

				snprintf(tmp, sizeof(tmp), "%s %s: %s %s\n", mk, marks[t].type, marks[t].note, mk2);
				v_bufwrite(&buf, &len, tmp, &cap);
			}
		}

		char *eol = strchr(in, '\n');
		size_t linelen = eol ? (size_t)(eol - in) : strlen(in);

		v_bufwrite(&buf, &len, in, &linelen);

		if (eol) {
			v_bufwrite(&buf, &len, "\n", &cap);

			in = eol + 1;
			line++;
		} else {
			break;
		}
	}

	free(src);

	return buf;
}

// check AI response and insert marks into source files
static inline bool _vmark_checks(peanuts_t *nut, char **res) {
	vmark_t marks[VMARK_MAX] = {0};
	char num[32];
	int n = _vmark_parser(*res, marks, VMARK_MAX);
	const char *path = NULL;

	if (!n) {
		free(nut->updates);

		nut->updates = *res;

		return 0;
	}

	// sort marks
	qsort(marks, n, sizeof(vmark_t), _vmark_sorted);

	for (int i = 0; i < n; ++i) {
		vmark_t mark = marks[i];

		if (strcmp(path, mark.path)) {
			path = mark.path;

			const char *mk = v_filemark(path);
			char *src = _vmark_source(path, marks, n, mk);
			int saved = v_filesave(path, src);

			v_out(VFILE_NOTICE_EDIT, path);
			free(src);

			if (saved < 1) return 0;
		}
	}

	v_out(VFILE_NOTICE_MARK, n);

	return 1;
}




// check AI response for code-worthy comments (used as safety callback)
static inline bool _vcode_writes(peanuts_t *nut, char **res) {
	return v_markscan(*res, vcode_keywords, nut->analysis, NULL);
}

// format an answer entry for code-related info
static inline int _vcode_answer(char **buf, size_t *len, size_t *cap, const char *topic, const char *notes) {
	v_bufwrite(buf, len, topic, cap);
	v_bufwrite(buf, len, "|", cap);
	v_bufwrite(buf, len, notes, cap);
	v_bufwrite(buf, len, "\n", cap);

	return 1;
}

// process AI search results: dispatch to info actions and collect answers
static inline bool _vcode_search(peanuts_t *nut, char **res) {
	int actions = 0;
	int replies = 0;
	char term[256];
	char note[2048];
	char *ans = malloc(128);
	size_t alen  = 0;
	size_t acap  = 128;
	char *buf = malloc(1024);
	size_t blen  = 0;
	size_t bcap  = 1024;
	char *line =  *res;

	while (*line) {
		switch(_vinfo_parser(&line, term, note)) {
			case VINFO_SEARCH: actions += _vinfo_search(&buf, &blen, &bcap, term, note); break;
			case VINFO_REVIEW: actions += _vinfo_review(&buf, &blen, &bcap, term, note); break;
			case VINFO_DIGEST: actions += _vinfo_digest(&buf, &blen, &bcap, term, note); break;
			case VINFO_MEMORY: actions += _vinfo_memory(&buf, &blen, &bcap, term, note); break;
			case VINFO_FORGET: actions += _vinfo_memory(&buf, &blen, &bcap, term, NULL); break;
			case VINFO_ANSWER: replies += _vinfo_answer(&ans, &alen, &acap, term, note); break;
			default: break;
		}
	}

	if (!actions && replies) {
		free(*res);
		free(buf);

		*res = ans;

		return 1;
	}

	v_bufwrite(&buf, &blen, "# " VCODE_SOURCE_PREAMBLE "\n" VCODE_SOURCE_DIRECTIVE "\n\n", &bcap);
	v_bufwrite(&buf, &blen, ans, &bcap);

	free(nut->analysis);
	free(nut->nudging);
	free(ans);

	nut->analysis = *res;
	nut->nudging = buf;

	return 0;
}

// scan source for a line starting with a known keyword marker
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

		if (p[0] == '.' && p[1] == '.' && p[2] == '.') return p;
	}

	if (p[0] == '.' && p[1] == '.' && p[2] == '.') return p;

	return NULL;
}



// detect file type from filename extension
vfiletype_t v_filetype(const char *filename) {
	const char *ext = strrchr(filename, '.');
	const char *extstr = ext ? ext : "";

	#define X(lang, exts, mark) if ((*extstr && strstr(exts, extstr)) || strstr(exts, filename)) return V_FTYPE_##lang;
	#include "../x/filetypes.x"
	#undef X

	return V_FTYPE_NONE;
}

// get human-readable language name from filename
const char *v_filelang(const char *filename) {
	vfiletype_t t = v_filetype(filename);

	#define X(lang, exts, mark) case V_FTYPE_##lang: return #lang;
	switch(t) {
	#include "../x/filetypes.x"
	default: return "text";
	}
	#undef X
}

// get the comment marker string for a file's language
const char *v_filemark(const char *filename) {
	vfiletype_t t = v_filetype(filename);

	#define X(lang, exts, mark) case V_FTYPE_##lang: return mark;
	switch(t) {
	#include "../x/filetypes.x"
	default: return "";
	}
	#undef X
}

// parse a pipe-delimited or newline-delimited file list into an array
int v_filelist(const char *src, char files[][256], int n) {
	int i = 0;
	const char *p = src;

	while (*p && i < n) {
		const char *start = p;

		while (*p && *p != '\n' && *p != '|' && !(strncmp(p, " ⟵ ", 4) == 0)) p++;

		if (p > start) {
			int len = p - start;

			if (len >= 256) len = 255;

			memcpy(files[i], start, len);
			files[i][len] = '\0';

			i++;
		}

		if (*p == '|' || *p == '\n') {
			p++;
		} else if (strncmp(p, " ⟵ ", 4) == 0) {
			while (*p && *p != '\n') p++;
			if (*p == '\n') p++;
		} else {
			if (*p) p++;
		}
	}

	return i;
}

// load a file (or stdin) into a malloc'd string
char *v_fileload(const char *filename) {
	FILE *f;

	if (strcmp(filename, "-") == 0) f = stdin;
	else f = fopen(filename, "r");

	if (!f) {
		v_err(VFILE_NOTICE_GONE, filename);

		return NULL;
	}

	char *buf = NULL;
	size_t cap = 128;
	size_t len = 0;

	v_bufstream(&buf, &len, f, &cap, 0);

	if (ferror(f)) {
		v_err(VFILE_NOTICE_OPEN, filename);

		if (f != stdin) fclose(f);

		free(buf);

		return NULL;
	}

	if (f != stdin) fclose(f);

	return buf;
}

// write a string to a file (or stdout), returning bytes written
size_t v_filesave(const char *filename, const char *str) {
	FILE *f = strcmp(filename, "-") ? fopen(filename, "w") : vcore_out;

	if (!f) {
		perror(filename);

		return (size_t)-1;
	}

	size_t len = strlen(str), n = fwrite(str, 1, len, f);

	if (n != len || ferror(f)) {
		if (f != vcore_out) fclose(f);

		return (size_t)-1;
	}

	if (f != stdout) fclose(f);

	return n;
}

// build an instruction template by filling in language and keywords
char *v_fileinst(const char *filename, const char **kw, const char *inst) {
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

// check if a line starts with a known todo marker keyword
const char* v_todoline(const char* line, const char* kw[], const char* mk) {
	const char *p = line;
	char st[8];
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

		if (p[0] == '.' && p[1] == '.' && p[2] == '.') return p;
	}

	if (p[0] == '.' && p[1] == '.' && p[2] == '.') return p;

	return NULL;
}

// find all todo markers in source and return a numbered list
int v_todofind(const char *src, const char *kw[], const char *mk, char **out) {
	char *buf = NULL;
	size_t cap = 128;
	size_t len = 0;
	const char *line = src;
	const char *next;
	const char *todo;
	int cnt = 0;

	v_bufwrite(&buf, &len, VTODO_SEARCH_PREAMBLE "\n\n", &cap);

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

	v_bufwrite(&buf, &len, "\n" VTODO_SEARCH_CONTINUE, &cap);

	if (out) {
		free(*out);

		*out = buf;
	} else {
		free(buf);
	}

	return cnt;
}

// parse a string into an array of todo_t structs
int v_todoscan(const char *str, vtodo_t *todos, size_t cap) {
	const char *p = str, *end = str + strlen(str);
	int cnt = 0;
	int adv = 0;

	while (cnt < cap && p < end) {
		vtodo_t *e = &todos[cnt];
		int n = sscanf(p, "%d|%127[^ |]|%27[A-Z]|%511[^\n]%n", &e->line, e->name, e->type, e->note, &adv);

		//fprintf(stderr, "todoscan %d (+%ld) - matched %d:\n%s\n\n", cnt, (long)(p - str), n, p);

		if (n != 4) return 0;

		cnt++;
		p += adv;

		if (!*p) break;

		p++;
	}

	return cnt;
}

// insert todo comments into source at the specified lines
char *v_todomark(const char *orig, vtodo_t *todos, const char *mk, size_t cnt) {
	if (!orig || !todos || !mk || cnt == 0) return NULL;

	// Split marker: prefix + suffix
	int len1 = 0;

	while (mk[len1] && mk[len1] != ' ') len1++;

	const char *mk2 = mk + len1;

	if (*mk2 == ' ') mk2++;

	int len2 = strlen(mk2);

	char *buf = NULL;
	size_t cap = 128;
	size_t len = 0;

	const char *in = orig;
	int line = 1;
	size_t t = 0;

	// Walk file line by line
	while (*in) {
		// Insert ALL todos matching current line BEFORE the line itself
		while (t < cnt && todos[t].line == line) {
			char tmp[1024];
			int n = snprintf(tmp, sizeof(tmp), "%s %s: %s %s\n", mk, todos[t].type, todos[t].note, mk2);

			if (n > 0) {
				v_bufwrite(&buf, &len, tmp, &cap);

				t++;
			}
		}

		// Copy current line
		const char *eol = strchr(in, '\n');
		size_t linelen = eol ? (size_t)(eol - in) : strlen(in);

		v_bufwrite(&buf, &len, in, &linelen);  // writes linelen bytes

		if (eol) {
			v_bufwrite(&buf, &len, "\n", &cap);

			in = eol + 1;
			line++;
		} else {
			break;  // EOF without trailing newline
		}
	}

	// Tack on any remaining todos (shouldn't happen, but just in case)
	while (t < cnt) {
		char tmp[1024];
		int n = snprintf(tmp, sizeof(tmp), "%s:%d - %s: %s %s\n", mk, todos[t].line, todos[t].type, todos[t].note, mk2);

		if (n > 0) v_bufwrite(&buf, &len, tmp, &cap);

		t++;
	}

	return buf;
}


// list file info, memories and notes
char *v_infolist(const char **files, int n, const char *note) {
	char *buf = NULL;
	size_t len = 0;
	size_t cap = 1024;
	char *summaries = _vtool_summary(files, n);
	char *memories = _vtool_memory(NULL);

	if (summaries) {
		v_bufwrite(&buf, &len, "# " VINFO_LABEL_CONTEXT "\n", &cap);
		v_bufwrite(&buf, &len, summaries, &cap);
		free(summaries);
	}

	if (memories) {
		v_bufwrite(&buf, &len, "\n", &cap);
		v_bufwrite(&buf, &len, memories, &cap);
		free(memories);
	}

	if (note) {
		v_bufwrite(&buf, &len, "\n", &cap);
		v_bufwrite(&buf, &len, note, &cap);
		v_bufwrite(&buf, &len, "\n", &cap);
	}

	return buf;
}

// show files source
char *v_infoshow(const char **files, int n, const char *note) {
	char *buf = NULL;
	size_t cap = 0;
	size_t len = 0;
	int i = 0;

	while(i < n) _vinfo_review(&buf, &len, &cap, files[i++], NULL);

	if (note) {
		v_bufwrite(&buf, &len, "\n\n", &cap);
		v_bufwrite(&buf, &len, note, &cap);
		v_bufwrite(&buf, &len, "\n", &cap);
	}

	return buf;
}

// research in files
char *v_infoscan(const char **files, int n, const char *note) {
	char *list = v_infolist(files, n, note);
	char *code = v_infoshow(files, n, note);
	peanuts_t nut  = {0};

	nut.persona    = v_fileinst(files[0], vinfo_keywords, vinfo_instruct);
	nut.evidence   = list;
	nut.analysis   = strdup(VINFO_REVIEW_REQUEST);
	nut.nudging    = code;
	nut.updates    = strdup(VINFO_REVIEW_ACCEPTED);
	nut.turnout    = VINFO_MICROFORMAT;
	nut.safety     = _vinfo_checks;

	VRETURN_NUTJOB(INFO, nut);
}

// research in codebase
char *v_infofind(const char *query) {
	char *list = v_infolist(NULL, 0, query);
	size_t len = strlen(query);
	char *quip = malloc(len + strlen(VINFO_SEARCH_PREAMBLE) + 4);
	char *need = malloc(len + strlen(VINFO_SEARCH_DIRECTIVE) + 4);
	peanuts_t nut  = {0};

	sprintf(quip, "%s\n\n%s", VINFO_SEARCH_PREAMBLE, query);
	sprintf(need, "%s\n\n%s", VINFO_SEARCH_DIRECTIVE, query);

	nut.persona    = v_fileinst(NULL, vinfo_keywords, vinfo_instruct);
	nut.evidence   = list;
	nut.analysis   = quip;
	nut.nudging	   = need;
	nut.updates	   = strdup(VINFO_SEARCH_ACCEPTED);
	nut.turnout    = VINFO_MICROFORMAT;
	nut.safety	 = _vinfo_checks;

	VRETURN_NUTJOB(INFO, nut);
}


// scan source for comment markers matching keywords and return a numbered list
int v_markscan(const char *src, const char *kw[], const char *mk, char **list) {
	char *buf = NULL;
	size_t cap = 1024;
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
			v_bufwrite(&buf, &len, tmp, &cap);
		}

		if (!*next) break;
		line = next + 1;
	}

	if (cnt == 0) {
		free(buf);
		if (list) *list = NULL;
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

// list files containing any of the given keywords via grep
char *v_marklist(const char **files, int n, const char **kw) {
	char *buf = NULL;
	size_t len = 0;
	size_t cap = 128;

	if (n == 0) {
		// Build combined pattern: (TODO|FIXME|BUG|HACK)
		char pattern[1024] = "(";
		const char **k = kw;
		int first = 1;

		while (*k) {
			if (!first) strcat(pattern, "|");

			strcat(pattern, *k);

			first = 0;
			k++;
		}
		strcat(pattern, ")");

		// Run grep once with combined pattern
		char *matches = _vtool_grep(pattern, ".");
		if (matches) {
			v_bufwrite(&buf, &len, matches, &cap);
			free(matches);
		}
	} else {
		// Search specific files
		for (int i = 0; i < n; i++) {
			char *content = v_fileload(files[i]);

			if (content) {
				int found = 0;
				const char **k = kw;

				while (*k && !found) {
					if (strstr(content, *k)) found = 1;

					k++;
				}

				if (found) {
					v_bufwrite(&buf, &len, files[i], &cap);
					v_bufwrite(&buf, &len, "\n", &cap);
				}

				free(content);
			}
		}
	}

	if (buf) buf[len] = '\0';

	return buf;
}

// add AI-generated comments to files based on a goal
char *v_markeach(const char **files, int n, const char *goal) {
	char local[256][256];
	char *code;
	char *quip = malloc(72 + strlen(goal));
	char *next = strdup(VMARK_SEARCH_REQUEST);
	peanuts_t nut = {0};

	if (!n) {
		sprintf(quip, "%s\n\n%s", goal, VMARK_SEARCH_CONTINUE);

		code = v_codefind(quip);

		if (!code) return v_err(VFILE_NOTICE_GOAL, goal);

		n = v_filelist(code, local, 256);
		files = (const char **)local;

		free((void*)code);
	}

	code = _vmark_target(files, n, goal);

	sprintf(quip, "%s\n\n%s\n\n%s", VMARK_SEARCH_PREAMBLE, goal, VMARK_SEARCH_ACCEPTED);

	nut.persona   = v_fileinst(files[0], vmark_keywords, vmark_instruct);
	nut.evidence  = code;
	nut.analysis  = next;
	nut.nudging   = strdup(goal);
	nut.updates   = quip;
	nut.turnout   = v_fileinst(files[0], vmark_keywords, VMARK_MICROFORMAT);
	nut.safety	= _vmark_checks;

	VRETURN_NUTJOB(MARK, nut);
}

// search the codebase via AI and return matching files
char *v_codefind(const char *quest) {
	size_t len = strlen(quest);
	char *list = v_infolist(NULL, 0, quest);
	char *quip = malloc(len + 54);
	char *need = malloc(len + 128);
	peanuts_t nut  = {0};

	sprintf(quip, "%s\n\n%s", VCODE_SEARCH_PREAMBLE, quest);
	sprintf(need, "%s\n\n%s\n\n%s\n\n", VCODE_SEARCH_DIRECTIVE, quest, VCODE_SEARCH_FORMAT);

	nut.persona    = v_fileinst(NULL, vinfo_keywords, vinfo_instruct);
	nut.evidence   = list;
	nut.analysis   = quip;
	nut.nudging    = need;
	nut.updates    = strdup(VCODE_SEARCH_ACCEPTED);
	nut.turnout    = VCODE_MICROFORMAT;
	nut.safety     = _vcode_search;

	VRETURN_NUTJOB(INFO, nut);
}

// search the codebase via AI and return code to move
char *v_codemove(const char *quest) {
	size_t len = strlen(quest);
	char *list = v_infolist(NULL, 0, quest);
	char *quip = malloc(len + 54);
	char *need = malloc(len + 128);
	peanuts_t nut  = {0};

	sprintf(quip, "%s\n\n%s", VCODE_SEARCH_PREAMBLE, quest);
	sprintf(need, "%s\n\n%s\n\n%s\n\n", VCODE_SEARCH_DIRECTIVE, quest, VCODE_SEARCH_FORMAT);

	nut.persona   = v_fileinst(NULL, vinfo_keywords, vinfo_instruct);
	nut.evidence  = list;
	nut.analysis  = quip;
	nut.nudging   = need;
	nut.updates   = strdup(VCODE_SEARCH_ACCEPTED);
	nut.turnout   = VCODE_MICROFORMAT;
	nut.safety    = _vcode_search;

	VRETURN_NUTJOB(INFO, nut);
}

// update a single file by having AI address its comment markers
int v_codefile(const char *filename, const char *goal) {
	const char *mk = v_filemark(filename);
	char *code = v_fileload(filename);
	char *inst = v_fileinst(filename, vinfo_keywords, vinfo_instruct);
	char *list;
	char *todo = malloc(1024);
	char *src;
	size_t len = 0;
	size_t cap = 1024;
	int t = v_markscan(code, vinfo_keywords, mk, &list);
	peanuts_t nut  = {0};

	if (!t && goal) {
		v_markeach(&filename, 1, goal);

		code = v_fileload(filename);
		t = v_markscan(code, vinfo_keywords, mk, &list);
	}

	if (!t) return !!v_err(VFILE_NOTICE_DONE, filename);

	v_bufwrite(&todo, &len, VCODE_SOURCE_ACCEPTED "\n\n", &cap);
	v_bufwrite(&todo, &len, list, &cap);
	free((void*)list);
	// TODO: Write a REF inclusion function so I can do
	// if (coderefs(code, mk, &list)) bufwrite(&todo, &len, list, &cap);
	// TODO: We may want doc/STANDARDS.md included here
	v_bufwrite(&todo, &len, "\n\n" VCODE_SOURCE_CONTINUE, &cap);

	nut.persona    = inst;
	nut.evidence   = strdup(goal);
	nut.analysis   = strdup(mk);
	nut.nudging    = code;
	nut.updates    = todo;
	nut.turnout    = inst;
	nut.safety     = _vcode_writes;

	VCALL_NUTJOB(CODE, nut, src);

	if (!src) v_err("AI Error - %s", nutbad());

	v_filesave(filename, src);
	v_out(VFILE_NOTICE_EDIT, filename);
	VWIPE_NUTJOB(nut);
	free(src);

	return 1;
}

/* TODO: if the keyword is move it write:
---- move to: <filename>
---- move from: <filename>

as bookends...

Then you grap that and do a vinfo with a goal using that is the goal.

*/

static inline char *_vplan_field(char **line) {
	if (!line || !*line || !**line) return NULL;

	char *field = *line;
	char *p = field;

	while (*p && !(*p == '\n' && p[1] == '-')) p++;

	if (*p) {
		*p = '\0';
		*line = p + 1;
	} else {
		*line = p;
	}

	return field;
}

static inline int _vplan_files(char *line, vstep_t *step) {
	char* next;

	if (!line || !step) return 0;

	while(line && *line) {
		next = strchr(line, '\n');

		if (!step->count || step->count % 8 == 0) {
			char **files = realloc(step->files, sizeof(char*) * (step->count + 8));

			if (!files) return step->count;

			step->files = files;
		}

		if (strncmp(line, "  - `", 5) != 0) break;

		char *name = line + 5;
		char *end = strchr(name, '`');

		if (!end) break;

		*end = '\0';
		step->files[step->count++] = name;
		line = (next ? next : end) + 1;
	}

	return step->count;
}

static inline int _vplan_steps(char **line, vplan_t *plan) {
	int cap = 2;
	char* next;

	while(line && *line) {
		if (strncmp(*line, "- [", 3) != 0 || ((*line)[3] != ' ' && (*line)[3] != 'x') || (*line)[4] != ']' || (*line)[5] != ' ') return plan->count;
		if (!plan->count || plan->count >= cap) {
			cap *= 2;
			plan->steps = realloc(plan->steps, sizeof(vstep_t*) * cap);

			if (!plan->steps) return plan->count;
		}

		vstep_t *step = calloc(1, sizeof(vstep_t));

		if (!step) return plan->count;

		step->done = ((*line)[3] == 'x');
		step->task = *line + 6;

		if ((next = strchr(*line, '\n'))) {
			*next = '\0';
			*line = next + 1;
			plan->steps[plan->count++] = step;
		} else {
			free((void*)step);
		}

		if (!_vplan_files(*line, step)) return plan->count;
	};

	return plan->count;
}

static inline int _vplan_parser(char *str, vplan_t *plan) {
	if (!str) return 0;
	if (plan) v_planfree(plan);

	(*plan) = (vplan_t){0};

	char *line = str;

	plan->title = _vplan_field(&line);

	if (!plan->title) return 0;
	if (strncmp(line, "---", 3) != 0) return 0;

	line += 4;

	plan->goals = _vplan_field(&line);

	if (_vplan_steps(&line, plan)) plan->notes = _vplan_field(&line);

	return plan->count;
}

static inline bool _vplan_update(peanuts_t *nut, char **res) {
	vplan_t plan = {0};

	return (_vplan_parser(*res, &plan)) ? v_plansave(&plan) : 0;
}

static inline bool _vplan_search(peanuts_t *nut, char **res) {
	int actions = 0;
	int replies = 0;
	char term[256];
	char note[2048];
	char *ans = malloc(128);
	size_t alen  = 0;
	size_t acap  = 128;
	char *buf = malloc(1024);
	size_t blen  = 0;
	size_t bcap  = 1024;
	char *line =  *res;

	while (*line) {
		switch(_vinfo_parser(&line, term, note)) {
			case VINFO_SEARCH: actions += _vinfo_search(&buf, &blen, &bcap, term, note); break;
			case VINFO_REVIEW: actions += _vinfo_review(&buf, &blen, &bcap, term, note); break;
			case VINFO_DIGEST: actions += _vinfo_digest(&buf, &blen, &bcap, term, note); break;
			case VINFO_MEMORY: actions += _vinfo_memory(&buf, &blen, &bcap, term, note); break;
			case VINFO_FORGET: actions += _vinfo_memory(&buf, &blen, &bcap, term, NULL); break;
			case VINFO_ANSWER: replies += _vinfo_answer(&buf, &alen, &acap, term, note); break;
			default: break;
		}
	}

	if (!actions && replies) {
		nut->updates = VPLAN_SEARCH_CONTINUE;
		nut->turnout = VPLAN_MICROFORMAT;
		nut->safety  = _vplan_update;
	}

	v_bufwrite(&buf, &blen, "# " VPLAN_SOURCE_PREAMBLE "\n" VPLAN_SOURCE_DIRECTIVE "\n\n", &bcap);
	v_bufwrite(&buf, &blen, ans, &bcap);

	free((void*)nut->analysis);
	free((void*)nut->nudging);
	free(ans);

	nut->analysis = *res;
	nut->nudging = buf;

	return 0;
}

void v_planfree(vplan_t *plan) {
	if (!plan) return;

	while(plan->count && plan->steps[--plan->count]) free(plan->steps[plan->count]);

	free(plan->steps);
	free(plan->title);
	free(plan);
}

void v_planwipe() {
	unlink(VFILENAME_PLAN);
}

vplan_t *v_planload() {
	char *str = v_fileload(VFILENAME_PLAN);
	vplan_t *plan = calloc(1, sizeof(vplan_t));

	_vplan_parser(str, plan);

	return plan;
}

char *v_plantext(vplan_t *plan) {
	if (!plan || !plan->goals) return NULL;

	// Estimate initial buffer size
	size_t cap = 1024;
	size_t len = 0;
	char *buf = malloc(cap);

	if (!buf) return NULL;

	// Write goal
	v_bufwrite(&buf, &len, plan->goals, &cap);
	v_bufwrite(&buf, &len, "\n---\n", &cap);

	// Write each step
	for (int i = 0; i < plan->count; i++) {
		vstep_t *step = plan->steps[i];

		if (!step) continue;

		// Write checkbox and task
		v_bufwrite(&buf, &len, step->done ? "\n- [x] " : "\n- [ ] ", &cap);
		v_bufwrite(&buf, &len, step->task, &cap);
		v_bufwrite(&buf, &len, "\n", &cap);

		// Write files
		for (int j = 0; j < step->count; j++) {
			v_bufwrite(&buf, &len, "  - `", &cap);
			v_bufwrite(&buf, &len, step->files[j], &cap);
			v_bufwrite(&buf, &len, "`\n", &cap);
		}
	}

	return buf;
}

int v_plansave(vplan_t *plan) {
	if (!plan) return 0;

	char *text = v_plantext(plan);
	if (!text) return 0;

	size_t written = v_filesave(VFILENAME_PLAN, text);
	free((void*)text);

	return (written != (size_t)-1);
}

int v_plannext(vplan_t *plan) {
	if (!plan) return 0;

	// Find first incomplete step
	for (int i = 0; i < plan->count; i++) {
		vstep_t *step = plan->steps[i];

		if (!step || step->done) continue;

		// Execute this step
		if (step->count > 0) {
			v_taskmark((const char**)step->files, step->count, step->task);
			v_taskcode((const char**)step->files, step->count, step->task);

			step->done = 1;

			v_plansave(plan);

			return 1;
		}
	}

	return 0;
}

int v_planexec(vplan_t *plan) {
	if (!plan) return 0;

	int n = 0;
	while (v_plannext(plan)) n++;

	return n;
}

char *v_planmore(const char *goal) {
	char *plan = v_fileload(VFILENAME_PLAN);
	char *list = v_infolist(NULL, 0, plan);
	char *quip = malloc(strlen(goal) + strlen(VPLAN_SEARCH_PREAMBLE) + 4);
	char *need = malloc(strlen(goal) + strlen(VPLAN_SEARCH_PREAMBLE) + 4);
	peanuts_t nut  = {0};

	sprintf(quip, "%s\n\n%s", VPLAN_SEARCH_PREAMBLE, goal);
	sprintf(need, "%s\n\n%s\n\n%s\n\n", VPLAN_SEARCH_DIRECTIVE, goal, VPLAN_SEARCH_FORMAT);

	nut.persona    = v_fileinst(NULL, vinfo_keywords, vinfo_instruct);
	nut.evidence   = list;
	nut.analysis   = quip;
	nut.nudging    = need;
	nut.updates    = strdup(VPLAN_SEARCH_ACCEPTED);
	nut.turnout    = VCODE_MICROFORMAT;
	nut.safety     = _vplan_search;

	VRETURN_NUTJOB(INFO, nut);
}

char *v_plantask(const char **files, int n, const char *goal) {
	char *info = v_infoscan(files, n, NULL);
	char *need = malloc(strlen(goal) + strlen(VPLAN_SEARCH_DIRECTIVE) + strlen(info) + 8);

	sprintf(need, "%s\n\n# %s\n\n%s", goal, VPLAN_SEARCH_DIRECTIVE, info);

	char *rtn = v_planmore(need);

	free(info);
	free(need);

	return rtn;
}

char *v_plantodo(const char **files, int n, const char *goal) {
	char *plan = v_fileload(VFILENAME_PLAN);
	char *need = malloc(strlen(goal) + strlen(VPLAN_REVIEW_DIRECTIVE) + strlen(plan) + 8);

	sprintf(need, "%s\n\n# %s\n\n%s", goal, VPLAN_REVIEW_DIRECTIVE, plan);

	return v_infoscan(files, n, need);
}

/**
 * NOTE: this is the central nervous system of vanguard.
 */

// answer questions about the codebase by searching files
char *v_taskinfo(const char **files, int n, const char *goal) {
	char *info = NULL;

	if (goal) v_out(VTASK_NOTICE_INFO, goal);
	if (!n && !goal) info = v_infolist(NULL, 0, NULL);
	else if (!n) info = v_infofind(goal);
	else info = v_infoscan(files, n, goal);

	return info;
}

// mark files for development by adding AI-generated comments
char *v_taskmark(const char **files, int n, const char *goal) {
	char *mark = NULL;

	if (goal) v_out(VTASK_NOTICE_MARK, goal);
	if (!n && !goal) mark = v_marklist(NULL, 0, vmark_keywords);
	else if (!goal) mark = v_marklist(files, n, vmark_keywords);
	else mark = v_markeach(files, n, goal);

	return mark;
}

// run code generation and marking across files
char *v_taskcode(const char **files, int n, const char *goal) {
	int i = 0;

	if (goal) v_out(VTASK_NOTICE_CODE, goal);
	if (n > 1) v_taskmark(files, n, goal);

	while (i < n) v_codefile(files[i++], goal);

	char *buf = NULL;
	size_t len = 0;
	size_t cap = 1024;
	char *summaries = _vtool_summary(files, n);

	if (summaries) {
		v_bufwrite(&buf, &len, "# " VINFO_LABEL_EDITED "\n", &cap);
		v_bufwrite(&buf, &len, summaries, &cap);
		free(summaries);
	}

	if (goal) {
		v_bufwrite(&buf, &len, "\n", &cap);
		v_bufwrite(&buf, &len, goal, &cap);
		v_bufwrite(&buf, &len, "\n", &cap);
	}

	return buf;
}

// manage plan items: list, add, or show plans for files (stub)
char *v_taskplan(const char **files, int n, const char *goal) {
	if (n > 1) v_plantask(files, n, goal);
	else if (goal) v_planmore(goal);

	return v_fileload(VFILENAME_PLAN);
}

// Show the current state of the plan or ask questions
char *v_tasktodo(const char **files, int n, const char *goal) {
	return v_plantodo(files, n, goal);
}

// Exectute everything left
char *v_taskauto(const char **files, int n, const char *goal) {
	if (goal) v_planmore(goal);

	vplan_t *plan = v_planload();

	v_planexec(plan);
	v_planfree(plan);

	return v_plantodo(files, n, goal);
}

// Exectute the next task
char *v_tasknext(const char **files, int n, const char *goal) {
	if (goal) v_planmore(goal);

	vplan_t *plan = v_planload();

	v_plannext(plan);
	v_planfree(plan);

	return v_plantodo(files, n, goal);
}

// Force a report on the tasks
char *v_taskdone(const char **files, int n, const char *goal) {
	if (!goal) goal = VPLAN_REVIEW_DELETION;

	char *rpt = v_plantodo(files, n, goal);

	v_planwipe();

	return rpt;
}

// TODO: run tests for the specified files (stub)
char *v_tasktest(const char **files, int n, const char *goal) {
	(void)files; (void)n;

	if (goal) v_out(VTASK_NOTICE_TEST, goal);

	return strdup(VTASK_NOTICE_NONE);
}

// TODO: open specified files in the editor (stub)
char *v_taskedit(const char **files, int n, const char *goal) {
	(void)files; (void)n;

	if (goal) v_out(VTASK_NOTICE_EDIT, goal);

	return strdup(VTASK_NOTICE_NONE);
}
