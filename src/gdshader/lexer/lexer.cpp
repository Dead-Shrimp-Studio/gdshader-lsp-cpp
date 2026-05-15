
#include "gdshader/lexer/lexer.h"
#include "utils/logger.hpp"

#include <cctype>
#include <unordered_map>

using namespace gdshader_lsp;

Lexer::Lexer(const std::string &source_code) : source(source_code), current_pos(0) 
{
    GDSHADER_WARN_IF(source.empty(), "Lexer initialized with an empty source string.");
    if (!source.empty()) {
        source_len = source.length();
        current_char = source.at(current_pos);
    } else {
        current_char = '\0'; // Null terminator for EOF
    }
}

/**
 * @brief Advances the current character pointer.
 */
void Lexer::advance() 
{
    GDSHADER_ASSERT(current_pos <= source_len, "Lexer attempted to advance past EOF!");
    if (current_char == '\n') {
        line++;
        column = 0;
    } else {
        column++;
    }

    current_pos++;
    if (current_pos < source_len) {
        current_char = source.at(current_pos);
    } else {
        current_char = '\0'; // Set to null terminator at EOF
    }
}

/**
 * @brief Skips Godot based comments.
 * 
 */
Token Lexer::parseComment(int startLine, int startCol) 
{
    std::string comment_text;

    GDSHADER_ASSERT(current_char == '/', "skipComment called but current character is '{}'", current_char);
    if (current_char == '/' && peek() == '/') {
        // Single-line comment: Skip until newline
        while (current_char != '\n' && current_char != '\0') {
            advance();
        }
    } 
    else if (current_char == '/' && peek() == '*') {
        // Multi-line comment
        comment_text += current_char; advance(); // '/'
        comment_text += current_char; advance(); // '*'
        
        while (current_char != '\0') {
            if (current_char == '*' && peek() == '/') {
                comment_text += current_char; advance(); // '*'
                comment_text += current_char; advance(); // '/'
                break;
            }
            comment_text += current_char;
            advance();
        }
    }

    return {TokenType::TOKEN_COMMENT, comment_text, startLine, startCol, (int)comment_text.length()};
}

/**
 * @brief Peeks at the next character without advancing.
 * @return The next character, or null terminator if at EOF.
 */
char Lexer::peek() {

    if (current_pos + 1 < source_len) {
        return source.at(current_pos + 1);
    }
    return '\0'; // Null terminator for EOF
}

/**
 * @brief Skips over all whitespace characters.
 */
void Lexer::skipWhitespace() {

    while (current_pos < source_len && isspace(current_char)) {
        advance();
    }
}

/**
 * @brief Parses a number token.
 * @return The number token.
 */
Token Lexer::parseNumber(int startLine, int startCol)
{
    GDSHADER_ASSERT(isdigit(current_char) || current_char == '.', "parseNumber called on invalid starting character: '{}'", current_char);

    std::string number_str;
    while (current_pos < source_len && (isdigit(current_char) || current_char == '.')) {
        number_str += current_char;
        advance();

    }

    if (number_str.empty()) {
        number_str = "0";
    }

    return {TokenType::TOKEN_NUMBER, number_str, startLine, startCol, (int)number_str.length()};

}

/**
 * @brief Parses a string token.
 * @return The string token.
 */
Token Lexer::parseString(int startLine, int startCol) 
{
    GDSHADER_ASSERT(current_char == '"', "parseString called without starting quote, found: '{}'", current_char);

    std::string str_val;
    advance(); // Consume the starting double quote

    while (current_pos < source_len && current_char != '"') {
       
        if (current_char == '\\' && peek() == '"') {
            str_val += '"';
            advance();
            advance();
            continue;
        }

        str_val += current_char;
        advance();
    }
    advance(); // Consume the ending double quote
    return {TokenType::TOKEN_STRING, str_val, startLine, startCol, (int)str_val.length()};
}

/**
 * @brief Parses an identifier or a keyword.
 * @return The appropriate token.
 */
