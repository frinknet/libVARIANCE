/* (c) 2026 FRINKnet & Friends - MIT Licence */
#include "variance.h"

FILE *vcore_out = NULL;
FILE *vcore_err = NULL;

/* Core Configuration */
#define X(mode, name, ctype, value) ctype v##mode##_##name = value;
#include "../x/prompts.x"
#undef X

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

// load a file (or stdin) into a malloc'd string
char *v_fileload(const char *filename) {
	FILE *f;

	if (strcmp(filename, "-") == 0) f = stdin;
	else f = fopen(filename, "r");

	if (!f) {
		v_err("Cannot find `%s`\n", filename);

		return NULL;
	}

	char *buf = NULL;
	size_t cap = 128;
	size_t len = 0;

	v_bufstream(&buf, &len, f, &cap, 0);

	if (ferror(f)) {
		v_err("Cannot read `%s`\n", filename);

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
		int n = snprintf(tmp, sizeof(tmp), "%s LINE %d - %s: %s %s\n", mk, todos[t].line, todos[t].type, todos[t].note, mk2);

		if (n > 0) v_bufwrite(&buf, &len, tmp, &cap);

		t++;
	}

	return buf;
}

// refresh info files
static inline char *_vinfo_refresh(const char *code) {
	char local[1024][256];
	int n = v_filelist(code, local, 1024);
	const char **files = (const char **)local;

	return v_infolist(files, n, NULL);
}

// search codebase with grep
static inline int _vinfo_search(char **buf, size_t *len, size_t *cap, const char *path, const char *terms) {
	char head[1024];

	snprintf(head, sizeof(head), "# LOCATE: `%s` in %s\n\n", terms, path);
	v_bufwrite(buf, len, head, cap);

	char *result = _vtool_grep(terms, path);

	if (result) {
		v_bufwrite(buf, len, result, cap);
		free(result);
	} else {
		v_bufwrite(buf, len, "(No matches found)\n", cap);
	}

	v_bufwrite(buf, len, "\n", cap);

	return 1;
}

// read a file to understand it
static inline int _vinfo_review(char **buf, size_t *len, size_t *cap, const char *path, const char *notes) {
	const char *lang = v_filelang(path);
	FILE *f = fopen(path, "r");
	char head[1024];

	if (notes) snprintf(head, 1024, "# REVIEW %s\n%s\n\n", path, notes);

	v_bufwrite(buf, len, head, cap);
	v_out("Reading `%s` - %s", path, notes);

	if (!f) {
		v_bufwrite(buf, len, "\n```", cap);
		v_bufwrite(buf, len, lang, cap);
		v_bufwrite(buf, len, "\n", cap);
		v_bufstream(buf, len, f, cap, 0);
		fclose(f);
		v_bufwrite(buf, len, "\n```\n\nPease write a digest of this file in your response.", cap);
	}

	return 1;
}

// summarize a file to remember
static inline int _vinfo_digest(char **buf, size_t *len, size_t *cap, const char *path, const char *notes) {
	char head[1024];

	snprintf(head, sizeof(head), "# SUMMARY %s\n%s\n\n", path, notes ? notes : "");
	v_bufwrite(buf, len, head, cap);

	return 1;
}

// remember something important
static inline int _vinfo_memory(char **buf, size_t *len, size_t *cap, const char *topic, const char *notes) {
	char path[256];

	snprintf(path, sizeof(path), "#%s", topic);

	return _vinfo_digest(buf, len, cap, path, notes);
}

// forget something unneeded
static inline int _vinfo_forget(char **buf, size_t *len, size_t *cap, const char *topic, const char *notes) {
	char path[256];

	snprintf(path, sizeof(path), "#%s", topic);

	return _vinfo_digest(buf, len, cap, path, NULL);
}

// TODO: giving an answer
static inline int _vinfo_genius(char **buf, size_t *len, size_t *cap, const char *source, const char *query) {
	char head[1024];

	snprintf(head, sizeof(head), "# GENIUS: %s\nQuery: %s\n\n", source, query);
	v_bufwrite(buf, len, head, cap);
	v_bufwrite(buf, len, "(Genius requires external API - stubbed)\n", cap);

	return 1;
}

// giving an answer
static inline int _vinfo_answer(char **buf, size_t *len, size_t *cap, const char *subject, const char *ideas) {
	v_bufwrite(buf, len, "# ", cap);
	v_bufwrite(buf, len, subject, cap);
	v_bufwrite(buf, len, "\n", cap);
	v_bufwrite(buf, len, ideas, cap);
	v_bufwrite(buf, len, "\n\n", cap);

	return 1;
}

