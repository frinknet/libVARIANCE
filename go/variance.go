// (c) 2026 FRINKnet & Friends - MIT Licence
//go:generate go run gen.go

package libvariance

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	peanuts "github.com/frinknet/libPEANUTS"
)

type Config struct {
	Keywords []string
	APIURL   string
	APIKey   string
	Model    string
	Instruct string
	Timeout  int
	Tokens   int
	Tries    int
	Pause    int
	Temp     float64
}

var configs map[string]Config

func init() {
	var err error
	fileTypes, configs, err = loadXConfig()
	if err != nil {
		panic(err)
	}
}

func LoadPrompts(path string) (map[string]Config, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	out := map[string]Config{}
	scanner := bufio.NewScanner(f)
	lineNo := 0
	for scanner.Scan() {
		lineNo++
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "//") {
			continue
		}

		args := splitXArgs(line)
		if len(args) != 4 {
			return nil, fmt.Errorf("%s:%d: expected X(mode, name, ctype, value), got %q", path, lineNo, line)
		}
		mode := cleanXName(args[0])
		name := strings.TrimSpace(args[1])
		ctype := strings.TrimSpace(args[2])
		value := trimXValue(args[3])
		key := name
		cfg := out[mode]
		switch {
		case key == "keywords" && ctype == "const char**":
			cfg.Keywords = splitList(value)
		case ctype == "const char*":
			switch key {
			case "apiurl":
				cfg.APIURL = value
			case "apikey":
				cfg.APIKey = value
			case "model":
				cfg.Model = value
			case "instruct":
				cfg.Instruct = value
			}
		case ctype == "int":
			switch key {
			case "timeout":
				cfg.Timeout = atoi(value)
			case "tokens":
				cfg.Tokens = atoi(value)
			case "tries":
				cfg.Tries = atoi(value)
			case "pause":
				cfg.Pause = atoi(value)
			}
		case ctype == "double":
			if key == "temp" {
				cfg.Temp = atof(value)
			}
		}
		out[mode] = cfg
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	return out, nil
}

func configFor(name string) Config {
	cfg := Config{}
	if configs != nil {
		cfg = configs[name]
	}
	switch name {
	case "info":
		cfg.Instruct = "You are a {{language}} coding agent focused ONLY on addressing {{keywords}} comments. Return with FULL fixed file ONLY!!! ({{filename}})"
	case "mark":
		cfg.Instruct = "You are a {{language}} coding agent focused ONLY on addressing {{keywords}} comments. Return with FULL fixed file ONLY!!! ({{filename}})"
	case "code":
		cfg.Instruct = "You are a {{language}} coding agent focused ONLY on addressing {{keywords}} comments. Return with FULL fixed file ONLY!!! ({{filename}})"
	case "plan":
		cfg.Instruct = "You are a {{language}} coding agent focused ONLY on addressing {{keywords}} comments. Return with FULL fixed file ONLY!!! ({{filename}})"
	}
	return cfg
}

func CoreConfig() Config {
	return configFor("core")
}

func InfoConfig() Config {
	return configFor("info")
}

func MarkConfig() Config {
	return configFor("mark")
}

func CodeConfig() Config {
	return configFor("code")
}

func PlanConfig() Config {
	return configFor("plan")
}

func (c Config) WithDefaults() Config {
	core := CoreConfig()
	if c.APIURL == "" {
		c.APIURL = core.APIURL
	}
	if c.APIKey == "" {
		c.APIKey = core.APIKey
	}
	if c.Model == "" {
		c.Model = core.Model
	}
	if c.Timeout == 0 {
		c.Timeout = core.Timeout
	}
	if c.Tokens == 0 {
		c.Tokens = core.Tokens
	}
	if c.Tries == 0 {
		c.Tries = core.Tries
	}
	if c.Pause == 0 {
		c.Pause = core.Pause
	}
	if c.Temp == 0 {
		c.Temp = core.Temp
	}
	return c
}

