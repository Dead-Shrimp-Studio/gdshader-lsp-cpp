#ifndef LEXER_TYPES_H
#define LEXER_TYPES_H

#include <string>

namespace gdshader_lsp {

enum class TokenType {
    // -------------------------------------------------------------------------
    // BASIC
    // -------------------------------------------------------------------------
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,     
    TOKEN_STRING,     
    TOKEN_PREPROCESSOR,

    // -------------------------------------------------------------------------
    // OPERATORS & PUNCTUATION
    // -------------------------------------------------------------------------
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_PLUS_EQUAL,   // +=
    TOKEN_MINUS_EQUAL,  // -=
    TOKEN_STAR_EQUAL,   // *=
    TOKEN_SLASH_EQUAL,  // /=
    TOKEN_PERCENT_EQUAL,// %=
    TOKEN_EQUAL,        // =
    TOKEN_EQ_EQ,        // ==
    TOKEN_NOT_EQ,       // !=
    TOKEN_LESS,         // <
    TOKEN_LESS_EQ,      // <=
    TOKEN_GREATER,      // >
    TOKEN_GREATER_EQ,   // >=
    TOKEN_AND,          // &&
    TOKEN_OR,           // ||
    TOKEN_AMPERSAND,    // &
    TOKEN_PIPE,         // |
    TOKEN_CARET,        // ^
    TOKEN_TILDE,        // ~
    
    TOKEN_DOT,          // .
    TOKEN_COMMA,        // ,
    TOKEN_COLON,        // :
    TOKEN_SEMI,         // ;
    TOKEN_QUESTION,     // ?
    TOKEN_EXCL,         // !

    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]

    // -------------------------------------------------------------------------
    // KEYWORDS: GODOT SPECIFIC
    // -------------------------------------------------------------------------
    KEYWORD_SHADER_TYPE,
    KEYWORD_RENDER_MODE,
    KEYWORD_GROUP_UNIFORMS,

    // -------------------------------------------------------------------------
    // KEYWORDS: GLSL STORAGE & QUALIFIERS
    // -------------------------------------------------------------------------
    KEYWORD_UNIFORM,
    KEYWORD_VARYING,
    KEYWORD_CONST,
    KEYWORD_IN,
    KEYWORD_OUT,
    KEYWORD_INOUT,
    KEYWORD_FLAT,
    KEYWORD_SMOOTH,
    KEYWORD_INSTANCE,
    KEYWORD_HIGH_PRECISION, // highp (optional, Godot handles mostly implicitly but good to have)

    // -------------------------------------------------------------------------
    // KEYWORDS: TYPES
    // -------------------------------------------------------------------------
    KEYWORD_VOID,
    KEYWORD_BOOL,
    KEYWORD_INT,
    KEYWORD_UINT,
    KEYWORD_FLOAT,
    KEYWORD_VEC2,
    KEYWORD_VEC3,
    KEYWORD_VEC4,
    KEYWORD_IVEC2,
    KEYWORD_IVEC3,
    KEYWORD_IVEC4,
    KEYWORD_UVEC2,
    KEYWORD_UVEC3,
    KEYWORD_UVEC4,
    KEYWORD_BVEC2,
    KEYWORD_BVEC3,
    KEYWORD_BVEC4,
    KEYWORD_MAT2,
    KEYWORD_MAT3,
    KEYWORD_MAT4,
    KEYWORD_SAMPLER2D,
    KEYWORD_ISAMPLER2D,
    KEYWORD_USAMPLER2D,
    KEYWORD_SAMPLER3D,
    KEYWORD_SAMPLERCUBE,
    KEYWORD_SAMPLER2DARRAY,

    // -------------------------------------------------------------------------
    // KEYWORDS: CONTROL FLOW
    // -------------------------------------------------------------------------
    KEYWORD_IF,
    KEYWORD_ELSE,
    KEYWORD_FOR,
    KEYWORD_WHILE,
    KEYWORD_DO,
    KEYWORD_SWITCH,
    KEYWORD_CASE,
    KEYWORD_DEFAULT,
    KEYWORD_BREAK,
    KEYWORD_CONTINUE,
    KEYWORD_RETURN,
    KEYWORD_DISCARD, // <-- Shader specific
    KEYWORD_STRUCT,
    KEYWORD_TRUE,
    KEYWORD_FALSE
};

// Helper for debug printing (Optional but recommended)
inline std::string tokenTypeToString(TokenType t) {
    switch(t) {
        case TokenType::TOKEN_EOF: return "EOF";
        case TokenType::TOKEN_IDENTIFIER: return "IDENTIFIER";
        // ... add others as needed for your debug logs
        default: return "TOKEN";
    }
}

struct Token {
    TokenType type;
    std::string value;

    int line;
    int column;
    int length;

    std::string toString() const {
        return "Token(" + std::to_string((int)type) + ", '" + value + "' @" 
               + std::to_string(line) + ":" + std::to_string(column) + ")";
    }

};

}

#endif // LEXER_TYPES_H