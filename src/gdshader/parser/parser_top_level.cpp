
#include "gdshader/parser/parser.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

// -------------------------------------------------------------------------
// TOP LEVEL
// -------------------------------------------------------------------------

std::unique_ptr<ASTNode> Parser::parseTopLevelDecl() 
{
    try {

        if (match(TokenType::TOKEN_PREPROCESSOR))       return parsePreprocessor();
        if (match(TokenType::KEYWORD_SHADER_TYPE))      return parseShaderType();
        if (match(TokenType::KEYWORD_RENDER_MODE))      return parseRenderMode();
        if (match(TokenType::KEYWORD_GROUP_UNIFORMS))   return parseGroupUniform();
        if (match(TokenType::KEYWORD_UNIFORM))          return parseUniform();
        if (match(TokenType::KEYWORD_INSTANCE)) {
            consume(TokenType::KEYWORD_UNIFORM, "Expected 'uniform' after 'instance'");
            auto node = parseUniform();
            if (auto* uniNode = dynamic_cast<UniformNode*>(node.get())) {
                uniNode->isInstance = true; 
            }
            return node;
        }

        if (match(TokenType::KEYWORD_FLAT) || match(TokenType::KEYWORD_SMOOTH)) {
            std::string interp = previous_token.value;
            consume(TokenType::KEYWORD_VARYING, "Expected 'varying' after interpolation qualifier");
            auto node = parseVarying();
            if (auto* varNode = dynamic_cast<VaryingNode*>(node.get())) {
                varNode->interpolation = interp; 
            }
            return node;
        }
        
        if (match(TokenType::KEYWORD_IN) || match(TokenType::KEYWORD_OUT) || match(TokenType::KEYWORD_INOUT)) {
            reportError("Expected return type for function declaration.");
            return nullptr;
        }
        if (match(TokenType::KEYWORD_VARYING))          return parseVarying();
        if (match(TokenType::KEYWORD_CONST))            return parseConst();
        if (match(TokenType::KEYWORD_STRUCT))           return parseStruct();
        
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

std::unique_ptr<ASTNode> gdshader_lsp::Parser::parseGroupUniform()
{
    Token start = previous_token; // We already consumed KEYWORD_GROUP_UNIFORMS
    auto node = std::make_unique<GroupUniformsNode>();

    if (check(TokenType::TOKEN_IDENTIFIER)) {
        node->name = current_token.value;
        advance();

        // Handle subgroups: MyGroup.SubGroup.Another
        while (match(TokenType::TOKEN_DOT)) {
            node->name += ".";
            if (check(TokenType::TOKEN_IDENTIFIER)) {
                node->name += current_token.value;
                advance();
            } else {
                reportError("Expected identifier after '.' in group name");
            }
        }
    } else {
        reportError("Expected identifier");
    }

    consume(TokenType::TOKEN_SEMI, "Expected ';' after group_uniforms");
    setRange(node.get(), start, previous_token);
    
    return node;
}

std::unique_ptr<ASTNode> Parser::parseUniform() 
{
    SPDLOG_TRACE("[Parser] Parsing Uniform decl");

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

    // Hint:  : hint_range(0, 1), source_color, filter_linear
    if (match(TokenType::TOKEN_COLON)) {
        
        // Loop to support multiple comma-separated hints
        while (check(TokenType::TOKEN_IDENTIFIER)) {
            
            if (!node->hint.empty()) {
                node->hint += ", ";
            }

            std::string hint_name = current_token.value;
            node->hint += current_token.value;
            advance();

            // If this specific hint has arguments
            if (match(TokenType::TOKEN_LPAREN)) {
                node->hint += "(";
                int depth = 1;
                
                std::vector<std::string> args;
                std::string current_arg = "";

                // Track depth to safely allow nested parentheses
                while (depth > 0 && !check(TokenType::TOKEN_EOF)) {
                    
                    // Safe-guard to prevent runaway consumption if ')' is missing
                    if (check(TokenType::TOKEN_SEMI) || check(TokenType::TOKEN_EQUAL)) {
                        break;
                    }

                    if (check(TokenType::TOKEN_LPAREN)) {
                        depth++;
                    } else if (check(TokenType::TOKEN_RPAREN)) {
                        depth--;
                        if (depth == 0) break; // Found the matching closing parenthesis
                    }

                    // Add a little formatting for readability in the hover text
                    if (check(TokenType::TOKEN_COMMA)) {
                        args.push_back(current_arg);
                        current_arg = "";
                        node->hint += ", ";
                    } else {
                        current_arg += current_token.value;
                        node->hint += current_token.value;
                    }
                    advance();
                }
                
                if (!current_arg.empty()) args.push_back(current_arg);
                consume(TokenType::TOKEN_RPAREN, "Expected ')' after hint arguments");
                node->hint += ")";

                if (hint_name == "hint_range" && args.size() >= 2) {
                    try {
                        float min_val = std::stof(args[0]);
                        float max_val = std::stof(args[1]);
                        if (min_val > max_val) {
                            reportError("hint_range min value must not be greater than max value.");
                        }
                    } catch (...) {} // Ignore parsing failures if they aren't numbers
                }
            }

            if (match(TokenType::TOKEN_COMMA)) continue;
            else break;
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
    SPDLOG_TRACE("[Parser] Parsing Struct decl");

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
    Range nameRange;

    if (check(TokenType::TOKEN_IDENTIFIER)) {
        name = current_token.value;

        nameRange.startLine = current_token.line;
        nameRange.startCol = current_token.column;
        nameRange.endLine = current_token.line;
        nameRange.endCol = current_token.column + current_token.length;
        advance();

        while (match(TokenType::TOKEN_LBRACKET)) {
            if (match(TokenType::TOKEN_NUMBER)) {
                try {
                    int size = std::stoi(previous_token.value);
                    if (size <= 0) reportError("Array size must be greater than 0.");
                } catch (...) {
                    reportError("Array size must be a valid integer.");
                }
            } else {
                reportError("Expected array size.");
            }
            consume(TokenType::TOKEN_RBRACKET, "Expected ']'");
        }

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
    SPDLOG_TRACE("[Parser] Parsing Function definition: '{}'", name);

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

    if (check(TokenType::TOKEN_LBRACE)) {
        node->body = parseBlock();
        node->is_function_definition = true;
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

} // namespace gdshader_lsp