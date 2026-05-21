#ifndef DIAGNOSTICS_HPP
#define DIAGNOSTICS_HPP

#include <string>

namespace gdshader_lsp
{

/**
 * @brief Standardized diagnostic codes for the GDShader language server.
 * Ranges are partitioned by analysis phase:
 * 1000 - 1999: Lexical and Parsing Errors
 * 2000 - 2999: Semantic and Type Checking Errors
 * 3000 - 3999: Shader-Specific Constraints
 * 4000 - 4999: Warnings
 * 5000 - 5999: Hints & Suggestions
 * 6000 - 6999: Information
 */
enum class DiagnosticCode 
{
    Unknown                 = 1,

    // --- Lexical & Parsing Errors (1000 - 1999) ---
    UnexpectedToken         = 1000,
    MissingSemicolon        = 1001, // consume(..., "Expected ';'")
    UnmatchedParen          = 1002, // consume(..., "Expected ')'")
    UnmatchedBracket        = 1003, // consume(..., "Expected ']'")
    UnmatchedBrace          = 1004, // consume(..., "Expected '}'")
    ExpectedExpression      = 1005, // "Expected expression."
    ExpectedIdentifier      = 1006, // "Expected identifier..."
    ExpectedTypeSpecifier   = 1007, // "Expected type specifier."
    InvalidArraySize        = 1008, // "Array size must be..."
    ExpectedColon           = 1009, // consume(..., "Expected ':'")
    ExpectedInitialization  = 1010, // "Const requires initialization"
    
    // --- Preprocessor Errors (1100 - 1199) ---
    ExpectedStringLiteral   = 1100, // "#include requires string literal"
    OrphanedElif            = 1101, // "#elif without #if"
    OrphanedElse            = 1102, // "#else without #if"
    OrphanedEndif           = 1103, // "#endif without #if"
    IncludeNotFound         = 1104,
    MacroRedefinition       = 1105,

    // --- Semantic & Type Errors (2000 - 2999) ---
    UndefinedIdentifier     = 2000,
    TypeMismatch            = 2001,
    InvalidOperation        = 2002,
    MissingReturnValue      = 2003,
    NotAllPathsReturn       = 2004,
    BreakOutsideLoop        = 2005,
    ContinueOutsideLoop     = 2006,
    MissingExecutionBranch  = 2007,
    SymbolRedefinition      = 2008,
    UnknownType             = 2009,
    ConditionMustBeBool         = 2010,
    SwitchExpressionMustBeInt   = 2011,
    VoidCannotReturnValue       = 2012,
    NotAssignable               = 2013,
    CannotAssignToReadOnly      = 2014,
    DivisionByZero              = 2015,
    BitwiseRequiresInteger      = 2016,
    UnknownFunction             = 2017,
    AmbiguousFunctionCall       = 2018,
    InvalidArgumentCount        = 2019,
    InvalidArgumentType         = 2020,
    CannotConstructType         = 2021, // For opaque, void, or string attempts
    ConstructorArgumentMismatch = 2022,
    NotIndexable                = 2023,
    InvalidArrayIndex           = 2024,
    InvalidMemberAccess         = 2025,
    InvalidSwizzle              = 2026,

    // --- Shader-Specific Constraints (3000 - 3999) ---
    ExpectedShaderType          = 3000, // "Expected shader type identifier"
    ExpectedRenderMode          = 3001, // "Expected render mode identifier"
    InvalidTopLevelDecl         = 3002, // "Unexpected token at top level"
    MissingUniformKeyword       = 3003, // "Expected 'uniform' after 'instance'"
    MissingVaryingKeyword       = 3004, // "Expected 'varying' after 'flat'"
    InvalidHintRange            = 3005, // "hint_range min > max"
    ProcessorMustReturnVoid     = 3006,
    ProcessorCannotHaveArgs     = 3007,
    RecursionNotAllowed         = 3008,
    InvalidDiscardUsage         = 3010,
    LocalSamplerNotAllowed      = 3011,
    MissingOrUnknownShaderType  = 3012,
    InvalidRenderModeScope      = 3013,
    UnknownRenderMode           = 3014,
    InvalidInstanceUniformType  = 3015,
    IntegerVaryingNeedsFlat     = 3016,
    VaryingReadOnlyInFragment   = 3017,

    // --- Warnings (4000 - 4999) ---
    UnusedVariable          = 4000,
    DeprecatedFeature       = 4001,
    UnreachableCode         = 4002,
    VariableShadowed        = 4003,

    // --- Hints (5000 - 5999) ---
    EmptyStatement          = 5000,
    
    // --- Information (6000 - 6999) ---
    IgnoredPrecision        = 6000
};

inline std::string diagnostic_code_to_string(DiagnosticCode code) 
{
    // A buffer of 8 characters is sufficient for "GDS" + 4 digits + null terminator.
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "GDS%04d", static_cast<int>(code));
    
    return std::string(buffer);
}

struct Range 
{
    int startLine = 0;
    int startCol = 0;
    int endLine = 0;
    int endCol = 0;
};

enum class DiagnosticLevel 
{
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

struct Diagnostic 
{
    std::string code;
    std::string source = "gdshader"; // Usually static per lsp
    std::string message;
    
    Range range;
    DiagnosticLevel level = DiagnosticLevel::Error;

    Diagnostic() = default;
    Diagnostic(DiagnosticCode errCode, const std::string& msg, DiagnosticLevel lvl = DiagnosticLevel::Error)
        : code(diagnostic_code_to_string(errCode)), message(msg), level(lvl) {}
};

} // namespace gdshader_lsp


#endif