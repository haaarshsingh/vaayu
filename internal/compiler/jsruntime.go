package compiler

import (
	"encoding/json"
	"fmt"

	"github.com/dop251/goja"
)

type JSRuntime struct {
	vm         *goja.Runtime
	cssImports []string
	jsImports  []string
}

func NewJSRuntime() *JSRuntime {
	vm := goja.New()
	rt := &JSRuntime{
		vm:         vm,
		cssImports: make([]string, 0),
		jsImports:  make([]string, 0),
	}

	vm.Set("importCSS", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) > 0 {
			path := call.Arguments[0].String()
			rt.cssImports = append(rt.cssImports, path)
		}
		return goja.Undefined()
	})

	vm.Set("importJS", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) > 0 {
			path := call.Arguments[0].String()
			rt.jsImports = append(rt.jsImports, path)
		}
		return goja.Undefined()
	})

	return rt
}

func (r *JSRuntime) RunDeclarations(code string) error {
	if code == "" {
		return nil
	}
	_, err := r.vm.RunString(code)
	if err != nil {
		return fmt.Errorf("error executing declarations: %w", err)
	}
	return nil
}

func (r *JSRuntime) Evaluate(expr string) (string, error) {
	val, err := r.vm.RunString(expr)
	if err != nil {
		return "", fmt.Errorf("error evaluating expression '%s': %w", expr, err)
	}
	return stringify(val), nil
}

func (r *JSRuntime) GetCSSImports() []string {
	return r.cssImports
}

func (r *JSRuntime) GetJSImports() []string {
	return r.jsImports
}

func stringify(val goja.Value) string {
	if val == nil || goja.IsUndefined(val) || goja.IsNull(val) {
		return ""
	}

	export := val.Export()
	if export == nil {
		return ""
	}

	switch v := export.(type) {
	case string:
		return v
	case int64:
		return fmt.Sprintf("%d", v)
	case float64:
		if v == float64(int64(v)) {
			return fmt.Sprintf("%d", int64(v))
		}
		return fmt.Sprintf("%g", v)
	case bool:
		if v {
			return "true"
		}
		return "false"
	default:
		bytes, err := json.Marshal(v)
		if err != nil {
			return fmt.Sprintf("%v", v)
		}
		return string(bytes)
	}
}

