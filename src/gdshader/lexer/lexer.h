
#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <iostream>
#include <deque>

#include "gdshader/lexer/lexer_types.h"

namespace gdshader_lsp {

class Lexer {
public:
    // Constructor takes the source code string.
    Lexer(const std::string& source_code);

    // Main method to get the next token.
    Token getNextToken();

    /**
     * @brief Allows peeking a number of tokens ahead, depending on the offset.
     * 
     * @param offset : Default value 1. An offset of 1 means the next available token.
     */
    Token peekToken(unsigned int offset = 0);

    std::string source;
    size_t current_pos;

private:

    unsigned long source_len = 0;

    char current_char = 0;

    int line = 0;   // 0-based or 1-based (LSP uses 0-based usually)
    int column = 0;

    // If we peek ahead on token we can actually store the peek and be a little faster here
    std::deque<Token> peek_buffer;

    // Helper functions to advance through the source.
    void advance();
    char peek();

    void skipWhitespace();

    // Helper functions to parse different token types.
    Token parseComment(int startLine, int startCol);
    Token parseNumber(int startLine, int startCol);
    Token parseIdentifier(int startLine, int startCol);
    Token parseString(int startLine, int startCol);

    // Helper to check for keywords
    Token createToken();
};

}

#endif // LEXER_H