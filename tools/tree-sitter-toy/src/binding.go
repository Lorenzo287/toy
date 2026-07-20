package toy

/*
#include "tree_sitter/parser.h"
const TSLanguage *tree_sitter_toy(void);
*/
import "C"

import (
	"unsafe"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
)

func Language() *tree_sitter.Language {
	return tree_sitter.NewLanguage(unsafe.Pointer(C.tree_sitter_toy()))
}