// parsing an answer
static inline vinfotype_t _vinfo_parser(const char **list, char term[256], char note[2048]) {
	char verb[16] = {0};
	const char *line = *list;
	term[0] = '\0';
	note[0] = '\0';

	int n = sscanf(line, "%15[^|\n]|%255[^|\n]|%2047[^\n]", verb, term, note);
	const char *eol = strchr(line, '\n');
	if (eol) *list = eol + 1;
	else *list = line + strlen(line);

	if (n < 3) return VINFO_OTHERS;
	if (strcmp(verb, "review") == 0)   return VINFO_REVIEW;
	if (strcmp(verb, "search") == 0)   return VINFO_SEARCH;
	if (strcmp(verb, "genius") == 0)   return VINFO_GENIUS;
	if (strcmp(verb, "digest") == 0)   return VINFO_DIGEST;
	if (strcmp(verb, "memory") == 0)   return VINFO_MEMORY;
	if (strcmp(verb, "forget") == 0)   return VINFO_FORGET;
	if (strcmp(verb, "answer") == 0)   return VINFO_ANSWER;
	return VINFO_OTHERS;
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
	const char *line = *res;

	while (*line) {
		switch(_vinfo_parser(&line, term, note)) {
			case VINFO_REVIEW: actions += _vinfo_review(&buf, &len, &cap, term, note); break;
			case VINFO_SEARCH: actions += _vinfo_search(&buf, &len, &cap, term, note); break;
			case VINFO_GENIUS: actions += _vinfo_genius(&buf, &len, &cap, term, note); break;
			case VINFO_DIGEST: actions += _vinfo_digest(&buf, &len, &cap, term, note); break;
			case VINFO_MEMORY: actions += _vinfo_memory(&buf, &len, &cap, term, note); break;
			case VINFO_FORGET: actions += _vinfo_forget(&buf, &len, &cap, term, note); break;
			case VINFO_ANSWER: replies += _vinfo_answer(&buf, &len, &cap, term, note); break;
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

static inline char *_vinfo_lookup(const char *path) {
	static char *cache = NULL;

	if (!cache) cache = v_fileload(".v-info");
	if (!cache) return NULL;

	char key[2048];

	snprintf(key, sizeof(key), "# %s |", path);

	char *p = strstr(cache, key);

	if (!p) return NULL;

	p += strlen(key);

	char *end = strchr(p, '\n');

	if (!end) return NULL;

	size_t len = end - p;
	char *res = malloc(len + 1);

	memcpy(res, p, len);

	res[len] = '\0';

	return res;
}

// list info for files and notes
char *v_infolist(const char **files, int n, const char *note) {
	char *buf = NULL;
	size_t len = 0;
	size_t cap = 128;
	char **list = NULL;
	int count = 0;

	if (n == 0) {
		char *walked = _vtool_find(NULL);

		if (!walked) return NULL;

		// Split walked into array for uniform processing
		char *line = walked;
		char *next;

		while ((next = strchr(line, '\n')) != NULL) {
			*next = '\0';

			if (strlen(line) > 0) {
				list = realloc(list, (count + 1) * sizeof(char *));
				list[count++] = line;
			}

			line = next + 1;
		}
	} else {
		list = (char **)files;
		count = n;
	}

	for (int i = 0; i < count; i++) {
		char *summary = _vinfo_lookup(list[i]);
		if (summary) {
			v_bufwrite(&buf, &len, "Remember ", &cap);
			v_bufwrite(&buf, &len, list[i], &cap);
			v_bufwrite(&buf, &len, ":\n", &cap);
			v_bufwrite(&buf, &len, summary, &cap);
			v_bufwrite(&buf, &len, "\n\n", &cap);
			free(summary);
		} else {
			v_bufwrite(&buf, &len, list[i], &cap);
			v_bufwrite(&buf, &len, "\n", &cap);
		}
	}

	if (n == 0 && list) free(list[0]); // Free the big walked block
	if (n == 0) free(list); // Free the pointer array

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

	while(_vinfo_review(&buf, &len, &cap, files[i++], NULL));

	if (note) {
		v_bufwrite(&buf, &len, "\n\n", &cap);
		v_bufwrite(&buf, &len, note, &cap);
		v_bufwrite(&buf, &len, "\n", &cap);
	}

	return buf;
}

// research in files
char *v_infoscan(const char **files, int n, const char *note) {
	const char *list = v_infolist(files, n, note);
	const char *code = v_infoshow(files, n, note);
	peanuts_t nut  = {0};

	nut.persona    = v_fileinst(files[0], vinfo_keywords, vinfo_instruct);
	nut.evidence   = list;
	nut.analysis   = strdup("Okay. Show me the contents of these files...");
	nut.nudging    = code;
	nut.updates    = strdup("Okay, so I need to review files first to figure out how to summarize.");
	nut.turnout    = INFO_TEMPLATES;
	nut.safety     = _vinfo_checks;

	VRETURN_NUTJOB(INFO, nut);
}

// research in codebase
char *v_infofind(const char *query) {
	const char *list = v_infolist(NULL, 0, query);
	size_t len = strlen(query);
	char *quip = malloc(len + 56);
	char *need = malloc(len + 56);
	peanuts_t nut  = {0};

	sprintf(quip, "Okay I see the files. But you want me to focus on:\n\n%s", query);
	sprintf(need, "Yes. What files do you need to answer this:\n\n%s", query);

	nut.persona	= v_fileinst(NULL, (const char*[]){"TODO", "FIXME", NULL}, "You are an architect.");
	nut.evidence   = list;
	nut.analysis   = quip;
	nut.nudging	= need;
	nut.updates	= strdup("Okay, so I need to search terms and review the code to figure this out.");
	nut.turnout	= "Respond with `review|path|notes` or `search|path|terms`";
	nut.safety	 = _vinfo_checks;

	VRETURN_NUTJOB(INFO, nut);
}

// render a file with line numbers into the buffer
static inline int _vmark_number(char **buf, size_t *len, size_t *cap, const char *path) {
	const char *lang = v_filelang(path);
	FILE *f = fopen(path, "r");
	char head[1024];

	v_bufwrite(buf, len, path, cap);
	v_out("Reading `%s`...", path);

	if (!f) {
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
static inline int _vmark_parser(const char *str, vmark_t *marks, size_t cap) {
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

	// Split marker: prefix + suffix
	int len1 = 0;

	while (mk[len1] && mk[len1] != ' ') len1++;

	const char *mk2 = mk + len1;

	if (*mk2 == ' ') mk2++;

	int len2 = strlen(mk2);
	char *buf = NULL;
	size_t cap = 128;
	size_t len = 0;
	const char *src = v_fileload(path);
	const char *in = src;
	int line = 1;
	size_t t = 0;

	// Walk file line by line
	while (*in) {
		// avoid marks that are not for this file
		if (strcmp(marks[t].path, path)) ++t;

		// Insert ALL marks matching current line BEFORE the line itself
		while (t < cnt && marks[t].line == line) {
			char tmp[1024];
			int n = snprintf(tmp, sizeof(tmp), "%s %s: %s %s\n", mk, marks[t].type, marks[t].note, mk2);

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

	free((void*)src);

	// Tack on any remaining marks (shouldn't happen, but just in case)
	while (t < cnt) {
		char tmp[1024];
		int n = snprintf(tmp, sizeof(tmp), "%s LINE %d - %s: %s %s\n", mk, marks[t].line, marks[t].type, marks[t].note, mk2);

		if (n > 0) v_bufwrite(&buf, &len, tmp, &cap);

		t++;
	}

	return buf;
}

// check AI response and insert marks into source files
static inline bool _vmark_checks(peanuts_t *nut, char **res) {
	vmark_t marks[MARK_MAX] = {0};
	int n = _vmark_parser(*res, marks, MARK_MAX);
	const char *path;

	if (!n) {
		free((void*)nut->updates);

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

			v_out("Updating `%s'", path);
			free((void*)src);

			if (saved < 1) return 0;
		}
	}

	v_out("Added %d new comments.", n);

	return 1;
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
	const char *code;
	char *quip = malloc(72 + strlen(goal));
	char *next = strdup("Okay. I see the source code. But what are your goals?");
	// Use configured keywords instead of hardcoded
	const char **kw = vmark_keywords ? vmark_keywords : (const char*[]){"TODO", "FIXME", NULL};
	const char *inst = vmark_instruct ? vmark_instruct : "You are a {{language}} coding coach adding only {{keyword}} comments for junior developers to fix.";
	peanuts_t nut = {0};

	if (!n) {
		sprintf(quip, "%s\n\nIf that is my only goal which files do I need to modify?", goal);

		code = v_codefind(quip);

		if (!code) return v_err("No files found for: %s", goal);

		n = v_filelist(code, local, 256);
		files = (const char **)local;

		free((void*)code);
	}

	code = _vmark_target(files, n, goal);

	sprintf(quip, "Okay, so you want:\n\n%s\n\nAnd I need to write comments to instruct you...", goal);

	nut.persona   = v_fileinst(files[0], kw, inst);
	nut.evidence  = code;
	nut.analysis  = next;
	nut.nudging   = goal;
	nut.updates   = quip;
	nut.turnout   = v_fileinst(files[0], kw, "Return ONLY strict insert points like: `filename|line|TYPE|comment` (of these types: {{keywords}})");
	nut.safety	= _vmark_checks;

	VRETURN_NUTJOB(MARK, nut);
}

// check AI response for code-worthy comments (used as safety callback)
static inline bool _vcode_writes(peanuts_t *nut, char **res) {
	return v_codescan(*res, vcode_keywords, nut->analysis, NULL);
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
	const char *line =  *res;

	while (*line) {
		switch(_vinfo_parser(&line, term, note)) {
			case VINFO_SEARCH: actions += _vinfo_search(&buf, &blen, &bcap, term, note); break;
			case VINFO_REVIEW: actions += _vinfo_review(&buf, &blen, &bcap, term, note); break;
			case VINFO_DIGEST: actions += _vinfo_digest(&buf, &blen, &bcap, term, note); break;
			case VINFO_MEMORY: actions += _vinfo_memory(&buf, &blen, &bcap, term, note); break;
			case VINFO_FORGET: actions += _vinfo_forget(&buf, &blen, &bcap, term, note); break;
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

	v_bufwrite(&buf, &blen, "# REMEMBER THESE FILES\nInclude them in your output...\n\n", &bcap);
	v_bufwrite(&buf, &blen, ans, &bcap);

	free((void*)nut->analysis);
	free((void*)nut->nudging);
	free(ans);

	nut->analysis = *res;
	nut->nudging = buf;

	return 0;
}

// scan source for a line starting with a known keyword marker
static inline const char* _vcode_markup(const char* src, const char* kw[], const char* mk) {
	const char *p = src;
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

// scan source for comment markers matching keywords and return a numbered list
int v_codescan(const char *src, const char *kw[], const char *mk, char **list) {
	char *buf = NULL;
	size_t cap = 128;
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

// search the codebase via AI and return matching files
char *v_codefind(const char *quest) {
	size_t len = strlen(quest);
	char *list = v_infolist(NULL, 0, quest);
	char *quip = malloc(len + 54);
	char *need = malloc(len + 128);
	peanuts_t nut  = {0};

	sprintf(quip, "Okay I see the files. But you want me to focus on:\n\n%s", quest);
	sprintf(need, "Search the codebase to answer:\n\n%s\n\nThen list the files as `answer|filename|comment` after you do you research.\n\n", quest);

	nut.persona    = v_fileinst(NULL, vinfo_keywords, vinfo_instruct);
	nut.evidence   = list;
	nut.analysis   = quip;
	nut.nudging    = need;
	nut.updates    = strdup("Okay, so I need to search code first to figure this out. Let me think about what make the most sense.");
	nut.turnout    = CODE_TEMPLATES;
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

	sprintf(quip, "Okay I see the files. But you want me to focus on:\n\n%s", quest);
	sprintf(need, "Search the codebase to answer:\n\n%s\n\nThen list the files as `answer|filename|comment` after you do you research.\n\n", quest);

	nut.persona   = v_fileinst(NULL, vinfo_keywords, vinfo_instruct);
	nut.evidence  = list;
	nut.analysis  = quip;
	nut.nudging   = need;
	nut.updates   = strdup("Okay, so I need to search code first to figure this out. Let me think about what make the most sense.");
	nut.turnout   = CODE_TEMPLATES;
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
	int t = v_codescan(code, vinfo_keywords, mk, &list);
	peanuts_t nut  = {0};

	if (!t && goal) {
		v_markeach(&filename, 1, goal);

		code = v_fileload(filename);
		t = v_codescan(code, vinfo_keywords, mk, &list);
	}

	if (!t) return !!v_err("No comments found in `%s`", filename);

	v_bufwrite(&todo, &len, "Okay...\n\n", &cap);
	v_bufwrite(&todo, &len, "I found these comments:\n\n", &cap);
	v_bufwrite(&todo, &len, list, &cap);
	free((void*)list);
	// TODO: Write a REF inclusion function so I can do
	// if (coderefs(code, mk, &list)) bufwrite(&todo, &len, list, &cap);
	// TODO: We may want doc/STANDARDS.md included here
	v_bufwrite(&todo, &len, "\n\nAre we ready to address the comments?", &cap);

	nut.persona    = inst;
	nut.evidence   = goal;
	nut.analysis   = mk;
	nut.nudging    = code;
	nut.updates    = todo;
	nut.turnout    = inst;
	nut.safety     = _vcode_writes;

	VCALL_NUTJOB(CODE, nut, src);

	if (!src) v_err("AI Error - %s", nutbad());

	v_filesave(filename, src);
	v_out("Updated `%s`%s%s", filename, !goal ? "" : " - ", goal);

	free((void*)inst);
	free((void*)code);
	free((void*)todo);
	free(src);

	return 1;
}

/* TODO: if the keyword is move it write:
---- move to: <filename>
---- move from: <filename>

as bookends...

Then you grap that and do a vinfo with a goal using that is the goal.

*/

/**
 * NOTE: this is the central nervous system of vanguard.
 */

// answer questions about the codebase by searching files
char *v_taskinfo(const char **files, int n, const char *goal) {
	char *info = NULL;

	if (goal) v_out("Searching Files: %s", goal);
	if (!n && !goal) info = v_infolist(NULL, 0, NULL);
	else if (!n) info = v_infofind(goal);
	else info = v_infoscan(files, n, goal);

	return info;
}

// mark files for development by adding AI-generated comments
char *v_taskmark(const char **files, int n, const char *goal) {
	char *mark = NULL;

	if (goal) v_out("Marking Files: %s", goal);
	if (!n && !goal) mark = v_marklist(NULL, 0, vmark_keywords);
	else if (!goal) mark = v_marklist(files, n, vmark_keywords);
	else mark = v_markeach(files, n, goal);

	return mark;
}

// run code generation and marking across files
char *v_taskcode(const char **files, int n, const char *goal) {
	int i = 0;
	char *result = NULL;
	size_t len = 0;
	size_t cap = 1024;
	int success_count = 0;
	int fail_count = 0;

	if (n > 1) {
		char *mark_result = v_taskmark(files, n, goal);

		if (mark_result) {
			v_bufwrite(&result, &len, mark_result, &cap);
			v_bufwrite(&result, &len, "\n", &cap);
			free(mark_result);
		}
	}

	while (i < n) {
		const char *filename = files[i++];

		if (v_codefile(filename, goal)) {
			success_count++;
			char msg[256];

			snprintf(msg, sizeof(msg), "✓ Updated: %s\n", filename);
			v_bufwrite(&result, &len, msg, &cap);
		} else {
			fail_count++;
			char msg[256];

			snprintf(msg, sizeof(msg), "✗ Failed: %s\n", filename);
			v_bufwrite(&result, &len, msg, &cap);
		}
	}

	char summary[256];

	snprintf(summary, sizeof(summary), "\nProcessed %d files: %d succeeded, %d failed\n", n, success_count, fail_count);
	v_bufwrite(&result, &len, summary, &cap);

	return result ? result : strdup("No files processed");
}

// TODO: manage plan items: list, add, or show plans for files (stub)
char *v_taskplan(const char **files, int n, const char *goal) {
	(void)files; (void)n; (void)goal;
	return strdup("Plan functionality not yet implemented");
}

// TODO: manage todo items: list, add, or show todos for files (stub)
char *v_tasktodo(const char **files, int n, const char *goal) {
	(void)files; (void)n; (void)goal;
	return strdup("Todo functionality not yet implemented");
}

// TODO: get the next task to work on (stub)
char *v_tasknext(const char **files, int n, const char *goal) {
	(void)files; (void)n; (void)goal;
	return strdup("Next task functionality not yet implemented");
}

// TODO: run tests for the specified files (stub)
char *v_tasktest(const char **files, int n, const char *goal) {
	(void)files; (void)n; (void)goal;
	return strdup("Test functionality not yet implemented");
}

// TODO: open specified files in the editor (stub)
char *v_taskedit(const char **files, int n, const char *goal) {
	(void)files; (void)n; (void)goal;
	char *result = NULL;
	size_t len = 0;
	size_t cap = 128;

	v_bufwrite(&result, &len, "Opening files in editor:\n", &cap);

	for (int i = 0; i < n; i++) {
		v_bufwrite(&result, &len, "  ", &cap);
		v_bufwrite(&result, &len, files[i], &cap);
		v_bufwrite(&result, &len, "\n", &cap);
	}

	return result;
}