func NutmegFor(name string, fallback Config) *peanuts.Nutmeg {
	cfg := fallback.WithDefaults()
	switch name {
	case "info":
		cfg = InfoConfig().WithDefaults()
	case "mark":
		cfg = MarkConfig().WithDefaults()
	case "code":
		cfg = CodeConfig().WithDefaults()
	case "plan":
		cfg = PlanConfig().WithDefaults()
	}
	ctx := peanuts.NewNutmeg(cfg.Model, cfg.APIURL, cfg.APIKey)
	ctx.Timeout = cfg.Timeout
	ctx.Tokens = cfg.Tokens
	ctx.Tries = cfg.Tries
	ctx.Pause = cfg.Pause
	ctx.Temp = cfg.Temp
	return ctx
}

func CoreNutmeg() *peanuts.Nutmeg {
	return NutmegFor("", CoreConfig())
}

func InfoNutmeg() *peanuts.Nutmeg {
	return NutmegFor("info", InfoConfig())
}

func MarkNutmeg() *peanuts.Nutmeg {
	return NutmegFor("mark", MarkConfig())
}

func CodeNutmeg() *peanuts.Nutmeg {
	return NutmegFor("code", CodeConfig())
}

func PlanNutmeg() *peanuts.Nutmeg {
	return NutmegFor("plan", PlanConfig())
}

type FileType struct {
	Lang string
	Ext  string
	Mark string
}

var fileTypes []FileType

func init() {
	var err error
	fileTypes, configs, err = loadXConfig()
	if err != nil {
		panic(err)
	}
}

func defaultFileTypesPath() string {
	return filepath.Join("..", "x", "filetypes.x")
}

func LoadFileTypes(path string) ([]FileType, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	var out []FileType
	scanner := bufio.NewScanner(f)
	lineNo := 0
	for scanner.Scan() {
		lineNo++
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "//") {
			continue
		}

		parts := strings.Split(line, ",")
		if len(parts) != 3 {
			return nil, fmt.Errorf("%s:%d: expected X(lang, exts, mark), got %q", path, lineNo, line)
		}

		lang := cleanXName(parts[0])
		exts := strings.TrimSpace(parts[1])
		mark := strings.TrimSpace(parts[2])
		mark = strings.TrimSuffix(mark, ")")
		mark = strings.TrimRight(mark, " ")
		mark = strings.Trim(mark, "\"")
		mark = strings.TrimSpace(mark)
		if lang == "" || exts == "" || mark == "" {
			return nil, fmt.Errorf("%s:%d: empty X fields", path, lineNo)
		}

		out = append(out, FileType{Lang: lang, Ext: exts, Mark: mark})
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	return out, nil
}

func cleanXName(value string) string {
	value = strings.TrimSpace(value)
	value = strings.TrimPrefix(value, "X(")
	return strings.TrimSuffix(value, ")")
}

func splitList(value string) []string {
	value = strings.TrimSpace(value)
	value = strings.TrimSuffix(value, ";")
	value = strings.Trim(value, "{}")
	value = strings.ReplaceAll(value, "\"", "")
	parts := strings.Split(value, ",")
	out := make([]string, 0, len(parts))
	for _, part := range parts {
		part = strings.TrimSpace(part)
		if part != "" && !strings.EqualFold(part, "NULL") {
			out = append(out, part)
		}
	}
	return out
}

func atoi(value string) int {
	n, _ := strconv.Atoi(value)
	return n
}

func atof(value string) float64 {
	f, _ := strconv.ParseFloat(value, 64)
	return f
}

func FileTypes() []FileType {
	out := make([]FileType, len(fileTypes))
	copy(out, fileTypes)
	return out
}

func VFileType(filename string) FileType {
	for _, ft := range fileTypes {
		if ft.Match(filename) {
			return ft
		}
	}
	return FileType{Lang: "none"}
}

func (ft FileType) Match(filename string) bool {
	ext := ""
	if idx := strings.LastIndex(filename, "."); idx >= 0 {
		ext = filename[idx:]
	}
	return (ext != "" && strings.Contains(ft.Ext, ext)) || strings.Contains(ft.Ext, filename)
}

func FileLang(filename string) string {
	return VFileType(filename).Lang
}

func FileMark(filename string) string {
	return VFileType(filename).Mark
}

func splitLines(src string) []string {
	fields := strings.FieldsFunc(src, func(r rune) bool { return r == '\n' })
	out := make([]string, 0, len(fields))
	for _, field := range fields {
		field = strings.TrimSpace(field)
		if field != "" {
			out = append(out, field)
		}
	}
	return out
}

