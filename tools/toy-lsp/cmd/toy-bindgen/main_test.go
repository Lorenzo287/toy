package main

import (
	"reflect"
	"testing"
)

func TestExtractSDKRoot(t *testing.T) {
	root, forwarded, err := extractSDKRoot([]string{
		"--check", "--sdk-root", "C:/Toy", "input.json", "output.c",
	})
	if err != nil {
		t.Fatal(err)
	}
	if root != "C:/Toy" {
		t.Fatalf("root = %q", root)
	}
	want := []string{"--check", "input.json", "output.c"}
	if !reflect.DeepEqual(forwarded, want) {
		t.Fatalf("forwarded = %#v, want %#v", forwarded, want)
	}
}

func TestExtractSDKRootRejectsDuplicates(t *testing.T) {
	_, _, err := extractSDKRoot([]string{"--sdk-root=one", "--sdk-root", "two"})
	if err == nil {
		t.Fatal("extractSDKRoot() unexpectedly accepted duplicate roots")
	}
}
