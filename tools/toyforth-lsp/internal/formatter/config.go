package formatter

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

const ConfigFileName = ".toyfmt"

// Config is a partial set of formatter options. Pointer fields distinguish an
// omitted setting from an explicit child-directory override.
type Config struct {
	IndentWidth      *int
	IndentStyle      *IndentStyle
	DelimiterSpacing *DelimiterSpacing
	Disabled         *bool
}

// Apply overlays the settings present in the config onto base.
func (c Config) Apply(base Options) Options {
	if c.IndentWidth != nil {
		base.IndentWidth = *c.IndentWidth
	}
	if c.IndentStyle != nil {
		base.IndentStyle = *c.IndentStyle
	}
	if c.DelimiterSpacing != nil {
		base.DelimiterSpacing = *c.DelimiterSpacing
	}
	if c.Disabled != nil {
		base.Disabled = *c.Disabled
	}
	return base
}

// DiscoverConfig reads every .toyfmt from the filesystem root down to the
// source file's directory. Settings closer to the source file override parent
// settings.
func DiscoverConfig(sourcePath string) (Config, error) {
	directories, err := configDirectories(sourcePath)
	if err != nil {
		return Config{}, err
	}

	var combined Config
	for _, directory := range directories {
		path := filepath.Join(directory, ConfigFileName)
		file, err := os.Open(path)
		if errors.Is(err, os.ErrNotExist) {
			continue
		}
		if err != nil {
			return Config{}, fmt.Errorf("open formatter config %s: %w", path, err)
		}

		config, parseErr := ParseConfig(file)
		closeErr := file.Close()
		if parseErr != nil {
			return Config{}, fmt.Errorf("parse formatter config %s: %w", path, parseErr)
		}
		if closeErr != nil {
			return Config{}, fmt.Errorf("close formatter config %s: %w", path, closeErr)
		}
		combined = mergeConfigs(combined, config)
	}
	return combined, nil
}

// OptionsForPath applies discovered project configuration to base.
func OptionsForPath(sourcePath string, base Options) (Options, error) {
	config, err := DiscoverConfig(sourcePath)
	if err != nil {
		return Options{}, err
	}
	return config.Apply(base), nil
}

// ParseConfig parses the dependency-free key = value .toyfmt format.
func ParseConfig(reader io.Reader) (Config, error) {
	var config Config
	scanner := bufio.NewScanner(reader)
	lineNumber := 0
	for scanner.Scan() {
		lineNumber++
		line := strings.TrimSpace(stripConfigComment(scanner.Text()))
		if line == "" {
			continue
		}

		key, value, ok := strings.Cut(line, "=")
		if !ok {
			return Config{}, fmt.Errorf("line %d: expected key = value", lineNumber)
		}
		key = strings.TrimSpace(key)
		value = strings.TrimSpace(value)
		if key == "" || value == "" {
			return Config{}, fmt.Errorf("line %d: expected key = value", lineNumber)
		}

		var err error
		switch key {
		case "indent_width":
			var width int
			width, err = strconv.Atoi(value)
			if err == nil && width < 1 {
				err = fmt.Errorf("must be at least 1")
			}
			if err == nil {
				config.IndentWidth = &width
			}
		case "indent_style":
			var style IndentStyle
			style, err = parseIndentStyle(unquoteConfigValue(value))
			if err == nil {
				config.IndentStyle = &style
			}
		case "delimiter_spacing":
			var spacing DelimiterSpacing
			spacing, err = parseDelimiterSpacing(unquoteConfigValue(value))
			if err == nil {
				config.DelimiterSpacing = &spacing
			}
		case "disable":
			var disabled bool
			disabled, err = strconv.ParseBool(value)
			if err == nil {
				config.Disabled = &disabled
			}
		default:
			return Config{}, fmt.Errorf("line %d: unknown setting %q", lineNumber, key)
		}
		if err != nil {
			return Config{}, fmt.Errorf("line %d: invalid %s value %q: %w", lineNumber, key, value, err)
		}
	}
	if err := scanner.Err(); err != nil {
		return Config{}, fmt.Errorf("read config: %w", err)
	}
	return config, nil
}

func configDirectories(sourcePath string) ([]string, error) {
	absolute, err := filepath.Abs(sourcePath)
	if err != nil {
		return nil, fmt.Errorf("resolve source path %s: %w", sourcePath, err)
	}

	directory := absolute
	if info, statErr := os.Stat(absolute); statErr == nil {
		if !info.IsDir() {
			directory = filepath.Dir(absolute)
		}
	} else if errors.Is(statErr, os.ErrNotExist) {
		directory = filepath.Dir(absolute)
	} else {
		return nil, fmt.Errorf("inspect source path %s: %w", absolute, statErr)
	}

	directories := make([]string, 0, 8)
	for {
		directories = append(directories, directory)
		parent := filepath.Dir(directory)
		if parent == directory {
			break
		}
		directory = parent
	}
	for left, right := 0, len(directories)-1; left < right; left, right = left+1, right-1 {
		directories[left], directories[right] = directories[right], directories[left]
	}
	return directories, nil
}

func mergeConfigs(parent, child Config) Config {
	if child.IndentWidth != nil {
		parent.IndentWidth = child.IndentWidth
	}
	if child.IndentStyle != nil {
		parent.IndentStyle = child.IndentStyle
	}
	if child.DelimiterSpacing != nil {
		parent.DelimiterSpacing = child.DelimiterSpacing
	}
	if child.Disabled != nil {
		parent.Disabled = child.Disabled
	}
	return parent
}

func stripConfigComment(line string) string {
	quoted := false
	escaped := false
	for index, character := range line {
		if escaped {
			escaped = false
			continue
		}
		if character == '\\' && quoted {
			escaped = true
			continue
		}
		if character == '"' {
			quoted = !quoted
			continue
		}
		if character == '#' && !quoted {
			return line[:index]
		}
	}
	return line
}

func unquoteConfigValue(value string) string {
	if len(value) >= 2 && value[0] == '"' && value[len(value)-1] == '"' {
		if unquoted, err := strconv.Unquote(value); err == nil {
			return unquoted
		}
	}
	return value
}

func parseIndentStyle(value string) (IndentStyle, error) {
	if value == string(indentTab) {
		return IndentTabs, nil
	}
	style := IndentStyle(value)
	if style != IndentSpaces && style != IndentTabs {
		return "", fmt.Errorf("want %q, %q, or %q", IndentSpaces, indentTab, IndentTabs)
	}
	return style, nil
}

func parseDelimiterSpacing(value string) (DelimiterSpacing, error) {
	spacing := DelimiterSpacing(value)
	if spacing != DelimiterSpaced && spacing != DelimiterCompact {
		return "", fmt.Errorf("want %q or %q", DelimiterSpaced, DelimiterCompact)
	}
	return spacing, nil
}
