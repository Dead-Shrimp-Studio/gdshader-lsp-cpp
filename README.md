# GDShader LSP

A high-performance, standalone **Language Server Protocol (LSP)** implementation for the Godot Shading Language (`.gdshader`), developed in C++20.

**GDShader LSP** provides enterprise-grade code intelligence, strict semantic analysis, and real-time diagnostics for Godot shaders. It is designed to be editor-agnostic, integrating seamlessly into VSCode, Neovim, Emacs, or directly into the Godot Engine via GDExtension.

## Key Features

### Advanced Static Analysis & Type Checking

Unlike basic syntax highlighters, GDShader LSP performs full semantic analysis of your shader code:

* **Strict Type Validation:** comprehensive checking for vector swizzling, matrix operations, and implicit type conversions.
* **Scope & Shadowing:** Accurate detection of variable shadowing, scope leakage, and uninitialized variables.
* **Control Flow Analysis:** Detection of unreachable code, missing return statements, and invalid discard usage in specific processor stages.
* **Constructor Validation:** Overload resolution for functions and type constructors (e.g., `vec3`, `mat4`).

### Modular Preprocessor Support

Full support for complex project structures using an AST-aware preprocessor:

* **`#include` Resolution:** Resolves dependencies via a Virtual File System, supporting both relative paths and Godot `res://` paths.
* **Cross-File Symbol Linking:** Functions, structs, and constants defined in included files are fully resolved, typed, and available for autocompletion.
* **Macro Support:** Handles `#define`, `#ifdef`, `#elif`, and `#endif` branching, ensuring that only active code paths are analyzed.

### LSP Capabilities

* **Code Completion:** Context-aware suggestions for built-ins, struct members, vector swizzles (`.xyz`), and user-defined symbols.
* **Signature Help:** Parameter hints and overload information for built-in and user functions.
* **Hover Documentation:** Type information and documentation for symbols.
* **Go to Definition:** Jump to variables, structs, and included files.
* **Document Symbols:** hierarchical outline view of the shader structure.
* **Semantic Highlighting:** precise token coloring based on symbol roles (uniforms, consts, varyings).

## Architecture & Performance

GDShader LSP is built for speed and low latency.

* **Virtual File System:** The server maintains a "Split-Brain" architecture, prioritizing in-memory dirty buffers from the editor while lazily loading dependencies from disk.
* **Performance Metrics:** Internal stress tests on 5,000+ line shader files with deep include hierarchies demonstrate a full analysis pipeline (Client → Server → Client) latency of **< 25ms**.
* **Memory Management:** The architecture utilizes standard C++ allocators efficiently. Given that shader translation units are typically small, this approach provides the optimal balance between code maintainability and runtime performance without the overhead of complex arena allocators.

## Building from Source

This project uses **SCons** as its build system.

### Prerequisites

* **C++ Compiler**: GCC, Clang, or MSVC supporting **C++20**.
* **SCons**: Install via Python (`pip install scons`).

### Compilation

To build the standalone LSP executable:

```bash
scons platform=<platform>

```

**Supported Platforms:**

* `linux`
* `windows`
* `macos`

The binary will be output to `bin/`.

## Configuration (VSCode Example)

To use this with VSCode, you can use a generic LSP client extension (like *glsl-canvas* or a custom configuration).

```json
{
    "godot_shader_lsp.serverPath": "/path/to/bin/gdshader-lsp",
    "godot_shader_lsp.trace.server": "verbose"
}

```

*Note: A dedicated VSCode extension wrapper is NOT planned. Feel free to implement one!*

## Roadmap

This project is currently in **Alpha**. While the core analysis engine is robust, the following features are planned:

* [ ] **Code Actions:** Quick fixes for common errors (e.g., "Did you mean...?").
* [ ] **Formatting:** AST-based code formatting (`textDocument/formatting`).
* [ ] **Rename Refactoring:** Project-wide symbol renaming.
* [ ] **Find References:** Locate all usages of a symbol across the project.

## Contributing

Contributions are welcome! Please submit Pull Requests or open Issues to discuss planned features.

## License

Copyright 2026 5cump1

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