Token Lexer::parseIdentifier(int startLine, int startCol) 
{
    GDSHADER_ASSERT(isalpha(current_char) || current_char == '_', "parseIdentifier started with invalid character: '{}'", current_char);

    std::string result;
    while (current_pos < source_len && (isalnum(current_char) || current_char == '_')) {
        result += current_char;
        advance();
    }

    static const std::unordered_map<std::string, TokenType> keywords = {
        // Godot

        {"shader_type", TokenType::KEYWORD_SHADER_TYPE},
        {"render_mode", TokenType::KEYWORD_RENDER_MODE},
        {"group_uniforms", TokenType::KEYWORD_GROUP_UNIFORMS},
        
        // Qualifiers

        {"uniform", TokenType::KEYWORD_UNIFORM},
        {"varying", TokenType::KEYWORD_VARYING},
        {"const", TokenType::KEYWORD_CONST},
        {"in", TokenType::KEYWORD_IN},
        {"out", TokenType::KEYWORD_OUT},
        {"inout", TokenType::KEYWORD_INOUT},
        {"flat", TokenType::KEYWORD_FLAT},
        {"smooth", TokenType::KEYWORD_SMOOTH},
        {"instance", TokenType::KEYWORD_INSTANCE},
        
        // Types

        {"void", TokenType::KEYWORD_VOID},
        {"bool", TokenType::KEYWORD_BOOL},
        {"int", TokenType::KEYWORD_INT},
        {"uint", TokenType::KEYWORD_UINT},
        {"float", TokenType::KEYWORD_FLOAT},
        {"vec2", TokenType::KEYWORD_VEC2},
        {"vec3", TokenType::KEYWORD_VEC3},
        {"vec4", TokenType::KEYWORD_VEC4},
        {"ivec2", TokenType::KEYWORD_IVEC2},
        {"ivec3", TokenType::KEYWORD_IVEC3},
        {"ivec4", TokenType::KEYWORD_IVEC4},
        {"uvec4", TokenType::KEYWORD_UVEC4},
        {"uvec4", TokenType::KEYWORD_UVEC4},
        {"uvec4", TokenType::KEYWORD_UVEC4},
        {"bvec4", TokenType::KEYWORD_BVEC4},
        {"bvec4", TokenType::KEYWORD_BVEC4},
        {"bvec4", TokenType::KEYWORD_BVEC4},
        {"mat2", TokenType::KEYWORD_MAT2},
        {"mat3", TokenType::KEYWORD_MAT3},
        {"mat4", TokenType::KEYWORD_MAT4},
        {"sampler2D", TokenType::KEYWORD_SAMPLER2D},
        {"sampler2DArray", TokenType::KEYWORD_SAMPLER2DARRAY},
        {"sampler3D", TokenType::KEYWORD_SAMPLER3D},
        {"samplerCube", TokenType::KEYWORD_SAMPLERCUBE},
        {"isampler2d", TokenType::KEYWORD_ISAMPLER2D},
        {"usampler2d", TokenType::KEYWORD_USAMPLER2D},
        
        // Control

        {"if", TokenType::KEYWORD_IF},
        {"else", TokenType::KEYWORD_ELSE},
        {"for", TokenType::KEYWORD_FOR},
        {"do", TokenType::KEYWORD_DO},
        {"while", TokenType::KEYWORD_WHILE},
        {"return", TokenType::KEYWORD_RETURN},
        {"switch", TokenType::KEYWORD_SWITCH},
        {"case", TokenType::KEYWORD_CASE},
        {"default", TokenType::KEYWORD_DEFAULT},
        {"break", TokenType::KEYWORD_BREAK},
        {"continue", TokenType::KEYWORD_CONTINUE},
        {"discard", TokenType::KEYWORD_DISCARD},
        {"struct", TokenType::KEYWORD_STRUCT},
        {"true", TokenType::KEYWORD_TRUE},
        {"false", TokenType::KEYWORD_FALSE}
    };

    auto it = keywords.find(result);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::TOKEN_IDENTIFIER;
    return {type, result, startLine, startCol, (int)result.length()};
}

