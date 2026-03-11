#include "gdshader/parser/parser.hpp"
#include "server/project_manager.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

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

} // namespace gdshader_lsp