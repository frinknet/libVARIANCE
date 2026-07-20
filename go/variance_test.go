// (c) 2026 FRINKnet & Friends - MIT Licence

package libvariance

import (
	"os"
	"strings"
	"testing"

	peanuts "github.com/frinknet/libPEANUTS"
)

func TestLoadPrompts(t *testing.T) {
	// Configs are baked in at compile time — verify via the public API.
	core := Configs["core"]
	if core.Model != "openrouter/free" {
		t.Fatalf("core model = %q", core.Model)
	}
	if strings.Join(core.Keywords, " ") != "TODO FIXME BUG HACK" {
		t.Fatalf("core keywords = %q", core.Keywords)
	}
	if core.Timeout != 9000 || core.Tries != 10 {
		t.Fatalf("bad core defaults: %#v", core)
	}
}

func TestLoadFileTypes(t *testing.T) {
	// File types are baked in at compile time — verify the pre-built slice.
	_ = GetFileTypes() // ensure init
	found := false
	for _, ft := range FileTypes {
		if ft.Lang == "golang" && strings.Contains(ft.Ext, ".go") && ft.Mark == "//" {
			found = true
			break
		}
	}
	if !found {
		t.Fatal("golang file type not found in pre-built fileTypes")
	}
}

func TestFileTypeFromSharedX(t *testing.T) {
	if got := FileLang("main.go"); got != "golang" {
		t.Fatalf("FileLang(main.go) = %q", got)
	}
	if got := FileMark("main.go"); got != "//" {
		t.Fatalf("FileMark(main.go) = %q", got)
	}
	if got := FileLang("sample.c"); got != "c" {
		t.Fatalf("FileLang(sample.c) = %q", got)
	}
}

func TestFileInstFromPrompts(t *testing.T) {
	got := FileInst("main.go", []string{"TODO", "FIXME"}, "{{language}} {{keywords}} {{filename}}")
	want := "GOLANG TODO, FIXME main.go"
	if got != want {
		t.Fatalf("FileInst = %q, want %q", got, want)
	}

	got = FileInst("", CoreConfig().Keywords, "{{keywords}}")
	want = "TODO, FIXME, BUG, HACK"
	if got != want {
		t.Fatalf("core FileInst = %q, want %q", got, want)
	}
}

func TestTodoLineAndScan(t *testing.T) {
	line := TodoLine("  // TODO: fix this", []string{"TODO", "FIXME"}, "//")
	if line != "TODO: fix this" {
		t.Fatalf("TodoLine = %q", line)
	}
	if line := TodoLine("  ...", []string{"TODO", "FIXME"}, "/*"); line != "..." {
		t.Fatalf("ellipsis without marker = %q", line)
	}

	todos, err := TodoScan("1|name|BUG|make it work\n2|name|TODO|add tests")
	if err != nil {
		t.Fatalf("TodoScan failed: %v", err)
	}
	if len(todos) != 2 || todos[0].Line != 1 || todos[1].Type != "TODO" {
		t.Fatalf("bad todos: %#v", todos)
	}
}

func TestTodoMark(t *testing.T) {
	src := "func main() {\n\tprintln(\"hi\")\n}\n"
	marked := TodoMark(src, []Todo{{Line: 1, Type: "TODO", Note: "add tests"}}, "//")
	if !strings.Contains(marked, "// TODO: add tests") {
		t.Fatalf("TodoMark did not insert comment:\n%s", marked)
	}
}

func TestTaskWrappersMatchCDefaults(t *testing.T) {
	got, err := VTaskinfo(nil, "")
	if err != nil {
		t.Fatalf("VTaskinfo(nil, \"\") failed: %v", err)
	}
	// VTaskinfo(nil, "") returns an empty context when the repo has no files.

	got, err = VTaskmark(nil, "")
	if err != nil {
		t.Fatalf("VTaskmark(nil, \"\") failed: %v", err)
	}
	// VTaskmark(nil, "") calls MarkList which searches codebase for keywords
	// Result depends on whether any files contain the literal keyword pattern

	got, err = VTaskcode(nil, "")
	if err != nil {
		t.Fatalf("VTaskcode(nil, \"\") failed: %v", err)
	}
	if got != "" {
		t.Fatalf("VTaskcode(nil, \"\") = %q", got)
	}

	got = VTaskedit(nil, "")
	if got != "Not yet implemented..." {
		t.Fatalf("VTaskedit(nil, \"\") = %q", got)
	}
}

func TestInfoSafetyUpdatesContextLikeC(t *testing.T) {
	res := "review|variance.go|fix this\nanswer|variance.go|done\n"
	nut := &peanuts.Peanuts{}
	if infoSafety(nut, &res) {
		t.Fatal("InfoSafety should not accept action plus answer")
	}
	if nut.Analysis != res {
		t.Fatalf("InfoSafety analysis = %q", nut.Analysis)
	}
	if !strings.Contains(nut.Nudging, "# REVIEW variance.go") {
		t.Fatalf("InfoSafety nudging missing review evidence:\n%s", nut.Nudging)
	}
	if nut.Updates != "Okay that helps. Is it enough info to answer? Let me think about it a bit more." {
		t.Fatalf("InfoSafety updates = %q", nut.Updates)
	}
}

