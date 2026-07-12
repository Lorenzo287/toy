package main

import (
	"log"
	"os"

	"toy-lsp/internal/dap"
)

func main() {
	logger := log.New(os.Stderr, "toy-dap: ", log.LstdFlags|log.Lshortfile)
	server := dap.NewServer(logger)
	if err := server.Serve(os.Stdin, os.Stdout); err != nil {
		logger.Fatal(err)
	}
}
