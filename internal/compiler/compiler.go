package compiler

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

type CompileOptions struct {
	DevMode     bool
	ViteDevURL  string
	ManifestMap map[string]string
	SiteDir     string
}

type CompileResult struct {
	HTML       string
	CSSImports []string
	JSImports  []string
}

func CompileFile(filePath string, opts CompileOptions) (*CompileResult, error) {
	content, err := os.ReadFile(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to read file %s: %w", filePath, err)
	}

	return Compile(string(content), filePath, opts)
}

func Compile(content string, filePath string, opts CompileOptions) (*CompileResult, error) {
	parsed, err := ParseVyu(content)
	if err != nil {
		return nil, fmt.Errorf("parse error in %s: %w", filePath, err)
	}

	rt := NewJSRuntime()

	if err := rt.RunDeclarations(parsed.Declarations); err != nil {
		return nil, fmt.Errorf("declarations error in %s: %w", filePath, err)
	}

	result := parsed.Body

	var cssLinks []string
	var jsScripts []string

	for i := len(parsed.Expressions) - 1; i >= 0; i-- {
		expr := parsed.Expressions[i]

		if path, ok := IsImportCSS(expr.Raw); ok {
			cssURL := resolveAssetURL(path, filePath, opts)
			cssLinks = append(cssLinks, fmt.Sprintf(`<link rel="stylesheet" href="%s">`, cssURL))
			result = replaceAt(result, expr.Start, expr.End, "")
			continue
		}

		if path, ok := IsImportJS(expr.Raw); ok {
			jsURL := resolveAssetURL(path, filePath, opts)
			jsScripts = append(jsScripts, fmt.Sprintf(`<script type="module" src="%s"></script>`, jsURL))
			result = replaceAt(result, expr.Start, expr.End, "")
			continue
		}

		evaluated, err := rt.Evaluate(expr.Raw)
		if err != nil {
			return nil, fmt.Errorf("evaluation error in %s: %w", filePath, err)
		}

		result = replaceAt(result, expr.Start, expr.End, evaluated)
	}

	result = injectAssets(result, cssLinks, jsScripts, opts.DevMode)

	return &CompileResult{
		HTML:       result,
		CSSImports: rt.GetCSSImports(),
		JSImports:  rt.GetJSImports(),
	}, nil
}

func replaceAt(s string, start, end int, replacement string) string {
	runes := []rune(s)
	if start < 0 || end > len(runes) || start > end {
		return s
	}
	return string(runes[:start]) + replacement + string(runes[end:])
}

func resolveAssetURL(path string, filePath string, opts CompileOptions) string {
	if strings.HasPrefix(path, "./") || strings.HasPrefix(path, "../") {
		dir := filepath.Dir(filePath)
		absPath := filepath.Join(dir, path)

		if opts.SiteDir != "" {
			relPath, err := filepath.Rel(opts.SiteDir, absPath)
			if err == nil {
				path = relPath
			}
		}
	}

	path = strings.TrimPrefix(path, "./")

	if opts.DevMode {
		return fmt.Sprintf("%s/%s", strings.TrimSuffix(opts.ViteDevURL, "/"), path)
	}

	if opts.ManifestMap != nil {
		if mapped, ok := opts.ManifestMap[path]; ok {
			return "/" + mapped
		}
	}

	return "/" + path
}

func injectAssets(html string, cssLinks, jsScripts []string, devMode bool) string {
	if len(cssLinks) == 0 && len(jsScripts) == 0 && !devMode {
		return html
	}

	var assetTags strings.Builder

	for _, link := range cssLinks {
		assetTags.WriteString("  ")
		assetTags.WriteString(link)
		assetTags.WriteString("\n")
	}

	for _, script := range jsScripts {
		assetTags.WriteString("  ")
		assetTags.WriteString(script)
		assetTags.WriteString("\n")
	}

	if devMode {
		assetTags.WriteString(`  <script>
    const es = new EventSource('/__vaayu_live_reload');
    es.onmessage = (e) => { if (e.data === 'reload') location.reload(); };
  </script>
`)
	}

	headCloseIdx := strings.Index(strings.ToLower(html), "</head>")
	if headCloseIdx != -1 {
		return html[:headCloseIdx] + assetTags.String() + html[headCloseIdx:]
	}

	bodyOpenIdx := strings.Index(strings.ToLower(html), "<body")
	if bodyOpenIdx != -1 {
		endOfBodyTag := strings.Index(html[bodyOpenIdx:], ">")
		if endOfBodyTag != -1 {
			insertIdx := bodyOpenIdx + endOfBodyTag + 1
			return html[:insertIdx] + "\n" + assetTags.String() + html[insertIdx:]
		}
	}

	return assetTags.String() + html
}