func FileList(src string, n int) []string {
	if n <= 0 {
		n = 1024
	}

	out := make([]string, 0, n)
	fields := strings.FieldsFunc(src, func(r rune) bool { return r == '|' || r == '\n' })
	for _, field := range fields {
		field = strings.TrimSpace(field)
		if field == "" {
			continue
		}
		out = append(out, field)
		if len(out) >= n {
			break
		}
	}
	return out
}

func FileLoad(filename string) (string, error) {
	if filename == "-" {
		data, err := io.ReadAll(os.Stdin)
		return string(data), err
	}
	data, err := os.ReadFile(filename)
	return string(data), err
}

func FileSave(filename, data string) (int, error) {
	if filename == "-" {
		return os.Stdout.Write([]byte(data))
	}
	err := os.WriteFile(filename, []byte(data), 0o666)
	return len(data), err
}

func VFiletype(filename string) string {
	return FileLang(filename)
}

func VFilelang(filename string) string {
	return FileLang(filename)
}

func VFilemark(filename string) string {
	return FileMark(filename)
}

func VFilelist(src string, n int) []string {
	return FileList(src, n)
}

func VFileload(filename string) (string, error) {
	return FileLoad(filename)
}

func VFilesave(filename, data string) (int, error) {
	return FileSave(filename, data)
}

func VFileinst(filename string, kw []string, inst string) string {
	return FileInst(filename, kw, inst)
}

func VTodoLine(line string, kw []string, mk string) string {
	return TodoLine(line, kw, mk)
}

func VTodofind(src string, kw []string, mk string) (string, int) {
	return TodoFind(src, kw, mk)
}

func VTodoscan(src string) ([]Todo, error) {
	return TodoScan(src)
}

func VTodomark(orig string, todos []Todo, mk string) string {
	return TodoMark(orig, todos, mk)
}

func VInfolist(files []string, note string) string {
	return InfoList(files, note)
}

func VInfoshow(files []string, note string) (string, error) {
	return InfoShow(files, note)
}

func VInfoscan(files []string, note string) (string, error) {
	return InfoScan(files, note)
}

func VInfofind(query string) (string, error) {
	return InfoFind(query)
}

func VMarklist(files []string, kw []string) string {
	return MarkList(files, kw)
}

func VMarkeach(files []string, goal string) (string, error) {
	return MarkEach(files, goal)
}

func VCodescan(src string, kw []string, mk string) (string, int) {
	return CodeScan(src, kw, mk)
}

func VCodefind(query string) (string, error) {
	return CodeFind(query)
}

func VCodemove(query string) (string, error) {
	return CodeMove(query)
}

func VCodefile(filename, goal string) error {
	return CodeFile(filename, goal)
}

func VTaskinfo(files []string, goal string) (string, error) {
	if len(files) == 0 && goal == "" {
		return InfoList(nil, ""), nil
	}
	if len(files) == 0 {
		return InfoFind(goal)
	}
	return InfoScan(files, goal)
}

func VTaskmark(files []string, goal string) (string, error) {
	if goal != "" {
		return MarkEach(files, goal)
	}
	return MarkList(files, MarkConfig().Keywords), nil
}

func VTaskcode(files []string, goal string) (string, error) {
	if len(files) == 0 {
		return "No files processed", nil
	}

	var result strings.Builder
	if len(files) > 1 {
		if mark, err := MarkEach(files, goal); err == nil {
			result.WriteString(mark)
			result.WriteByte('\n')
		}
	}

	success := 0
	fail := 0
	for _, file := range files {
		if err := CodeFile(file, goal); err != nil {
			fail++
			result.WriteString("✗ Failed: ")
			result.WriteString(file)
			result.WriteByte('\n')
		} else {
			success++
			result.WriteString("✓ Updated: ")
			result.WriteString(file)
			result.WriteByte('\n')
		}
	}
	result.WriteString(fmt.Sprintf("\nProcessed %d files: %d succeeded, %d failed\n", len(files), success, fail))
	return result.String(), nil
}

func VTaskplan(files []string, goal string) string {
	return "Plan functionality not yet implemented"
}

func VTasktodo(files []string, goal string) string {
	return "Todo functionality not yet implemented"
}

