package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"unicode"

	"toy-tools/internal/sdkroot"
)

type options struct {
	compiler     string
	sdkRoot      string
	includeDirs  []string
	libraryDirs  []string
	libraries    []string
	definitions  []string
	compileFlags []string
	linkFlags    []string
	debug        bool
	positionals  []string
}

type compiler struct {
	executable string
	msvcStyle  bool
}

func main() {
	os.Exit(run(os.Args[1:], os.Stdout, os.Stderr))
}

func run(args []string, stdout, stderr io.Writer) int {
	parsed, help, err := parseArgs(args)
	if help {
		printUsage(stdout)
		return 0
	}
	if err != nil {
		fmt.Fprintf(stderr, "toy-c-package: %v\n\n", err)
		printUsage(stderr)
		return 2
	}

	packageDirectory, err := filepath.Abs(parsed.positionals[0])
	if err != nil {
		fmt.Fprintf(stderr, "toy-c-package: resolve package directory: %v\n", err)
		return 1
	}
	source, err := filepath.Abs(parsed.positionals[1])
	if err != nil {
		fmt.Fprintf(stderr, "toy-c-package: resolve source: %v\n", err)
		return 1
	}
	if info, statErr := os.Stat(packageDirectory); statErr != nil || !info.IsDir() {
		fmt.Fprintf(stderr, "toy-c-package: package directory does not exist: %s\n",
			packageDirectory)
		return 1
	}
	if info, statErr := os.Stat(source); statErr != nil || info.IsDir() {
		fmt.Fprintf(stderr, "toy-c-package: C source does not exist: %s\n", source)
		return 1
	}
	packageName := filepath.Base(filepath.Clean(packageDirectory))
	if !validPackageName(packageName) {
		fmt.Fprintf(stderr,
			"toy-c-package: package directory name %q is not a Toy identifier\n",
			packageName)
		return 1
	}

	root, err := sdkroot.Resolve(parsed.sdkRoot, "include", "toy.h")
	if err != nil {
		fmt.Fprintf(stderr, "toy-c-package: %v\n", err)
		return 1
	}
	selected, err := selectCompiler(parsed.compiler)
	if err != nil {
		fmt.Fprintf(stderr, "toy-c-package: %v\n", err)
		return 1
	}

	buildDirectory, err := os.MkdirTemp("", "toy-c-package-")
	if err != nil {
		fmt.Fprintf(stderr, "toy-c-package: create temporary build directory: %v\n", err)
		return 1
	}
	defer os.RemoveAll(buildDirectory)

	extensionFile := "toy_" + packageName + sharedLibrarySuffix(runtime.GOOS)
	output := filepath.Join(packageDirectory, extensionFile)
	compile, link := buildCommands(parsed, selected, runtime.GOOS, root,
		packageDirectory, source, output, buildDirectory)
	if err := runCommand(compile, stdout, stderr); err != nil {
		return reportCommandError(stderr, "compile C extension", err)
	}
	if err := runCommand(link, stdout, stderr); err != nil {
		return reportCommandError(stderr, "link C extension", err)
	}

	manifest := filepath.Join(packageDirectory, "toy.package")
	contents := renderPackageManifest(packageName, extensionFile)
	if err := os.WriteFile(manifest, contents, 0o644); err != nil {
		fmt.Fprintf(stderr, "toy-c-package: write %s: %v\n", manifest, err)
		return 1
	}
	fmt.Fprintf(stdout, "built C extension %s\n", output)
	fmt.Fprintf(stdout, "wrote %s\n", manifest)
	return 0
}

func renderPackageManifest(packageName, extensionFile string) []byte {
	return []byte(fmt.Sprintf("name = %s\nextension = %s\n",
		packageName, extensionFile))
}

