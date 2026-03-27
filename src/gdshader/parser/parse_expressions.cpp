#include "gdshader/parser/parser.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

// Precedence mapper for all token types
Precedence Parser::getPrecedence(TokenType type) 
{
    switch (type) {
        case TokenType::TOKEN_EQUAL:
        case TokenType::TOKEN_PLUS_EQUAL:
        case TokenType::TOKEN_MINUS_EQUAL:
        case TokenType::TOKEN_STAR_EQUAL:
        case TokenType::TOKEN_SLASH_EQUAL:
        case TokenType::TOKEN_PERCENT_EQUAL:
        case TokenType::TOKEN_LESS_LESS_EQUAL:
        case TokenType::TOKEN_GREATER_GREATER_EQUAL:
        case TokenType::TOKEN_AMPERSAND_EQUAL:
        case TokenType::TOKEN_PIPE_EQUAL:
        case TokenType::TOKEN_CARET_EQUAL:
            return PREC_ASSIGNMENT;

        case TokenType::TOKEN_CARET_CARET:
            return PREC_LOGICAL_XOR;
        
        case TokenType::TOKEN_LESS_LESS:
        case TokenType::TOKEN_GREATER_GREATER:
            return PREC_SHIFT;

        case TokenType::TOKEN_PLUS_PLUS:
        case TokenType::TOKEN_MINUS_MINUS:
            return PREC_CALL;

        case TokenType::TOKEN_QUESTION:
            return PREC_TERNARY;

        case TokenType::TOKEN_OR:
            return PREC_OR;

        case TokenType::TOKEN_AND:
            return PREC_AND;

        case TokenType::TOKEN_PIPE:
            return PREC_BITWISE_OR;

        case TokenType::TOKEN_CARET:
            return PREC_BITWISE_XOR;

        case TokenType::TOKEN_AMPERSAND:
            return PREC_BITWISE_AND;

        case TokenType::TOKEN_EQ_EQ:
        case TokenType::TOKEN_NOT_EQ:
            return PREC_EQUALITY;

        case TokenType::TOKEN_LESS:
        case TokenType::TOKEN_LESS_EQ:
        case TokenType::TOKEN_GREATER:
        case TokenType::TOKEN_GREATER_EQ:
            return PREC_COMPARISON;

        case TokenType::TOKEN_PLUS:
        case TokenType::TOKEN_MINUS:
            return PREC_TERM;

        case TokenType::TOKEN_STAR:
        case TokenType::TOKEN_SLASH:
        case TokenType::TOKEN_PERCENT:
            return PREC_FACTOR;

        // Postfix operators have high precedence
        case TokenType::TOKEN_LPAREN:   // Function call: foo()
        case TokenType::TOKEN_DOT:      // Member access: foo.bar
        case TokenType::TOKEN_LBRACKET: // Array access: foo[0]
            return PREC_CALL;

        default:
            return PREC_NONE;
    }
}

std::unique_ptr<ExpressionNode> Parser::parseExpression() {
    // Parse any expression that has a precedence of ASSIGNMENT or higher
    return parsePrecedence(PREC_ASSIGNMENT);
}

