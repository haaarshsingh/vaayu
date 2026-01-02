package devserver

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/harshsingh/vaayu/internal/compiler"
	"github.com/harshsingh/vaayu/internal/vite"
)

type Server struct {
	siteDir    string
	viteDir    string
	port       int
	viteDevURL string
	watcher    *Watcher
	viteDev    *vite.DevServer
	clients    map[chan struct{}]struct{}
	clientsMu  sync.RWMutex
	server     *http.Server
}

func New(siteDir, viteDir string, port int) *Server {
	return &Server{
		siteDir:    siteDir,
		viteDir:    viteDir,
		port:       port,
		viteDevURL: "http://localhost:5173",
		clients:    make(map[chan struct{}]struct{}),
	}
}

func (s *Server) Start(ctx context.Context) error {
	if err := vite.EnsureInstalled(s.viteDir); err != nil {
		return fmt.Errorf("failed to install vite dependencies: %w", err)
	}

	s.viteDev = vite.NewDevServer(s.viteDir)
	if err := s.viteDev.Start(ctx); err != nil {
		log.Printf("Warning: Vite dev server failed to start: %v", err)
	}

	watcher, err := NewWatcher(s.siteDir, s.notifyClients)
	if err != nil {
		return fmt.Errorf("failed to create watcher: %w", err)
	}
	s.watcher = watcher

	if err := s.watcher.Start(); err != nil {
		return fmt.Errorf("failed to start watcher: %w", err)
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/__vaayu_live_reload", s.handleSSE)
	mux.HandleFunc("/", s.handleRequest)

	s.server = &http.Server{
		Addr:    fmt.Sprintf(":%d", s.port),
		Handler: mux,
	}

	fmt.Printf("\n  Vaayu dev server running at:\n")
	fmt.Printf("  → http://localhost:%d\n\n", s.port)

	go func() {
		<-ctx.Done()
		s.Stop()
	}()

	if err := s.server.ListenAndServe(); err != http.ErrServerClosed {
		return err
	}
	return nil
}

func (s *Server) Stop() {
	if s.watcher != nil {
		s.watcher.Stop()
	}
	if s.viteDev != nil {
		s.viteDev.Stop()
	}
	if s.server != nil {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		s.server.Shutdown(ctx)
	}
}

func (s *Server) handleSSE(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "SSE not supported", http.StatusInternalServerError)
		return
	}

	ch := make(chan struct{}, 1)
	s.clientsMu.Lock()
	s.clients[ch] = struct{}{}
	s.clientsMu.Unlock()

	defer func() {
		s.clientsMu.Lock()
		delete(s.clients, ch)
		s.clientsMu.Unlock()
	}()

	fmt.Fprintf(w, "data: connected\n\n")
	flusher.Flush()

	for {
		select {
		case <-r.Context().Done():
			return
		case <-ch:
			fmt.Fprintf(w, "data: reload\n\n")
			flusher.Flush()
		}
	}
}

func (s *Server) notifyClients() {
	s.clientsMu.RLock()
	defer s.clientsMu.RUnlock()

	for ch := range s.clients {
		select {
		case ch <- struct{}{}:
		default:
		}
	}
}

func (s *Server) handleRequest(w http.ResponseWriter, r *http.Request) {
	path := r.URL.Path
	if path == "/" {
		path = "/index"
	}

	path = strings.TrimPrefix(path, "/")
	path = strings.TrimSuffix(path, ".html")

	vyuPath := filepath.Join(s.siteDir, path+".vyu")

	if _, err := os.Stat(vyuPath); os.IsNotExist(err) {
		staticPath := filepath.Join(s.siteDir, r.URL.Path)
		if info, err := os.Stat(staticPath); err == nil && !info.IsDir() {
			http.ServeFile(w, r, staticPath)
			return
		}

		http.NotFound(w, r)
		return
	}

	opts := compiler.CompileOptions{
		DevMode:    true,
		ViteDevURL: s.viteDevURL,
		SiteDir:    s.siteDir,
	}

	result, err := compiler.CompileFile(vyuPath, opts)
	if err != nil {
		log.Printf("Compile error: %v", err)
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprintf(w, `<!DOCTYPE html>
<html>
<head><title>Compile Error</title></head>
<body>
<h1>Compile Error</h1>
<pre style="color: red; background: #fee; padding: 1em; border-radius: 4px;">%s</pre>
<script>
const es = new EventSource('/__vaayu_live_reload');
es.onmessage = () => location.reload();
</script>
</body>
</html>`, err.Error())
		return
	}

	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Write([]byte(result.HTML))
}
