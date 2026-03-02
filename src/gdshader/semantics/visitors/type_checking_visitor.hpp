#ifndef TYPE_CHECKING_VISITOR_HPP
#define TYPE_CHECKING_VISITOR_HPP

#include "gdshader/ast/ast_visitor.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include "gdshader/diagnostics.hpp"
#include "gdshader/lexer/lexer_types.h"

namespace gdshader_lsp 
{
    class ASTNode;
    class ExpressionNode;
}

namespace gdshader_lsp 
{

class TypeCheckingVisitor : public ASTVisitor 
{

private:

    SymbolTable& symbols;
    TypeRegistry& typeRegistry;
    std::vector<Diagnostic>& diagnostics;
    std::vector<RawToken>& tokens; // For semantic highlighting

    // --- State ---
    ShaderStage currentProcessorFunction = ShaderStage::Global;
    TypePtr currentExpectedReturnType = nullptr;

    // --- Helpers (Ported from SemanticAnalyzer) ---
    TypePtr resolveType(const ExpressionNode* node);
    TypePtr getBinaryOpResultType(TypePtr l, TypePtr r, TokenType op);
    int getConversionCost(TypePtr from, TypePtr to);
    const Symbol* getRootSymbol(const ExpressionNode* node);
    const Symbol* findBestOverload(const FunctionCallNode* node, const std::vector<TypePtr>& argTypes);
    void validateConstructor(const FunctionCallNode* node, const std::string& typeName);
    void reportTypeMismatch(const ASTNode* node, const std::string& expected, const std::string& found);

    void visitAssignment(const BinaryOpNode* node);

public:

    TypeCheckingVisitor(SymbolTable& syms, TypeRegistry& types, std::vector<Diagnostic>& diags, std::vector<RawToken>& toks)
        : symbols(syms), typeRegistry(types), diagnostics(diags), tokens(toks) {}

    // Pass-throughs (Just visit children)
    void visit(ProgramNode* node) override;
    void visit(BlockNode* node) override;
    void visit(ForNode* node) override;
    void visit(WhileNode* node) override;
    void visit(DoWhileNode* node) override;
    void visit(ExpressionStatementNode* node) override;

    // Declarations & Statements
    void visit(FunctionNode* node) override;
    void visit(VariableDeclNode* node) override;
    void visit(UniformNode* node) override;
    void visit(ConstNode* node) override;
    void visit(IfNode* node) override;
    void visit(SwitchNode* node) override;
    void visit(ReturnNode* node) override;
    void visit(DiscardNode* node) override {};

    // Expressions
    void visit(BinaryOpNode* node) override;
    void visit(UnaryOpNode* node) override;
    void visit(FunctionCallNode* node) override;
    void visit(MemberAccessNode* node) override;
    void visit(ArrayAccessNode* node) override;
    void visit(IdentifierNode* node) override;
    
    // Ignored in this pass
    void visit(TypeNode* node) override {}
    void visit(LiteralNode* node) override {}
    void visit(TernaryNode* node) override { /* Add logic if needed */ }
    void visit(ConstructorNode* node) override {}
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
    void visit(VaryingNode* node) override {}
    void visit(StructNode* node) override {}
};

} // namespace gdshader_lsp

#endif