func VTasknext(files []string, goal string) string {
	return "Next task functionality not yet implemented"
}

func VTasktest(files []string, goal string) string {
	return "Test functionality not yet implemented"
}

func VTaskedit(files []string, goal string) string {
	var b strings.Builder
	b.WriteString("Opening files in editor:\n")
	for _, file := range files {
		b.WriteString("  ")
		b.WriteString(file)
		b.WriteByte('\n')
	}
	return b.String()
}

func FileInst(filename string, kw []string, inst string) string {
	if inst == "" {
		return ""
	}
	language := strings.ToUpper(FileLang(filename))
	keywords := strings.Join(kw, ", ")
	return strings.NewReplacer(
		"{{language}}", language,
		"{{keywords}}", keywords,
		"{{filename}}", filename,
	).Replace(inst)
}

func WithLineNumbers(src string) string {
	if src == "" {
		return ""
	}

	var b strings.Builder
	scanner := bufio.NewScanner(strings.NewReader(src))
	line := 1
	for scanner.Scan() {
		fmt.Fprintf(&b, "%d | %s\n", line, scanner.Text())
		line++
	}
	return b.String()
}

func VToolFind(path string) string {
	var b strings.Builder
	if path == "" {
		path = "."
	}
	walkFiles(path, 0, &b)
	return b.String()
}

func VToolGrep(term, path string) string {
	var b strings.Builder
	if path == "" {
		path = "."
	}
	for _, file := range walkFilesList(path, 0) {
		data, err := FileLoad(file)
		if err != nil {
			continue
		}
		if strings.Contains(data, term) {
			b.WriteString(file)
			b.WriteByte('\n')
		}
	}
	return b.String()
}

func walkFilesList(path string, depth int) []string {
	entries, err := os.ReadDir(path)
	if err != nil {
		return nil
	}

	out := make([]string, 0)
	for _, entry := range entries {
		name := entry.Name()
		if name == "." || name == ".." || strings.HasPrefix(name, ".") {
			continue
		}

		full := filepath.Join(path, name)
		info, err := entry.Info()
		if err != nil {
			continue
		}
		if info.IsDir() {
			if depth <= 3 {
				out = append(out, walkFilesList(full, depth+1)...)
			}
			continue
		}
		out = append(out, full)
	}
	sort.Strings(out)
	return out
}

func walkFiles(path string, depth int, b *strings.Builder) {
	for _, file := range walkFilesList(path, depth) {
		b.WriteString(file)
		b.WriteByte('\n')
	}
}

type Todo struct {
	Line int
	Name string
	Type string
	Note string
}

func TodoLine(line string, kw []string, mk string) string {
	mk1, _ := splitMarker(mk)
	if mk1 == "" {
		return ""
	}

	p := strings.TrimSpace(line)
	if !strings.HasPrefix(p, mk1) {
		return ""
	}
	if strings.HasPrefix(p, "...") {
		return p
	}

	p = strings.TrimSpace(strings.TrimPrefix(p, mk1))
	for _, key := range kw {
		key = strings.TrimSpace(key)
		if key != "" && strings.HasPrefix(p, key) && strings.HasPrefix(p[len(key):], ":") {
			return p
		}
	}
	return ""
}

func TodoFind(src string, kw []string, mk string) (string, int) {
	var b strings.Builder
	lineNo := 1
	scanner := bufio.NewScanner(strings.NewReader(src))
	count := 0
	for scanner.Scan() {
		line := scanner.Text()
		mark := TodoLine(line, kw, mk)
		if mark == "" {
			lineNo++
			continue
		}
		count++
		fmt.Fprintf(&b, "%d. %s\n", count, mark)
		lineNo++
	}
	if err := scanner.Err(); err != nil {
		return "", 0
	}
	if count == 0 {
		return "", 0
	}
	return b.String(), count
}

func TodoScan(src string) ([]Todo, error) {
	var out []Todo
	scanner := bufio.NewScanner(strings.NewReader(src))
	lineNo := 1
	for scanner.Scan() {
		parts := strings.SplitN(scanner.Text(), "|", 4)
		if len(parts) != 4 {
			return nil, fmt.Errorf("line %d: expected line|name|TYPE|note", lineNo)
		}
		n, err := strconv.Atoi(strings.TrimSpace(parts[0]))
		if err != nil {
			return nil, fmt.Errorf("line %d: bad line number: %w", lineNo, err)
		}
		out = append(out, Todo{Line: n, Name: strings.TrimSpace(parts[1]), Type: strings.TrimSpace(parts[2]), Note: strings.TrimSpace(parts[3])})
		lineNo++
	}
	return out, scanner.Err()
}

