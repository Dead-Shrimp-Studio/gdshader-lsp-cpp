#ifndef AST_VISITOR_HPP
#define AST_VISITOR_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/diagnostics.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp 
{
    class ASTVisitor 
    {
        public:

            virtual ~ASTVisitor() = default;

            // Base Types
            virtual void visit(TypeNode* node) = 0;

            // Expressions
            virtual void visit(LiteralNode* node) = 0;
            virtual void visit(IdentifierNode* node) = 0;
            virtual void visit(BinaryOpNode* node) = 0;
            virtual void visit(UnaryOpNode* node) = 0;
            virtual void visit(FunctionCallNode* node) = 0;
            virtual void visit(ConstructorNode* node) = 0;
            virtual void visit(MemberAccessNode* node) = 0;
            virtual void visit(ArrayAccessNode* node) = 0;
            virtual void visit(TernaryNode* node) = 0;

            // Statements
            virtual void visit(BlockNode* node) = 0;
            virtual void visit(ExpressionStatementNode* node) = 0;
            virtual void visit(VariableDeclNode* node) = 0;
            virtual void visit(ParameterNode* node) = 0;
            virtual void visit(StructMemberNode* node) = 0;
            virtual void visit(IfNode* node) = 0;
            virtual void visit(WhileNode* node) = 0;
            virtual void visit(ForNode* node) = 0;
            virtual void visit(ReturnNode* node) = 0;
            virtual void visit(DoWhileNode* node) = 0;
            virtual void visit(CaseNode* node) = 0;
            virtual void visit(SwitchNode* node) = 0;
            virtual void visit(DiscardNode* node) = 0;
            virtual void visit(BreakNode* node) = 0;
            virtual void visit(ContinueNode* node) = 0;
            
            // Preprocessing
            virtual void visit(DefineNode* node) = 0;
            virtual void visit(IncludeNode* node) = 0;

            // Top Level Declarations
            virtual void visit(ShaderTypeNode* node) = 0;
            virtual void visit(RenderModeNode* node) = 0;
            virtual void visit(GroupUniformsNode* node) = 0;
            virtual void visit(UniformNode* node) = 0;
            virtual void visit(VaryingNode* node) = 0;
            virtual void visit(ConstNode* node) = 0;
            virtual void visit(StructNode* node) = 0;
            virtual void visit(FunctionNode* node) = 0;

            // Root
            virtual void visit(ProgramNode* node) = 0;

            Diagnostic reportError(const ASTNode* node, const std::string& msg)
            {
                GDSHADER_ALWAYS_ASSERT(node != nullptr, "Node to report error on is nullptr.");
                SPDLOG_DEBUG("Reporting error: {}", msg);
                return {msg, DiagnosticLevel::Error, node->range};
            }

            Diagnostic reportWarning(const ASTNode* node, const std::string& msg)
            {
                GDSHADER_ALWAYS_ASSERT(node != nullptr, "Node to report warning on is nullptr.");
                SPDLOG_DEBUG("Reporting warning: {}", msg);
                return {msg, DiagnosticLevel::Warning, node->range};
            }

    };
} // namespace gdshader_lsp

#endif