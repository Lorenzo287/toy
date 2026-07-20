package sdkroot

import (
	"os"
	"path/filepath"
	"testing"
)

func TestResolveExplicitRoot(t *testing.T) {
	root := t.TempDir()
	marker := filepath.Join(root, "include", "toy.h")
	if err := os.MkdirAll(filepath.Dir(marker), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(marker, []byte("test"), 0o644); err != nil {
		t.Fatal(err)
	}

	resolved, err := Resolve(root, "include", "toy.h")
	if err != nil {
		t.Fatal(err)
	}
	if resolved != filepath.Clean(root) {
		t.Fatalf("Resolve() = %q, want %q", resolved, filepath.Clean(root))
	}
}

func TestResolveRejectsInvalidExplicitRoot(t *testing.T) {
	_, err := Resolve(t.TempDir(), "include", "toy.h")
	if err == nil {
		t.Fatal("Resolve() unexpectedly accepted a root without its marker")
	}
}