func TodoMark(orig string, todos []Todo, mk string) string {
	if orig == "" || len(todos) == 0 || mk == "" {
		return ""
	}

	_, mk2 := splitMarker(mk)
	copyTodos := append([]Todo(nil), todos...)
	sort.Slice(copyTodos, func(i, j int) bool { return copyTodos[i].Line < copyTodos[j].Line })

	var b strings.Builder
	t := 0
	for lineNo, line := range strings.SplitAfter(orig, "\n") {
		for t < len(copyTodos) && copyTodos[t].Line == lineNo+1 {
			fmt.Fprintf(&b, "%s %s: %s %s\n", mk, copyTodos[t].Type, copyTodos[t].Note, mk2)
			t++
		}
		b.WriteString(line)
	}
	for t < len(copyTodos) {
		fmt.Fprintf(&b, "%s LINE %d - %s: %s %s\n", mk, copyTodos[t].Line, copyTodos[t].Type, copyTodos[t].Note, mk2)
		t++
	}
	return b.String()
}

func splitMarker(mk string) (string, string) {
	mk = strings.TrimSpace(mk)
	if mk == "" {
		return "", ""
	}
	if idx := strings.IndexByte(mk, ' '); idx >= 0 {
		return strings.TrimSpace(mk[:idx]), strings.TrimSpace(mk[idx+1:])
	}
	return strings.TrimSpace(mk), ""
}

const InfoTemplates = `Respond with these lines and I'll help you:

- ` + "`search|path|words`" + `    Locate words in the code
- ` + "`review|path|query`" + `    Inspect source from a file
- ` + "`digest|path|notes`" + `    Write a digest of the file
- ` + "`memory|topic|thought`" + ` Save a memory for future you
- ` + "`forget|topic|thought`" + ` Remove a memory for future you
- ` + "`genius|source|query`" + `   Get advice from top experts
- ` + "`answer|subject|ideas`" + ` Explain your thought to the user

Lines must be this format or I'll discard them...`

const CodeTemplates = `Respond with as many of these lines as you need:

` + "`search|path|terms`" + ` - Search a portion of the codebase
` + "`review|path|notes`" + ` - Show the source code of a file
` + "`digest|path|notes`" + ` - Write a digest of the file
` + "`memory|topic|notes`" + ` - Save a memory for future you
` + "`forget|topic|notes`" + ` - Remove a memory for future you
` + "`answer|path|notes`" + ` - Explain why the file fits

Any other lines will be discarded...`

func InfoList(files []string, note string) string {
	var b strings.Builder
	list := files
	if len(list) == 0 {
		list = splitLines(VToolFind(""))
	}

	for _, path := range list {
		if summary := ReadInfo(path); summary != "" {
			b.WriteString("Remember ")
			b.WriteString(path)
			b.WriteString(":\n")
			b.WriteString(summary)
			b.WriteString("\n\n")
		} else {
			b.WriteString(path)
			b.WriteByte('\n')
		}
	}
	if note != "" {
		b.WriteByte('\n')
		b.WriteString(note)
		b.WriteByte('\n')
	}
	return b.String()
}

func InfoShow(files []string, note string) (string, error) {
	var b strings.Builder
	for _, path := range files {
		src, err := FileLoad(path)
		if err != nil {
			return "", err
		}
		b.WriteString("# REVIEW ")
		b.WriteString(path)
		if note != "" {
			b.WriteByte('\n')
			b.WriteString(note)
			b.WriteString("\n\n")
		}
		b.WriteString(WithLineNumbers(src))
		b.WriteString("\n```\n\n")
	}
	if note != "" {
		b.WriteString("\n\n")
		b.WriteString(note)
		b.WriteByte('\n')
	}
	return b.String(), nil
}

