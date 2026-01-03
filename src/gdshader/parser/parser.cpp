
#include "gdshader/parser/parser.hpp"

#include "server/project_manager.hpp"

#include <iostream>

namespace gdshader_lsp {

// -------------------------------------------------------------------------
// CORE
// -------------------------------------------------------------------------

Parser::Parser(Lexer& lexer, const std::string& path) : lexer(lexer), currentPath(path) 
{
    advance(); // Load first token
}

std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();

    while (current_token.type != TokenType::TOKEN_EOF) {
        auto node = parseTopLevelDecl();
        if (node) {
            program->nodes.push_back(std::move(node));
        }
    }
    return program;
}

void Parser::advance() {
    previous_token = current_token;
    
    // Skip ERROR tokens from lexer loop if necessary, or just report them
    while (true) {
        current_token = lexer.getNextToken();
        if (current_token.type != TokenType::TOKEN_ERROR) break;
        
        reportErrorAt(current_token, "Lexical error: " + current_token.value);
    }
}

void Parser::consume(TokenType type, const std::string& message) {
    if (current_token.type == type) {
        advance();
        return;
    }
    reportError(message);
}

bool Parser::match(TokenType type) {
    if (current_token.type == type) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) {
    return current_token.type == type;
}

void Parser::reportError(const std::string& message) {
    reportErrorAt(current_token, message);
}

void Parser::reportErrorAt(const Token& token, const std::string& message) {
    if (panicMode) return; // Suppress cascade errors
    panicMode = true;
    
    diagnostics.push_back({token.line, token.column, message});
    std::cerr << "[Parser] Error at line " << token.line << ": " << message << std::endl;
}

void Parser::synchronize() {
    panicMode = false;
    while (current_token.type != TokenType::TOKEN_EOF) {
        if (previous_token.type == TokenType::TOKEN_SEMI) return;
        
        switch (current_token.type) {
            case TokenType::KEYWORD_SHADER_TYPE:
            case TokenType::KEYWORD_UNIFORM:
            case TokenType::KEYWORD_VARYING:
            case TokenType::KEYWORD_CONST:
            case TokenType::KEYWORD_STRUCT:
            case TokenType::KEYWORD_VOID:
            case TokenType::KEYWORD_IF:
            case TokenType::KEYWORD_FOR:
            case TokenType::KEYWORD_WHILE:
            case TokenType::KEYWORD_RETURN:
                return; // Found a synchronization point
            default:
                advance();
        }
    }
}

// -------------------------------------------------------------------------
// PREPROCESSOR STACK
// -------------------------------------------------------------------------

std::unique_ptr<ASTNode> gdshader_lsp::Parser::parsePreprocessor()
{
    int startLine = previous_token.line; // The line of the '#' token

    // 1. Consume the directive name (define, ifdef, etc.)
    if (check(TokenType::TOKEN_IDENTIFIER)) {
        std::string directive = current_token.value;
        advance();

        // --- #define NAME value ---
        if (directive == "define") {
            auto node = std::make_unique<DefineNode>();
            node->range.startLine = startLine;

            if (match(TokenType::TOKEN_IDENTIFIER)) {
                node->name = previous_token.value;
                activeDefines.insert(node->name); // Track for local #ifdef logic
                localDefines.insert(node->name);
                
                // Parse value (only if on the same line!)
                if (current_token.line == startLine) {
                    node->value = parseExpression();
                }
                return node;
            }
        }
        
        // --- #include PATH ---
        else if (directive == "include") {
            if (match(TokenType::TOKEN_STRING)) {
                auto node = std::make_unique<IncludeNode>();
                node->range.startLine = startLine;
                node->path = previous_token.value;

                auto pm = ProjectManager::get_singleton();
                std::string absPath = pm->resolvePath(currentPath, node->path);

                auto unit = pm->getUnit(absPath);
                pm->getExports(absPath);

                for (const auto& def : unit->defines) {
                    activeDefines.insert(def);
                }

                return node;
            }
        }

        // --- #ifdef NAME ---
        else if (directive == "ifdef") {
            if (match(TokenType::TOKEN_IDENTIFIER)) {
                bool condition = evaluatePreprocessorExpression();
                preprocessorStack.push_back(condition);
                if (!condition) skipBlock();
                return nullptr;
            }
        }

        else if (directive == "elif") {
             if (preprocessorStack.empty()) {
                reportError("#elif without #if");
            } else {
                // Logic: If the previous #if was TRUE, we are currently parsing.
                // We must now SKIP this #elif block regardless of condition.
                // If previous was FALSE, we evaluate this condition.
                
                bool previousWasTrue = preprocessorStack.back();
                
                if (previousWasTrue) {
                    // Previous block ran, so we skip this one
                    preprocessorStack.back() = true; // Stay in "True" state to keep skipping future elif/else? 
                    // Actually, stack usually tracks "Is the *Current* block active?"
                    // Better logic: The stack should track "Has any branch in this chain executed?"
                    // This gets complex. Simplified version:
                    skipBlock(); 
                } else {
                    bool condition = evaluatePreprocessorExpression();
                    preprocessorStack.back() = condition; // Update state
                    if (!condition) skipBlock();
                }
            }
            return nullptr;
        }
        
        // --- #ifndef NAME ---
        else if (directive == "ifndef") {
             if (match(TokenType::TOKEN_IDENTIFIER)) {
                bool exists = activeDefines.count(previous_token.value);
                preprocessorStack.push_back(!exists);
                
                if (exists) skipBlock(); // Skip if true
                return nullptr;
            }
        }
        
        // --- #else ---
        else if (directive == "else") {
            if (preprocessorStack.empty()) {
                reportError("#else without #if");
                return nullptr;
            }
            
            // Toggle state: If we were parsing (true), now we skip.
            // If we were skipping (false), was it because the parent was skipped?
            // Simplification: logic is tricky, usually we just flip the top.
            
            bool wasParsing = preprocessorStack.back();
            preprocessorStack.back() = !wasParsing;
            
            if (wasParsing) skipBlock(); // Skip the 'else' block
            return nullptr;
        }

        // --- #endif ---
        else if (directive == "endif") {
            if (preprocessorStack.empty()) {
                reportError("#endif without #if");
            } else {
                preprocessorStack.pop_back();
            }
            return nullptr;
        }
    }
    return nullptr;
}

