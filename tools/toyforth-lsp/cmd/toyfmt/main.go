package main

import (
	"bytes"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"

	"toyforth-lsp/internal/formatter"
)

type cliOptions struct {
	check            bool
	write            bool
	stdinFilepath    string
	indentWidth      int
	indentWidthSet   bool
	indentStyle      string
	delimiterSpacing string
}

func main() {
	os.Exit(run(os.Args[1:], os.Stdin, os.Stdout, os.Stderr))
}

func run(args []string, stdin io.Reader, stdout, stderr io.Writer) int {
	flags := flag.NewFlagSet("toyfmt", flag.ContinueOnError)
	flags.SetOutput(stderr)
	var cli cliOptions
	flags.BoolVar(&cli.check, "check", false, "report files whose formatting differs")
	flags.BoolVar(&cli.write, "write", false, "write formatted source back to files")
	flags.StringVar(&cli.stdinFilepath, "stdin-filepath", "", "path used to discover .toyfmt when reading stdin")
	flags.IntVar(&cli.indentWidth, "indent-width", 0, "override indentation width")
	flags.StringVar(&cli.indentStyle, "indent-style", "", "override indentation style (spaces, tab, or tabs)")
	flags.StringVar(&cli.delimiterSpacing, "delimiter-spacing", "", "override delimiter spacing (spaced or compact)")
	flags.Usage = func() {
		fmt.Fprintln(stderr, "usage: toyfmt [options] [file]")
		fmt.Fprintln(stderr, "       toyfmt --check [options] [files...]")
		fmt.Fprintln(stderr, "       toyfmt --write [options] files...")
		flags.PrintDefaults()
	}
	if err := flags.Parse(args); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return 0
		}
		return 2
	}
	flags.Visit(func(setting *flag.Flag) {
		if setting.Name == "indent-width" {
			cli.indentWidthSet = true
		}
	})

	paths := flags.Args()
	if cli.check && cli.write {
		fmt.Fprintln(stderr, "toyfmt: --check and --write cannot be used together")
		return 2
	}
	if cli.write && len(paths) == 0 {
		fmt.Fprintln(stderr, "toyfmt: --write requires at least one file")
		return 2
	}
	if !cli.check && !cli.write && len(paths) > 1 {
		fmt.Fprintln(stderr, "toyfmt: multiple files require --check or --write")
		return 2
	}
	if err := validateOverrides(cli); err != nil {
		fmt.Fprintf(stderr, "toyfmt: %v\n", err)
		return 2
	}

	changed := false
	if len(paths) == 0 {
		source, err := io.ReadAll(stdin)
		if err != nil {
			fmt.Fprintf(stderr, "toyfmt: read stdin: %v\n", err)
			return 2
		}
		formatted, err := formatSource(source, cli.stdinFilepath, cli)
		if err != nil {
			fmt.Fprintf(stderr, "toyfmt: %v\n", err)
			return 2
		}
		changed = !bytes.Equal(formatted, source)
		if cli.check {
			if changed {
				fmt.Fprintln(stderr, "<stdin> needs formatting")
			}
		} else if _, err := stdout.Write(formatted); err != nil {
			fmt.Fprintf(stderr, "toyfmt: write stdout: %v\n", err)
			return 2
		}
	} else {
		for _, path := range paths {
			source, err := os.ReadFile(path)
			if err != nil {
				fmt.Fprintf(stderr, "toyfmt: read %s: %v\n", path, err)
				return 2
			}
			formatted, err := formatSource(source, path, cli)
			if err != nil {
				fmt.Fprintf(stderr, "toyfmt: format %s: %v\n", path, err)
				return 2
			}
			fileChanged := !bytes.Equal(formatted, source)
			changed = changed || fileChanged
			switch {
			case cli.check && fileChanged:
				fmt.Fprintf(stderr, "%s needs formatting\n", path)
			case cli.write && fileChanged:
				if err := os.WriteFile(path, formatted, 0o666); err != nil {
					fmt.Fprintf(stderr, "toyfmt: write %s: %v\n", path, err)
					return 2
				}
			case !cli.check && !cli.write:
				if _, err := stdout.Write(formatted); err != nil {
					fmt.Fprintf(stderr, "toyfmt: write stdout: %v\n", err)
					return 2
				}
			}
		}
	}

	if cli.check && changed {
		return 1
	}
	return 0
}

func formatSource(source []byte, sourcePath string, cli cliOptions) ([]byte, error) {
	options := formatter.DefaultOptions()
	if sourcePath != "" {
		configured, err := formatter.OptionsForPath(sourcePath, options)
		if err != nil {
			return nil, err
		}
		options = configured
	}
	if cli.indentWidthSet {
		options.IndentWidth = cli.indentWidth
	}
	if cli.indentStyle != "" {
		options.IndentStyle = formatter.IndentStyle(cli.indentStyle)
	}
	if cli.delimiterSpacing != "" {
		options.DelimiterSpacing = formatter.DelimiterSpacing(cli.delimiterSpacing)
	}
	return formatter.Format(source, options)
}

func validateOverrides(cli cliOptions) error {
	if cli.indentWidthSet && cli.indentWidth < 1 {
		return fmt.Errorf("--indent-width must be at least 1")
	}
	if cli.indentStyle != "" && cli.indentStyle != string(formatter.IndentSpaces) &&
		cli.indentStyle != "tab" && cli.indentStyle != string(formatter.IndentTabs) {
		return fmt.Errorf("invalid --indent-style %q (want spaces, tab, or tabs)", cli.indentStyle)
	}
	if cli.delimiterSpacing != "" && cli.delimiterSpacing != string(formatter.DelimiterSpaced) && cli.delimiterSpacing != string(formatter.DelimiterCompact) {
		return fmt.Errorf("invalid --delimiter-spacing %q (want spaced or compact)", cli.delimiterSpacing)
	}
	return nil
}