Token Lexer::createToken() 
{    
    skipWhitespace();

    int startLine = line;
    int startCol = column;
    
    auto traceToken = [&](TokenType type, std::string text) {
        SPDLOG_TRACE("Token: '{}' ({}:{})", text, startLine, startCol);
        return Token{type, text, startLine, startCol, (int)text.length()};
    };

    if (current_pos >= source_len) {
        SPDLOG_TRACE("Lexer hit EOF.");
        return {TokenType::TOKEN_EOF, "", startLine, startCol, {}};
    }

    char current = current_char;

    // --- COMMENTS ---
    if (current_char == '/') {
        char next = peek();
        if (next == '/' || next == '*') {
            Token t = parseComment(startLine, startCol);
            SPDLOG_TRACE("Token: [Comment] '{}' ({}:{})", t.value, startLine, startCol);
            return t;
        }
    }

    // --- PREPROCESSOR ---
    if (current == '#') {
        advance();
        return traceToken(TokenType::TOKEN_PREPROCESSOR, "#");
    }

    // --- DOT ACCESS ---
    if (current == '.') {
        advance();
        return traceToken(TokenType::TOKEN_DOT, ".");
    }

    // --- NUMBERS ---
    if (isdigit(current)) {
        Token t = parseNumber(startLine, startCol);
        SPDLOG_TRACE("Token: [Number] '{}' ({}:{})", t.value, startLine, startCol);
        return t;
    }

    // --- IDENTIFIERS / KEYWORDS ---
    if (isalpha(current) || current == '_') {
        Token t = parseIdentifier(startLine, startCol);
        // Note: You might want to distinguish Keywords vs IDs in the log if you like
        SPDLOG_TRACE("Token: [ID/Key] '{}' ({}:{})", t.value, startLine, startCol);
        return t;
    }
    
    // --- STRINGS ---
    if (current == '"') {
        Token t = parseString(startLine, startCol);
        SPDLOG_TRACE("Token: [String] '{}' ({}:{})", t.value, startLine, startCol);
        return t;
    }

    // --- OPERATORS (Compound vs Single) ---
    
    if (current == '+') { 
        if (peek() == '+') { advance(); advance(); return traceToken(TokenType::TOKEN_PLUS_PLUS, "++"); }
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_PLUS_EQUAL, "+="); }
        advance(); return traceToken(TokenType::TOKEN_PLUS, "+"); 
    }
    if (current == '-') { 
        if (peek() == '-') { advance(); advance(); return traceToken(TokenType::TOKEN_MINUS_MINUS, "--"); }
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_MINUS_EQUAL, "-="); }
        advance(); return traceToken(TokenType::TOKEN_MINUS, "-"); 
    }

    if (current == '*') { 
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_STAR_EQUAL, "*="); }
        advance(); return traceToken(TokenType::TOKEN_STAR, "*"); 
    }
    if (current == '/') { 
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_SLASH_EQUAL, "/="); }
        advance(); return traceToken(TokenType::TOKEN_SLASH, "/"); 
    }
    if (current == '%') { 
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_PERCENT_EQUAL, "%="); }
        advance(); return traceToken(TokenType::TOKEN_PERCENT, "%"); 
    }

    if (current == '&') { 
        if (peek() == '&') { advance(); advance(); return traceToken(TokenType::TOKEN_AND, "&&"); }
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_AMPERSAND_EQUAL, "&="); }
        advance(); return traceToken(TokenType::TOKEN_AMPERSAND, "&"); 
    }

    if (current == '|') { 
        if (peek() == '|') { advance(); advance(); return traceToken(TokenType::TOKEN_OR, "||"); }
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_PIPE_EQUAL, "|="); }
        advance(); return traceToken(TokenType::TOKEN_PIPE, "|"); 
    }

    if (current == '^') { 
        if (peek() == '^') { advance(); advance(); return traceToken(TokenType::TOKEN_CARET_CARET, "^^"); }
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_CARET_EQUAL, "^="); }
        advance(); return traceToken(TokenType::TOKEN_CARET, "^"); 
    }

    if (current == '<') {
        if (peek() == '<') { 
            advance(); 
            if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_LESS_LESS_EQUAL, "<<="); }
            advance(); return traceToken(TokenType::TOKEN_LESS_LESS, "<<"); 
        }
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_LESS_EQ, "<="); }
        advance(); return traceToken(TokenType::TOKEN_LESS, "<");
    }

    if (current == '>') {
        if (peek() == '>') { 
            advance(); 
            if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_GREATER_GREATER_EQUAL, ">>="); }
            advance(); return traceToken(TokenType::TOKEN_GREATER_GREATER, ">>"); 
        }
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_GREATER_EQ, ">="); }
        advance(); return traceToken(TokenType::TOKEN_GREATER, ">");
    }

    if (current == '=') {
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_EQ_EQ, "=="); }
        advance(); return traceToken(TokenType::TOKEN_EQUAL, "=");
    }

    if (current == '!') {
        if (peek() == '=') { advance(); advance(); return traceToken(TokenType::TOKEN_NOT_EQ, "!="); }
        advance(); return traceToken(TokenType::TOKEN_EXCL, "!");
    }

    // --- SINGLE CHARACTERS ---
    // Using the lambda makes this section much cleaner than copy-pasting lines
    
    if (current == ':') { advance(); return traceToken(TokenType::TOKEN_COLON, ":"); }
    if (current == ';') { advance(); return traceToken(TokenType::TOKEN_SEMI, ";"); }
    if (current == '(') { advance(); return traceToken(TokenType::TOKEN_LPAREN, "("); }
    if (current == ')') { advance(); return traceToken(TokenType::TOKEN_RPAREN, ")"); }
    if (current == '{') { advance(); return traceToken(TokenType::TOKEN_LBRACE, "{"); }
    if (current == '}') { advance(); return traceToken(TokenType::TOKEN_RBRACE, "}"); }
    if (current == '[') { advance(); return traceToken(TokenType::TOKEN_LBRACKET, "["); }
    if (current == ']') { advance(); return traceToken(TokenType::TOKEN_RBRACKET, "]"); }
    if (current == ',') { advance(); return traceToken(TokenType::TOKEN_COMMA, ","); }
    if (current == '?') { advance(); return traceToken(TokenType::TOKEN_QUESTION, "?"); }
    if (current == '~') { advance(); return traceToken(TokenType::TOKEN_TILDE, "~"); }

    // Fallback
    advance();

    SPDLOG_ERROR("Lexer Error: Unexpected character '{}' (0x{:x}) at {}:{}", current, (int)current, startLine, startCol);
    spdlog::dump_backtrace();

    return {TokenType::TOKEN_ERROR, std::string(1, current), startLine, startCol, 1};
}

/**
 * @brief Provides access to the next token. This is the main lexing function.
 * @return The next token.
 */
Token Lexer::getNextToken() 
{
    if(!peek_buffer.empty()) {

        Token t = peek_buffer.front();
        peek_buffer.pop_front();

        return t;
    }
    return createToken();
}

/**
 * @brief Lets us peek a number of tokens ahead. The usual token peek (offset = 0) is the next token, (offset = 1) would mean the token after the next (2 ahead).
 * 
 * @param offset 
 * @return Token 
 */
Token Lexer::peekToken(unsigned int offset) 
{
    GDSHADER_WARN_IF(offset > 50, "Lexer peeking unusually far ahead (offset: {} tokens)", offset);
    
    if (offset < peek_buffer.size()) {
        return peek_buffer[offset];
    }

    for (size_t i = peek_buffer.size(); i <= offset; ++i) {
        peek_buffer.push_back(createToken());
    }

    return peek_buffer[offset];

}