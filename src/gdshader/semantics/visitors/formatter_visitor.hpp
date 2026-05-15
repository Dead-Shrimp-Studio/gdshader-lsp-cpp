#ifndef FORMATTER_VISITOR_HPP
#define FORMATTER_VISITOR_HPP

#include "gdshader/ast/ast_visitor.hpp"
#include <sstream>
#include <string>

namespace gdshader_lsp 
{
    // These align with the LSP DocumentFormattingParams
    struct FormattingOptions {
        int tabSize = 4;
        bool insertSpaces = true;
    };

    class FormatterVisitor : public ASTVisitor 
    {
        private:
            
            std::ostringstream out;
            int indentLevel = 0;
            FormattingOptions options;

            // --- Formatting Helpers ---
            void printIndent();
            void printLeadingComments(const ASTNode* node);
            void printTrailingComments(const ASTNode* node);
            std::string getOpString(TokenType op);

            bool inControlHeader = false;
            void formatIf(IfNode* node, bool isElseIf);

        public:

            FormatterVisitor(FormattingOptions opts = {}) : options(opts) {}

            // Call this after accepting the visitor on the root ProgramNode
            std::string getFormattedCode() const { return out.str(); }

            // --- Base Types ---
            void visit(TypeNode* node) override;

            // --- Expressions ---
            void visit(LiteralNode* node) override;
            void visit(IdentifierNode* node) override;
            void visit(BinaryOpNode* node) override;
            void visit(UnaryOpNode* node) override;
            void visit(FunctionCallNode* node) override;
            void visit(ConstructorNode* node) override;
            void visit(MemberAccessNode* node) override;
            void visit(ArrayAccessNode* node) override;
            void visit(TernaryNode* node) override;

            // --- Statements ---
            void visit(BlockNode* node) override;
            void visit(ExpressionStatementNode* node) override;
            void visit(VariableDeclNode* node) override;
            void visit(ParameterNode* node) override;
            void visit(StructMemberNode* node) override;
            void visit(IfNode* node) override;
            void visit(WhileNode* node) override;
            void visit(ForNode* node) override;
            void visit(ReturnNode* node) override;
            void visit(DoWhileNode* node) override;
            void visit(CaseNode* node) override;
            void visit(SwitchNode* node) override;
            void visit(DiscardNode* node) override;
            void visit(BreakNode* node) override;
            void visit(ContinueNode* node) override;
            
            // --- Preprocessing ---
            void visit(DefineNode* node) override;
            void visit(IncludeNode* node) override;

            // --- Top Level Declarations ---
            void visit(ShaderTypeNode* node) override;
            void visit(RenderModeNode* node) override;
            void visit(GroupUniformsNode* node) override;
            void visit(UniformNode* node) override;
            void visit(VaryingNode* node) override;
            void visit(ConstNode* node) override;
            void visit(StructNode* node) override;
            void visit(FunctionNode* node) override;

            // --- Root ---
            void visit(ProgramNode* node) override;
    };

} // namespace gdshader_lsp

#endif // FORMATTER_VISITOR_HPP