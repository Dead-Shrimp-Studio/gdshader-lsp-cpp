#include "gdshader/semantics/visitors/formatter_visitor.hpp"
#include "gdshader/ast/ast.h"

namespace gdshader_lsp 
{

    void FormatterVisitor::printIndent() 
    {
        if (options.insertSpaces) {
            out << std::string(indentLevel * options.tabSize, ' ');
        } else {
            out << std::string(indentLevel, '\t');
        }
    }

    void FormatterVisitor::printLeadingComments(const ASTNode* node) 
    {
        if (!node) return;
        for (const auto& comment : node->leadingComments) {
            printIndent();
            out << comment.value << "\n";
        }
    }

    void FormatterVisitor::printTrailingComments(const ASTNode* node) 
    {
        if (!node || node->trailingComments.empty()) {
            out << "\n"; // If no trailing comments, just cap off the line
            return;
        }

        // Print comments on the same line
        for (const auto& comment : node->trailingComments) {
            out << " " << comment.value;
        }
        out << "\n";
    }

    void FormatterVisitor::formatIf(IfNode* node, bool isElseIf) 
    {
        if (!node) return;
        
        if (!isElseIf) {
            printLeadingComments(node);
            printIndent();
        }
        
        out << "if (";
        if (node->condition) node->condition->accept(*this);
        out << ") ";

        // Handle single-line vs block bodies
        if (node->thenBranch) {
            if (dynamic_cast<BlockNode*>(node->thenBranch.get())) {
                node->thenBranch->accept(*this);
            } else {
                out << "\n";
                indentLevel++;
                node->thenBranch->accept(*this);
                indentLevel--;
            }
        }

        if (node->elseBranch) {
            if (dynamic_cast<BlockNode*>(node->thenBranch.get())) {
                out << " else ";
            } else {
                out << "\n";
                printIndent();
                out << "else ";
            }
            
            // Recursively handle 'else if' chains to keep them on the same line
            if (auto nextIf = dynamic_cast<IfNode*>(node->elseBranch.get())) {
                formatIf(nextIf, true);
            } else if (dynamic_cast<BlockNode*>(node->elseBranch.get())) {
                node->elseBranch->accept(*this);
            } else {
                out << "\n";
                indentLevel++;
                node->elseBranch->accept(*this);
                indentLevel--;
            }
        }
        
        if (!isElseIf) {
            printTrailingComments(node);
        }
    }

    std::string FormatterVisitor::getOpString(TokenType op) 
    {
        switch (op) {
            case TokenType::TOKEN_PLUS: return "+";
            case TokenType::TOKEN_MINUS: return "-";
            case TokenType::TOKEN_STAR: return "*";
            case TokenType::TOKEN_SLASH: return "/";
            case TokenType::TOKEN_PERCENT: return "%";
            case TokenType::TOKEN_PLUS_EQUAL: return "+=";
            case TokenType::TOKEN_MINUS_EQUAL: return "-=";
            case TokenType::TOKEN_STAR_EQUAL: return "*=";
            case TokenType::TOKEN_SLASH_EQUAL: return "/=";
            case TokenType::TOKEN_PERCENT_EQUAL: return "%=";
            case TokenType::TOKEN_EQUAL: return "=";
            case TokenType::TOKEN_EQ_EQ: return "==";
            case TokenType::TOKEN_NOT_EQ: return "!=";
            case TokenType::TOKEN_LESS: return "<";
            case TokenType::TOKEN_LESS_EQ: return "<=";
            case TokenType::TOKEN_GREATER: return ">";
            case TokenType::TOKEN_GREATER_EQ: return ">=";
            case TokenType::TOKEN_AND: return "&&";
            case TokenType::TOKEN_OR: return "||";
            case TokenType::TOKEN_AMPERSAND: return "&";
            case TokenType::TOKEN_PIPE: return "|";
            case TokenType::TOKEN_CARET: return "^";
            case TokenType::TOKEN_TILDE: return "~";
            case TokenType::TOKEN_PLUS_PLUS: return "++";
            case TokenType::TOKEN_MINUS_MINUS: return "--";
            case TokenType::TOKEN_LESS_LESS: return "<<";
            case TokenType::TOKEN_GREATER_GREATER: return ">>";
            case TokenType::TOKEN_LESS_LESS_EQUAL: return "<<=";
            case TokenType::TOKEN_GREATER_GREATER_EQUAL: return ">>=";
            case TokenType::TOKEN_AMPERSAND_EQUAL: return "&=";
            case TokenType::TOKEN_PIPE_EQUAL: return "|=";
            case TokenType::TOKEN_CARET_EQUAL: return "^=";
            case TokenType::TOKEN_CARET_CARET: return "^^";
            case TokenType::TOKEN_EXCL: return "!";
            default: return "";
        }
    }

    // =========================================================================
    // ROOT & STRUCTURAL NODES
    // =========================================================================