func InfoScan(files []string, note string) (string, error) {
	if len(files) == 0 {
		return "", fmt.Errorf("no files")
	}
	cfg := CoreConfig()
	list := InfoList(files, note)
	code, err := InfoShow(files, note)
	if err != nil {
		return "", err
	}
	nut := &peanuts.Peanuts{
		Persona:  FileInst(files[0], cfg.Keywords, cfg.Instruct),
		Evidence: list,
		Analysis: "Okay. Show me the contents of these files...",
		Nudging:  code,
		Updates:  "Okay, so I need to review files first to figure out how to summarize.",
		Turnout:  InfoTemplates,
		Safety:   InfoSafety,
	}
	return peanuts.Nutjob(InfoNutmeg(), nut)
}

func InfoFind(query string) (string, error) {
	list := InfoList(nil, query)
	quip := fmt.Sprintf("Okay I see the files. But you want me to focus on:\n\n%s", query)
	need := fmt.Sprintf("Yes. What files do you need to answer this:\n\n%s", query)
	nut := &peanuts.Peanuts{
		Persona:  FileInst("", []string{"TODO", "FIXME"}, "You are an architect."),
		Evidence: list,
		Analysis: quip,
		Nudging:  need,
		Updates:  "Okay, so I need to search terms and review the code to figure this out.",
		Turnout:  "Respond with `review|path|notes` or `search|path|terms`",
		Safety:   InfoSafety,
	}
	return peanuts.Nutjob(InfoNutmeg(), nut)
}

func InfoSafety(nut *peanuts.Peanuts, res *string) bool {
	actions := 0
	replies := 0
	var ans strings.Builder
	var buf strings.Builder

	for _, line := range strings.Split(*res, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "```") {
			continue
		}
		verb, term, note, ok := parseInfoLine(line)
		if !ok {
			continue
		}
		switch verb {
		case "review":
			actions++
			if data, err := FileLoad(term); err == nil {
				buf.WriteString("\n\n# REVIEW ")
				buf.WriteString(term)
				buf.WriteString("\n")
				buf.WriteString(data)
			}
		case "search":
			actions++
			buf.WriteString("\n\n# SEARCH ")
			buf.WriteString(term)
			buf.WriteString("\n")
			buf.WriteString(VToolGrep(note, term))
		case "digest", "memory", "forget":
			actions++
			buf.WriteString("\n\n# ")
			if verb == "forget" {
				buf.WriteString("FORGET ")
			} else {
				buf.WriteString(verb)
			}
			buf.WriteString(" ")
			buf.WriteString(term)
			buf.WriteString("\n")
			buf.WriteString(note)
		case "answer":
			replies++
			fmt.Fprintf(&ans, "%s|%s\n", term, note)
		}
	}

	if actions == 0 && replies > 0 {
		nut.Nudging = ans.String()
		return true
	}
	nut.Evidence = InfoList(nil, "")
	nut.Analysis = *res
	nut.Nudging = buf.String()
	nut.Updates = "Okay that helps. But do I have enough info to answer? Let me think about it a bit more."
	return false
}

func parseInfoLine(line string) (string, string, string, bool) {
	parts := strings.SplitN(line, "|", 3)
	if len(parts) != 3 {
		return "", "", "", false
	}
	return strings.TrimSpace(parts[0]), strings.TrimSpace(parts[1]), strings.TrimSpace(parts[2]), true
}

const MarkMax = 128

type Mark struct {
	Line int
	Path string
	Type string
	Note string
}

func MarkList(files []string, kw []string) string {
	var b strings.Builder
	if len(files) == 0 {
		if len(kw) == 0 {
			return b.String()
		}
		pattern := strings.Join(kw, "|")
		b.WriteString(VToolGrep(pattern, ""))
		return b.String()
	}

	for _, path := range files {
		data, err := FileLoad(path)
		if err != nil {
			continue
		}
		for _, key := range kw {
			if strings.Contains(data, key) {
				b.WriteString(path)
				b.WriteByte('\n')
				break
			}
		}
	}
	return b.String()
}

