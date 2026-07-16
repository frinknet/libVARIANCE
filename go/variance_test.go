// (c) 2026 FRINKnet & Friends - MIT Licence

package libvariance

import (
	"path/filepath"
	"strings"
	"testing"

	peanuts "github.com/frinknet/libPEANUTS"
)

func TestLoadPrompts(t *testing.T) {
	path := filepath.Join("..", "x", "configure.x")
	config, err := LoadPrompts(path)
	if err != nil {
		t.Fatalf("LoadPrompts failed: %v", err)
	}

	core := config["core"]
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
	path := filepath.Join("..", "x", "filetypes.x")
	types, err := LoadFileTypes(path)
	if err != nil {
		t.Fatalf("LoadFileTypes failed: %v", err)
	}

	found := false
	for _, ft := range types {
		if ft.Lang == "golang" && strings.Contains(ft.Ext, ".go") && ft.Mark == "//" {
			found = true
			break
		}
	}
	if !found {
		t.Fatalf("golang file type not found in %s", path)
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
	if line := TodoLine("  ...", []string{"TODO", "FIXME"}, "/*"); line != "" {
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
	if got == "" {
		t.Fatal("VTaskinfo(nil, \"\") returned empty output")
	}

	got, err = VTaskmark(nil, "")
	if err != nil {
		t.Fatalf("VTaskmark(nil, \"\") failed: %v", err)
	}
	if got != "" {
		t.Fatalf("VTaskmark(nil, \"\") = %q", got)
	}

	got, err = VTaskcode(nil, "")
	if err != nil {
		t.Fatalf("VTaskcode(nil, \"\") failed: %v", err)
	}
	if got != "No files processed" {
		t.Fatalf("VTaskcode(nil, \"\") = %q", got)
	}

	got = VTaskedit(nil, "")
	if got != "Opening files in editor:\n" {
		t.Fatalf("VTaskedit(nil, \"\") = %q", got)
	}
}

func TestInfoSafetyUpdatesContextLikeC(t *testing.T) {
	res := "review|variance.go|fix this\nanswer|variance.go|done\n"
	nut := &peanuts.Peanuts{}
	if InfoSafety(nut, &res) {
		t.Fatal("InfoSafety should not accept action plus answer")
	}
	if nut.Analysis != res {
		t.Fatalf("InfoSafety analysis = %q", nut.Analysis)
	}
	if !strings.Contains(nut.Nudging, "# REVIEW variance.go") {
		t.Fatalf("InfoSafety nudging missing review evidence:\n%s", nut.Nudging)
	}
	if nut.Updates != "Okay that helps. But do I have enough info to answer? Let me think about it a bit more." {
		t.Fatalf("InfoSafety updates = %q", nut.Updates)
	}
}

func TestCodeSafetyUpdatesAnalysisLikeC(t *testing.T) {
	res := "search|go|TODO\nanswer|go/variance.go|done\n"
	nut := &peanuts.Peanuts{}
	if CodeSafety(nut, &res) {
		t.Fatal("CodeSafety should not accept action plus answer")
	}
	if nut.Analysis != res {
		t.Fatalf("CodeSafety analysis = %q", nut.Analysis)
	}
	if !strings.Contains(nut.Nudging, "# REMEMBER THESE FILES") {
		t.Fatalf("CodeSafety nudging missing remembered files:\n%s", nut.Nudging)
	}
}