func parseArgs(args []string) (options, bool, error) {
	var parsed options
	value := func(index *int, name string) (string, error) {
		if *index+1 >= len(args) {
			return "", fmt.Errorf("%s requires a value", name)
		}
		(*index)++
		return args[*index], nil
	}
	appendValue := func(target *[]string, index *int, name string) error {
		item, err := value(index, name)
		if err != nil {
			return err
		}
		*target = append(*target, item)
		return nil
	}

	for index := 0; index < len(args); index++ {
		argument := args[index]
		var err error
		switch {
		case argument == "-h" || argument == "--help":
			return options{}, true, nil
		case argument == "--debug":
			parsed.debug = true
		case argument == "--cc":
			parsed.compiler, err = value(&index, argument)
		case strings.HasPrefix(argument, "--cc="):
			parsed.compiler = strings.TrimPrefix(argument, "--cc=")
		case argument == "--sdk-root":
			parsed.sdkRoot, err = value(&index, argument)
		case strings.HasPrefix(argument, "--sdk-root="):
			parsed.sdkRoot = strings.TrimPrefix(argument, "--sdk-root=")
		case argument == "--include":
			err = appendValue(&parsed.includeDirs, &index, argument)
		case strings.HasPrefix(argument, "--include="):
			parsed.includeDirs = append(parsed.includeDirs,
				strings.TrimPrefix(argument, "--include="))
		case argument == "--lib-dir":
			err = appendValue(&parsed.libraryDirs, &index, argument)
		case strings.HasPrefix(argument, "--lib-dir="):
			parsed.libraryDirs = append(parsed.libraryDirs,
				strings.TrimPrefix(argument, "--lib-dir="))
		case argument == "--lib":
			err = appendValue(&parsed.libraries, &index, argument)
		case strings.HasPrefix(argument, "--lib="):
			parsed.libraries = append(parsed.libraries,
				strings.TrimPrefix(argument, "--lib="))
		case argument == "--define":
			err = appendValue(&parsed.definitions, &index, argument)
		case strings.HasPrefix(argument, "--define="):
			parsed.definitions = append(parsed.definitions,
				strings.TrimPrefix(argument, "--define="))
		case argument == "--cflag":
			err = appendValue(&parsed.compileFlags, &index, argument)
		case strings.HasPrefix(argument, "--cflag="):
			parsed.compileFlags = append(parsed.compileFlags,
				strings.TrimPrefix(argument, "--cflag="))
		case argument == "--ldflag":
			err = appendValue(&parsed.linkFlags, &index, argument)
		case strings.HasPrefix(argument, "--ldflag="):
			parsed.linkFlags = append(parsed.linkFlags,
				strings.TrimPrefix(argument, "--ldflag="))
		case strings.HasPrefix(argument, "-"):
			return options{}, false, fmt.Errorf("unknown option: %s", argument)
		default:
			parsed.positionals = append(parsed.positionals, argument)
		}
		if err != nil {
			return options{}, false, err
		}
	}
	if len(parsed.positionals) != 2 {
		return options{}, false,
			fmt.Errorf("expected a package directory and one C source file")
	}
	if parsed.compiler == "" {
		parsed.compiler = os.Getenv("CC")
	}
	return parsed, false, nil
}

func selectCompiler(requested string) (compiler, error) {
	if requested == "" {
		candidates := []string{"cc", "clang", "gcc"}
		if runtime.GOOS == "windows" {
			candidates = []string{"clang", "gcc", "cl"}
		}
		for _, candidate := range candidates {
			if executable, err := exec.LookPath(candidate); err == nil {
				return compiler{executable: executable,
					msvcStyle: isMSVCStyle(candidate)}, nil
			}
		}
		return compiler{}, fmt.Errorf(
			"no C compiler found on PATH; pass --cc gcc, clang, msvc, or clang-cl")
	}
	if requested == "msvc" {
		requested = "cl"
	}
	executable, err := exec.LookPath(requested)
	if err != nil {
		return compiler{}, fmt.Errorf("C compiler %q was not found on PATH", requested)
	}
	return compiler{executable: executable, msvcStyle: isMSVCStyle(requested)}, nil
}

func isMSVCStyle(executable string) bool {
	name := strings.ToLower(filepath.Base(executable))
	name = strings.TrimSuffix(name, ".exe")
	return name == "cl" || name == "clang-cl" || name == "msvc"
}

func buildCommands(parsed options, selected compiler, goos, root,
	packageDirectory, source, output, buildDirectory string) ([]string, []string) {
	objectSuffix := ".o"
	if selected.msvcStyle {
		objectSuffix = ".obj"
	}
	object := filepath.Join(buildDirectory, "package"+objectSuffix)
	includeDirectories := append([]string{filepath.Join(root, "include"),
		packageDirectory}, parsed.includeDirs...)

	if selected.msvcStyle {
		compile := []string{selected.executable, "/nologo", "/std:c11", "/W3",
			"/D_CRT_SECURE_NO_WARNINGS"}
		if parsed.debug {
			compile = append(compile, "/Od", "/Z7")
		} else {
			compile = append(compile, "/O2", "/DNDEBUG")
		}
		for _, directory := range includeDirectories {
			compile = append(compile, "/I"+directory)
		}
		for _, definition := range parsed.definitions {
			compile = append(compile, "/D"+definition)
		}
		compile = append(compile, parsed.compileFlags...)
		compile = append(compile, "/c", source, "/Fo"+object)

		link := []string{selected.executable, "/nologo", "/LD", object}
		for _, library := range parsed.libraries {
			if libraryIsPath(library) {
				link = append(link, library)
			} else {
				link = append(link, library+".lib")
			}
		}
		link = append(link, "/link", "/OUT:"+output,
			"/IMPLIB:"+filepath.Join(buildDirectory, "package.lib"),
			"/PDB:"+filepath.Join(buildDirectory, "package.pdb"))
		for _, directory := range parsed.libraryDirs {
			link = append(link, "/LIBPATH:"+directory)
		}
		link = append(link, parsed.linkFlags...)
		return compile, link
	}

	compile := []string{selected.executable, "-std=c11", "-Wall", "-Wextra",
		"-Wpedantic"}
	if parsed.debug {
		compile = append(compile, "-O0", "-g")
	} else {
		compile = append(compile, "-O2", "-DNDEBUG")
	}
	if goos != "windows" {
		compile = append(compile, "-fPIC")
	}
	compile = append(compile, "-fvisibility=hidden")
	for _, directory := range includeDirectories {
		compile = append(compile, "-I", directory)
	}
	for _, definition := range parsed.definitions {
		compile = append(compile, "-D"+definition)
	}
	compile = append(compile, parsed.compileFlags...)
	compile = append(compile, "-c", source, "-o", object)

	link := []string{selected.executable}
	if goos == "darwin" {
		link = append(link, "-dynamiclib")
	} else {
		link = append(link, "-shared")
	}
	link = append(link, object, "-o", output)
	for _, directory := range parsed.libraryDirs {
		link = append(link, "-L", directory)
	}
	for _, library := range parsed.libraries {
		if libraryIsPath(library) {
			link = append(link, library)
		} else {
			link = append(link, "-l"+library)
		}
	}
	link = append(link, parsed.linkFlags...)
	return compile, link
}

