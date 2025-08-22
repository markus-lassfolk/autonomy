package main

import (
	"fmt"
	"strings"

	"golang.org/x/crypto/ssh"
)

// debugNeighborCells shows the raw neighbor cell data for analysis
func debugNeighborCells(client *ssh.Client) error {
	fmt.Println("🔍 DEBUG: Raw Neighbor Cell Data")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Get raw neighbor cell output
	output, err := executeCommand(client, "gsmctl -A 'AT+QENG=\"neighbourcell\"'")
	if err != nil {
		return fmt.Errorf("failed to get neighbor cells: %w", err)
	}

	fmt.Println("📡 Raw AT Command Output:")
	fmt.Println(output)
	fmt.Println()

	// Parse and analyze each line
	fmt.Println("📊 Parsed Neighbor Cell Analysis:")
	lines := strings.Split(output, "\n")
	cellCount := 0

	for i, line := range lines {
		line = strings.TrimSpace(line)
		if strings.Contains(line, "+QENG:") && strings.Contains(line, "neighbourcell") {
			cellCount++
			fmt.Printf("  %d. Line %d: %s\n", cellCount, i+1, line)

			// Parse the line
			parts := strings.Split(line, ",")
			fmt.Printf("     Parts count: %d\n", len(parts))

			if len(parts) >= 12 {
				cellType := "unknown"
				if strings.Contains(line, "intra") {
					cellType = "intra"
				} else if strings.Contains(line, "inter") {
					cellType = "inter"
				}

				fmt.Printf("     Cell Type: %s\n", cellType)
				fmt.Printf("     Technology: %s\n", strings.TrimSpace(parts[1]))
				fmt.Printf("     EARFCN: %s\n", strings.TrimSpace(parts[2]))
				fmt.Printf("     PCID: %s\n", strings.TrimSpace(parts[3]))

				if len(parts) >= 7 {
					fmt.Printf("     Signal Fields: [4]=%s, [5]=%s, [6]=%s\n",
						strings.TrimSpace(parts[4]),
						strings.TrimSpace(parts[5]),
						strings.TrimSpace(parts[6]))
				}
			}
			fmt.Println()
		}
	}

	fmt.Printf("📈 Total neighbor cells found: %d\n", cellCount)
	return nil
}

// testDebugNeighborCells runs the neighbor cell debug analysis
func testDebugNeighborCells() error {
	fmt.Println("🔍 DEBUGGING NEIGHBOR CELL PARSING")
	fmt.Println("=" + strings.Repeat("=", 35))

	// Connect to RutOS
	client, err := createSSHClient()
	if err != nil {
		return fmt.Errorf("failed to connect to RutOS: %w", err)
	}
	defer client.Close()

	// Debug neighbor cells
	return debugNeighborCells(client)
}
