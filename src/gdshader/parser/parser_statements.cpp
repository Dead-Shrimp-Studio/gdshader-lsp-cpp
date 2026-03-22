#include "gdshader/parser/parser.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

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

    if (check(TokenType::KEYWORD_CONST) || isTypeStart()) {
        return parseVarDecl();
    }

    return parseExpressionStatement();
}

std::unique_ptr<StatementNode> Parser::parseVarDecl() 
{
    Token start = current_token;
    auto node = std::make_unique<VariableDeclNode>();
    
    if (match(TokenType::KEYWORD_CONST)) {
        node->isConst = true;
    }

    node->type = parseType();
    if (check(TokenType::TOKEN_IDENTIFIER)) 
    {
        node->name = current_token.value;
        node->nameRange.startLine = current_token.line;
        node->nameRange.startCol = current_token.column;
        node->nameRange.endLine = current_token.line;
        node->nameRange.endCol = current_token.column + current_token.length;
        advance();

        while (match(TokenType::TOKEN_LBRACKET)) {
            if (match(TokenType::TOKEN_NUMBER)) {
                try {
                    int size = std::stoi(previous_token.value);
                    if (size <= 0) reportError("Array size must be greater than 0.");
                    node->type->arraySizes.push_back(size);
                } catch (...) {
                    reportError("Array size must be a valid integer.");
                }
            } else {
                reportError("Expected array size.");
            }
            consume(TokenType::TOKEN_RBRACKET, "Expected ']'");
        }

    } else {
        reportError("Expected variable name, got '" + current_token.value + "'");

        node->nameRange.startLine = current_token.line;
        node->nameRange.startCol = current_token.column;
        node->nameRange.endLine = current_token.line;
        node->nameRange.endCol = current_token.column + current_token.length;
        
        if (current_token.type != TokenType::TOKEN_SEMI && current_token.type != TokenType::TOKEN_EQUAL && current_token.type != TokenType::TOKEN_EOF) 
        {
            SPDLOG_DEBUG("Parser consuming token for VarDeclNode that is non-identifier");
            advance();
        }
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

} // namespace gdshader_lsp