func TestCodeSearchUpdatesAnalysisLikeC(t *testing.T) {
	res := "search|go|TODO\nanswer|go/variance.go|done\n"
	nut := &peanuts.Peanuts{}
	if codeSearch(nut, &res) {
		t.Fatal("CodeSafety should not accept action plus answer")
	}
	if nut.Analysis != res {
		t.Fatalf("CodeSafety analysis = %q", nut.Analysis)
	}
	if !strings.Contains(nut.Nudging, "# REMEMBER THESE FILES") {
		t.Fatalf("CodeSafety nudging missing remembered files:\n%s", nut.Nudging)
	}
}

func TestInfoPersistenceMatchesCFormat(t *testing.T) {
	withTempDir(t)

	if err := writeInfo("go/variance.go", "summary"); err != nil {
		t.Fatalf("WriteInfo failed: %v", err)
	}
	data, err := os.ReadFile(".v-info")
	if err != nil {
		t.Fatalf("ReadFile .v-info failed: %v", err)
	}
	if strings.Contains(string(data), "# go/variance.go |") {
		t.Fatalf(".v-info kept legacy Go-only format:\n%s", data)
	}
	if got := toolSummary([]string{"go/variance.go"}); got != "go/variance.go ⟵ summary\n" {
		t.Fatalf("toolSummary = %q", got)
	}
	if got := InfoList([]string{"go/variance.go"}, ""); !strings.Contains(got, "# Project Files\ngo/variance.go ⟵ summary\n") {
		t.Fatalf("InfoList did not use C-style summary:\n%s", got)
	}
}

func TestPlanTextParseRoundTrip(t *testing.T) {
	plan := &Plan{
		Title: "Refactor plan",
		Goals: "Keep behavior unchanged.",
		Steps: []Step{{
			Task:  "Update parity tests",
			Files: []string{"go/variance_test.go", "c/variance.c"},
		}},
		Notes: "Keep the shared format stable.",
	}

	text := PlanText(plan)
	parsed := parsePlan(text)
	if parsed.Title != plan.Title || parsed.Goals != plan.Goals || parsed.Notes != plan.Notes {
		t.Fatalf("PlanText/ParsePlan mismatch: %#v", parsed)
	}
	if len(parsed.Steps) != 1 || parsed.Steps[0].Task != plan.Steps[0].Task {
		t.Fatalf("steps mismatch: %#v", parsed.Steps)
	}
	if strings.Join(parsed.Steps[0].Files, ",") != strings.Join(plan.Steps[0].Files, ",") {
		t.Fatalf("files mismatch: %#v", parsed.Steps[0].Files)
	}
	if PlanText(parsed) != text {
		t.Fatalf("round trip changed plan:\nbefore:\n%s\nafter:\n%s", text, PlanText(parsed))
	}
}

func TestAnswerOnlySafetyReplacesResponse(t *testing.T) {
	res := "answer|go/variance.go|done\n"
	nut := &peanuts.Peanuts{}
	if !infoSafety(nut, &res) {
		t.Fatal("InfoSafety should accept answer-only response")
	}
	if res != "go/variance.go|done\n" {
		t.Fatalf("InfoSafety response = %q", res)
	}

	res = "answer|go/variance.go|done\n"
	if !codeSearch(nut, &res) {
		t.Fatal("CodeSearch should accept answer-only response")
	}
	if res != "go/variance.go|done\n" {
		t.Fatalf("CodeSearch response = %q", res)
	}
}

func TestVfenceChk(t *testing.T) {
	if !VfenceChk("```go\nfoo\n```") {
		t.Fatal("should detect fenced block")
	}
	if !VfenceChk("  ```\nfoo\n```  ") {
		t.Fatal("should tolerate outer whitespace")
	}
	if VfenceChk("") {
		t.Fatal("empty string")
	}
	if VfenceChk("```") {
		t.Fatal("unclosed fence")
	}
	if VfenceChk("```\n") {
		t.Fatal("fence with no content")
	}
}

func TestVfenceClr(t *testing.T) {
	if got := VfenceClr("```go\nprint(1)\n```"); got != "print(1)\n" {
		t.Fatalf("got %q", got)
	}
	if got := VfenceClr("  ```\nhello\n```  "); got != "hello\n" {
		t.Fatalf("got %q", got)
	}
	if got := VfenceClr("no fence"); got != "" {
		t.Fatalf("got %q", got)
	}
	if got := VfenceClr("```\nno closing"); got != "" {
		t.Fatalf("got %q", got)
	}
}

func withTempDir(t *testing.T) {
	t.Helper()

	dir := t.TempDir()
	old, err := os.Getwd()
	if err != nil {
		t.Fatalf("Getwd failed: %v", err)
	}
	if err := os.Chdir(dir); err != nil {
		t.Fatalf("Chdir failed: %v", err)
	}
	t.Cleanup(func() {
		_ = os.Chdir(old)
	})
}
