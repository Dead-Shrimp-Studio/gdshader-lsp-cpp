#ifndef LINTING_VISITOR_HPP
#define LINTING_VISITOR_HPP

#include "gdshader/ast/ast_visitor.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include "gdshader/diagnostics.hpp"

namespace gdshader_lsp 
{
    class ASTNode;
}

namespace gdshader_lsp {

class LintingVisitor : public ASTVisitor {

private:

    SymbolTable& symbols;
    std::vector<Diagnostic>& diagnostics;

    // State
    std::string currentFunctionName = "";
    ShaderStage currentProcessorFunction = ShaderStage::Global;
    bool currentFunctionHasReturn = false;

    // Helpers ported from the old SemanticAnalyzer
    bool allPathsReturn(const ASTNode* node);

public:

    LintingVisitor(SymbolTable& syms, std::vector<Diagnostic>& diags)
        : symbols(syms), diagnostics(diags) {}

    // --- Active Checkers ---
    void visit(ProgramNode* node) override;
    void visit(FunctionNode* node) override;
    void visit(BlockNode* node) override;
    void visit(ReturnNode* node) override;
    void visit(FunctionCallNode* node) override;
    void visit(DiscardNode* node) override;

    // --- Pass-Throughs (Structural) ---
    void visit(IfNode* node) override;
    void visit(WhileNode* node) override;
    void visit(ForNode* node) override;
    void visit(DoWhileNode* node) override;
    void visit(SwitchNode* node) override;

    // --- Ignored (No-ops for this pass) ---
    void visit(TypeNode* node) override {}
    void visit(LiteralNode* node) override {}
    void visit(IdentifierNode* node) override {}
    void visit(BinaryOpNode* node) override {}
    void visit(UnaryOpNode* node) override {}
    void visit(ConstructorNode* node) override {}
    void visit(MemberAccessNode* node) override {}
    void visit(ArrayAccessNode* node) override {}
    void visit(TernaryNode* node) override {}
    void visit(ExpressionStatementNode* node) override {}
    void visit(VariableDeclNode* node) override {}
    void visit(ParameterNode* node) override {}
    void visit(StructMemberNode* node) override {}
    void visit(CaseNode* node) override {}
    void visit(BreakNode* node) override {}
    void visit(ContinueNode* node) override {}
    void visit(DefineNode* node) override {}
    void visit(IncludeNode* node) override {}
    void visit(ShaderTypeNode* node) override {}
    void visit(RenderModeNode* node) override {}
    void visit(GroupUniformsNode* node) override {}
    void visit(UniformNode* node) override {}
    void visit(VaryingNode* node) override {}
    void visit(ConstNode* node) override {}
    void visit(StructNode* node) override {}
};

} // namespace gdshader_lsp

#endif