func runCommand(arguments []string, stdout, stderr io.Writer) error {
	fmt.Fprintln(stderr, "+ "+formatCommand(arguments))
	command := exec.Command(arguments[0], arguments[1:]...)
	command.Stdout = stdout
	command.Stderr = stderr
	return command.Run()
}

func reportCommandError(stderr io.Writer, operation string, err error) int {
	var exitError *exec.ExitError
	if errors.As(err, &exitError) {
		fmt.Fprintf(stderr, "toy-c-package: %s failed with exit code %d\n",
			operation, exitError.ExitCode())
		return exitError.ExitCode()
	}
	fmt.Fprintf(stderr, "toy-c-package: %s: %v\n", operation, err)
	return 1
}

func formatCommand(arguments []string) string {
	formatted := make([]string, len(arguments))
	for index, argument := range arguments {
		if strings.IndexFunc(argument, unicode.IsSpace) >= 0 || argument == "" {
			formatted[index] = strconv.Quote(argument)
		} else {
			formatted[index] = argument
		}
	}
	return strings.Join(formatted, " ")
}

func validPackageName(name string) bool {
	for index := 0; index < len(name); index++ {
		character := name[index]
		if index == 0 {
			if !asciiLetter(character) && character != '_' {
				return false
			}
		} else if !asciiLetter(character) && !asciiDigit(character) &&
			character != '_' && character != '-' {
			return false
		}
	}
	return name != ""
}

func asciiLetter(character byte) bool {
	return character >= 'A' && character <= 'Z' ||
		character >= 'a' && character <= 'z'
}

func asciiDigit(character byte) bool {
	return character >= '0' && character <= '9'
}

func sharedLibrarySuffix(goos string) string {
	switch goos {
	case "windows":
		return ".dll"
	case "darwin":
		return ".dylib"
	default:
		return ".so"
	}
}

func libraryIsPath(library string) bool {
	if strings.ContainsAny(library, `/\\`) {
		return true
	}
	lower := strings.ToLower(library)
	for _, suffix := range []string{".a", ".lib", ".so", ".dylib", ".dll"} {
		if strings.HasSuffix(lower, suffix) {
			return true
		}
	}
	return false
}

func printUsage(output io.Writer) {
	fmt.Fprintln(output, "usage: toy-c-package [options] <package-directory> <source.c>")
	fmt.Fprintln(output)
	fmt.Fprintln(output, "Compile one C extension for a Toy package and write toy.package.")
	fmt.Fprintln(output)
	fmt.Fprintln(output, "options:")
	fmt.Fprintln(output, "  --cc COMPILER      cc, gcc, clang, msvc, clang-cl, or an executable")
	fmt.Fprintln(output, "  --sdk-root DIR     override the Toy SDK root")
	fmt.Fprintln(output, "  --include DIR      add a C include directory (repeatable)")
	fmt.Fprintln(output, "  --lib-dir DIR      add a library search directory (repeatable)")
	fmt.Fprintln(output, "  --lib NAME         link a library name or exact path (repeatable)")
	fmt.Fprintln(output, "  --define VALUE     add a C preprocessor definition (repeatable)")
	fmt.Fprintln(output, "  --cflag VALUE      pass one extra compiler flag (repeatable)")
	fmt.Fprintln(output, "  --ldflag VALUE     pass one extra linker flag (repeatable)")
	fmt.Fprintln(output, "  --debug            build without optimization and include debug information")
}
