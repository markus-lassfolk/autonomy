package main

import (
	"fmt"
	"strings"

	"golang.org/x/crypto/ssh"
)

// debug5GCells shows 5G-specific cell data for analysis
func debug5GCells(client *ssh.Client) error {
	fmt.Println("📡 DEBUG: 5G Cell Data Analysis")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Test various 5G-specific AT commands
	commands := map[string]string{
		"Serving Cell (All)":   "gsmctl -A 'AT+QENG=\"servingcell\"'",
		"Neighbor Cells (All)": "gsmctl -A 'AT+QENG=\"neighbourcell\"'",
		"5G NR Serving Cell":   "gsmctl -A 'AT+QENG=\"NR5G-NSA\",\"servingcell\"'",
		"5G NR Neighbor Cells": "gsmctl -A 'AT+QENG=\"NR5G-NSA\",\"neighbourcell\"'",
		"5G SA Serving Cell":   "gsmctl -A 'AT+QENG=\"NR5G-SA\",\"servingcell\"'",
		"5G SA Neighbor Cells": "gsmctl -A 'AT+QENG=\"NR5G-SA\",\"neighbourcell\"'",
		"Network Registration": "gsmctl -A 'AT+CEREG?'",
		"5G Registration":      "gsmctl -A 'AT+C5GREG?'",
		"Radio Access Tech":    "gsmctl -A 'AT+COPS?'",
		"Network Mode":         "gsmctl -F",
		"Band Information":     "gsmctl -b",
		"Carrier Aggregation":  "gsmctl -G",
	}

	for name, cmd := range commands {
		fmt.Printf("\n🔍 %s:\n", name)
		fmt.Printf("   Command: %s\n", cmd)

		output, err := executeCommand(client, cmd)
		if err != nil {
			fmt.Printf("   ❌ Error: %v\n", err)
			continue
		}

		output = strings.TrimSpace(output)
		if output == "" {
			fmt.Printf("   📭 No output\n")
			continue
		}

		// Parse and display the output
		lines := strings.Split(output, "\n")
		for _, line := range lines {
			line = strings.TrimSpace(line)
			if line != "" {
				fmt.Printf("   📊 %s\n", line)
			}
		}
	}

	return nil
}

// test5GCellDebug runs the 5G cell debug analysis
func test5GCellDebug() error {
	fmt.Println("📡 5G CELL DATA DEBUGGING")
	fmt.Println("=" + strings.Repeat("=", 25))

	// Connect to RutOS
	client, err := createSSHClient()
	if err != nil {
		return fmt.Errorf("failed to connect to RutOS: %w", err)
	}
	defer client.Close()

	// Debug 5G cells
	return debug5GCells(client)
}