    void FormatterVisitor::visit(ProgramNode* node) 
    {
        if (!node) return;
        
        // Print any file-header comments
        printLeadingComments(node);

        for (size_t i = 0; i < node->nodes.size(); ++i) {
            if (node->nodes[i]) {
                node->nodes[i]->accept(*this);
                
                // Add an extra blank line between top-level declarations (like functions)
                // But don't add it after the very last node.
                if (i < node->nodes.size() - 1) {
                    out << "\n";
                }
            }
        }
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(FunctionNode* node) 
    {
        if (!node) return;
        
        printLeadingComments(node);
        printIndent();
        
        // e.g. "void fragment("
        out << node->returnType->toString() << " " << node->name << "(";
        
        // Print Parameters
        for (size_t i = 0; i < node->parameters.size(); ++i) {
            node->parameters[i]->accept(*this);
            if (i < node->parameters.size() - 1) {
                out << ", ";
            }
        }
        out << ") ";
        
        // If it's a function definition, print the body
        if (node->body) {
            node->body->accept(*this);
        } else {
            out << ";";
            printTrailingComments(node);
        }
    }

    void FormatterVisitor::visit(ParameterNode* node) 
    {
        // Parameters are inline, so we don't print indents or newlines!
        if (!node->qualifier.empty()) {
            out << node->qualifier << " ";
        }
        out << node->type->toString() << " " << node->name;
    }

    void FormatterVisitor::visit(BlockNode* node) 
    {
        if (!node) return;
        
        out << "{";
        printTrailingComments(node); // Usually just a newline, or `{ // start loop`
        
        indentLevel++;
        
        for (const auto& stmt : node->statements) {
            if (stmt) {
                stmt->accept(*this);
            }
        }
        
        indentLevel--;
        
        printIndent();
        out << "}";
        
        // We intentionally don't print a newline here because Blocks are often 
        // part of 'if/else' chains. The parent node will handle the newline
    }

    // =========================================================================
    // STATEMENTS
    // =========================================================================

    void FormatterVisitor::visit(VariableDeclNode* node) 
    {
        if (!node) return;
        
        // Suppress leading comments and indents if inside a 'for' loop header
        if (!inControlHeader) {
            printLeadingComments(node);
            printIndent();
        }
        
        if (node->isConst) out << "const ";
        
        out << node->type->toString() << " " << node->name;
        
        if (node->initializer) {
            out << " = ";
            node->initializer->accept(*this);
        }
        
        out << ";";
        
        // Suppress trailing newlines if inside a 'for' loop header
        if (!inControlHeader) printTrailingComments(node);
    }

    void FormatterVisitor::visit(ExpressionStatementNode* node) 
    {
        if (!node) return;
        
        if (!inControlHeader) {
            printLeadingComments(node);
            printIndent();
        }
        
        if (node->expr) node->expr->accept(*this);
        
        out << ";";
        if (!inControlHeader) printTrailingComments(node);
    }

    // =========================================================================
    // EXPRESSIONS
    // =========================================================================

    void FormatterVisitor::visit(TypeNode* node) {
        if (node) out << node->toString();
    }

    void FormatterVisitor::visit(LiteralNode* node) {
        if (node) out << node->value;
    }

    void FormatterVisitor::visit(IdentifierNode* node) {
        if (node) out << node->name;
    }

    void FormatterVisitor::visit(BinaryOpNode* node) {
        if (!node) return;
        
        if (node->left) node->left->accept(*this);
        
        // Add nice spacing around binary operators
        out << " " << getOpString(node->op) << " ";
        
        if (node->right) node->right->accept(*this);
    }

    void FormatterVisitor::visit(UnaryOpNode* node) {
        if (!node) return;
        
        // Unary operators sit flush against their operands (e.g. -1.0 or i++)
        if (!node->isPostfix) out << getOpString(node->op);
        if (node->operand) node->operand->accept(*this);
        if (node->isPostfix) out << getOpString(node->op);
    }

    void FormatterVisitor::visit(FunctionCallNode* node) {
        if (!node) return;
        
        out << node->functionName << "(";
        for (size_t i = 0; i < node->arguments.size(); ++i) {
            if (node->arguments[i]) node->arguments[i]->accept(*this);
            
            // Add a space after the comma for multiple arguments
            if (i < node->arguments.size() - 1) out << ", ";
        }
        out << ")";
    }

    void FormatterVisitor::visit(ConstructorNode* node) {
        if (!node) return;
        
        out << node->typeName << "(";
        for (size_t i = 0; i < node->arguments.size(); ++i) {
            if (node->arguments[i]) node->arguments[i]->accept(*this);
            if (i < node->arguments.size() - 1) out << ", ";
        }
        out << ")";
    }

    void FormatterVisitor::visit(MemberAccessNode* node) {
        if (!node) return;
        
        if (node->base) node->base->accept(*this);
        out << "." << node->member;
    }

    void FormatterVisitor::visit(ArrayAccessNode* node) {
        if (!node) return;
        
        if (node->base) node->base->accept(*this);
        out << "[";
        if (node->index) node->index->accept(*this);
        out << "]";
    }

    void FormatterVisitor::visit(TernaryNode* node) {
        if (!node) return;
        
        if (node->condition) node->condition->accept(*this);
        out << " ? ";
        if (node->trueExpr) node->trueExpr->accept(*this);
        out << " : ";
        if (node->falseExpr) node->falseExpr->accept(*this);
    }

    // =========================================================================
    // CONTROL FLOW
    // =========================================================================

    void FormatterVisitor::visit(IfNode* node) 
    {
        formatIf(node, false);
    }

    void FormatterVisitor::visit(WhileNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "while (";
        if (node->condition) node->condition->accept(*this);
        out << ") ";

        if (node->body) {
            if (dynamic_cast<BlockNode*>(node->body.get())) {
                node->body->accept(*this);
            } else {
                out << "\n";
                indentLevel++;
                node->body->accept(*this);
                indentLevel--;
            }
        }
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(DoWhileNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "do ";
        
        if (node->body) {
            if (dynamic_cast<BlockNode*>(node->body.get())) {
                node->body->accept(*this);
            } else {
                out << "\n";
                indentLevel++;
                node->body->accept(*this);
                indentLevel--;
                out << "\n";
                printIndent();
            }
        }
        
        out << " while (";
        if (node->condition) node->condition->accept(*this);
        out << ");";
        
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(ForNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "for (";
        
        // Temporarily disable indents/newlines for the init statement
        inControlHeader = true;
        if (node->init) node->init->accept(*this);
        out << " ";
        
        if (node->condition) node->condition->accept(*this);
        out << "; ";
        
        if (node->increment) node->increment->accept(*this);
        inControlHeader = false;
        
        out << ") ";
        
        if (node->body) {
            if (dynamic_cast<BlockNode*>(node->body.get())) {
                node->body->accept(*this);
            } else {
                out << "\n";
                indentLevel++;
                node->body->accept(*this);
                indentLevel--;
            }
        }
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(SwitchNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "switch (";
        if (node->expression) node->expression->accept(*this);
        out << ") {\n";
        
        indentLevel++;
        for (const auto& c : node->cases) {
            if (c) c->accept(*this);
        }
        indentLevel--;
        
        printIndent();
        out << "}";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(CaseNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        if (node->isDefault) {
            out << "default:\n";
        } else {
            out << "case ";
            if (node->value) node->value->accept(*this);
            out << ":\n";
        }
        
        indentLevel++;
        for (const auto& stmt : node->statements) {
            if (stmt) stmt->accept(*this);
        }
        indentLevel--;
    }

    void FormatterVisitor::visit(ReturnNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "return";
        if (node->value) {
            out << " ";
            node->value->accept(*this);
        }
        out << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(BreakNode* node) {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        out << "break;";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(ContinueNode* node) {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        out << "continue;";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(DiscardNode* node) {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        out << "discard;";
        printTrailingComments(node);
    }

    // =========================================================================
    // TOP-LEVEL DECLARATIONS & PREPROCESSOR
    // =========================================================================

    void FormatterVisitor::visit(ShaderTypeNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        out << "shader_type " << node->shaderType << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(RenderModeNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "render_mode ";
        for (size_t i = 0; i < node->modes.size(); ++i) {
            out << node->modes[i];
            if (i < node->modes.size() - 1) {
                out << ", ";
            }
        }
        out << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(StructNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "struct " << node->name << " {\n";
        
        indentLevel++;
        for (const auto& member : node->members) {
            if (member) member->accept(*this);
        }
        indentLevel--;
        
        printIndent();
        out << "};";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(StructMemberNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << node->type->toString() << " " << node->name << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(GroupUniformsNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "group_uniforms " << node->name;
        out << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(UniformNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        if (node->isInstance) out << "instance ";
        out << "uniform " << node->type->toString() << " " << node->name;
        
        // Check for hints like: hint_range(0.0, 1.0)
        if (!node->hint.empty()) {
            out << " : " << node->hint;
        }

        if (node->defaultValue) {
            out << " = ";
            node->defaultValue->accept(*this);
        }
        
        out << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(VaryingNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "varying ";
        if (!node->interpolation.empty()) {
            out << node->interpolation << " "; // e.g., "flat" or "smooth"
        }
        out << node->type->toString() << " " << node->name << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(ConstNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "const " << node->type->toString() << " " << node->name;
        if (node->value) {
            out << " = ";
            node->value->accept(*this);
        }
        out << ";";
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(DefineNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "#define " << node->name;
        if (node->value) {
            out << " ";
            node->value->accept(*this);
        }
        
        // Preprocessor directives don't have semicolons, but printTrailingComments 
        // will safely add the required \n for us.
        printTrailingComments(node);
    }

    void FormatterVisitor::visit(IncludeNode* node) 
    {
        if (!node) return;
        printLeadingComments(node);
        printIndent();
        
        out << "#include \"" << node->path << "\"";
        printTrailingComments(node);
    }

} // namespace gdshader_lsp