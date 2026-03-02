#ifndef PREPROCESSOR_VISITOR_HPP
#define PREPROCESSOR_VISITOR_HPP

#include "gdshader/ast/ast_visitor.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include "gdshader/diagnostics.hpp"

#include <unordered_set>
#include <string>

namespace gdshader_lsp 
{
    struct ExpressionNode;
}

namespace gdshader_lsp {

class PreprocessorVisitor : public ASTVisitor {
private:

    SymbolTable& symbols;
    TypeRegistry& typeRegistry;
    std::vector<Diagnostic>& diagnostics;
    std::string currentFilePath;
    std::unordered_set<std::string>& processedFiles;

    // Helper ported from your original analyzer to resolve macro literal types
    TypePtr resolveType(const ExpressionNode* node);

public:
    PreprocessorVisitor(SymbolTable& syms, TypeRegistry& types, std::vector<Diagnostic>& diags, 
                        const std::string& filePath, std::unordered_set<std::string>& processed)
        : symbols(syms), typeRegistry(types), diagnostics(diags), 
          currentFilePath(filePath), processedFiles(processed) {}

    // --- Structural Pass-throughs ---
    void visit(ProgramNode* node) override;
    void visit(BlockNode* node) override;

    // --- The Targets ---
    void visit(IncludeNode* node) override;
    void visit(DefineNode* node) override;

    // --- Ignored Nodes (No-ops) ---
    void visit(TypeNode* node) override {}
    void visit(LiteralNode* node) override {}
    void visit(IdentifierNode* node) override {}
    void visit(BinaryOpNode* node) override {}
    void visit(UnaryOpNode* node) override {}
    void visit(FunctionCallNode* node) override {}
    void visit(ConstructorNode* node) override {}
    void visit(MemberAccessNode* node) override {}
    void visit(ArrayAccessNode* node) override {}
    void visit(TernaryNode* node) override {}
    void visit(ExpressionStatementNode* node) override {}
    void visit(VariableDeclNode* node) override {}
    void visit(ParameterNode* node) override {}
    void visit(StructMemberNode* node) override {}
    void visit(IfNode* node) override {}
    void visit(WhileNode* node) override {}
    void visit(ForNode* node) override {}
    void visit(ReturnNode* node) override {}
    void visit(DoWhileNode* node) override {}
    void visit(CaseNode* node) override {}
    void visit(SwitchNode* node) override {}
    void visit(DiscardNode* node) override {}
    void visit(BreakNode* node) override {}
    void visit(ContinueNode* node) override {}
    void visit(ShaderTypeNode* node) override {}
    void visit(RenderModeNode* node) override {}
    void visit(GroupUniformsNode* node) override {}
    void visit(UniformNode* node) override {}
    void visit(VaryingNode* node) override {}
    void visit(ConstNode* node) override {}
    void visit(StructNode* node) override {}
    void visit(FunctionNode* node) override {}
};

} // namespace gdshader_lsp

#endif