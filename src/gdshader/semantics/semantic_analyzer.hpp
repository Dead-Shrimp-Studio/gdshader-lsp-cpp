#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include "gdshader/parser/parser.hpp"

#include "gdshader/builtins.hpp"

namespace gdshader_lsp {

struct AnalysisResult {
    SymbolTable symbols;
    TypeRegistry types;
    std::vector<Diagnostic> diagnostics;
};

class SemanticAnalyzer {

private:

    SymbolTable symbols;
    TypeRegistry typeRegistry;

    std::vector<Diagnostic> diagnostics;

    // State
    
    std::string currentFilePath;

    ShaderType currentShaderType = ShaderType::Spatial; // Default
    ShaderStage currentProcessorFunction = ShaderStage::Global;
    
    TypePtr currentExpectedReturnType = std::shared_ptr<Type>();
    bool currentFunctionHasReturn = false;

    void visit(const ASTNode* node);

    // Top Level

    void visitProgram(const ProgramNode* node);
    void visitInclude(const IncludeNode* node);
    void visitDefine(const DefineNode* node);
    void visitShaderType(const ShaderTypeNode* node);
    void visitUniform(const UniformNode* node);
    void visitVarying(const VaryingNode* node);
    void visitConst(const ConstNode* node);
    void visitStruct(const StructNode* node);
    void visitFunction(const FunctionNode* node);
    
    // Statements

    void visitBlock(const BlockNode* node);
    void visitVarDecl(const VariableDeclNode* node);
    void visitIf(const IfNode* node);
    void visitFor(const ForNode* node);
    void visitWhile(const WhileNode* node);
    void visitReturn(const ReturnNode* node);
    void visitExpressionStatement(const ExpressionStatementNode* node);
    void visitDoWhile(const DoWhileNode* node);
    void visitSwitch(const SwitchNode* node);

    void visitDiscard(const DiscardNode* node);

    // Expressions

    void visitExpression(const ExpressionNode* node);
    void visitIdentifier(const IdentifierNode* node);
    void visitBinaryOp(const BinaryOpNode* node);
    void visitUnaryOp(const UnaryOpNode* node);
    void visitFunctionCall(const FunctionCallNode* node);
    void visitMemberAccess(const MemberAccessNode* node);
    void visitAssignment(const BinaryOpNode* node); // Special case of BinaryOp
    void visitArrayAccess(const ArrayAccessNode* node);

    // Helper

    void validateConstructor(const FunctionCallNode* node, const std::string& typeName);

    bool isProcessorFunction(const std::string& name);
    bool allPathsReturn(const ASTNode* node);

    TypePtr getBinaryOpResultType(TypePtr l, TypePtr r, TokenType op);
    const Symbol* findBestOverload(const FunctionCallNode* node, const std::vector<TypePtr>& argTypes);
    int getConversionCost(TypePtr from, TypePtr to);
    const Symbol* getRootSymbol(const ExpressionNode* node, const SymbolTable& symbols);    
    int getNodeLength(const ASTNode* node);

    void reportError(const ASTNode* node, const std::string& msg);
    void reportWarning(const ASTNode* node, const std::string& msg);
    void reportTypeMismatch(const ASTNode* node, const std::string& expected, const std::string& found);

    void loadBuiltinsForFunction(const std::string& funcName);
    TypePtr resolveType(const ExpressionNode* node);

    void registerGlobalFunctions();

public:

    /**
     * @brief The main function for semantic anlysis. Return an AnalysisResult object containing a symbol table, type registry and diagnostic message array.
     * 
     * @param ast 
     * @return AnalysisResult 
     */
    AnalysisResult analyze(const ProgramNode* ast);
    
    void setFilePath(const std::string& path) { currentFilePath = path; }

};

}

#endif