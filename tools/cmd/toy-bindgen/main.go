package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"toy-tools/internal/sdkroot"
)

func main() {
	os.Exit(run(os.Args[1:], os.Stdin, os.Stdout, os.Stderr))
}

func run(args []string, stdin io.Reader, stdout, stderr io.Writer) int {
	if len(args) == 0 {
		printUsage(stderr)
		return 2
	}
	for _, argument := range args {
		if argument == "-h" || argument == "--help" {
			printUsage(stdout)
			return 0
		}
	}

	sdkOverride, forwarded, err := extractSDKRoot(args)
	if err != nil {
		fmt.Fprintf(stderr, "toy-bindgen: %v\n", err)
		return 2
	}
	root, err := sdkroot.Resolve(sdkOverride, "share", "toy", "bindgen",
		"generate-binding.js")
	if err != nil {
		fmt.Fprintf(stderr, "toy-bindgen: %v\n", err)
		return 1
	}
	node, err := exec.LookPath("node")
	if err != nil {
		fmt.Fprintln(stderr, "toy-bindgen: Node.js is required to generate bindings")
		return 1
	}
	script := filepath.Join(root, "share", "toy", "bindgen",
		"generate-binding.js")
	command := exec.Command(node, append([]string{script}, forwarded...)...)
	command.Stdin = stdin
	command.Stdout = stdout
	command.Stderr = stderr
	if err := command.Run(); err != nil {
		var exitError *exec.ExitError
		if errors.As(err, &exitError) {
			return exitError.ExitCode()
		}
		fmt.Fprintf(stderr, "toy-bindgen: start Node.js: %v\n", err)
		return 1
	}
	return 0
}

func extractSDKRoot(args []string) (string, []string, error) {
	var root string
	forwarded := make([]string, 0, len(args))
	for index := 0; index < len(args); index++ {
		argument := args[index]
		switch {
		case argument == "--sdk-root":
			if index+1 >= len(args) {
				return "", nil, fmt.Errorf("--sdk-root requires a directory")
			}
			if root != "" {
				return "", nil, fmt.Errorf("--sdk-root may only be specified once")
			}
			index++
			root = args[index]
		case strings.HasPrefix(argument, "--sdk-root="):
			if root != "" {
				return "", nil, fmt.Errorf("--sdk-root may only be specified once")
			}
			root = strings.TrimPrefix(argument, "--sdk-root=")
			if root == "" {
				return "", nil, fmt.Errorf("--sdk-root requires a directory")
			}
		default:
			forwarded = append(forwarded, argument)
		}
	}
	return root, forwarded, nil
}

func printUsage(output io.Writer) {
	fmt.Fprintln(output, "usage: toy-bindgen [options] <manifest.json> <output.c>")
	fmt.Fprintln(output)
	fmt.Fprintln(output, "options:")
	fmt.Fprintln(output, "  --check          compare output without rewriting it")
	fmt.Fprintln(output, "  --package NAME   require the manifest package to match NAME")
	fmt.Fprintln(output, "  --sdk-root DIR   override the Toy SDK root")
	fmt.Fprintln(output)
	fmt.Fprintln(output, "Node.js is required. Compile generated C with toy-c-package.")
}
