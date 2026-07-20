package main

import (
	"path/filepath"
	"reflect"
	"testing"
)

func TestRenderPackageManifestUsesExtension(t *testing.T) {
	got := string(renderPackageManifest("sqlite", "toy_sqlite.dll"))
	want := "name = sqlite\nextension = toy_sqlite.dll\n"
	if got != want {
		t.Fatalf("manifest = %q, want %q", got, want)
	}
}

func TestParseArgsAcceptsInterspersedOptions(t *testing.T) {
	parsed, help, err := parseArgs([]string{
		"vendor/widget", "widget.c", "--cc", "clang", "--include=inc",
		"--lib-dir", "lib", "--lib", "widget", "--define", "WIDGET=1",
		"--cflag=-pthread", "--ldflag", "-pthread", "--debug",
	})
	if err != nil {
		t.Fatal(err)
	}
	if help {
		t.Fatal("parseArgs() unexpectedly requested help")
	}
	if parsed.compiler != "clang" || !parsed.debug {
		t.Fatalf("unexpected compiler/debug: %#v", parsed)
	}
	if !reflect.DeepEqual(parsed.positionals,
		[]string{"vendor/widget", "widget.c"}) {
		t.Fatalf("positionals = %#v", parsed.positionals)
	}
	if !reflect.DeepEqual(parsed.libraries, []string{"widget"}) {
		t.Fatalf("libraries = %#v", parsed.libraries)
	}
}

func TestCPackageBuildCommandsForGCCStyleCompiler(t *testing.T) {
	parsed := options{
		includeDirs: []string{"external/include"},
		libraryDirs: []string{"external/lib"},
		libraries:   []string{"widget", "exact.a"},
		definitions: []string{"FEATURE=1"},
	}
	compile, link := buildCommands(parsed,
		compiler{executable: "clang"}, "linux", "/sdk", "/project/widget",
		"/project/widget/widget.c", "/project/widget/toy_widget.so", "/tmp/build")

	for _, expected := range []string{"-fPIC", filepath.Join("/sdk", "include"), "/project/widget",
		"external/include", "-DFEATURE=1"} {
		if !contains(compile, expected) {
			t.Fatalf("compile command lacks %q: %#v", expected, compile)
		}
	}
	for _, expected := range []string{"-shared", "-L", "external/lib", "-lwidget",
		"exact.a"} {
		if !contains(link, expected) {
			t.Fatalf("link command lacks %q: %#v", expected, link)
		}
	}
}

func TestCPackageBuildCommandsForMSVCStyleCompiler(t *testing.T) {
	parsed := options{libraryDirs: []string{"C:/widget/lib"},
		libraries: []string{"widget"}}
	compile, link := buildCommands(parsed,
		compiler{executable: "cl", msvcStyle: true}, "windows", "C:/Toy",
		"C:/project/widget", "C:/project/widget/widget.c",
		"C:/project/widget/toy_widget.dll", "C:/Temp/build")

	if !contains(compile, "/I"+filepath.Join("C:/Toy", "include")) ||
		contains(compile, "/LD") {
		t.Fatalf("unexpected compile command: %#v", compile)
	}
	for _, expected := range []string{"/LD", "widget.lib", "/LIBPATH:C:/widget/lib",
		"/OUT:C:/project/widget/toy_widget.dll"} {
		if !contains(link, expected) {
			t.Fatalf("link command lacks %q: %#v", expected, link)
		}
	}
}

func TestValidPackageName(t *testing.T) {
	for _, name := range []string{"sqlite", "image_v2", "raylib-5"} {
		if !validPackageName(name) {
			t.Errorf("validPackageName(%q) = false", name)
		}
	}
	for _, name := range []string{"", "5sqlite", "bad.name", "has space"} {
		if validPackageName(name) {
			t.Errorf("validPackageName(%q) = true", name)
		}
	}
}

func contains(items []string, value string) bool {
	for _, item := range items {
		if item == value {
			return true
		}
	}
	return false
}