std::unique_ptr<ExpressionNode> Parser::parsePrecedence(Precedence precedence) 
{
    advance(); // Grab the first token

    // 1. Try to parse a prefix expression (number, string, id, unary -, etc.)
    auto prefixRuleToken = previous_token.type;
    auto left = parsePrefix(prefixRuleToken);

    if (!left) {
        reportError("Expected expression.");
        return nullptr;
    }

    // 2. While the NEXT token has a higher or equal binding power to our current context,
    // let it "absorb" the expression we just parsed as its left operand.
    while (precedence <= getPrecedence(current_token.type)) {
        advance();
        auto infixRuleToken = previous_token.type;
        left = parseInfix(std::move(left), infixRuleToken);
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parsePrefix(TokenType type) 
{
    switch (type) {
        case TokenType::TOKEN_NUMBER:       return parseNumber();
        case TokenType::TOKEN_STRING:       return parseString();
        case TokenType::KEYWORD_TRUE:
        case TokenType::KEYWORD_FALSE:      return parseBoolean();
        case TokenType::TOKEN_IDENTIFIER:   return parseIdentifier();
        case TokenType::TOKEN_LPAREN:       return parseGrouping();
        case TokenType::TOKEN_PLUS_PLUS:
        case TokenType::TOKEN_MINUS_MINUS:  
        case TokenType::TOKEN_MINUS:
        case TokenType::TOKEN_EXCL:         
        case TokenType::TOKEN_TILDE:
            return parseUnary();
        default:
            // Handle type constructors (like vec3, float, etc.)
            if (isTypeKeyword(type)) {
                return parseIdentifier(); // We treat type names as identifiers in expressions
            }
            return nullptr;
    }
}

std::unique_ptr<ExpressionNode> Parser::parseInfix(std::unique_ptr<ExpressionNode> left, TokenType type) 
{
    switch (type) {
        case TokenType::TOKEN_PLUS:
        case TokenType::TOKEN_MINUS:
        case TokenType::TOKEN_STAR:
        case TokenType::TOKEN_SLASH:
        case TokenType::TOKEN_PERCENT:
        case TokenType::TOKEN_EQ_EQ:
        case TokenType::TOKEN_NOT_EQ:
        case TokenType::TOKEN_LESS:
        case TokenType::TOKEN_LESS_EQ:
        case TokenType::TOKEN_GREATER:
        case TokenType::TOKEN_GREATER_EQ:
        case TokenType::TOKEN_AND:
        case TokenType::TOKEN_OR:
        case TokenType::TOKEN_AMPERSAND:    
        case TokenType::TOKEN_PIPE:
        case TokenType::TOKEN_CARET:
        case TokenType::TOKEN_LESS_LESS:
        case TokenType::TOKEN_GREATER_GREATER:
        case TokenType::TOKEN_CARET_CARET:
            return parseBinary(std::move(left));
            
        case TokenType::TOKEN_EQUAL:
        case TokenType::TOKEN_PLUS_EQUAL:
        case TokenType::TOKEN_MINUS_EQUAL:
        case TokenType::TOKEN_STAR_EQUAL:
        case TokenType::TOKEN_SLASH_EQUAL:
        case TokenType::TOKEN_PERCENT_EQUAL:
        case TokenType::TOKEN_LESS_LESS_EQUAL:
        case TokenType::TOKEN_GREATER_GREATER_EQUAL:
        case TokenType::TOKEN_AMPERSAND_EQUAL:
        case TokenType::TOKEN_PIPE_EQUAL:
        case TokenType::TOKEN_CARET_EQUAL:
            // Assignments are right-associative, so we'll handle them slightly differently in parseBinary
            return parseBinary(std::move(left));

        case TokenType::TOKEN_QUESTION:     return parseTernary(std::move(left));
        case TokenType::TOKEN_LPAREN:       return parseCall(std::move(left));
        case TokenType::TOKEN_DOT:          return parseMemberAccess(std::move(left));
        case TokenType::TOKEN_LBRACKET:     return parseArrayAccess(std::move(left));

        case TokenType::TOKEN_PLUS_PLUS:
        case TokenType::TOKEN_MINUS_MINUS: 
        {
            auto node = std::make_unique<UnaryOpNode>();
            node->op = type;
            node->operand = std::move(left);
            node->isPostfix = true;
            
            node->range.startLine = node->operand->range.startLine;
            node->range.startCol = node->operand->range.startCol;
            node->range.endLine = previous_token.line;
            node->range.endCol = previous_token.column + previous_token.length;
            
            return node;
        }

        default:
            reportError("Unexpected infix token.");
            return left;
    }
}

std::unique_ptr<ExpressionNode> Parser::parseNumber() 
{
    auto node = std::make_unique<LiteralNode>();
    node->type = TokenType::TOKEN_NUMBER;
    node->value = previous_token.value;
    setRange(node.get(), previous_token, previous_token);
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseString() 
{
    auto node = std::make_unique<LiteralNode>();
    node->type = TokenType::TOKEN_STRING;
    node->value = previous_token.value;
    setRange(node.get(), previous_token, previous_token);
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseBoolean() 
{
    auto node = std::make_unique<LiteralNode>();
    node->type = previous_token.type;
    node->value = previous_token.value;
    setRange(node.get(), previous_token, previous_token);
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseIdentifier() 
{
    auto node = std::make_unique<IdentifierNode>();
    node->name = previous_token.value;
    setRange(node.get(), previous_token, previous_token);
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseGrouping() 
{
    auto expr = parseExpression(); // Parse the expression inside the parens
    consume(TokenType::TOKEN_RPAREN, "Expected ')' after expression.");
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parseUnary() 
{
    Token start = previous_token;
    auto node = std::make_unique<UnaryOpNode>();
    node->op = start.type;
    node->isPostfix = false;
    
    // Parse the operand with Unary precedence
    node->operand = parsePrecedence(PREC_UNARY);

    if (node->operand) {
        node->range.startLine = start.line;
        node->range.startCol  = start.column;
        node->range.endLine   = node->operand->range.endLine;
        node->range.endCol    = node->operand->range.endCol;
    }
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseBinary(std::unique_ptr<ExpressionNode> left) 
{
    auto node = std::make_unique<BinaryOpNode>();
    GDSHADER_RETURN_VAL_IF(left == nullptr, node, "parseBinary failed with emtpy left hand node");
    
    node->op = previous_token.type;
    node->left = std::move(left);

    Precedence precedence = getPrecedence(node->op);
    
    // Check for right-associativity (Assignments)
    // If it's right-associative, we parse with slightly lower precedence to allow chaining: a = b = c
    if (precedence == PREC_ASSIGNMENT) {
        node->right = parsePrecedence(static_cast<Precedence>(precedence));
    } else {
        // Left-associative (standard)
        node->right = parsePrecedence(static_cast<Precedence>(precedence + 1));
    }

    mergeBinaryRange(node.get());
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseTernary(std::unique_ptr<ExpressionNode> left) 
{
    auto node = std::make_unique<TernaryNode>();
    GDSHADER_RETURN_VAL_IF(left == nullptr, node, "parseTernary failed with emtpy left hand node");

    node->condition = std::move(left);
    
    node->trueExpr = parseExpression();
    consume(TokenType::TOKEN_COLON, "Expected ':' in ternary operator");
    
    // Ternary is also usually right-associative
    node->falseExpr = parsePrecedence(static_cast<Precedence>(PREC_TERNARY));
    
    if (node->falseExpr) {
        node->range.endLine = node->falseExpr->range.endLine;
        node->range.endCol  = node->falseExpr->range.endCol;
    }
    
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseCall(std::unique_ptr<ExpressionNode> left) 
{
    auto callNode = std::make_unique<FunctionCallNode>();
    GDSHADER_RETURN_VAL_IF(left == nullptr, callNode, "parseCall failed with emtpy left hand node");

    callNode->range.startLine = left->range.startLine;
    callNode->range.startCol  = left->range.startCol;
    
    // Extract name if the left side was an identifier
    if (auto id = dynamic_cast<IdentifierNode*>(left.get())) {
        callNode->functionName = id->name;
        callNode->nameRange = id->range;
    } else {
        callNode->functionName = "unknown";
    }

    if (!check(TokenType::TOKEN_RPAREN)) {
        do {
            callNode->arguments.push_back(parseExpression());
        } while (match(TokenType::TOKEN_COMMA));
    }
    consume(TokenType::TOKEN_RPAREN, "Expected ')' after arguments");

    callNode->range.endLine = previous_token.line;
    callNode->range.endCol  = previous_token.column + previous_token.length;

    return callNode;
}

std::unique_ptr<ExpressionNode> Parser::parseMemberAccess(std::unique_ptr<ExpressionNode> left) 
{
    auto dotNode = std::make_unique<MemberAccessNode>();
    GDSHADER_RETURN_VAL_IF(left == nullptr, dotNode, "parseMemberAccess failed with emtpy left hand node");

    dotNode->base = std::move(left);
    
    dotNode->range.startLine = dotNode->base->range.startLine;
    dotNode->range.startCol  = dotNode->base->range.startCol;
    
    if (check(TokenType::TOKEN_IDENTIFIER)) {
        dotNode->member = current_token.value;
        advance();

        dotNode->range.endLine = previous_token.line;
        dotNode->range.endCol  = previous_token.column + previous_token.length;
    } else {
        reportError("Expected property name after '.'");

        dotNode->range.endLine = previous_token.line;
        dotNode->range.endCol  = previous_token.column + previous_token.length;
    }
    return dotNode;
}

std::unique_ptr<ExpressionNode> Parser::parseArrayAccess(std::unique_ptr<ExpressionNode> left) 
{
    auto indexNode = std::make_unique<ArrayAccessNode>();
    GDSHADER_RETURN_VAL_IF(left == nullptr, indexNode, "parseArrayAccess failed with emtpy left hand node");

    indexNode->base = std::move(left);
    
    indexNode->range.startLine = indexNode->base->range.startLine;
    indexNode->range.startCol  = indexNode->base->range.startCol;

    indexNode->index = parseExpression();
    consume(TokenType::TOKEN_RBRACKET, "Expected ']'");

    indexNode->range.endLine = previous_token.line;
    indexNode->range.endCol  = previous_token.column + previous_token.length;

    return indexNode;
}

} // namespace gdshader_lsp