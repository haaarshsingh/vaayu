![image](https://vaayu.lol/og.png)

<p align="center">
  <a href="#">
    <h2 align="center">Vaayu</h2>
  </a>
</p>

<p align="center">Tiny (<5kB), zero-config static site generator</p>

### Installation

#### Prerequisites

- Go 1.22 or later
- Node.js 18+ and npm

#### Install from source

```bash
# Clone the repo
git clone https://github.com/harshsingh/vaayu.git
cd vaayu

go mod download

# Install Vite dependencies
cd vite && npm install && cd ..

# Build the CLI
go build -o vaayu ./cmd/vaayu
```

### Quick Start

```bash
# Start the dev server
./vaayu dev

# Build for prod
./vaayu build
```

The dev server runs at `http://localhost:3000` by default.

### `.vyu` File Syntax

A `.vyu` file consists of two parts:

1. **Declarations Block** (optional): JavaScript code at the top of the file, wrapped in `{{ }}`
2. **HTML Template Body**: HTML with embedded `{{ expression }}` placeholders

#### Basic Example

```html
{{ const title = 'Hello, World!'; const items = ['Apple', 'Banana', 'Cherry'];
}}
<!DOCTYPE html>
<html>
  <head>
    <title>{{ title }}</title>
  </head>
  <body>
    <h1>{{ title }}</h1>
    <ul>
      {{ items.map(item => `
      <li>${item}</li>
      `).join('') }}
    </ul>
  </body>
</html>
```

#### Output

```html
<!DOCTYPE html>
<html>
  <head>
    <title>Hello, World!</title>
  </head>
  <body>
    <h1>Hello, World!</h1>
    <ul>
      <li>Apple</li>
      <li>Banana</li>
      <li>Cherry</li>
    </ul>
  </body>
</html>
```

### Template Expressions

Expressions inside `{{ }}` are evaluated as JavaScript and their results are inserted into the HTML:

| Expression Type    | Result                |
| ------------------ | --------------------- |
| String             | Inserted as-is        |
| Number             | Converted to string   |
| Boolean            | `"true"` or `"false"` |
| `null`/`undefined` | Empty string          |
| Object/Array       | `JSON.stringify()`    |

#### Examples

```html
{{ "Hello" }} → Hello {{ 42 }} → 42 {{ 3.14159 }} → 3.14159 {{ true }} → true {{
null }} → (empty) {{ { a: 1, b: 2 } }} → {"a":1,"b":2} {{ [1, 2, 3] }} → [1,2,3]
```

### Asset Imports

Use special helper functions to import CSS and JavaScript assets:

```html
{{ importCSS("./styles.css") }} {{ importJS("./main.ts") }}
```

#### Development Mode

In dev mode, assets are served from Vite's dev server with HMR support:

```html
<link rel="stylesheet" href="http://localhost:5173/styles.css" />
<script type="module" src="http://localhost:5173/main.ts"></script>
```

#### Production Build

In production, assets are bundled by Vite with hashed filenames:

```html
<link rel="stylesheet" href="/assets/styles-abc123.css" />
<script type="module" src="/assets/main-def456.js"></script>
```

### CLI Commands

#### `vaayu dev`

Start the development server with live reload.

```bash
vaayu dev [flags]

Flags:
  -p, --port int    Dev server port (default 3000)
      --site string Source directory (default "site")
      --vite string Vite config directory (default "vite")
```

#### `vaayu build`

Build the site for production.

```bash
vaayu build [flags]

Flags:
      --site string Source directory (default "site")
      --dist string Output directory (default "dist")
      --vite string Vite config directory (default "vite")
```

### Routing

Files in `site/` are mapped to URLs:

| Source File          | URL          | Output File           |
| -------------------- | ------------ | --------------------- |
| `site/index.vyu`     | `/`          | `dist/index.html`     |
| `site/about.vyu`     | `/about`     | `dist/about.html`     |
| `site/blog/post.vyu` | `/blog/post` | `dist/blog/post.html` |

### Development

#### Running Tests

```bash
go test ./...
```

#### Building for Different Platforms

```bash
# macOS
GOOS=darwin GOARCH=amd64 go build -o vaayu-darwin ./cmd/vaayu

# Linux
GOOS=linux GOARCH=amd64 go build -o vaayu-linux ./cmd/vaayu

# Windows
GOOS=windows GOARCH=amd64 go build -o vaayu.exe ./cmd/vaayu
```
