#include "gdshader/parser/parser.hpp"
#include "utils/logger.hpp"

#include <iostream>

namespace gdshader_lsp {

// -------------------------------------------------------------------------
// CORE
// -------------------------------------------------------------------------

Parser::Parser(Lexer& lexer, const std::string& path) : lexer(lexer), currentPath(path) 
{
    advance(); // Load first token
}

std::unique_ptr<ProgramNode> Parser::parse() 
{
    auto program = std::make_unique<ProgramNode>();

    while (current_token.type != TokenType::TOKEN_EOF) {
        auto node = parseTopLevelDecl();
        if (node) {
            program->nodes.push_back(std::move(node));
        }
    }

    for (const auto& comment : commentBuffer) {
        program->trailingComments.push_back(comment);
    }
    commentBuffer.clear();

    return program;
}

void Parser::advance() 
{
    previous_token = current_token;
    
    // Skip ERROR tokens from lexer loop if necessary, or just report them
    while (true) {
        current_token = lexer.getNextToken();
        
        if (current_token.type == TokenType::TOKEN_COMMENT) {
            commentBuffer.push_back(current_token);
            continue;
        }

        if (current_token.type != TokenType::TOKEN_ERROR) break;
        reportErrorAt(current_token, "Lexical error: " + current_token.value);
    }
}

void Parser::consume(TokenType type, const std::string& message) 
{
    if (current_token.type == type) {
        advance();
        return;
    }
    reportError(message);
}

bool Parser::match(TokenType type) 
{
    if (current_token.type == type) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) 
{
    return current_token.type == type;
}

void Parser::reportError(const std::string& message) 
{
    reportErrorAt(current_token, message);
}

void Parser::reportErrorAt(const Token& token, const std::string& message) 
{
    if (panicMode) return; // Suppress cascade errors
    panicMode = true;
    
    SPDLOG_DEBUG("Parse Error at {}:{}: {}", token.line, token.column, message);
    diagnostics.push_back({message, DiagnosticLevel::Error, {token.line, token.column, token.line, token.column + token.length}});
}

void Parser::synchronize() 
{
    SPDLOG_WARN("[Parser] Entering Panic Mode. Synchronizing...");

    panicMode = false;
    while (current_token.type != TokenType::TOKEN_EOF) {
        if (previous_token.type == TokenType::TOKEN_SEMI) {
            SPDLOG_INFO("[Parser] Recovered at semicolon.");
            return;
        }
        
        switch (current_token.type) {
            case TokenType::KEYWORD_SHADER_TYPE:
            case TokenType::KEYWORD_GROUP_UNIFORMS:
            case TokenType::KEYWORD_UNIFORM:
            case TokenType::KEYWORD_VARYING:
            case TokenType::KEYWORD_CONST:
            case TokenType::KEYWORD_STRUCT:
            case TokenType::KEYWORD_VOID:
            case TokenType::KEYWORD_IF:
            case TokenType::KEYWORD_FOR:
            case TokenType::KEYWORD_WHILE:
            case TokenType::KEYWORD_RETURN:
                SPDLOG_INFO("[Parser] Found sync point at {}", tokenTypeToString(current_token.type));
                return;
            default:
                advance();
        }
    }
}

// -------------------------------------------------------------------------
// UTILS
// -------------------------------------------------------------------------

std::string Parser::parseTypeString() 
{
    std::string type = current_token.value;
    advance(); // Consume the keyword/identifier
    return type;
}

bool Parser::isTypeKeyword(TokenType type) 
{
    switch (type) {
        case TokenType::KEYWORD_VOID:
        case TokenType::KEYWORD_BOOL:
        case TokenType::KEYWORD_INT:
        case TokenType::KEYWORD_UINT:
        case TokenType::KEYWORD_FLOAT:
        case TokenType::KEYWORD_VEC2:
        case TokenType::KEYWORD_VEC3:
        case TokenType::KEYWORD_VEC4:
        case TokenType::KEYWORD_IVEC2:
        case TokenType::KEYWORD_IVEC3:
        case TokenType::KEYWORD_IVEC4:
        case TokenType::KEYWORD_UVEC2:
        case TokenType::KEYWORD_UVEC3:
        case TokenType::KEYWORD_UVEC4:
        case TokenType::KEYWORD_BVEC2:
        case TokenType::KEYWORD_BVEC3:
        case TokenType::KEYWORD_BVEC4:
        case TokenType::KEYWORD_MAT2:
        case TokenType::KEYWORD_MAT3:
        case TokenType::KEYWORD_MAT4:
        case TokenType::KEYWORD_SAMPLER2D:
        case TokenType::KEYWORD_ISAMPLER2D:
        case TokenType::KEYWORD_USAMPLER2D:
        case TokenType::KEYWORD_SAMPLER3D:
        case TokenType::KEYWORD_SAMPLERCUBE:
        case TokenType::KEYWORD_SAMPLER2DARRAY:
            return true;
        default:
            return false;
    }
}

std::unique_ptr<TypeNode> gdshader_lsp::Parser::parseType()
{
    Token start = current_token;
    auto node = std::make_unique<TypeNode>();

    // 1. Optional Precision (highp/lowp) - mostly ignored in Godot but valid syntax
    if (match(TokenType::KEYWORD_HIGH_PRECISION)) {
        node->precision = previous_token.value;
    }

    // 2. Base Type Name
    if (isTypeStart()) {
        node->baseName = current_token.value;
        
        // Capture specific range for the "float" or "vec3" part (for highlighting)
        node->baseNameRange.startLine = current_token.line;
        node->baseNameRange.startCol = current_token.column;
        node->baseNameRange.endLine = current_token.line;
        node->baseNameRange.endCol = current_token.column + current_token.length;

        advance(); // Consume the type keyword/identifier
    } else {
        reportError("Expected type specifier.");
    }

    // 3. Array Dimensions: float[5][3]
    while (match(TokenType::TOKEN_LBRACKET)) {
        if (match(TokenType::TOKEN_NUMBER)) {
            try {
                int size = std::stoi(previous_token.value);
                node->arraySizes.push_back(size);
            } catch (...) {
                reportError("Array dimension specifier must be a integer value.");
                node->arraySizes.push_back(0); // Error fallback
            }
        } else {
            reportError("Expected array size. Must be an integer value.");
        }
        consume(TokenType::TOKEN_RBRACKET, "Expected ']'");
    }

    setRange(node.get(), start, previous_token);
    return node;
}

void gdshader_lsp::Parser::mergeBinaryRange(BinaryOpNode *node)
{
    if (node->left) {
        node->range.startLine = node->left->range.startLine;
        node->range.startCol  = node->left->range.startCol;
    }
    if (node->right) {
        node->range.endLine = node->right->range.endLine;
        node->range.endCol  = node->right->range.endCol;
    }
}

void gdshader_lsp::Parser::setRange(ASTNode *node, const Token &start, const Token &end)
{
    node->range.startLine = start.line;
    node->range.startCol  = start.column;
    node->range.endLine   = end.line;
    node->range.endCol    = end.column + end.length;

    auto it = commentBuffer.begin();
    while (it != commentBuffer.end()) {
        
        // Leading comments: appear before this node started
        if (it->line < start.line || (it->line == start.line && it->column < start.column)) {
            node->leadingComments.push_back(*it);
            it = commentBuffer.erase(it);
        }
        // Trailing comments: appear on the exact same line AFTER this node ends
        else if (it->line == end.line && it->column >= end.column) {
            node->trailingComments.push_back(*it);
            it = commentBuffer.erase(it);
        }
        else {
            ++it;
        }
    }
}

bool Parser::isTypeStart() 
{
    SPDLOG_TRACE("Checking is type start for token {}", current_token.toString());
    switch (current_token.type) {

        case TokenType::KEYWORD_VOID:
        case TokenType::KEYWORD_BOOL:
        case TokenType::KEYWORD_INT:
        case TokenType::KEYWORD_UINT:
        case TokenType::KEYWORD_FLOAT:
        case TokenType::KEYWORD_VEC2:
        case TokenType::KEYWORD_VEC3:
        case TokenType::KEYWORD_VEC4:
        case TokenType::KEYWORD_IVEC2:
        case TokenType::KEYWORD_IVEC3:
        case TokenType::KEYWORD_IVEC4:
        case TokenType::KEYWORD_UVEC2:
        case TokenType::KEYWORD_UVEC3:
        case TokenType::KEYWORD_UVEC4:
        case TokenType::KEYWORD_BVEC2:
        case TokenType::KEYWORD_BVEC3:
        case TokenType::KEYWORD_BVEC4:
        case TokenType::KEYWORD_MAT2:
        case TokenType::KEYWORD_MAT3:
        case TokenType::KEYWORD_MAT4:
        case TokenType::KEYWORD_SAMPLER2D:
        case TokenType::KEYWORD_ISAMPLER2D:
        case TokenType::KEYWORD_USAMPLER2D:
        case TokenType::KEYWORD_SAMPLER3D:
        case TokenType::KEYWORD_SAMPLERCUBE:
        case TokenType::KEYWORD_SAMPLER2DARRAY:
        case TokenType::KEYWORD_STRUCT: // struct MyStruct x;
            return true;

        case TokenType::TOKEN_IDENTIFIER: {
            
            Token next = lexer.peekToken(0);
            if (next.type == TokenType::TOKEN_IDENTIFIER) {
                return true;
            }
            
            // "myVar.x" (DOT)
            // "myVar = 5" (EQUAL)
            // "myVar;" (SEMI)
            // "myVar()" (LPAREN - could be Constructor or Func Call)
            // If LPAREN, it's ambiguous: MyType(1) vs myFunc(1).
            // But 'MyType(1);' is an expression statement anyway.
            // 'MyType(1) x;' is NOT valid GLSL/Godot (constructors in decl handled differently).
            
            return false;
        }

        default:
            return false;
    }
}

} // namespace gdshader_lsp