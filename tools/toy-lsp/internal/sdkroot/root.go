package sdkroot

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// Resolve finds the Toy SDK root containing marker. An explicit override or
// TOY_ROOT is authoritative; executable-relative and working-directory
// candidates support installed tools and source-tree development respectively.
func Resolve(override string, marker ...string) (string, error) {
	if override != "" {
		return requireMarker(override, marker)
	}
	if root := os.Getenv("TOY_ROOT"); root != "" {
		return requireMarker(root, marker)
	}

	var candidates []string
	if executable, err := os.Executable(); err == nil {
		if resolved, resolveErr := filepath.EvalSymlinks(executable); resolveErr == nil {
			executable = resolved
		}
		directory := filepath.Dir(executable)
		if strings.EqualFold(filepath.Base(directory), "bin") {
			candidates = append(candidates, filepath.Dir(directory))
		}
		candidates = append(candidates, directory)
	}
	if workingDirectory, err := os.Getwd(); err == nil {
		candidates = append(candidates, workingDirectory)
	}

	seen := make(map[string]bool)
	for _, candidate := range candidates {
		absolute, err := filepath.Abs(candidate)
		if err != nil {
			continue
		}
		absolute = filepath.Clean(absolute)
		if seen[absolute] {
			continue
		}
		seen[absolute] = true
		if markerExists(absolute, marker) {
			return absolute, nil
		}
	}

	return "", fmt.Errorf(
		"could not find a Toy SDK containing %s; reinstall Toy or set TOY_ROOT",
		filepath.Join(marker...),
	)
}

func requireMarker(root string, marker []string) (string, error) {
	absolute, err := filepath.Abs(root)
	if err != nil {
		return "", fmt.Errorf("resolve Toy SDK root %q: %w", root, err)
	}
	absolute = filepath.Clean(absolute)
	if !markerExists(absolute, marker) {
		return "", fmt.Errorf("Toy SDK root %s does not contain %s",
			absolute, filepath.Join(marker...))
	}
	return absolute, nil
}

func markerExists(root string, marker []string) bool {
	parts := append([]string{root}, marker...)
	info, err := os.Stat(filepath.Join(parts...))
	return err == nil && !info.IsDir()
}
