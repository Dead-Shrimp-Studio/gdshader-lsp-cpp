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

inline std::string tokenTypeToString(TokenType t) {
    switch (t) {

        // ---------------------------------------------------------------------
        // BASIC
        // ---------------------------------------------------------------------
        case TokenType::TOKEN_EOF:            return "TOKEN_EOF";
        case TokenType::TOKEN_ERROR:          return "TOKEN_ERROR";
        case TokenType::TOKEN_IDENTIFIER:     return "TOKEN_IDENTIFIER";
        case TokenType::TOKEN_NUMBER:         return "TOKEN_NUMBER";
        case TokenType::TOKEN_STRING:         return "TOKEN_STRING";
        case TokenType::TOKEN_PREPROCESSOR:   return "TOKEN_PREPROCESSOR";

        // ---------------------------------------------------------------------
        // OPERATORS & PUNCTUATION
        // ---------------------------------------------------------------------
        case TokenType::TOKEN_PLUS:            return "TOKEN_PLUS";
        case TokenType::TOKEN_MINUS:           return "TOKEN_MINUS";
        case TokenType::TOKEN_STAR:            return "TOKEN_STAR";
        case TokenType::TOKEN_SLASH:           return "TOKEN_SLASH";
        case TokenType::TOKEN_PERCENT:         return "TOKEN_PERCENT";
        case TokenType::TOKEN_PLUS_EQUAL:      return "TOKEN_PLUS_EQUAL";
        case TokenType::TOKEN_MINUS_EQUAL:     return "TOKEN_MINUS_EQUAL";
        case TokenType::TOKEN_STAR_EQUAL:      return "TOKEN_STAR_EQUAL";
        case TokenType::TOKEN_SLASH_EQUAL:     return "TOKEN_SLASH_EQUAL";
        case TokenType::TOKEN_PERCENT_EQUAL:   return "TOKEN_PERCENT_EQUAL";
        case TokenType::TOKEN_EQUAL:           return "TOKEN_EQUAL";
        case TokenType::TOKEN_EQ_EQ:           return "TOKEN_EQ_EQ";
        case TokenType::TOKEN_NOT_EQ:          return "TOKEN_NOT_EQ";
        case TokenType::TOKEN_LESS:            return "TOKEN_LESS";
        case TokenType::TOKEN_LESS_EQ:         return "TOKEN_LESS_EQ";
        case TokenType::TOKEN_GREATER:         return "TOKEN_GREATER";
        case TokenType::TOKEN_GREATER_EQ:      return "TOKEN_GREATER_EQ";
        case TokenType::TOKEN_AND:             return "TOKEN_AND";
        case TokenType::TOKEN_OR:              return "TOKEN_OR";
        case TokenType::TOKEN_AMPERSAND:       return "TOKEN_AMPERSAND";
        case TokenType::TOKEN_PIPE:            return "TOKEN_PIPE";
        case TokenType::TOKEN_CARET:           return "TOKEN_CARET";
        case TokenType::TOKEN_TILDE:           return "TOKEN_TILDE";

        case TokenType::TOKEN_DOT:             return "TOKEN_DOT";
        case TokenType::TOKEN_COMMA:           return "TOKEN_COMMA";
        case TokenType::TOKEN_COLON:           return "TOKEN_COLON";
        case TokenType::TOKEN_SEMI:            return "TOKEN_SEMI";
        case TokenType::TOKEN_QUESTION:        return "TOKEN_QUESTION";
        case TokenType::TOKEN_EXCL:            return "TOKEN_EXCL";

        case TokenType::TOKEN_LPAREN:          return "TOKEN_LPAREN";
        case TokenType::TOKEN_RPAREN:          return "TOKEN_RPAREN";
        case TokenType::TOKEN_LBRACE:          return "TOKEN_LBRACE";
        case TokenType::TOKEN_RBRACE:          return "TOKEN_RBRACE";
        case TokenType::TOKEN_LBRACKET:        return "TOKEN_LBRACKET";
        case TokenType::TOKEN_RBRACKET:        return "TOKEN_RBRACKET";

        // ---------------------------------------------------------------------
        // KEYWORDS: GODOT SPECIFIC
        // ---------------------------------------------------------------------
        case TokenType::KEYWORD_SHADER_TYPE:   return "KEYWORD_SHADER_TYPE";
        case TokenType::KEYWORD_RENDER_MODE:   return "KEYWORD_RENDER_MODE";
        case TokenType::KEYWORD_GROUP_UNIFORMS:return "KEYWORD_GROUP_UNIFORMS";

        // ---------------------------------------------------------------------
        // KEYWORDS: GLSL STORAGE & QUALIFIERS
        // ---------------------------------------------------------------------
        case TokenType::KEYWORD_UNIFORM:        return "KEYWORD_UNIFORM";
        case TokenType::KEYWORD_VARYING:        return "KEYWORD_VARYING";
        case TokenType::KEYWORD_CONST:          return "KEYWORD_CONST";
        case TokenType::KEYWORD_IN:             return "KEYWORD_IN";
        case TokenType::KEYWORD_OUT:            return "KEYWORD_OUT";
        case TokenType::KEYWORD_INOUT:          return "KEYWORD_INOUT";
        case TokenType::KEYWORD_FLAT:           return "KEYWORD_FLAT";
        case TokenType::KEYWORD_SMOOTH:         return "KEYWORD_SMOOTH";
        case TokenType::KEYWORD_INSTANCE:       return "KEYWORD_INSTANCE";
        case TokenType::KEYWORD_HIGH_PRECISION: return "KEYWORD_HIGH_PRECISION";

        // ---------------------------------------------------------------------
        // KEYWORDS: TYPES
        // ---------------------------------------------------------------------
        case TokenType::KEYWORD_VOID:           return "KEYWORD_VOID";
        case TokenType::KEYWORD_BOOL:           return "KEYWORD_BOOL";
        case TokenType::KEYWORD_INT:            return "KEYWORD_INT";
        case TokenType::KEYWORD_UINT:           return "KEYWORD_UINT";
        case TokenType::KEYWORD_FLOAT:          return "KEYWORD_FLOAT";
        case TokenType::KEYWORD_VEC2:           return "KEYWORD_VEC2";
        case TokenType::KEYWORD_VEC3:           return "KEYWORD_VEC3";
        case TokenType::KEYWORD_VEC4:           return "KEYWORD_VEC4";
        case TokenType::KEYWORD_IVEC2:          return "KEYWORD_IVEC2";
        case TokenType::KEYWORD_IVEC3:          return "KEYWORD_IVEC3";
        case TokenType::KEYWORD_IVEC4:          return "KEYWORD_IVEC4";
        case TokenType::KEYWORD_UVEC2:          return "KEYWORD_UVEC2";
        case TokenType::KEYWORD_UVEC3:          return "KEYWORD_UVEC3";
        case TokenType::KEYWORD_UVEC4:          return "KEYWORD_UVEC4";
        case TokenType::KEYWORD_BVEC2:          return "KEYWORD_BVEC2";
        case TokenType::KEYWORD_BVEC3:          return "KEYWORD_BVEC3";
        case TokenType::KEYWORD_BVEC4:          return "KEYWORD_BVEC4";
        case TokenType::KEYWORD_MAT2:           return "KEYWORD_MAT2";
        case TokenType::KEYWORD_MAT3:           return "KEYWORD_MAT3";
        case TokenType::KEYWORD_MAT4:           return "KEYWORD_MAT4";
        case TokenType::KEYWORD_SAMPLER2D:      return "KEYWORD_SAMPLER2D";
        case TokenType::KEYWORD_ISAMPLER2D:     return "KEYWORD_ISAMPLER2D";
        case TokenType::KEYWORD_USAMPLER2D:     return "KEYWORD_USAMPLER2D";
        case TokenType::KEYWORD_SAMPLER3D:      return "KEYWORD_SAMPLER3D";
        case TokenType::KEYWORD_SAMPLERCUBE:    return "KEYWORD_SAMPLERCUBE";
        case TokenType::KEYWORD_SAMPLER2DARRAY: return "KEYWORD_SAMPLER2DARRAY";

        // ---------------------------------------------------------------------
        // KEYWORDS: CONTROL FLOW
        // ---------------------------------------------------------------------
        case TokenType::KEYWORD_IF:             return "KEYWORD_IF";
        case TokenType::KEYWORD_ELSE:           return "KEYWORD_ELSE";
        case TokenType::KEYWORD_FOR:            return "KEYWORD_FOR";
        case TokenType::KEYWORD_WHILE:          return "KEYWORD_WHILE";
        case TokenType::KEYWORD_DO:             return "KEYWORD_DO";
        case TokenType::KEYWORD_SWITCH:         return "KEYWORD_SWITCH";
        case TokenType::KEYWORD_CASE:           return "KEYWORD_CASE";
        case TokenType::KEYWORD_DEFAULT:        return "KEYWORD_DEFAULT";
        case TokenType::KEYWORD_BREAK:          return "KEYWORD_BREAK";
        case TokenType::KEYWORD_CONTINUE:       return "KEYWORD_CONTINUE";
        case TokenType::KEYWORD_RETURN:         return "KEYWORD_RETURN";
        case TokenType::KEYWORD_DISCARD:        return "KEYWORD_DISCARD";
        case TokenType::KEYWORD_STRUCT:         return "KEYWORD_STRUCT";
        case TokenType::KEYWORD_TRUE:           return "KEYWORD_TRUE";
        case TokenType::KEYWORD_FALSE:          return "KEYWORD_FALSE";
    }

    return "UNKNOWN_TOKEN";
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