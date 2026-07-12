package parser

import (
	tree_sitter "github.com/tree-sitter/go-tree-sitter"

	"toy-lsp/internal/parser/toy"
)

func Parse(src []byte) (*tree_sitter.Tree, error) {
	p := tree_sitter.NewParser()
	defer p.Close()

	if err := p.SetLanguage(toy.Language()); err != nil {
		return nil, err
	}

	return p.Parse(src, nil), nil
}