void gdshader_lsp::Parser::skipBlock()
{
    int depth = 0;

    while (current_token.type != TokenType::TOKEN_EOF) {
        
        if (current_token.type == TokenType::TOKEN_PREPROCESSOR) {
            Token peek = lexer.peekToken(0); // Look at directive name
            
            if (peek.value == "ifdef" || peek.value == "ifndef" || peek.value == "if") {
                depth++;
            }
            else if (peek.value == "endif") {
                if (depth == 0) {
                    // Do NOT consume the #endif here, 
                    // main loop does it so it pops the stack.
                    return; 
                }
                depth--;
            }
            else if (peek.value == "else" && depth == 0) {
                // Found our toggle point
                return;
            }
        }
        
        advance(); // Eat token
    }
}

bool gdshader_lsp::Parser::evaluatePreprocessorExpression()
{
    bool result = true;
    
    // Handle "defined(X)"
    if (current_token.value == "defined") {
        advance();
        consume(TokenType::TOKEN_LPAREN, "Expected '(' after defined");
        if (check(TokenType::TOKEN_IDENTIFIER)) {
            std::string name = current_token.value;
            result = (activeDefines.count(name) > 0);
            advance();
        }
        consume(TokenType::TOKEN_RPAREN, "Expected ')'");
    } 
    // Handle boolean literals
    else if (current_token.type == TokenType::KEYWORD_TRUE) {
        result = true;
        advance();
    }
    else if (current_token.type == TokenType::KEYWORD_FALSE) {
        result = false;
        advance();
    }
    // Handle identifiers as boolean flags
    else if (current_token.type == TokenType::TOKEN_IDENTIFIER) {
        result = (activeDefines.count(current_token.value) > 0);
        advance();
    }
    // Handle Negation "!"
    else if (match(TokenType::TOKEN_EXCL)) {
        return !evaluatePreprocessorExpression();
    }

    // Handle "&&" and "||" (Basic support)
    if (match(TokenType::TOKEN_AND)) {
        bool right = evaluatePreprocessorExpression();
        return result && right;
    }
    if (match(TokenType::TOKEN_OR)) {
        bool right = evaluatePreprocessorExpression();
        return result || right;
    }

    return result;
}

// -------------------------------------------------------------------------
// TOP LEVEL
// -------------------------------------------------------------------------