func MarkEach(files []string, goal string) (string, error) {
	if len(files) == 0 {
		code, err := CodeFind(goal)
		if err != nil {
			return "", err
		}
		files = AnswerFiles(code)
		if len(files) == 0 {
			return "", fmt.Errorf("no files found for: %s", goal)
		}
	}

	target, err := MarkTarget(files, goal)
	if err != nil {
		return "", err
	}
	cfg := MarkConfig()
	nut := &peanuts.Peanuts{
		Persona:  FileInst(files[0], cfg.Keywords, cfg.Instruct),
		Evidence: target,
		Analysis: "Okay. I see the source code. But what are your goals?",
		Nudging:  goal,
		Updates:  goal,
		Turnout:  FileInst(files[0], cfg.Keywords, "Return ONLY strict insert points like: `filename|line|TYPE|comment` (of these types: {{keywords}})"),
		Safety:   MarkSafety,
	}
	return peanuts.Nutjob(MarkNutmeg(), nut)
}

func MarkTarget(files []string, goal string) (string, error) {
	var b strings.Builder
	for _, path := range files {
		src, err := FileLoad(path)
		if err != nil {
			return "", err
		}
		b.WriteString(WithLineNumbers(src))
		b.WriteByte('\n')
	}
	b.WriteString(goal)
	return b.String(), nil
}

func MarkSafety(nut *peanuts.Peanuts, res *string) bool {
	marks, ok := ParseMarks(*res)
	if !ok || len(marks) == 0 {
		return false
	}

	byPath := map[string][]Mark{}
	for _, mark := range marks {
		byPath[mark.Path] = append(byPath[mark.Path], mark)
	}

	for path := range byPath {
		src, err := FileLoad(path)
		if err != nil {
			return false
		}
		updated := MarkSource(src, byPath[path], FileMark(path))
		if updated == "" {
			return false
		}
		if _, err := FileSave(path, updated); err != nil {
			return false
		}
	}
	nut.Nudging = fmt.Sprintf("Added %d new comments.", len(marks))
	return true
}

func ParseMarks(src string) ([]Mark, bool) {
	var marks []Mark
	for _, line := range strings.Split(src, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "```") {
			continue
		}
		mark, ok := ParseMark(line)
		if !ok {
			return nil, false
		}
		marks = append(marks, mark)
	}
	sortMarks(marks)
	return marks, true
}

func ParseMark(line string) (Mark, bool) {
	parts := strings.SplitN(line, "|", 4)
	if len(parts) != 4 {
		return Mark{}, false
	}
	lineNo, err := strconv.Atoi(strings.TrimSpace(parts[0]))
	if err != nil || lineNo <= 0 {
		return Mark{}, false
	}
	return Mark{Line: lineNo, Path: strings.TrimSpace(parts[1]), Type: strings.TrimSpace(parts[2]), Note: strings.TrimSpace(parts[3])}, true
}

func MarkSource(src string, marks []Mark, mk string) string {
	if src == "" || len(marks) == 0 || mk == "" {
		return ""
	}
	copyMarks := append([]Mark(nil), marks...)
	sortMarks(copyMarks)

	var b strings.Builder
	m := 0
	for lineNo, line := range strings.SplitAfter(src, "\n") {
		for m < len(copyMarks) && copyMarks[m].Path == "" {
			m++
		}
		for m < len(copyMarks) && copyMarks[m].Line == lineNo+1 {
			mk1, mk2 := splitMarker(mk)
			if mk1 == "" {
				mk1 = mk
			}
			fmt.Fprintf(&b, "%s %s: %s %s\n", mk1, copyMarks[m].Type, copyMarks[m].Note, mk2)
			m++
		}
		b.WriteString(line)
	}
	for m < len(copyMarks) {
		mk1, mk2 := splitMarker(mk)
		if mk1 == "" {
			mk1 = mk
		}
		fmt.Fprintf(&b, "%s LINE %d - %s: %s %s\n", mk1, copyMarks[m].Line, copyMarks[m].Type, copyMarks[m].Note, mk2)
		m++
	}
	return b.String()
}

func ReadInfo(path string) string {
	data, err := os.ReadFile(".v-info")
	if err != nil {
		return ""
	}
	key := "# " + path + " |"
	for _, line := range strings.Split(string(data), "\n") {
		if strings.HasPrefix(line, key) {
			return strings.TrimSpace(strings.TrimPrefix(line, key))
		}
	}
	return ""
}

