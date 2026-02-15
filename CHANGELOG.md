# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.2] - 2026-02-15

### Added

- Dedicated test environment using Github Actions

- Description on how to build the LSP framework to correctly compile the Server using the scons
- Detailed documentation for some builtin functions (not all yet)
- Missing group_uniforms support (including nested structures)

### Changed

- In code documentation

### Fixed

- Shader type behaviour:
  - Included sky shaders in shader scope
  - Sky shaders now correctly have acces to their builtin variables
  - Default behaviour of shader files that are missing the shader_type declaration no longer is spatial shader

- Parser
  - Const declarations in parseStatement are correctly detected now

- Semantic Analyzer
  - Binary operations with bools now correctly report an error

## [0.2.1] - 2025-12-01

Inital release!