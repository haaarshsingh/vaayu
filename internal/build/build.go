package build

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"github.com/harshsingh/vaayu/internal/compiler"
	"github.com/harshsingh/vaayu/internal/vite"
)

type Builder struct {
	siteDir string
	distDir string
	viteDir string
}

func New(siteDir, distDir, viteDir string) *Builder {
	return &Builder{
		siteDir: siteDir,
		distDir: distDir,
		viteDir: viteDir,
	}
}

func (b *Builder) Build() error {
	fmt.Println("Building Vaayu site...")

	if err := vite.EnsureInstalled(b.viteDir); err != nil {
		return fmt.Errorf("failed to install vite dependencies: %w", err)
	}

	fmt.Println("Building assets with Vite...")
	if err := vite.Build(b.viteDir, b.distDir); err != nil {
		return fmt.Errorf("vite build failed: %w", err)
	}

	manifestMap, err := vite.ReadManifest(b.distDir)
	if err != nil {
		fmt.Printf("Warning: Could not read Vite manifest: %v\n", err)
		manifestMap = make(map[string]string)
	}

	fmt.Println("Compiling .vyu files...")
	if err := b.compilePages(manifestMap); err != nil {
		return fmt.Errorf("page compilation failed: %w", err)
	}

	fmt.Println("Copying static assets...")
	if err := b.copyStaticAssets(); err != nil {
		return fmt.Errorf("static asset copying failed: %w", err)
	}

	fmt.Printf("\nBuild complete! Output in: %s\n", b.distDir)
	return nil
}

func (b *Builder) copyStaticAssets() error {
	return filepath.Walk(b.siteDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		if info.IsDir() {
			return nil
		}

		// Skip compiled/bundled files
		ext := strings.ToLower(filepath.Ext(path))
		switch ext {
		case ".vyu", ".ts", ".js", ".tsx", ".jsx", ".css":
			return nil
		}

		relPath, err := filepath.Rel(b.siteDir, path)
		if err != nil {
			return err
		}

		destPath := filepath.Join(b.distDir, relPath)

		if err := os.MkdirAll(filepath.Dir(destPath), 0755); err != nil {
			return err
		}

		if err := copyFile(path, destPath); err != nil {
			return fmt.Errorf("failed to copy %s: %w", relPath, err)
		}

		fmt.Printf("  ✓ %s (static)\n", relPath)
		return nil
	})
}

func copyFile(src, dst string) error {
	sourceFile, err := os.Open(src)
	if err != nil {
		return err
	}
	defer sourceFile.Close()

	destFile, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer destFile.Close()

	_, err = io.Copy(destFile, sourceFile)
	return err
}

func (b *Builder) compilePages(manifestMap map[string]string) error {
	return filepath.Walk(b.siteDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		if info.IsDir() || !strings.HasSuffix(path, ".vyu") {
			return nil
		}

		relPath, err := filepath.Rel(b.siteDir, path)
		if err != nil {
			return err
		}

		outPath := filepath.Join(b.distDir, strings.TrimSuffix(relPath, ".vyu")+".html")

		if err := os.MkdirAll(filepath.Dir(outPath), 0755); err != nil {
			return err
		}

		opts := compiler.CompileOptions{
			DevMode:     false,
			ManifestMap: manifestMap,
			SiteDir:     b.siteDir,
		}

		result, err := compiler.CompileFile(path, opts)
		if err != nil {
			return fmt.Errorf("failed to compile %s: %w", relPath, err)
		}

		if err := os.WriteFile(outPath, []byte(result.HTML), 0644); err != nil {
			return fmt.Errorf("failed to write %s: %w", outPath, err)
		}

		fmt.Printf("  ✓ %s → %s\n", relPath, strings.TrimSuffix(relPath, ".vyu")+".html")
		return nil
	})
}
