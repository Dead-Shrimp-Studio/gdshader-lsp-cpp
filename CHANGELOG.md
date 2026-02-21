# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.3] - Unreleased

### Added

- Features

  - Seperation of data and LSP: The builtin functions and similar are now seperated into JSON files that make maintainence simpler and efficient
    - The JSON files are baked into the binary for simpler deployment as c++ auto-generated header files using Scons

  - Inlay hints are now possible

- More cases to the test suite
  - Testing for incremental change events and updates

- Performance optimizations
  - Debouncing timer saving cpu cycles
  - Background threading so parsing does not block the network loop anymore

### Changed

- LSP now runs on incremental change events, which saves a lot of network overhead

### Fixed

## [0.2.2] - 2026-02-15

### Added

- Dedicated test environment using Github Actions

- Description on how to build the LSP framework to correctly compile the Server using the scons
- Detailed documentation for some builtin functions (not all yet)

- Sematics
  - Missing group_uniforms support (including nested structures)
  - Detection for recursion calls and appropiate errors

### Changed

- Symbol Table
  - Symbol lookups have a scope depth paremeter now
  - Added functionality to lookup all symbols across all scopes
  - Symbols now seperate symbol type and mutability. This resolves a lot of problems with tracking builtins and godots "in" or "out" parameters

### Fixed

- Shader type behaviour:
  - Included sky shaders in shader scope
  - Sky shaders now correctly have acces to their builtin variables
  - Default behaviour of shader files that are missing the shader_type declaration no longer is spatial shader

- Parser
  - Const declarations in parseStatement are correctly detected now

- Semantic Analyzer
  - Const evaluation of builtins and user declared symbols
  - Binary operations with bools now correctly report an error
  - Scalar constructor checks are not skippe anymoren through logic error
  - Segfaulting on RootSymbol acces for swizzles and array due to unchecked nullptr return

## [0.2.1] - 2025-12-01

Inital release!