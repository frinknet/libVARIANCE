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

// Output writers, matching C's vcore_out / vcore_err globals.
var VOut io.Writer = os.Stdout
var VErr io.Writer = os.Stderr

// vOut prints to VOut with the same format as C's v_out().
func vOut(format string, args ...interface{}) {
	if VOut == nil {
		return
	}
	msg := fmt.Sprintf(format, args...)
	fmt.Fprintf(VOut, "%s\n\n", msg)
}

type Config struct {
	Keywords []string
	Endpoint string
	APIKey   string
	Model    string
	Instruct string
	Timeout  int
	Tokens   int
	Tries    int
	Pause    int
	Temp     float64
}

var (
	// Configs holds the mode-keyed configuration, baked in from x/configure.x.
	// Override in your own init() to use custom configs:
	//
	//	func init() { libvariance.Configs = myConfigs }
	Configs  map[string]Config

	// FileTypes holds the file-type mappings, baked in from x/filetypes.x.
	// Override in your own init() to use custom mappings:
	//
	//	func init() { libvariance.FileTypes = myFileTypes }
	FileTypes []FileType
)

func init() {
	var err error
	FileTypes, Configs, err = loadXConfig()
	if err != nil {
		panic(err)
	}
}

func configFor(mode string) Config {
	if Configs == nil {
		return Config{}
	}
	return Configs[mode]
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
	if c.Endpoint == "" {
		c.Endpoint = core.Endpoint
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
	ctx := peanuts.NewNutmeg(cfg.Model, cfg.Endpoint, cfg.APIKey)
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

func cleanXName(value string) string {
	value = strings.TrimSpace(value)
	value = strings.TrimPrefix(value, "X(")
	return strings.TrimSuffix(value, ")")
}

func atoi(value string) int {
	n, _ := strconv.Atoi(value)
	return n
}

func atof(value string) float64 {
	f, _ := strconv.ParseFloat(value, 64)
	return f
}

func GetFileTypes() []FileType {
	out := make([]FileType, len(FileTypes))
	copy(out, FileTypes)
	return out
}

func VFileType(filename string) FileType {
	for _, ft := range FileTypes {
		if ft.Match(filename) {
			return ft
		}
	}
	return FileType{Lang: "text"}
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
		// Strip " ⟵ " summary suffix (matches C's v_filelist behavior)
		if idx := strings.Index(field, " ⟵ "); idx >= 0 {
			field = field[:idx]
		}
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
	if goal != "" {
		vOut(VTASK_NOTICE_INFO, goal)
	}
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
		vOut(VTASK_NOTICE_MARK, goal)
		return MarkEach(files, goal)
	}
	if len(files) == 0 {
		return MarkList(nil, MarkConfig().Keywords), nil
	}
	return MarkList(files, MarkConfig().Keywords), nil
}

func VTaskcode(files []string, goal string) (string, error) {
	if len(files) == 0 {
		return "", nil
	}

	if goal != "" {
		vOut(VTASK_NOTICE_CODE, goal)
	}

	// Call markeach/marklist as a side effect (result discarded, matching C behavior).
	if len(files) > 1 {
		if goal != "" {
			vOut(VTASK_NOTICE_MARK, goal)
			_, _ = MarkEach(files, goal)
		} else {
			_ = MarkList(files, MarkConfig().Keywords)
		}
	}

	for _, file := range files {
		_ = CodeFile(file, goal)
	}

	var result strings.Builder
	if summaries := toolSummary(files); summaries != "" {
		result.WriteString("# " + VINFO_LABEL_EDITED + "\n")
		result.WriteString(summaries)
	}
	if goal != "" {
		result.WriteByte('\n')
		result.WriteString(goal)
		result.WriteByte('\n')
	}
	return result.String(), nil
}

func VTaskplan(files []string, goal string) string {
	if len(files) > 1 {
		PlanTask(files, goal)
	} else if goal != "" {
		PlanMore(goal)
	}
	content, _ := FileLoad(VFILENAME_PLAN)
	return content
}

func VTasktodo(files []string, goal string) string {
	return PlanTodo(files, goal)
}

func vMarkscan(src string, kw []string, mk string) (string, int) {
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

func VTaskauto(files []string, goal string) string {
	if goal != "" {
		PlanMore(goal)
	}
	plan := PlanLoad()
	PlanExec(plan)
	return PlanTodo(files, goal)
}

func VTaskdone(files []string, goal string) string {
	if goal == "" {
		goal = VPLAN_REVIEW_DELETION
	}
	rpt := PlanTodo(files, goal)
	PlanWipe()
	return rpt
}

func VTasknext(files []string, goal string) string {
	if goal != "" {
		PlanMore(goal)
	}
	plan := PlanLoad()
	PlanNext(plan)
	return PlanTodo(files, goal)
}

func VTasktest(files []string, goal string) string {
	_ = files
	if goal != "" {
		vOut(VTASK_NOTICE_TEST, goal)
	}
	return VTASK_NOTICE_NONE
}

func VTaskedit(files []string, goal string) string {
	_ = files
	if goal != "" {
		vOut(VTASK_NOTICE_EDIT, goal)
	}
	return VTASK_NOTICE_NONE
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

func withLineNumbers(src string) string {
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

func vToolFind(path string) string {
	var b strings.Builder
	if path == "" {
		path = "."
	}
	walkFiles(path, 0, &b)
	return b.String()
}

func vToolGrep(term, path string) string {
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

type Step struct {
	Done  bool
	Task  string
	Files []string
}

type Plan struct {
	Title string
	Goals string
	Notes string
	Steps []Step
}

func TodoLine(line string, kw []string, mk string) string {
	mk1, _ := splitMarker(mk)
	if mk1 == "" {
		return ""
	}

	p := strings.TrimSpace(line)
	if !strings.HasPrefix(p, mk1) {
		if strings.HasPrefix(p, "...") {
			return p
		}
		return ""
	}

	p = strings.TrimSpace(strings.TrimPrefix(p, mk1))
	for _, key := range kw {
		key = strings.TrimSpace(key)
		if key != "" && strings.HasPrefix(p, key) && strings.HasPrefix(p[len(key):], ":") {
			return p
		}
	}
	if strings.HasPrefix(p, "...") {
		return p
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
		if count == 0 {
			b.WriteString(VTODO_SEARCH_PREAMBLE + "\n\n")
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
	b.WriteString("\nWant me to fix them?")
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
		fmt.Fprintf(&b, "%s:%d - %s: %s %s\n", mk, copyTodos[t].Line, copyTodos[t].Type, copyTodos[t].Note, mk2)
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

// VINFO_MICROFORMAT is auto-generated from x/configure.x in x_config.go.

// VCODE_MICROFORMAT and VPLAN_REVIEW_DIRECTIVE are auto-generated
// from x/configure.x in x_config.go.

func InfoList(files []string, note string) string {
	var b strings.Builder

	if summaries := toolSummary(files); summaries != "" {
		b.WriteString("# ")
		b.WriteString(VINFO_LABEL_CONTEXT)
		b.WriteString("\n")
		b.WriteString(summaries)
	}

	memories := memoryList()
	if memories != "" {
		b.WriteString("\n")
		b.WriteString(memories)
	}
	if note != "" {
		b.WriteString("\n")
		b.WriteString(note)
		b.WriteByte('\n')
	}
	return b.String()
}

func toolSummary(files []string) string {
	data, err := os.ReadFile(".v-info")
	if err != nil || len(files) == 0 {
		return ""
	}

	var b strings.Builder
	for _, path := range files {
		key := path + "|"
		found := false
		for _, line := range strings.Split(string(data), "\n") {
			line = strings.TrimLeft(line, " ")
			if strings.Contains(line, key) {
				b.WriteString(path)
				b.WriteString(" ⟵ ")
				b.WriteString(strings.TrimSpace(strings.TrimPrefix(line, key)))
				b.WriteByte('\n')
				found = true
				break
			}
			if strings.HasPrefix(line, "# "+key) {
				b.WriteString(path)
				b.WriteString(" ⟵ ")
				b.WriteString(strings.TrimSpace(strings.TrimPrefix(line, "# "+key)))
				b.WriteByte('\n')
				found = true
				break
			}
		}
		if !found {
			b.WriteString(path)
			b.WriteByte('\n')
		}
	}
	return b.String()
}

func memoryList() string {
	info, err := os.ReadFile(".v-info")
	if err != nil {
		return ""
	}

	var b strings.Builder
	for _, line := range strings.Split(string(info), "\n") {
		if strings.HasPrefix(line, "# ") {
			pipe := strings.Index(line, "|")
			if pipe > 2 {
				topic := line[2:pipe]
				note := line[pipe+1:]
				b.WriteString("# " + VINFO_LABEL_MEMORIES + " ")
				b.WriteString(topic)
				b.WriteByte('\n')
				b.WriteString(note)
				b.WriteString("\n\n")
			}
		}
	}
	return b.String()
}

func InfoShow(files []string, note string) (string, error) {
	var b strings.Builder
	for _, path := range files {
		vOut(VFILE_NOTICE_VIEW, path, note)
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
		b.WriteString("```")
		b.WriteString(FileLang(path))
		b.WriteByte('\n')
		b.WriteString(src)
		b.WriteString("```\n\n")
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
	cfg := InfoConfig()
	list := InfoList(files, note)
	code, err := InfoShow(files, note)
	if err != nil {
		return "", err
	}
	nut := &peanuts.Peanuts{
		Persona:  FileInst(files[0], cfg.Keywords, cfg.Instruct),
		Evidence: list,
		Analysis: VINFO_REVIEW_REQUEST,
		Nudging:  code,
		Updates:  VINFO_REVIEW_ACCEPTED,
		Turnout:  VINFO_MICROFORMAT,
		Safety:   infoSafety,
	}
	return peanuts.Nutjob(InfoNutmeg(), nut)
}

func InfoFind(query string) (string, error) {
	cfg := InfoConfig()
	list := InfoList(nil, query)
	quip := fmt.Sprintf(VINFO_SEARCH_PREAMBLE+"\n\n%s", query)
	need := fmt.Sprintf(VINFO_SEARCH_DIRECTIVE+"\n\n%s", query)
	nut := &peanuts.Peanuts{
		Persona:  FileInst("", cfg.Keywords, cfg.Instruct),
		Evidence: list,
		Analysis: quip,
		Nudging:  need,
		Updates:  VINFO_SEARCH_ACCEPTED,
		Turnout:  VINFO_MICROFORMAT,
		Safety:   infoSafety,
	}
	return peanuts.Nutjob(InfoNutmeg(), nut)
}

func infoSafety(nut *peanuts.Peanuts, res *string) bool {
	actions, replies, evidence, answers := infoEvidence(*res)
	if actions == 0 && replies > 0 {
		*res = answers.String()
		return true
	}
	nut.Evidence = refreshInfo(nut.Evidence)
	nut.Analysis = *res
	nut.Nudging = evidence.String()
	nut.Updates = VINFO_REVIEW_CONTINUE
	return false
}

func infoEvidence(src string) (int, int, strings.Builder, strings.Builder) {
	actions := 0
	replies := 0
	var answers strings.Builder
	var evidence strings.Builder

	for _, line := range strings.Split(src, "\n") {
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
			vOut(VFILE_NOTICE_VIEW, term, note)
			if data, err := FileLoad(term); err == nil {
				evidence.WriteString("\n\n# REVIEW ")
				evidence.WriteString(term)
				evidence.WriteByte('\n')
				evidence.WriteString("```")
				evidence.WriteString(FileLang(term))
				evidence.WriteByte('\n')
				evidence.WriteString(data)
				evidence.WriteString("\n```\n\n")
			}
		case "search":
			actions++
			evidence.WriteString("\n\n# SEARCH ")
			evidence.WriteString(term)
			evidence.WriteByte('\n')
			evidence.WriteString(vToolGrep(note, term))
		case "digest", "memory", "forget", "genius":
			actions++
			vOut(VFILE_NOTICE_NOTE, term, note)
			evidence.WriteString("\n\n# ")
			if verb == "forget" {
				evidence.WriteString("FORGET ")
			} else {
				evidence.WriteString(verb)
			}
			evidence.WriteString(" ")
			evidence.WriteString(term)
			evidence.WriteByte('\n')
			evidence.WriteString(note)
			switch verb {
			case "digest":
				_ = writeInfo(term, note)
			case "memory":
				_ = writeInfo("#"+term, note)
			case "forget":
				_ = writeInfo("#"+term, "")
			}
		case "answer":
			replies++
			fmt.Fprintf(&answers, "%s|%s\n", term, note)
		}
	}
	return actions, replies, evidence, answers
}

func refreshInfo(evidence string) string {
	files := infoFiles(evidence)
	if len(files) == 0 {
		return InfoList(nil, "")
	}
	return InfoList(files, "")
}

func infoFiles(src string) []string {
	seen := map[string]bool{}
	var out []string
	for _, line := range strings.Split(src, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		for _, part := range splitInfoLine(line) {
			part = strings.TrimSpace(part)
			if part == "" || seen[part] {
				continue
			}
			seen[part] = true
			out = append(out, part)
		}
	}
	return out
}

func splitInfoLine(line string) []string {
	var out []string
	start := 0
	for start < len(line) {
		end := len(line)
		for i := start; i < len(line); i++ {
			if line[i] == '|' || line[i] == '\n' {
				end = i
				break
			}
			if i+3 < len(line) && line[i:i+4] == " ⟵ " {
				end = i
				break
			}
		}
		part := strings.TrimSpace(line[start:end])
		if part != "" {
			out = append(out, part)
		}
		if end >= len(line) {
			break
		}
		if line[end] == '|' || line[end] == '\n' {
			start = end + 1
			continue
		}
		start = end + 4
	}
	return out
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
		pattern := "(" + strings.Join(kw, "|") + ")"
		b.WriteString(vToolGrep(pattern, ""))
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
		files = answerFiles(code)
		if len(files) == 0 {
			return "", fmt.Errorf(VFILE_NOTICE_GOAL, goal)
		}
	}

	target, err := markTarget(files, goal)
	if err != nil {
		return "", err
	}
	cfg := MarkConfig()
	nut := &peanuts.Peanuts{
		Persona:  FileInst(files[0], cfg.Keywords, cfg.Instruct),
		Evidence: target,
		Analysis: VMARK_SEARCH_REQUEST,
		Nudging:  goal,
		Updates:  goal,
		Turnout:  FileInst(files[0], cfg.Keywords, VMARK_MICROFORMAT),
		Safety:   markSafety,
	}
	return peanuts.Nutjob(MarkNutmeg(), nut)
}

func markTarget(files []string, goal string) (string, error) {
	var b strings.Builder
	for _, path := range files {
		vOut(VFILE_NOTICE_READ, path)
		src, err := FileLoad(path)
		if err != nil {
			return "", err
		}
		b.WriteString(path)
		b.WriteString("\n```")
		b.WriteString(FileLang(path))
		b.WriteByte('\n')
		b.WriteString(withLineNumbers(src))
		b.WriteString("```\n\n")
	}
	b.WriteString(goal)
	return b.String(), nil
}

func markSafety(nut *peanuts.Peanuts, res *string) bool {
	marks, ok := parseMarks(*res)
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
		updated := markSource(src, byPath[path], FileMark(path))
		if updated == "" {
			return false
		}
		if _, err := FileSave(path, updated); err != nil {
			return false
		}
		vOut(VFILE_NOTICE_EDIT, path)
	}
	vOut(VFILE_NOTICE_MARK, len(marks))
	nut.Nudging = fmt.Sprintf("Added %d new comments.", len(marks))
	return true
}

func parseMarks(src string) ([]Mark, bool) {
	var marks []Mark
	for _, line := range strings.Split(src, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "```") {
			continue
		}
		mark, ok := parseMark(line)
		if !ok {
			return nil, false
		}
		marks = append(marks, mark)
	}
	sortMarks(marks)
	return marks, true
}

func parseMark(line string) (Mark, bool) {
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

func markSource(src string, marks []Mark, mk string) string {
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
		fmt.Fprintf(&b, "%s:%d - %s: %s %s\n", mk, copyMarks[m].Line, copyMarks[m].Type, copyMarks[m].Note, mk2)
		m++
	}
	return b.String()
}

func readInfo(path string) string {
	data, err := os.ReadFile(".v-info")
	if err != nil {
		return ""
	}
	key := path + "|"
	legacy := "# " + path + " |"
	for _, line := range strings.Split(string(data), "\n") {
		line = strings.TrimLeft(line, " ")
		if strings.HasPrefix(line, key) {
			return strings.TrimSpace(strings.TrimPrefix(line, key))
		}
		if strings.HasPrefix(line, legacy) {
			return strings.TrimSpace(strings.TrimPrefix(line, legacy))
		}
	}
	return ""
}

func writeInfo(path, note string) error {
	data := ""
	if existing, err := os.ReadFile(".v-info"); err == nil {
		data = string(existing)
	}
	var b strings.Builder
	scanner := bufio.NewScanner(strings.NewReader(data))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, path+"|") || strings.HasPrefix(line, "# "+path+" |") {
			continue
		}
		b.WriteString(line)
		b.WriteByte('\n')
	}
	if note != "" {
		b.WriteString(path)
		b.WriteString("|")
		b.WriteString(note)
		b.WriteByte('\n')
	}
	return os.WriteFile(".v-info", []byte(b.String()), 0o666)
}

func answerFiles(src string) []string {
	var out []string
	for _, line := range strings.Split(src, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "```") {
			continue
		}
		parts := strings.SplitN(line, "|", 3)
		if len(parts) >= 2 {
			fname := strings.TrimSpace(parts[0])
			if fname == "answer" && len(parts) == 3 {
				fname = strings.TrimSpace(parts[1])
			}
			if fname != "" {
				out = append(out, fname)
			}
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

func CodeScan(src string, kw []string, mk string) (string, int) {
	return TodoFind(src, kw, mk)
}

func CodeFind(quest string) (string, error) {
	cfg := InfoConfig()
	list := InfoList(nil, quest)
	quip := fmt.Sprintf(VCODE_SEARCH_PREAMBLE+"\n\n%s", quest)
	need := fmt.Sprintf(VCODE_SEARCH_DIRECTIVE+"\n\n%s\n\n"+VCODE_SEARCH_FORMAT+"\n\n", quest)
	nut := &peanuts.Peanuts{
		Persona:  FileInst("", cfg.Keywords, cfg.Instruct),
		Evidence: list,
		Analysis: quip,
		Nudging:  need,
		Updates:  VCODE_SEARCH_ACCEPTED,
		Turnout:  VCODE_MICROFORMAT,
		Safety:   codeSearch,
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
	cfg := InfoConfig()
	inst := FileInst(filename, cfg.Keywords, cfg.Instruct)
	list, count := CodeScan(code, cfg.Keywords, mk)
	if count == 0 && goal != "" {
		if _, err := MarkEach([]string{filename}, goal); err != nil {
			return err
		}
		code, err = FileLoad(filename)
		if err != nil {
			return err
		}
		list, count = CodeScan(code, cfg.Keywords, mk)
	}
	if count == 0 {
		return fmt.Errorf(VFILE_NOTICE_DONE, filename)
	}

	todo := VCODE_SOURCE_ACCEPTED + "\n\n" + list + "\n\n" + VCODE_SOURCE_CONTINUE
	nut := &peanuts.Peanuts{
		Persona:  inst,
		Evidence: goal,
		Analysis: mk,
		Nudging:  code,
		Updates:  todo,
		Turnout:  inst,
		Safety:   codeWrites,
	}
	src, err := peanuts.Nutjob(CodeNutmeg(), nut)
	if err != nil {
		return err
	}
	_, err = FileSave(filename, src)
	if err != nil {
		return err
	}
	vOut(VFILE_NOTICE_EDIT, filename)
	return nil
}

func codeSearch(nut *peanuts.Peanuts, res *string) bool {
	actions, replies, _, answers := infoEvidence(*res)
	if actions == 0 && replies > 0 {
		*res = answers.String()
		return true
	}
	nut.Analysis = *res
	nut.Nudging = "# " + VCODE_SOURCE_PREAMBLE + "\n" + VCODE_SOURCE_DIRECTIVE + "\n\n" + answers.String()
	return false
}

// codeWrites is a safety callback for CodeFile that checks the AI response
// for keyword markers (e.g. "// TODO:", "# FIXME:"). If any are found, the
// response is accepted as the file content. Equivalent to C's _vcode_writes.
func codeWrites(nut *peanuts.Peanuts, res *string) bool {
	_, count := vMarkscan(*res, CodeConfig().Keywords, nut.Analysis)
	return count > 0
}

// PlanSearch is a safety callback for PlanMore that handles info actions
// (search, review, digest, memory, forget) during the research phase, then
// switches to planUpdate when the AI answers. Equivalent to C's _vplan_search.
func planSearch(nut *peanuts.Peanuts, res *string) bool {
	actions, replies, evidence, answers := infoEvidence(*res)
	if actions == 0 && replies > 0 {
		nut.Updates = VPLAN_SEARCH_CONTINUE
		nut.Turnout = VPLAN_MICROFORMAT
		nut.Safety = planUpdate
	}

	evidence.WriteString("# " + VPLAN_SOURCE_PREAMBLE + "\n" + VPLAN_SOURCE_DIRECTIVE + "\n\n")
	evidence.WriteString(answers.String())

	nut.Analysis = *res
	nut.Nudging = evidence.String()
	return false
}

// PlanUpdate is the second-phase safety for PlanMore. It validates and saves
// the plan. Equivalent to C's _vplan_update.
func planUpdate(nut *peanuts.Peanuts, res *string) bool {
	plan := parsePlan(*res)
	if len(plan.Steps) > 0 {
		PlanSave(plan)
		return true
	}
	return false
}

func PlanLoad() *Plan {
	data, err := os.ReadFile(VFILENAME_PLAN)
	if err != nil {
		return &Plan{}
	}
	return parsePlan(string(data))
}

func parsePlan(src string) *Plan {
	plan := &Plan{}
	lines := strings.Split(src, "\n")
	if len(lines) == 0 {
		return plan
	}

	plan.Title = strings.TrimSpace(lines[0])
	if len(lines) > 1 && strings.HasPrefix(lines[1], "---") {
		idx := 2
		for ; idx < len(lines); idx++ {
			line := strings.TrimSpace(lines[idx])
			if line == "" {
				continue
			}
			if strings.HasPrefix(line, "- [") {
				break
			}
			plan.Goals += line + "\n"
		}
		plan.Goals = strings.TrimSpace(plan.Goals)

		for ; idx < len(lines); idx++ {
			line := lines[idx]
			if !strings.HasPrefix(line, "- [") {
				break
			}
			step := parseStep(line)
			if step == nil {
				continue
			}
			for idx+1 < len(lines) && strings.HasPrefix(lines[idx+1], "  - `") {
				idx++
				file := strings.TrimSpace(strings.TrimPrefix(lines[idx], "  - `"))
				file = strings.Trim(file, "`")
				step.Files = append(step.Files, file)
			}
			plan.Steps = append(plan.Steps, *step)
		}

		idx++
		for ; idx < len(lines); idx++ {
			line := strings.TrimSpace(lines[idx])
			if line == "" {
				continue
			}
			plan.Notes += line + "\n"
		}
		plan.Notes = strings.TrimSpace(plan.Notes)
	}
	return plan
}

func parseStep(line string) *Step {
	if !strings.HasPrefix(line, "- [") {
		return nil
	}
	closed := strings.Index(line, "] ")
	if closed == -1 {
		return nil
	}
	done := line[3] == 'x'
	task := strings.TrimSpace(line[closed+2:])

	return &Step{
		Done: done,
		Task: task,
	}
}

func PlanText(plan *Plan) string {
	if plan == nil {
		return ""
	}

	var b strings.Builder
	b.WriteString(plan.Title)
	b.WriteString("\n---\n")
	if plan.Goals != "" {
		b.WriteString(plan.Goals)
		b.WriteString("\n")
	}

	for _, step := range plan.Steps {
		if step.Done {
			b.WriteString("\n- [x] ")
		} else {
			b.WriteString("\n- [ ] ")
		}
		b.WriteString(step.Task)
		b.WriteString("\n")
		for _, file := range step.Files {
			b.WriteString("  - `")
			b.WriteString(file)
			b.WriteString("`\n")
		}
	}

	if plan.Notes != "" {
		b.WriteString("\n")
		b.WriteString(plan.Notes)
	}

	return b.String()
}

func PlanSave(plan *Plan) error {
	text := PlanText(plan)
	return os.WriteFile(VFILENAME_PLAN, []byte(text), 0o666)
}

func PlanNext(plan *Plan) bool {
	if plan == nil {
		return false
	}
	for i := range plan.Steps {
		step := &plan.Steps[i]
		if step.Done || len(step.Files) == 0 {
			continue
		}
		_, _ = MarkEach(step.Files, step.Task)
		for _, file := range step.Files {
			_ = CodeFile(file, step.Task)
		}
		step.Done = true
		_ = PlanSave(plan)
		return true
	}
	return false
}

func PlanExec(plan *Plan) int {
	if plan == nil {
		return 0
	}
	count := 0
	for PlanNext(plan) {
		count++
	}
	return count
}

func PlanMore(goal string) string {
	plan, _ := FileLoad(VFILENAME_PLAN)
	list := InfoList(nil, plan)
	cfg := InfoConfig()
	quip := fmt.Sprintf(VPLAN_SEARCH_PREAMBLE+"\n\n%s", goal)
	need := fmt.Sprintf(VPLAN_SEARCH_DIRECTIVE+"\n\n%s\n\n"+VPLAN_SEARCH_FORMAT+"\n\n", goal)
	nut := &peanuts.Peanuts{
		Persona:  FileInst("", cfg.Keywords, cfg.Instruct),
		Evidence: list,
		Analysis: quip,
		Nudging:  need,
		Updates:  VPLAN_SEARCH_ACCEPTED,
		Turnout:  VCODE_MICROFORMAT,
		Safety:   planSearch,
	}
	result, err := peanuts.Nutjob(InfoNutmeg(), nut)
	if err != nil {
		return ""
	}
	return result
}

func PlanTask(files []string, goal string) string {
	info, _ := InfoScan(files, "")
	need := fmt.Sprintf("%s\n\n# %s\n\n%s", goal, VPLAN_SEARCH_DIRECTIVE, info)
	return PlanMore(need)
}

func PlanTodo(files []string, goal string) string {
	plan, _ := FileLoad(VFILENAME_PLAN)
	need := goal + "\n\n# " + VPLAN_REVIEW_DIRECTIVE + "\n\n" + plan
	result, _ := InfoScan(files, need)
	return result
}

func PlanWipe() {
	os.Remove(VFILENAME_PLAN)
}

func VPlanWipe() {
	PlanWipe()
}

func VPlanLoad() *Plan {
	return PlanLoad()
}

func VPlanText(plan *Plan) string {
	return PlanText(plan)
}

func VPlanSave(plan *Plan) error {
	return PlanSave(plan)
}

func VPlanNext(plan *Plan) bool {
	return PlanNext(plan)
}

func VPlanExec(plan *Plan) int {
	return PlanExec(plan)
}

func VPlanMore(goal string) string {
	return PlanMore(goal)
}

func VPlanTask(files []string, goal string) string {
	return PlanTask(files, goal)
}

func VPlanTodo(files []string, goal string) string {
	return PlanTodo(files, goal)
}

func VMarkscan(src string, kw []string, mk string) (string, int) {
	return vMarkscan(src, kw, mk)
}

// VfenceChk checks whether s is wrapped in code fences (```...```),
// ignoring leading/trailing whitespace. Ported from C vfence_chk.
func VfenceChk(s string) bool {
	p := strings.TrimLeft(s, " \t\r\n")
	if !strings.HasPrefix(p, "```") {
		return false
	}
	// Find last non-whitespace character index (matching C's pointer arithmetic).
	last := len(s) - 1
	for last >= 0 && (s[last] == ' ' || s[last] == '\t' || s[last] == '\r' || s[last] == '\n') {
		last--
	}
	if last < 3 {
		return false
	}
	return s[last-2:last+1] == "```"
}

// VfenceClr strips outer code fences from s and returns the content
// between them, or empty if not fenced. Ported from C vfence_clr.
// Unlike C, this returns a copy (no buffer mutation).
func VfenceClr(s string) string {
	p := strings.TrimLeft(s, " \t\r\n")
	if !strings.HasPrefix(p, "```") {
		return ""
	}
	nl := strings.IndexByte(p, '\n')
	if nl < 0 {
		return ""
	}
	body := p[nl+1:]
	last := strings.TrimRight(body, " \t\r\n")
	if !strings.HasSuffix(last, "```") {
		return ""
	}
	return strings.TrimSuffix(last, "```")
}
