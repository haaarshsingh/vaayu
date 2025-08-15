#!/bin/bash
set -e

# Find a free port
find_free_port() {
    python3 -c "
import socket
s = socket.socket()
s.bind(('', 0))
print(s.getsockname()[1])
s.close()
"
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[TEST]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Test function
run_test() {
    local test_name="$1"
    local test_cmd="$2"
    local expected_pattern="$3"
    
    log "Running: $test_name"
    
    if result=$(eval "$test_cmd" 2>&1); then
        if [[ -n "$expected_pattern" ]] && ! echo "$result" | grep -q "$expected_pattern"; then
            error "Test '$test_name' failed: expected pattern '$expected_pattern' not found in output"
            echo "Output was: $result"
            return 1
        else
            log "✓ $test_name passed"
            return 0
        fi
    else
        error "Test '$test_name' failed: command returned non-zero exit code"
        echo "Output was: $result"
        return 1
    fi
}

# Setup
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_BIN="$SCRIPT_DIR/../../examples/static_server/static_server"
WWW_DIR="$SCRIPT_DIR/../../examples/static_server/www"

# Check if server binary exists
if [[ ! -x "$SERVER_BIN" ]]; then
    error "Server binary not found at $SERVER_BIN"
    error "Please run: bazel build //examples/static_server:static_server"
    exit 1
fi

# Check if www directory exists
if [[ ! -d "$WWW_DIR" ]]; then
    error "www directory not found at $WWW_DIR"
    exit 1
fi

# Find free port
PORT=$(find_free_port)
log "Using port $PORT for testing"

# Start server in background
log "Starting server on port $PORT with docroot $WWW_DIR"
"$SERVER_BIN" -p "$PORT" -r "$WWW_DIR" &
SERVER_PID=$!

# Give server time to start
sleep 1

# Cleanup function
cleanup() {
    log "Cleaning up..."
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Check if server is running
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    error "Server failed to start"
    exit 1
fi

# Test 1: GET request for index.html
run_test "GET /index.html" \
    "curl -s -i http://localhost:$PORT/index.html" \
    "HTTP/1.0 200 OK"

# Test 2: GET request for root (should serve index.html)
run_test "GET / (index redirect)" \
    "curl -s -i http://localhost:$PORT/" \
    "HTTP/1.0 200 OK"

# Test 3: HEAD request
run_test "HEAD /index.html" \
    "curl -s -I http://localhost:$PORT/index.html | head -1" \
    "HTTP/1.0 200 OK"

# Test 4: Check required headers
run_test "Response headers check" \
    "curl -s -i http://localhost:$PORT/index.html" \
    "Server: tinyc/0.1"

run_test "Date header check" \
    "curl -s -i http://localhost:$PORT/index.html" \
    "Date:"

run_test "Content-Type header check" \
    "curl -s -i http://localhost:$PORT/index.html" \
    "Content-Type: text/html; charset=utf-8"

run_test "Content-Length header check" \
    "curl -s -i http://localhost:$PORT/index.html" \
    "Content-Length:"

run_test "Connection close header check" \
    "curl -s -i http://localhost:$PORT/index.html" \
    "Connection: close"

# Test 5: Different MIME types
run_test "CSS MIME type" \
    "curl -s -i http://localhost:$PORT/styles.css" \
    "Content-Type: text/css"

run_test "JSON MIME type" \
    "curl -s -i http://localhost:$PORT/data.json" \
    "Content-Type: application/json"

run_test "JavaScript MIME type" \
    "curl -s -i http://localhost:$PORT/script.js" \
    "Content-Type: application/javascript"

run_test "Plain text MIME type" \
    "curl -s -i http://localhost:$PORT/test.txt" \
    "Content-Type: text/plain; charset=utf-8"

# Test 6: Subdirectory access
run_test "Subdirectory access" \
    "curl -s -i http://localhost:$PORT/subdir/nested.html" \
    "HTTP/1.0 200 OK"

# Test 7: 404 for non-existent files
run_test "404 for missing file" \
    "curl -s -i http://localhost:$PORT/nonexistent.html" \
    "HTTP/1.0 404 Not Found"

# Test 8: Method not allowed (POST)
run_test "405 for POST method" \
    "curl -s -i -X POST http://localhost:$PORT/index.html" \
    "HTTP/1.0 405 Method Not Allowed"

run_test "Allow header for 405" \
    "curl -s -i -X POST http://localhost:$PORT/index.html" \
    "Allow: GET, HEAD"

# Test 9: Path traversal protection
run_test "Block .. traversal" \
    "curl -s -i http://localhost:$PORT/../../../etc/passwd" \
    "HTTP/1.0 404 Not Found"

run_test "Block encoded .. traversal" \
    "curl -s -i http://localhost:$PORT/%2e%2e/etc/passwd" \
    "HTTP/1.0 404 Not Found"

# Test 10: HEAD vs GET body difference
HEAD_RESPONSE=$(curl -s -I http://localhost:$PORT/test.txt)
GET_RESPONSE=$(curl -s -i http://localhost:$PORT/test.txt)

if echo "$HEAD_RESPONSE" | grep -q "HTTP/1.0 200 OK" && \
   echo "$GET_RESPONSE" | grep -q "This is a plain text file"; then
    log "✓ HEAD/GET body difference test passed"
else
    error "HEAD/GET body difference test failed"
    exit 1
fi

# Test 11: Bad request handling
run_test "400 for malformed request" \
    "echo -e 'INVALID REQUEST\\r\\n\\r\\n' | nc localhost $PORT" \
    "HTTP/1.0 400 Bad Request"

# Test 12: Query string stripping
run_test "Query string ignored" \
    "curl -s -i http://localhost:$PORT/index.html?param=value" \
    "HTTP/1.0 200 OK"

# Test 13: Connection closes after each request
if ! echo -e "GET /index.html HTTP/1.0\\r\\n\\r\\nGET /test.txt HTTP/1.0\\r\\n\\r\\n" | nc localhost $PORT | grep -q "Connection: close"; then
    error "Connection close test failed"
    exit 1
else
    log "✓ Connection close test passed"
fi

log "All smoke tests passed!"
exit 0
