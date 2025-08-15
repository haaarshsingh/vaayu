## Vaayu

A tiny, auditable HTTP/1.0 static file server in C.

### Development

#### Requirements

- Bazel (https://bazel.build/)
- C99 compiler (GCC or Clang)
- POSIX-compliant system (Linux, macOS)

#### Building

```bash
# Build all targets
bazel build //...

# Build just server
bazel build //examples/static_server:static_server

# Run tests
bazel test //tests/smoke:smoke_test

# Or run smoke tests directly
cd tests/smoke && ./test_smoke.sh
```

#### Usage

```bash
# Basic usage
bazel run //examples/static_server:static_server

# Custom port and document root
bazel run //examples/static_server:static_server -- -p 3000 -r /path/to/files

# All
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

# HEAD
curl -I http://localhost:8080/index.html

# Test different MIME types
curl -i http://localhost:8080/styles.css
curl -i http://localhost:8080/data.json

# Test 404
curl -i http://localhost:8080/nonexistent.txt

# Test 405
curl -i -X POST http://localhost:8080/
```
