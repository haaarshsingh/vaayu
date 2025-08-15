## Vaayu

A tiny, auditable HTTP/1.0 server in C designed for serving static files safely. Built with security and simplicity in mind.

⚠️ **Important**: This is a small HTTP server designed for personal use. Use at your own discretion.

### Development

#### Requirements

- Bazel (https://bazel.build/)
- C99 compiler (GCC or Clang)
- POSIX-compliant system (Linux, macOS)

#### Building

```bash
# Build all targets
bazel build //...

# Build just the server
bazel build //examples/static_server:static_server

# Run tests
bazel test //tests/smoke:smoke_test

# Or run smoke tests directly
cd tests/smoke && ./test_smoke.sh
```

#### Usage

```bash
# Basic usage (serves ./www on port 8080)
bazel run //examples/static_server:static_server

# Custom port and document root
bazel run //examples/static_server:static_server -- -p 3000 -r /path/to/files

# All options
bazel run //examples/static_server:static_server -- -h
```

Usage instructions:

- `-p PORT`: Listen port (default: 8080)
- `-r DOCROOT`: Document root directory (default: ./www)
- `-i INDEX`: Index file name (default: index.html)
- `-H BYTES`: Maximum header bytes (default: 16384)

Try these commands after starting the server:

```bash
# Basic GET request
curl -i http://localhost:8080/

# HEAD request (headers only)
curl -I http://localhost:8080/index.html

# Test different MIME types
curl -i http://localhost:8080/styles.css
curl -i http://localhost:8080/data.json

# Test 404
curl -i http://localhost:8080/nonexistent.txt

# Test 405 (method not allowed)
curl -i -X POST http://localhost:8080/
```
