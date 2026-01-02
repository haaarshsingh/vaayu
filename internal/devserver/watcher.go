package devserver

import (
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/fsnotify/fsnotify"
)

type Watcher struct {
	watcher       *fsnotify.Watcher
	watchDir      string
	onChange      func()
	done          chan struct{}
	debounceTimer *time.Timer
	debounceMu    sync.Mutex
}

func NewWatcher(watchDir string, onChange func()) (*Watcher, error) {
	w, err := fsnotify.NewWatcher()
	if err != nil {
		return nil, err
	}

	watcher := &Watcher{
		watcher:  w,
		watchDir: watchDir,
		onChange: onChange,
		done:     make(chan struct{}),
	}

	return watcher, nil
}

func (w *Watcher) Start() error {
	if err := w.addDirRecursive(w.watchDir); err != nil {
		return err
	}

	go w.watch()
	return nil
}

func (w *Watcher) Stop() {
	close(w.done)
	w.watcher.Close()
}

func (w *Watcher) addDirRecursive(dir string) error {
	return filepath.Walk(dir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() {
			return w.watcher.Add(path)
		}
		return nil
	})
}

func (w *Watcher) watch() {
	for {
		select {
		case <-w.done:
			return
		case event, ok := <-w.watcher.Events:
			if !ok {
				return
			}
			if event.Op&(fsnotify.Write|fsnotify.Create|fsnotify.Remove) != 0 {
				ext := filepath.Ext(event.Name)
				if ext == ".vyu" || ext == ".css" || ext == ".ts" || ext == ".js" {
					log.Printf("File changed: %s", event.Name)
					w.debounce()
				}
			}
		case err, ok := <-w.watcher.Errors:
			if !ok {
				return
			}
			log.Printf("Watcher error: %v", err)
		}
	}
}

func (w *Watcher) debounce() {
	w.debounceMu.Lock()
	defer w.debounceMu.Unlock()

	if w.debounceTimer != nil {
		w.debounceTimer.Stop()
	}

	w.debounceTimer = time.AfterFunc(100*time.Millisecond, w.onChange)
}
