#include "gdshader/parser/parser.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

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

std::unique_ptr<ExpressionNode> Parser::parseTernary() 
{
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

std::unique_ptr<ExpressionNode> Parser::parseLogicOr() 
{
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

std::unique_ptr<ExpressionNode> Parser::parseLogicAnd() 
{
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

std::unique_ptr<ExpressionNode> Parser::parseEquality() 
{
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

std::unique_ptr<ExpressionNode> Parser::parseComparison() 
{
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

std::unique_ptr<ExpressionNode> Parser::parseTerm() 
{
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

std::unique_ptr<ExpressionNode> Parser::parseFactor() 
{
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

std::unique_ptr<ExpressionNode> Parser::parseUnary() 
{
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
            
            if (auto id = dynamic_cast<IdentifierNode*>(expr.get())) 
            {
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

    SPDLOG_ERROR("[Parser] ParsePrimary failed. Token: {} (Value: '{}')", tokenTypeToString(current_token.type), current_token.value);
    spdlog::dump_backtrace();

    return nullptr;
}

} // namespace gdshader_lsp