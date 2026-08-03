# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - Upcoming v0.3.x (Godot 4.7)

### Added
- Support for the new **`blist`** shader type introduced in Godot 4.7 (implementation in progress)

## [0.2.5.2] - 2026-07-XX

### Added
- Handling for optional arguments (and updated the data layer accordingly)
- Test cases for all bugfixes

### Changed
- Varyings are not read-only internally, but instead validated at each point of assignment

### Fixed
- A bug...
  - Missing definition of clamp() in data layer
  - False positive "read-only variable" when assigning to varying in vertex()

## [0.2.5.1] - 2026-07-17

More code actions!

### Added
- Dedicated handler for code actions

- Simple Code Actions for:
  - Missing Semicolom, Colon
  - Missing Parenthes, Brackets, Braces
  - Invalid Argument Counts
  - Keywords 'break' or 'continue' outside of a loop
  - Invalid 'discard' usage
  - Void functions that return a value
  - Missing shader type declaration (with the two most commong options)
  - Unused variables (by commenting them out)

- Advanced Code Actions for:
  - Typo Corrections ("Did you mean ...")
  - Extracting magic numbers to uniforms
  - Auto-Generate function/variable stubs
  - Extract to functionr refactoring

- Test for all of the above

### Changed

### Fixed
- A couple edge cases in type casting confusion when resolving function arguments
- Early return bug in the type checking visitor for function arguments
- Missing parentheses around an if-statement condition or-clause with multiple arguments that caused bugs

## [0.2.5] - 2026-05-15

New features: Code formatting, cross-file symbol renaming, and more!

### Added 

- Features:
  - Cross-file symbol renaming
  - Code formatting
  - Workspace Symbol search
  - Call hierarchy
  - Primitive Code Actions (BETA)

- Fastly improved diagnostic error codes
- Improved support for windows named pipes as communication protocol

- Tests for all of the above

### Changed

- Standard port for the lps is now 6010

### Fixed

- Symbol store adding logic was faulty in edge cases for varyings & uniforms
- Wrong instantiation of local & global symbols in category "Variables" instead of "Builtin"
- Registering global functions & environment shader variables into the "builtin" file scope now
- Wrong registering of some symbols as function definition in symbol declaration visitor
- To narrow type checking in Textdocument_Rename
- Parser bug when initializing uniforms after hint_range with '= ...'
- Range identification for many nodes

## [0.2.4.1] - 2026-03-28

A patch for all the immediate bugfixes.

### Added 

- Test cases for all the fixed problems below

### Changed

- The default port for the lsp is now 6007

### Fixed

- Wrong global level built-ins registering for all shader types
- This lsp no longer overlaps with the default Godot language lsp port 6005

## [0.2.4] - 2026-03-28

Major improvements to performance, autocomplete, and stability. Closing the last few edge cases on syntax.

### Added

- Feature tests in the test suite & more cases to the test suite (100 diagnostic tests now)
- Semantic tokens are back

- Performance improvements
  - AST decoration used for frontend requests dimini
  - shing the amount of string manipulations needed
  - AST improvements for range tracking and lookup

### Changed

- Cleaned up the fragmented test files
- AST information is now better used in the frontend server requests

### Fixed

- Several bugs regarding wrong order of top-level parsing when using 'instance' and 'flat' markers
- Missing checks for structs containing themselves
- Several missing checks for array acces in nested arrays
- A missing range for DotNodes in MemberAccess resulting in broken hover tips
- Several misalignments of AST line indexes and ranges

## [0.2.3] - 2026-03-11

### Added

- Features
  - Implemented the double-visitor pattern using mutliple AST passes
    - Significantly improves performance, maintainability, and accuracy
  - Seperation of data and LSP: The builtin functions and similar are now seperated into JSON files that make maintainence simpler and efficient
    - The JSON files are baked into the binary for simpler deployment as c++ auto-generated header files using Scons

  - Inlay hints are now possible
  - Added render_mode from spatial type shaders and its errors

- More cases to the test suite
  - Testing for incremental change events and updates
  - render_mode
  - Increased diagnostic batch testing to 75 test cases

- Performance optimizations
  - Pratt-Parser implementation
  - The double-visitor pattern as explained above
  - Debouncing timer saving cpu cycles
  - Background threading so parsing does not block the network loop anymore

- Drastically improved logging

### Changed

- LSP now runs on incremental change events, which saves a lot of network overhead
- Symbols now have full range resolution

### Fixed

- Missing types in the typeRegistry for almost all Sampler types
- uniform hints in several cases caused major problems in the parser
- Minor bug in Sconstruct

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