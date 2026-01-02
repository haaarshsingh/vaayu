package vite

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"time"
)

type DevServer struct {
	cmd     *exec.Cmd
	viteDir string
	mu      sync.Mutex
	running bool
}

func NewDevServer(viteDir string) *DevServer {
	return &DevServer{
		viteDir: viteDir,
	}
}

func (s *DevServer) Start(ctx context.Context) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.running {
		return nil
	}

	s.cmd = exec.CommandContext(ctx, "npm", "run", "dev")
	s.cmd.Dir = s.viteDir
	s.cmd.Stdout = os.Stdout
	s.cmd.Stderr = os.Stderr

	if err := s.cmd.Start(); err != nil {
		return fmt.Errorf("failed to start vite dev server: %w", err)
	}

	s.running = true

	time.Sleep(2 * time.Second)

	return nil
}

func (s *DevServer) Stop() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if !s.running || s.cmd == nil || s.cmd.Process == nil {
		return nil
	}

	if err := s.cmd.Process.Signal(os.Interrupt); err != nil {
		s.cmd.Process.Kill()
	}

	s.running = false
	return nil
}

func Build(viteDir, outDir string) error {
	if err := os.MkdirAll(outDir, 0755); err != nil {
		return fmt.Errorf("failed to create output directory: %w", err)
	}

	cmd := exec.Command("npm", "run", "build")
	cmd.Dir = viteDir
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("vite build failed: %w", err)
	}

	return nil
}

func EnsureInstalled(viteDir string) error {
	nodeModules := filepath.Join(viteDir, "node_modules")
	if _, err := os.Stat(nodeModules); os.IsNotExist(err) {
		fmt.Println("Installing Vite dependencies...")
		cmd := exec.Command("npm", "install")
		cmd.Dir = viteDir
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("npm install failed: %w", err)
		}
	}
	return nil
}

