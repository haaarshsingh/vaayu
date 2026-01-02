package vite

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
)

type ManifestEntry struct {
	File    string   `json:"file"`
	Src     string   `json:"src"`
	IsEntry bool     `json:"isEntry"`
	CSS     []string `json:"css,omitempty"`
}

type Manifest map[string]ManifestEntry

func ReadManifest(distDir string) (map[string]string, error) {
	manifestPath := filepath.Join(distDir, ".vite", "manifest.json")

	data, err := os.ReadFile(manifestPath)
	if err != nil {
		return nil, fmt.Errorf("failed to read manifest: %w", err)
	}

	var manifest Manifest
	if err := json.Unmarshal(data, &manifest); err != nil {
		return nil, fmt.Errorf("failed to parse manifest: %w", err)
	}

	result := make(map[string]string)
	for src, entry := range manifest {
		result[src] = entry.File

		for _, css := range entry.CSS {
			cssKey := src + ".css"
			result[cssKey] = css
		}
	}

	return result, nil
}

func GetAssetURL(manifestMap map[string]string, srcPath string) string {
	if file, ok := manifestMap[srcPath]; ok {
		return "/assets/" + file
	}
	return "/assets/" + srcPath
}
