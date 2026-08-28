# GDShader LSP

![Build Status](https://github.com/scump1/gdshader-lsp-cpp/actions/workflows/build.yml/badge.svg)
![Test Status](https://github.com/scump1/gdshader-lsp-cpp/actions/workflows/tests.yml/badge.svg)

A high-performance, standalone **Language Server Protocol (LSP)** implementation for the Godot Shading Language (`.gdshader`), developed in C++20.

**GDShader LSP** provides enterprise-grade code intelligence, strict semantic analysis, and real-time diagnostics for Godot shaders. It is designed to be editor-agnostic, integrating seamlessly into VSCode, Neovim, Emacs, or directly into the Godot Engine via GDExtension.

For information on changes per version, see **CHANGELOG.md**.

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

## Compatibility & Versioning

The GDShader language evolves with every Godot Engine release (adding new built-ins, changing keywords, etc.). To ensure strict semantic accuracy, **GDShader LSP** releases are pinned to specific Godot versions.

We follow a **"Master is Latest"** strategy:
* The `master` branch always tracks the **latest stable** version of Godot.
* Previous Godot versions are supported via long-lived `support/` branches.

| LSP Version | Godot Version | Git Branch |
| :--- | :--- | :--- |
| **v0.3.x** *(Upcoming)* | **Godot 4.7** (incl. `blist`) | `master` |
| **v0.2.x** *(Current)* | **Godot 4.5 / 4.6** | `support/4.5+` |

*Note: The upcoming **Godot 4.7 `blist`** shader type is supported exclusively on `master`. If you are building from source, ensure you checkout the branch matching your project's Godot version.*

## Building from Source

This project uses **SCons** as its build system.

### Prerequisites

* **C++ Compiler**: GCC, Clang, or MSVC supporting **C++20**.
* **SCons**: Install via Python (`pip install scons`).
* **LSP build**: You need to build the LSP Framework library for your platform.

### Building LSP Framework

This project expects the **LSP Framework** to be located at `extern/lsp-framework`. You must compile the framework for your target platform and ensure the build artifacts are located in the specific directories expected by the `SConstruct` script:

* **Linux**: `extern/lsp-framework/build_linux` 
* **Windows**: `extern/lsp-framework/build_windows` 
* **macOS**: `extern/lsp-framework/build_macos` 

Ensure your directory structure looks like this before compiling the main project:

```text
extern/
└── lsp-framework/
    ├── build_linux/    # Contains liblsp.a for Linux
    ├── build_windows/  # Contains lsp.lib for Windows
    └── build_macos/    # Contains liblsp.a (Universal Binary) for macOS

```

#### Linux

```bash
cd extern/lsp-framework
cmake -B build_linux -DCMAKE_BUILD_TYPE=Release
cmake --build build_linux
```

#### Windows

Use this if you are compiling with MinGW (as expected by the SConstruct configuration).
```bash
cd extern/lsp-framework
cmake -B build_windows -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_windows
```

#### Macos

To support both Intel and Apple Silicon Macs, build a universal binary using the architectural flags.

```bash
cd extern/lsp-framework

cmake -B build_macos \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build_macos
```

### Compilation

To build the standalone LSP executable:

```bash
scons platform=<platform> target=<debug/release>

```

**Supported Platforms:**

* `linux`
* `windows`
* `macos`

The binary will be output to `bin/`.

## Configuration (VSCode Example)

There is a dedicated VSCode Extension available in the Extension store. You can take a look at the files [`here`](https://github.com/scump1/gdshader-lsp-client).

## Configuration (Neovim Example)

Using Neovim's native LSP client (0.11+), the server must be passed `--stdio`
explicitly — without it, it defaults to TCP mode and silently fails to attach
as a stdio-spawned client.

```lua
vim.lsp.config("gdshader_lsp", {
	cmd = {
		"/path/to/bin/gdshader-lsp-release",
		"--stdio",
	},
	filetypes = { "gdshader", "gdshaderinc" },
	root_markers = { "project.godot", ".git" },
})
vim.lsp.enable("gdshader_lsp")
```

## Roadmap

This project will be maintained for the foreseeable future. Godot and its developers are planning on extending the shader capabilities every other update, and we will need a good language server to go along!

## Contributing

Contributions are welcome! Please submit Pull Requests or open Issues to discuss planned features.