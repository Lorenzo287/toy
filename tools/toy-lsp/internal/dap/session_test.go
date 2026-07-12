package dap

import (
	"bytes"
	"testing"
)

func TestMachineStreamSeparatesOutputAndEvents(t *testing.T) {
	session := &debugSession{
		events:  make(chan machineEvent, 8),
		outputs: make(chan processOutput, 8),
	}
	waitCode := make(chan int, 1)
	waitCode <- 0
	stream := bytes.NewBufferString(
		"before\n\x1e{\"event\":\"stopped\",\"reason\":\"entry\",\"line\":3}\n" +
			"after\x1e{\"event\":\"terminated\",\"exitCode\":0}\n",
	)

	session.readMachineStream(stream, waitCode)
	events := make([]machineEvent, 0, 4)
	for event := range session.events {
		events = append(events, event)
	}
	if len(events) != 4 || events[0].Event != "output" ||
		events[0].Message != "before\n" || events[1].Event != "stopped" ||
		events[1].Reason != "entry" || events[2].Event != "output" ||
		events[2].Message != "after" || events[3].Event != "terminated" {
		t.Fatalf("unexpected events: %#v", events)
	}
}

func TestSamePath(t *testing.T) {
	left := t.TempDir() + "/program.toy"
	if !samePath(left, left) {
		t.Fatal("samePath rejected an identical path")
	}
}