std::unique_ptr<ASTNode> Parser::parseTopLevelDecl() 
{
    try {

        if (match(TokenType::TOKEN_PREPROCESSOR))  return parsePreprocessor();
        if (match(TokenType::KEYWORD_SHADER_TYPE)) return parseShaderType();
        if (match(TokenType::KEYWORD_RENDER_MODE)) return parseRenderMode();
        if (match(TokenType::KEYWORD_UNIFORM))     return parseUniform();
        if (match(TokenType::KEYWORD_VARYING))     return parseVarying();
        if (match(TokenType::KEYWORD_CONST))       return parseConst();
        if (match(TokenType::KEYWORD_STRUCT))      return parseStruct();
        
        if (isTypeStart()) {
            return parseTypeIdentifierDecl();
        }

        reportError("Unexpected token at top level: " + current_token.value);
        advance();
        return nullptr;

    } catch (...) {
        synchronize();
        return nullptr;
    }
}

std::unique_ptr<ASTNode> Parser::parseShaderType() 
{
    Token start = current_token;
    auto node = std::make_unique<ShaderTypeNode>();

    if (current_token.type == TokenType::TOKEN_IDENTIFIER) {
        node->shaderType = current_token.value;
        advance();
    } else {
        reportError("Expected shader type identifier (e.g. spatial)");
    }
    consume(TokenType::TOKEN_SEMI, "Expected ';' after shader_type");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseRenderMode() 
{
    Token start = current_token;
    auto node = std::make_unique<RenderModeNode>();

    do {
        if (current_token.type == TokenType::TOKEN_IDENTIFIER) {
            node->modes.push_back(current_token.value);
            advance();
        } else {
            reportError("Expected render mode identifier");
        }
    } while (match(TokenType::TOKEN_COMMA));

    consume(TokenType::TOKEN_SEMI, "Expected ';' after render_mode");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseUniform() 
{
    Token start = current_token;
    auto node = std::make_unique<UniformNode>();

    node->type = parseType();

    if (check(TokenType::TOKEN_IDENTIFIER)) {
        node->name = current_token.value;
        node->nameRange.startLine = previous_token.line;
        node->nameRange.startCol = previous_token.column;
        node->nameRange.endLine = previous_token.line;
        node->nameRange.endCol = previous_token.column + previous_token.length;
        advance();
    } else {
        reportError("Expected uniform name");
    }

    // Hint:  : hint_range(0, 1)
    if (match(TokenType::TOKEN_COLON)) {
        if (check(TokenType::TOKEN_IDENTIFIER)) {
            node->hint = current_token.value;
            advance();

            if (match(TokenType::TOKEN_LPAREN)) {
                node->hint += "(";
                while (!check(TokenType::TOKEN_RPAREN) && !check(TokenType::TOKEN_EOF)) {
                    // Just lazily consume token values for the hint string
                    node->hint += current_token.value;
                    advance();
                }
                consume(TokenType::TOKEN_RPAREN, "Expected ')' after hint arguments");
                node->hint += ")";
            }
        }
    }

    // Default Value: = 0.5
    if (match(TokenType::TOKEN_EQUAL)) {
        node->defaultValue = parseExpression();
    }

    consume(TokenType::TOKEN_SEMI, "Expected ';' after uniform declaration");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseVarying() 
{
    Token start = current_token;
    auto node = std::make_unique<VaryingNode>();
    
    node->type = parseType();
    
    if (check(TokenType::TOKEN_IDENTIFIER)) {
        node->name = current_token.value;
        node->nameRange.startLine = previous_token.line;
        node->nameRange.startCol = previous_token.column;
        node->nameRange.endLine = previous_token.line;
        node->nameRange.endCol = previous_token.column + previous_token.length;
        advance();
    } else {
        reportError("Expected varying name");
    }
    consume(TokenType::TOKEN_SEMI, "Expected ';' after varying");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseConst() 
{
    Token start = current_token;
    auto node = std::make_unique<ConstNode>();

    node->type = parseType();
    
    if (check(TokenType::TOKEN_IDENTIFIER)) {
        node->name = current_token.value;
        node->nameRange.startLine = previous_token.line;
        node->nameRange.startCol = previous_token.column;
        node->nameRange.endLine = previous_token.line;
        node->nameRange.endCol = previous_token.column + previous_token.length;
        advance();
    } else {
        reportError("Expected const name");
    }

    consume(TokenType::TOKEN_EQUAL, "Const requires an initialization value");
    node->value = parseExpression();

    consume(TokenType::TOKEN_SEMI, "Expected ';' after const declaration");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseStruct() 
{
    Token start = current_token;
    auto node = std::make_unique<StructNode>();

    if (check(TokenType::TOKEN_IDENTIFIER)) {
        node->name = current_token.value;
        node->nameRange.startLine = current_token.line;
        node->nameRange.startCol  = current_token.column;
        node->nameRange.endLine   = current_token.line;
        node->nameRange.endCol    = current_token.column + current_token.length;
        advance();
    } else {
        reportError("Expected struct name");
    }

    consume(TokenType::TOKEN_LBRACE, "Expected '{' before struct body");

    while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) {
        Token memberStart = current_token;
        auto member = std::make_unique<StructMemberNode>();

        member->type = parseType();

        if (check(TokenType::TOKEN_IDENTIFIER)) {
            member->name = current_token.value;
            
            member->nameRange.startLine = current_token.line;
            member->nameRange.startCol = current_token.column;
            member->nameRange.endLine = current_token.line;
            member->nameRange.endCol = current_token.column + current_token.length;

            advance();
        } else {
            reportError("Expected struct member name");
        }

        consume(TokenType::TOKEN_SEMI, "Expected ';' after struct member");

        setRange(member.get(), memberStart, previous_token);
        node->members.push_back(std::move(member));
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after struct body");
    consume(TokenType::TOKEN_SEMI, "Expected ';' after struct definition");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseTypeIdentifierDecl() 
{
    Token start = current_token;
    std::unique_ptr<TypeNode> typeNode = parseType();

    std::string name;
    SourceRange nameRange;

    if (check(TokenType::TOKEN_IDENTIFIER)) {
        name = current_token.value;

        nameRange.startLine = current_token.line;
        nameRange.startCol = current_token.column;
        nameRange.endLine = current_token.line;
        nameRange.endCol = current_token.column + current_token.length;

        advance();
    } else {
        reportError("Expected identifier after type");
        return nullptr;
    }

    // Look ahead to decide if Function or Variable
    if (check(TokenType::TOKEN_LPAREN)) {
        auto funcNode = parseFunction(std::move(typeNode), name);
        funcNode->nameRange = nameRange;
        return funcNode;
    } else {
        // Global Variable
        auto varNode = std::make_unique<VariableDeclNode>();
        varNode->type = std::move(typeNode);
        varNode->name = name;
        varNode->nameRange = nameRange;
        varNode->isConst = false;

        if (match(TokenType::TOKEN_EQUAL)) {
            varNode->initializer = parseExpression();
        }
        consume(TokenType::TOKEN_SEMI, "Expected ';' after variable declaration");

        setRange(varNode.get(), start, previous_token);
        return varNode;
    }
}

// -------------------------------------------------------------------------
// FUNCTIONS
// -------------------------------------------------------------------------

std::unique_ptr<FunctionNode> Parser::parseFunction(std::unique_ptr<TypeNode> returnType, const std::string& name) 
{
    auto node = std::make_unique<FunctionNode>();
    node->returnType = std::move(returnType);
    node->name = name;

    Token startToken; 
    startToken.line = node->returnType->range.startLine;
    startToken.column = node->returnType->range.startCol;

    consume(TokenType::TOKEN_LPAREN, "Expected '('");

    // Parse Arguments
    if (!check(TokenType::TOKEN_RPAREN)) {
        do {
            auto param = std::make_unique<ParameterNode>();
            Token paramStart = current_token;
            
            // Check for in/out qualifiers
            if (match(TokenType::KEYWORD_IN)) param->qualifier = "in";
            else if (match(TokenType::KEYWORD_OUT)) param->qualifier = "out";
            else if (match(TokenType::KEYWORD_INOUT)) param->qualifier = "inout";
            
            param->type = parseType();

            if (check(TokenType::TOKEN_IDENTIFIER)) {
                param->name = current_token.value;
                
                param->nameRange.startLine = current_token.line;
                param->nameRange.startCol = current_token.column;
                param->nameRange.endLine = current_token.line;
                param->nameRange.endCol = current_token.column + current_token.length;
                
                advance();
            }

            setRange(param.get(), paramStart, previous_token);
            node->parameters.push_back(std::move(param));

        } while (match(TokenType::TOKEN_COMMA));
    }

    consume(TokenType::TOKEN_RPAREN, "Expected ')'");

    // Block
    if (check(TokenType::TOKEN_LBRACE)) {
        node->body = parseBlock();
    } else {
        consume(TokenType::TOKEN_SEMI, "Expected body or ';'");
    }

    Token endToken = previous_token;
    setRange(node.get(), startToken, endToken);

    return node;
}

std::unique_ptr<BlockNode> Parser::parseBlock() 
{
    Token start = current_token; // Snapshot '{'
    auto node = std::make_unique<BlockNode>();
    
    consume(TokenType::TOKEN_LBRACE, "Expected '{'");
    
    while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) 
    {
        Token startToken = current_token; // Snapshot current state
        
        auto stmt = parseStatement();
        
        if (stmt) {
            node->statements.push_back(std::move(stmt));
        }

        // Force advance to prevent infinite loop.
        if (current_token.line == startToken.line && 
            current_token.column == startToken.column && 
            current_token.type != TokenType::TOKEN_EOF) {
            
            reportError("Parser stuck on '" + current_token.value + "'. Skipping.");
            advance(); 
        }
    }
    
    consume(TokenType::TOKEN_RBRACE, "Expected '}'");
    setRange(node.get(), start, previous_token);
    return node;
}

// -------------------------------------------------------------------------
// STATEMENTS
// -------------------------------------------------------------------------

std::unique_ptr<StatementNode> Parser::parseStatement() 
{
    if (match(TokenType::KEYWORD_IF)) return parseIf();
    if (match(TokenType::KEYWORD_FOR)) return parseFor();
    if (match(TokenType::KEYWORD_WHILE)) return parseWhile();
    if (match(TokenType::KEYWORD_RETURN)) return parseReturn();
    if (match(TokenType::KEYWORD_DO)) return parseDoWhile();
    if (match(TokenType::KEYWORD_SWITCH)) return parseSwitch();
    if (match(TokenType::KEYWORD_BREAK)) return parseBreak();
    if (match(TokenType::KEYWORD_CONTINUE)) return parseContinue();

    if (match(TokenType::KEYWORD_DISCARD)) {

        Token start = previous_token;
        auto node = std::make_unique<DiscardNode>();
        consume(TokenType::TOKEN_SEMI, "Expected ';'");

        setRange(node.get(), start, previous_token);
        return node;
    }

    if (check(TokenType::TOKEN_LBRACE)) {
        return parseBlock(); 
    }
    
    // Variable Declaration? "int x = 5;"
    if (isTypeStart()) {
        return parseVarDecl();
    }

    return parseExpressionStatement();
}

std::unique_ptr<StatementNode> Parser::parseVarDecl() 
{
    Token start = current_token;
    auto node = std::make_unique<VariableDeclNode>();
    node->type = parseType();
    
    if (check(TokenType::TOKEN_IDENTIFIER)) {
        node->name = current_token.value;

        node->nameRange.startLine = current_token.line;
        node->nameRange.startCol = current_token.column;
        node->nameRange.endLine = current_token.line;
        node->nameRange.endCol = current_token.column + current_token.length;

        advance();
    } else {
        reportError("Expected variable name");
    }

    if (match(TokenType::TOKEN_EQUAL)) {
        node->initializer = parseExpression();
    }

    consume(TokenType::TOKEN_SEMI, "Expected ';'");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseIf() 
{
    Token start = current_token; // Snapshot 'if'
    auto node = std::make_unique<IfNode>();

    consume(TokenType::TOKEN_LPAREN, "Expected '(' after if");
    node->condition = parseExpression();
    consume(TokenType::TOKEN_RPAREN, "Expected ')' after condition");

    node->thenBranch = parseStatement();
    
    if (match(TokenType::KEYWORD_ELSE)) {
        node->elseBranch = parseStatement();
    }

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseFor() 
{
    Token start = current_token; // Snapshot 'for'
    auto node = std::make_unique<ForNode>();

    consume(TokenType::TOKEN_LPAREN, "Expected '(' after for");
    
    // Init
    if (!match(TokenType::TOKEN_SEMI)) {
        if (isTypeStart()) node->init = parseVarDecl();
        else node->init = parseExpressionStatement();
    }
    // Condition
    if (!check(TokenType::TOKEN_SEMI)) {
        node->condition = parseExpression();
    }
    consume(TokenType::TOKEN_SEMI, "Expected ';' after for loop condition");

    if (!check(TokenType::TOKEN_RPAREN)) {
        node->increment = parseExpression();
    }
    consume(TokenType::TOKEN_RPAREN, "Expected ')' after for clauses");

    node->body = parseStatement();

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseWhile() 
{
    Token start = current_token; // Snapshot 'while'
    auto node = std::make_unique<WhileNode>();

    consume(TokenType::TOKEN_LPAREN, "Expected '(' after while");
    node->condition = parseExpression();
    consume(TokenType::TOKEN_RPAREN, "Expected ')'");
    
    node->body = parseStatement();

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseReturn() 
{
    Token start = current_token; // Snapshot 'return'
    auto node = std::make_unique<ReturnNode>();

    if (!check(TokenType::TOKEN_SEMI)) {
        node->value = parseExpression();
    }
    consume(TokenType::TOKEN_SEMI, "Expected ';'");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseExpressionStatement() 
{
    Token start = current_token;
    auto node = std::make_unique<ExpressionStatementNode>();
    node->expr = parseExpression();

    if (!node->expr) {
        // If we have a semicolon, consume it and return a "empty" statement (valid-ish)
        if (match(TokenType::TOKEN_SEMI)) {
            return node; 
        }
        return nullptr; 
    }

    consume(TokenType::TOKEN_SEMI, "Expected ';'");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseDoWhile() 
{
    Token start = current_token; // Snapshot 'do'
    auto node = std::make_unique<DoWhileNode>();
    
    // Parse Body (e.g., do { ... })
    node->body = parseStatement();
    
    consume(TokenType::KEYWORD_WHILE, "Expected 'while' after 'do' body");
    consume(TokenType::TOKEN_LPAREN, "Expected '(' after 'while'");
    
    node->condition = parseExpression();
    
    consume(TokenType::TOKEN_RPAREN, "Expected ')' after condition");
    consume(TokenType::TOKEN_SEMI, "Expected ';' after do-while loop");
    
    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseSwitch() 
{
    Token start = current_token; // Snapshot 'switch'
    auto node = std::make_unique<SwitchNode>();

    consume(TokenType::TOKEN_LPAREN, "Expected '(' after 'switch'");
    node->expression = parseExpression();
    consume(TokenType::TOKEN_RPAREN, "Expected ')' after switch expression");
    
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before switch body");
    
    while (!check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) {
        
        // 1. Identify Case or Default
        bool isDefault = false;
        if (match(TokenType::KEYWORD_CASE)) {
            isDefault = false;
        } else if (match(TokenType::KEYWORD_DEFAULT)) {
            isDefault = true;
        } else {
            reportError("Expected 'case' or 'default' inside switch block.");
            // Stuck parser protection (from our previous fix)
            advance(); 
            continue;
        }

        Token caseStart = current_token;
        auto caseNode = std::make_unique<CaseNode>();

        // 2. Parse Value (if not default)
        if (!isDefault) {
            caseNode->value = parseExpression();
        }
        
        consume(TokenType::TOKEN_COLON, "Expected ':' after case label");

        // 3. Parse Statements until next case/default or end of switch
        // This handles "Fallthrough" naturally (statements list will be empty)
        while (!check(TokenType::KEYWORD_CASE) && !check(TokenType::KEYWORD_DEFAULT) && !check(TokenType::TOKEN_RBRACE) && !check(TokenType::TOKEN_EOF)) 
        {
            Token startToken = current_token;

            auto stmt = parseStatement();
            if (stmt) caseNode->statements.push_back(std::move(stmt));

            if (current_token.line == startToken.line && 
                current_token.column == startToken.column && 
                current_token.type != TokenType::TOKEN_EOF) {
                
                // Only report if we haven't already panicked
                if (!panicMode) reportError("Unexpected token '" + current_token.value + "' inside switch case.");
                advance(); 
            }
        }
        setRange(caseNode.get(), caseStart, previous_token);
        node->cases.push_back(std::move(caseNode));
    }
    
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after switch body");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseBreak() 
{
    Token start = current_token;
    auto node = std::make_unique<BreakNode>();

    consume(TokenType::TOKEN_SEMI, "Expected ';' after 'break'");

    setRange(node.get(), start, previous_token);
    return node;
}

std::unique_ptr<StatementNode> Parser::parseContinue() 
{
    Token start = current_token;
    auto node = std::make_unique<ContinueNode>();

    consume(TokenType::TOKEN_SEMI, "Expected ';' after 'continue'");

    setRange(node.get(), start, previous_token);
    return node;
}

// -------------------------------------------------------------------------
// EXPRESSIONS
// -------------------------------------------------------------------------

std::unique_ptr<ExpressionNode> Parser::parseExpression() {
    return parseAssignment();
}

std::unique_ptr<ExpressionNode> Parser::parseAssignment() 
{
    auto expr = parseTernary();

    if (match(TokenType::TOKEN_EQUAL) || 
        match(TokenType::TOKEN_PLUS_EQUAL) ||
        match(TokenType::TOKEN_MINUS_EQUAL) ||
        match(TokenType::TOKEN_STAR_EQUAL) ||
        match(TokenType::TOKEN_SLASH_EQUAL) ||
        match(TokenType::TOKEN_PERCENT_EQUAL)) {
        
        TokenType op = previous_token.type;
        auto right = parseAssignment();
        
        auto node = std::make_unique<BinaryOpNode>();
        node->op = op;
        node->left = std::move(expr);
        node->right = std::move(right);

        mergeBinaryRange(node.get());
        return node;
    }

    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseTernary() {
    auto expr = parseLogicOr();
    
    if (match(TokenType::TOKEN_QUESTION)) {
        auto node = std::make_unique<TernaryNode>();
        node->condition = std::move(expr);
        node->trueExpr = parseExpression();
        consume(TokenType::TOKEN_COLON, "Expected ':' in ternary operator");
        node->falseExpr = parseTernary();
        return node;
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseLogicOr() {
    auto expr = parseLogicAnd();
    while (match(TokenType::TOKEN_OR)) {
        auto node = std::make_unique<BinaryOpNode>();
        node->op = TokenType::TOKEN_OR;
        node->left = std::move(expr);
        node->right = parseLogicAnd();
        mergeBinaryRange(node.get());
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseLogicAnd() {
    auto expr = parseEquality();
    while (match(TokenType::TOKEN_AND)) {
        auto node = std::make_unique<BinaryOpNode>();
        node->op = TokenType::TOKEN_AND;
        node->left = std::move(expr);
        node->right = parseEquality();
        mergeBinaryRange(node.get());
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseEquality() {
    auto expr = parseComparison();
    while (match(TokenType::TOKEN_EQ_EQ) || match(TokenType::TOKEN_NOT_EQ)) {
        auto node = std::make_unique<BinaryOpNode>();
        node->op = previous_token.type;
        node->left = std::move(expr);
        node->right = parseComparison();
        mergeBinaryRange(node.get());
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseComparison() {
    auto expr = parseTerm();
    while (match(TokenType::TOKEN_LESS) || match(TokenType::TOKEN_LESS_EQ) ||
           match(TokenType::TOKEN_GREATER) || match(TokenType::TOKEN_GREATER_EQ)) {
        auto node = std::make_unique<BinaryOpNode>();
        node->op = previous_token.type;
        node->left = std::move(expr);
        node->right = parseTerm();
        mergeBinaryRange(node.get());
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseTerm() {
    auto expr = parseFactor();
    while (match(TokenType::TOKEN_PLUS) || match(TokenType::TOKEN_MINUS)) {
        auto node = std::make_unique<BinaryOpNode>();
        node->op = previous_token.type;
        node->left = std::move(expr);
        node->right = parseFactor();
        mergeBinaryRange(node.get());
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseFactor() {
    auto expr = parseUnary();
    while (match(TokenType::TOKEN_STAR) || match(TokenType::TOKEN_SLASH) || match(TokenType::TOKEN_PERCENT)) {
        auto node = std::make_unique<BinaryOpNode>();
        node->op = previous_token.type;
        node->left = std::move(expr);
        node->right = parseUnary();
        mergeBinaryRange(node.get());
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseUnary() {
    if (match(TokenType::TOKEN_MINUS) || match(TokenType::TOKEN_EXCL)) {

        Token start = previous_token;
        auto node = std::make_unique<UnaryOpNode>();

        node->op = previous_token.type;
        node->operand = parseUnary();
        node->isPostfix = false;

        if (node->operand) {
            // We can't use setRange(start, previous) because 'previous' might be deep inside the recursion.
            // We manually construct it:
            node->range.startLine = start.line;
            node->range.startCol  = start.column;
            node->range.endLine   = node->operand->range.endLine;
            node->range.endCol    = node->operand->range.endCol;
        }
        return node;
    }
    return parseCallOrAccess();
}

std::unique_ptr<ExpressionNode> Parser::parseCallOrAccess() 
{
    auto expr = parsePrimary();
    if (!expr) return nullptr;

    while (true) {
        // Function Call: ident(...)
        if (match(TokenType::TOKEN_LPAREN)) {
            
            auto callNode = std::make_unique<FunctionCallNode>();

            callNode->range.startLine = expr->range.startLine;
            callNode->range.startCol  = expr->range.startCol;
            
            if (auto id = dynamic_cast<IdentifierNode*>(expr.get())) {
                callNode->functionName = id->name;
                callNode->nameRange = id->range;
            } else {
                callNode->functionName = "unknown";
            }

            if (!check(TokenType::TOKEN_RPAREN)) {
                do {
                    if (check(TokenType::TOKEN_RPAREN)) break; // Handling trailing commata
                    callNode->arguments.push_back(parseExpression());
                } while (match(TokenType::TOKEN_COMMA));
            }
            consume(TokenType::TOKEN_RPAREN, "Expected ')' after arguments");

            callNode->range.endLine = previous_token.line;
            callNode->range.endCol  = previous_token.column + previous_token.length;

            expr = std::move(callNode);
        }
        // Member Access: .xyz
        else if (match(TokenType::TOKEN_DOT)) {
            auto dotNode = std::make_unique<MemberAccessNode>();
            dotNode->base = std::move(expr);
            
            dotNode->range.startLine = dotNode->base->range.startLine;
            dotNode->range.startCol  = dotNode->base->range.startCol;
            
            if (check(TokenType::TOKEN_IDENTIFIER)) {
                dotNode->member = current_token.value;

                dotNode->range.endLine = previous_token.line;
                dotNode->range.endCol  = previous_token.column + previous_token.length;

                advance();
            } else {
                reportError("Expected property name after '.'");
            }
            expr = std::move(dotNode);
        }
        // Array Access: [0]
        else if (match(TokenType::TOKEN_LBRACKET)) {
            auto indexNode = std::make_unique<ArrayAccessNode>();
            indexNode->base = std::move(expr);
            
            indexNode->range.startLine = indexNode->base->range.startLine;
            indexNode->range.startCol  = indexNode->base->range.startCol;

            indexNode->index = parseExpression();
            consume(TokenType::TOKEN_RBRACKET, "Expected ']'");

            indexNode->range.endLine = previous_token.line;
            indexNode->range.endCol  = previous_token.column + previous_token.length;

            expr = std::move(indexNode);
        }
        else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parsePrimary() 
{
    if (match(TokenType::TOKEN_NUMBER)) {
        auto node = std::make_unique<LiteralNode>();
        node->type = TokenType::TOKEN_NUMBER;
        node->value = previous_token.value;
        setRange(node.get(), previous_token, previous_token);
        return node;
    }
    if (match(TokenType::TOKEN_STRING)) {
        auto node = std::make_unique<LiteralNode>();
        node->type = TokenType::TOKEN_STRING;
        node->value = previous_token.value;
        setRange(node.get(), previous_token, previous_token);
        return node;
    }
    if (match(TokenType::KEYWORD_TRUE)) {
        auto node = std::make_unique<LiteralNode>();
        node->type = TokenType::KEYWORD_TRUE;
        node->value = "true";
        setRange(node.get(), previous_token, previous_token);
        return node;
    }
    if (match(TokenType::KEYWORD_FALSE)) {
        auto node = std::make_unique<LiteralNode>();
        node->type = TokenType::KEYWORD_FALSE;
        node->value = "false";
        setRange(node.get(), previous_token, previous_token);
        return node;
    }
    if (match(TokenType::TOKEN_IDENTIFIER)) {
        auto node = std::make_unique<IdentifierNode>();
        node->name = previous_token.value;
        setRange(node.get(), previous_token, previous_token);
        return node;
    }
    // Types (vec3, float) as primaries (constructors)?
    // Usually constructors start with a Type Keyword.
    // Let's check isTypeStart()
    if (isTypeStart()) {
        auto node = std::make_unique<IdentifierNode>();
        node->name = parseTypeString();
        setRange(node.get(), previous_token, previous_token);
        return node;
    }

    if (match(TokenType::TOKEN_LPAREN)) {
        auto expr = parseExpression();
        consume(TokenType::TOKEN_RPAREN, "Expected ')'");
        return expr;
    }

    reportError("Expected expression.");

    std::cout << "ParsePrimary failed on token: " << tokenTypeToString(current_token.type) 
              << " Value: " << current_token.value << std::endl;

    return nullptr;
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

std::unique_ptr<TypeNode> gdshader_lsp::Parser::parseType()
{
    Token start = current_token;
    auto node = std::make_unique<TypeNode>();

    // 1. Optional Precision (highp/lowp) - mostly ignored in Godot but valid syntax
    if (match(TokenType::KEYWORD_HIGH_PRECISION)) {
        node->precision = previous_token.value;
    } 
    // Add mediump/lowp checks here if you added those tokens

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
                reportError("Array dimension specifier must be a integer value,");
                node->arraySizes.push_back(0); // Error fallback
            }
        } else {
            // Handle complex array sizes (const expressions) if needed, 
            // or just report error for now.
            reportError("Expected array size.");
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
}

bool Parser::isTypeStart() {
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
            
            Token next = lexer.peekToken(0); // Peek next (offset 0 usually means next in queue?)

            if (next.type == TokenType::TOKEN_IDENTIFIER) {
                // "MyType varName" -> It's a declaration
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