func WriteInfo(path, note string) error {
	data := ""
	if existing, err := os.ReadFile(".v-info"); err == nil {
		data = string(existing)
	}
	var b strings.Builder
	scanner := bufio.NewScanner(strings.NewReader(data))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "# "+path+" |") {
			continue
		}
		b.WriteString(line)
		b.WriteByte('\n')
	}
	b.WriteString("# ")
	b.WriteString(path)
	b.WriteString(" | ")
	b.WriteString(note)
	b.WriteByte('\n')
	return os.WriteFile(".v-info", []byte(b.String()), 0o666)
}

func AnswerFiles(src string) []string {
	var out []string
	for _, line := range strings.Split(src, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "```") {
			continue
		}
		parts := strings.SplitN(line, "|", 3)
		if len(parts) == 3 && strings.TrimSpace(parts[0]) == "answer" {
			out = append(out, strings.TrimSpace(parts[1]))
		}
	}
	return out
}

func sortMarks(marks []Mark) {
	sort.Slice(marks, func(i, j int) bool {
		if marks[i].Path != marks[j].Path {
			return marks[i].Path < marks[j].Path
		}
		return marks[i].Line < marks[j].Line
	})
}

func filepathJoin(a, b string) string {
	return filepath.Join(a, b)
}

func CodeScan(src string, kw []string, mk string) (string, int) {
	return TodoFind(src, kw, mk)
}

func CodeFind(quest string) (string, error) {
	cfg := CoreConfig()
	list := InfoList(nil, quest)
	quip := fmt.Sprintf("Okay I see the files. But you want me to focus on:\n\n%s", quest)
	need := fmt.Sprintf("Search the codebase to answer:\n\n%s\n\nThen list the files as `answer|filename|comment` after you do you research.\n\n", quest)
	nut := &peanuts.Peanuts{
		Persona:  FileInst("", cfg.Keywords, cfg.Instruct),
		Evidence: list,
		Analysis: quip,
		Nudging:  need,
		Updates:  "Okay, so I need to search code first to figure this out. Let me think about what make the most sense.",
		Turnout:  CodeTemplates,
		Safety:   CodeSafety,
	}
	return peanuts.Nutjob(InfoNutmeg(), nut)
}

func CodeMove(quest string) (string, error) {
	return CodeFind(quest)
}

func CodeFile(filename, goal string) error {
	mk := FileMark(filename)
	code, err := FileLoad(filename)
	if err != nil {
		return err
	}
	inst := FileInst(filename, CoreConfig().Keywords, CoreConfig().Instruct)
	list, count := CodeScan(code, CoreConfig().Keywords, mk)
	if count == 0 && goal != "" {
		if _, err := MarkEach([]string{filename}, goal); err != nil {
			return err
		}
		code, err = FileLoad(filename)
		if err != nil {
			return err
		}
		list, count = CodeScan(code, CoreConfig().Keywords, mk)
	}
	if count == 0 {
		return fmt.Errorf("no comments found in `%s`", filename)
	}

	todo := "Okay...\n\nI found these comments:\n\n" + list + "\n\nAre we ready to address the comments?"
	nut := &peanuts.Peanuts{
		Persona:  inst,
		Evidence: goal,
		Analysis: mk,
		Nudging:  code,
		Updates:  todo,
		Turnout:  inst,
		Safety:   CodeSafety,
	}
	src, err := peanuts.Nutjob(CodeNutmeg(), nut)
	if err != nil {
		return err
	}
	_, err = FileSave(filename, src)
	return err
}

func CodeSafety(nut *peanuts.Peanuts, res *string) bool {
	actions := 0
	replies := 0
	var ans strings.Builder

	for _, line := range strings.Split(*res, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "```") {
			continue
		}
		verb, term, note, ok := parseInfoLine(line)
		if !ok {
			continue
		}
		switch verb {
		case "search", "review", "digest", "memory", "forget":
			actions++
		case "answer":
			replies++
			fmt.Fprintf(&ans, "%s|%s\n", term, note)
		}
	}

	if actions == 0 && replies > 0 {
		nut.Nudging = "# REMEMBER THESE FILES\nInclude them in your output...\n\n" + ans.String()
		return true
	}
	if actions > 0 {
		nut.Analysis = *res
		nut.Nudging = "# REMEMBER THESE FILES\nInclude them in your output...\n\n" + ans.String()
		return false
	}
	return false
}
