#ifndef PARSER_HPP
#define PARSER_HPP

#include "gdshader/parser/parser_types.hpp"

#include "gdshader/lexer/lexer.h"
#include "gdshader/lexer/lexer_types.h"
#include "gdshader/ast/ast.h"
#include "gdshader/diagnostics.hpp"

#include <vector>
#include <memory>
#include <string>
#include <optional>

#include <unordered_set>

namespace gdshader_lsp 
{
    class Parser {
    
    public:
    
        Parser(Lexer& lexer, const std::string& path);

        // Main entry point
        std::unique_ptr<ProgramNode> parse();

        // Retrieve errors after parsing
        const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics; }
        const std::unordered_set<std::string>& getDefines() const { return localDefines; }

    private:

        Lexer& lexer;
        Token current_token {};
        Token previous_token {}; // Useful for getting locations of consumed tokens
        std::vector<Token> commentBuffer; // Add this to your Parser state
        
        std::vector<Diagnostic> diagnostics {};
        bool panicMode = false;

        // --- Preprocessor stack ---

        std::vector<bool> preprocessorStack {};
        std::unordered_set<std::string> activeDefines {};

        std::unique_ptr<ASTNode> parsePreprocessor();
        void skipBlock();

        // --- State ---

        std::string currentPath {};
        std::unordered_set<std::string> localDefines {};
        bool evaluatePreprocessorExpression();

        // --- Core Helpers ---

        void advance();
        void consume(TokenType type, DiagnosticCode code, const std::string& message);
        bool match(TokenType type);
        bool check(TokenType type);

        // --- Errors ---

        void reportDiagnosticAt(const Token& token, DiagnosticCode code, DiagnosticLevel level, const std::string& message);
        void reportError(DiagnosticCode code, const std::string& message);
        void reportErrorAt(const Token& token, DiagnosticCode code, const std::string& message);
        void reportWarning(DiagnosticCode code, const std::string& message);
        void reportHint(DiagnosticCode code, const std::string& message);
        void reportInformation(DiagnosticCode code, const std::string& message);

        void synchronize(); // Error recovery

        // --- Top Level Parsing ---
        std::unique_ptr<ASTNode> parseTopLevelDecl();
        std::unique_ptr<ASTNode> parseShaderType();
        std::unique_ptr<ASTNode> parseRenderMode();
        std::unique_ptr<ASTNode> parseGroupUniform();
        std::unique_ptr<ASTNode> parseUniform();
        std::unique_ptr<ASTNode> parseVarying();
        std::unique_ptr<ASTNode> parseConst();
        std::unique_ptr<ASTNode> parseStruct();
        
        // Handles both Functions ("void foo() {}") and Global Vars ("vec3 x;")
        std::unique_ptr<ASTNode> parseTypeIdentifierDecl(); 

        // --- Function Parsing ---
        std::unique_ptr<FunctionNode> parseFunction(std::unique_ptr<TypeNode> returnType, const std::string& name);
        std::unique_ptr<BlockNode> parseBlock();
        
        // --- Statement Parsing ---
        std::unique_ptr<StatementNode> parseStatement();
        std::unique_ptr<StatementNode> parseVarDecl();
        std::unique_ptr<StatementNode> parseIf();
        std::unique_ptr<StatementNode> parseFor();
        std::unique_ptr<StatementNode> parseWhile();
        std::unique_ptr<StatementNode> parseReturn();
        std::unique_ptr<StatementNode> parseExpressionStatement();
        std::unique_ptr<StatementNode> parseDoWhile();
        std::unique_ptr<StatementNode> parseSwitch();
        std::unique_ptr<StatementNode> parseBreak();
        std::unique_ptr<StatementNode> parseContinue();

        // --- Expression Parsing (Pratt Parser) ---
        std::unique_ptr<ExpressionNode> parseExpression();
        std::unique_ptr<ExpressionNode> parsePrecedence(Precedence precedence);
        
        // Dispatchers
        std::unique_ptr<ExpressionNode> parsePrefix(TokenType type);
        std::unique_ptr<ExpressionNode> parseInfix(std::unique_ptr<ExpressionNode> left, TokenType type);

        // Precedence mapping
        Precedence getPrecedence(TokenType type);

        // Individual expression parsers
        std::unique_ptr<ExpressionNode> parseNumber();
        std::unique_ptr<ExpressionNode> parseString();
        std::unique_ptr<ExpressionNode> parseBoolean();
        std::unique_ptr<ExpressionNode> parseIdentifier();
        std::unique_ptr<ExpressionNode> parseGrouping();
        std::unique_ptr<ExpressionNode> parseUnary();
        std::unique_ptr<ExpressionNode> parseBinary(std::unique_ptr<ExpressionNode> left);
        std::unique_ptr<ExpressionNode> parseTernary(std::unique_ptr<ExpressionNode> left);
        std::unique_ptr<ExpressionNode> parseCall(std::unique_ptr<ExpressionNode> left);
        std::unique_ptr<ExpressionNode> parseMemberAccess(std::unique_ptr<ExpressionNode> left);
        std::unique_ptr<ExpressionNode> parseArrayAccess(std::unique_ptr<ExpressionNode> left);
        
        // Helper for parsing types (e.g. "vec3", "void", "Sampler2D")
        std::string parseTypeString();
        bool isTypeKeyword(TokenType type);
        std::unique_ptr<TypeNode> parseType();

        void mergeBinaryRange(BinaryOpNode* node);
        void setRange(ASTNode* node, const Token& start, const Token& end);

        bool isTypeStart();
    };

} // namespace gdshader_lsp

#endif // PARSER_HPP