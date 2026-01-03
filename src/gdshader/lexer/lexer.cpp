
#include "gdshader/lexer/lexer.h"

#include <cctype>
#include <unordered_map>

using namespace gdshader_lsp;

Lexer::Lexer(const std::string &source_code) 
    : source(source_code), current_pos(0) 
{
    
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
void Lexer::advance() {

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
void Lexer::skipComment() {
    if (current_char == '/' && peek() == '/') {
        // Single-line comment: Skip until newline
        while (current_char != '\n' && current_char != '\0') {
            advance();
        }
    } 
    else if (current_char == '/' && peek() == '*') {
        // Multi-line comment: Skip until */
        advance(); advance(); // consume /*
        while (current_char != '\0') {
            if (current_char == '*' && peek() == '/') {
                advance(); advance(); // consume */
                break;
            }
            advance();
        }
    }
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
Token Lexer::parseNumber(int startLine, int startCol) {

    std::string number_str;
    while (current_pos < source_len && (isdigit(current_char) || current_char == '.')) {
        number_str += current_char;
        advance();

    }

    if (number_str.empty()) {
        number_str = "0";
    }

    return {TokenType::TOKEN_NUMBER, number_str, startLine, startCol};

}

/**
 * @brief Parses a string token.
 * @return The string token.
 */
Token Lexer::parseString(int startLine, int startCol) {

    std::string str_val;
    advance(); // Consume the starting double quote

    while (current_pos < source_len && current_char != '"') {
        str_val += current_char;
        advance();
    }
    advance(); // Consume the ending double quote
    return {TokenType::TOKEN_STRING, str_val, startLine, startCol};
}

/**
 * @brief Parses an identifier or a keyword.
 * @return The appropriate token.
 */
Token Lexer::parseIdentifier(int startLine, int startCol) {

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
        {"mat3", TokenType::KEYWORD_MAT3},
        {"mat4", TokenType::KEYWORD_MAT4},
        {"sampler2D", TokenType::KEYWORD_SAMPLER2D},
        
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
        {"discard", TokenType::KEYWORD_DISCARD},
        {"struct", TokenType::KEYWORD_STRUCT},
        {"true", TokenType::KEYWORD_TRUE},
        {"false", TokenType::KEYWORD_FALSE}
    };

    auto it = keywords.find(result);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::TOKEN_IDENTIFIER;
    return {type, result, startLine, startCol};
}

Token Lexer::createToken() {
    
    skipWhitespace();

    bool commentFound = true;
    while (commentFound) {
        commentFound = false;

        if (current_char == '/') {
            char next = peek();
            if (next == '/' || next == '*') {
                skipComment();
                skipWhitespace(); 
                commentFound = true; // We moved, so check again
            }
        }
    }

    int startLine = line;
    int startCol = column;
    // int len = current_pos - startPos;

    if (current_pos >= source_len) {
        return {TokenType::TOKEN_EOF, "", startLine, startCol};
    }

    char current = current_char;

    // Handle Preprocessor (#include, #define)
    // For LSP, we might just treat the whole line as a preprocessor token
    // or just the '#' so the parser handles the rest.
    if (current == '#') {
        advance();
        return {TokenType::TOKEN_PREPROCESSOR, "#", startLine, startCol};
    }

    // Handle Dot (.) access (e.g. vec.x)
    if (current == '.') {
        advance();
        return {TokenType::TOKEN_DOT, ".", startLine, startCol};
    }

    if (isdigit(current)) {
        return parseNumber(startLine, startCol);
    }
    if (isalpha(current) || current == '_') {
        return parseIdentifier(startLine, startCol);
    }
    
    if (current == '"') {
        return parseString(startLine, startCol);
    }

    if (current == '+') { 
        if (peek() == '=') { advance(); advance(); return {TokenType::TOKEN_PLUS_EQUAL, "+=", startLine, startCol}; }
        advance(); return {TokenType::TOKEN_PLUS, "+", startLine, startCol}; 
    }
    if (current == '-') { 
        if (peek() == '=') { advance(); advance(); return {TokenType::TOKEN_MINUS_EQUAL, "-=", startLine, startCol}; }
        advance(); return {TokenType::TOKEN_MINUS, "-", startLine, startCol}; 
    }
    if (current == '*') { 
        if (peek() == '=') { advance(); advance(); return {TokenType::TOKEN_STAR_EQUAL, "*=", startLine, startCol}; }
        advance(); return {TokenType::TOKEN_STAR, "*", startLine, startCol}; 
    }
    if (current == '/') { 
        if (peek() == '=') { advance(); advance(); return {TokenType::TOKEN_SLASH_EQUAL, "/=", startLine, startCol}; }
        advance(); return {TokenType::TOKEN_SLASH, "/", startLine, startCol}; 
    }
    if (current == '%') { 
        if (peek() == '=') { advance(); advance(); return {TokenType::TOKEN_PERCENT_EQUAL, "%=", startLine, startCol}; }
        advance(); return {TokenType::TOKEN_PERCENT, "%", startLine, startCol}; 
    }

    if (current == '&') { 
        if (peek() == '&') { 
            advance(); advance(); 
            return {TokenType::TOKEN_AND, "&&", startLine, startCol}; 
        }
        advance(); 
        return {TokenType::TOKEN_AMPERSAND, "&", startLine, startCol}; 
    }

    if (current == '|') { 
        if (peek() == '|') { 
            advance(); advance(); 
            return {TokenType::TOKEN_OR, "||", startLine, startCol}; 
        }
        advance(); 
        return {TokenType::TOKEN_PIPE, "|", startLine, startCol}; 
    }

    if (current == '<') {
        if (peek() == '=') {
            advance(); advance();
            return {TokenType::TOKEN_LESS_EQ, "<=", startLine, startCol};
        }
        advance();
        return {TokenType::TOKEN_LESS, "<", startLine, startCol};
    }

    // Handle Greater Than (>) and Greater Equal (>=)
    if (current == '>') {
        if (peek() == '=') {
            advance(); advance();
            return {TokenType::TOKEN_GREATER_EQ, ">=", startLine, startCol};
        }
        advance();
        return {TokenType::TOKEN_GREATER, ">", startLine, startCol};
    }

    // Handle Equal (=) and Equal Equal (==)
    if (current == '=') {
        if (peek() == '=') {
            advance(); advance();
            return {TokenType::TOKEN_EQ_EQ, "==", startLine, startCol};
        }
        advance();
        return {TokenType::TOKEN_EQUAL, "=", startLine, startCol};
    }

    // Handle Not (!) and Not Equal (!=)
    if (current == '!') {
        if (peek() == '=') {
            advance(); advance();
            return {TokenType::TOKEN_NOT_EQ, "!=", startLine, startCol};
        }
        advance();
        return {TokenType::TOKEN_EXCL, "!", startLine, startCol};
    }

    // Handle single-character tokens.
    if (current == ':') { advance(); return {TokenType::TOKEN_COLON, ":", startLine, startCol}; }
    if (current == ';') { advance(); return {TokenType::TOKEN_SEMI, ";", startLine, startCol}; }
    if (current == '(') { advance(); return {TokenType::TOKEN_LPAREN, "(", startLine, startCol}; }
    if (current == ')') { advance(); return {TokenType::TOKEN_RPAREN, ")", startLine, startCol}; }
    if (current == '{') { advance(); return {TokenType::TOKEN_LBRACE, "{", startLine, startCol}; }
    if (current == '}') { advance(); return {TokenType::TOKEN_RBRACE, "}", startLine, startCol}; }
    if (current == '[') { advance(); return {TokenType::TOKEN_LBRACKET, "[", startLine, startCol}; }
    if (current == ']') { advance(); return {TokenType::TOKEN_RBRACKET, "]", startLine, startCol}; }
    if (current == ',') { advance(); return {TokenType::TOKEN_COMMA, ",", startLine, startCol}; }
    if (current == '?') { advance(); return {TokenType::TOKEN_QUESTION, "?", startLine, startCol}; }

    // Fallback
    advance();
    return {TokenType::TOKEN_ERROR, std::string(1, current), startLine, startCol};
}

/**
 * @brief Provides access to the next token. This is the main lexing function.
 * @return The next token.
 */
Token Lexer::getNextToken() {

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
Token Lexer::peekToken(unsigned int offset) {

    if (offset < peek_buffer.size()) {
        return peek_buffer[offset];
    }

    for (size_t i = peek_buffer.size(); i <= offset; ++i) {
        peek_buffer.push_back(createToken());
    }

    return peek_buffer[offset];

}