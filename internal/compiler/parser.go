package compiler

import (
	"fmt"
	"strings"
	"unicode"
)

type ParsedVyu struct {
	Declarations string
	Body         string
	Expressions  []Expression
}

type Expression struct {
	Raw      string
	Start    int
	End      int
	FullText string
}

func ParseVyu(content string) (*ParsedVyu, error) {
	result := &ParsedVyu{}

	trimmed := strings.TrimLeftFunc(content, unicode.IsSpace)
	prefixLen := len(content) - len(trimmed)

	if strings.HasPrefix(trimmed, "{{") {
		declEnd := findMatchingClose(trimmed, 2)
		if declEnd == -1 {
			return nil, fmt.Errorf("unclosed declarations block")
		}

		result.Declarations = strings.TrimSpace(trimmed[2:declEnd])
		result.Body = trimmed[declEnd+2:]
	} else {
		result.Declarations = ""
		result.Body = content[prefixLen:]
	}

	expressions, err := findExpressions(result.Body)
	if err != nil {
		return nil, err
	}
	result.Expressions = expressions

	return result, nil
}

func findMatchingClose(s string, start int) int {
	depth := 1
	i := start
	inString := false
	stringChar := rune(0)

	runes := []rune(s)
	for i < len(runes) {
		ch := runes[i]

		if inString {
			if ch == '\\' && i+1 < len(runes) {
				i += 2
				continue
			}
			if ch == stringChar {
				inString = false
			}
			i++
			continue
		}

		if ch == '"' || ch == '\'' || ch == '`' {
			inString = true
			stringChar = ch
			i++
			continue
		}

		if ch == '{' && i+1 < len(runes) && runes[i+1] == '{' {
			depth++
			i += 2
			continue
		}

		if ch == '}' && i+1 < len(runes) && runes[i+1] == '}' {
			depth--
			if depth == 0 {
				return i
			}
			i += 2
			continue
		}

		i++
	}

	return -1
}

func findExpressions(body string) ([]Expression, error) {
	var expressions []Expression
	runes := []rune(body)
	i := 0

	for i < len(runes) {
		if runes[i] == '{' && i+1 < len(runes) && runes[i+1] == '{' {
			start := i
			closeIdx := findMatchingClose(string(runes), i+2)
			if closeIdx == -1 {
				return nil, fmt.Errorf("unclosed expression starting at position %d", i)
			}

			exprContent := strings.TrimSpace(string(runes[i+2 : closeIdx]))
			fullText := string(runes[start : closeIdx+2])

			expressions = append(expressions, Expression{
				Raw:      exprContent,
				Start:    start,
				End:      closeIdx + 2,
				FullText: fullText,
			})

			i = closeIdx + 2
		} else {
			i++
		}
	}

	return expressions, nil
}

func IsImportCSS(expr string) (string, bool) {
	expr = strings.TrimSpace(expr)
	if strings.HasPrefix(expr, "importCSS(") && strings.HasSuffix(expr, ")") {
		inner := strings.TrimPrefix(expr, "importCSS(")
		inner = strings.TrimSuffix(inner, ")")
		inner = strings.TrimSpace(inner)

		if (strings.HasPrefix(inner, "\"") && strings.HasSuffix(inner, "\"")) ||
			(strings.HasPrefix(inner, "'") && strings.HasSuffix(inner, "'")) {
			return inner[1 : len(inner)-1], true
		}
	}
	return "", false
}

func IsImportJS(expr string) (string, bool) {
	expr = strings.TrimSpace(expr)
	if strings.HasPrefix(expr, "importJS(") && strings.HasSuffix(expr, ")") {
		inner := strings.TrimPrefix(expr, "importJS(")
		inner = strings.TrimSuffix(inner, ")")
		inner = strings.TrimSpace(inner)

		if (strings.HasPrefix(inner, "\"") && strings.HasSuffix(inner, "\"")) ||
			(strings.HasPrefix(inner, "'") && strings.HasSuffix(inner, "'")) {
			return inner[1 : len(inner)-1], true
		}
	}
	return "", false
}

