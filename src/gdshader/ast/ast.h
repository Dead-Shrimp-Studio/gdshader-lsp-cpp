#ifndef AST_H
#define AST_H

#include <vector>
#include <string>
#include <memory>
#include <optional>

#include "gdshader/lexer/lexer_types.h"
#include "gdshader/diagnostics.hpp"

namespace gdshader_lsp 
{
    class ASTVisitor;
}

namespace gdshader_lsp {

// -------------------------------------------------------------------------
// BASE NODE
// -------------------------------------------------------------------------
struct ASTNode 
{
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
    Range range;
};

struct TypeNode : public ASTNode 
{
    void accept(ASTVisitor& visitor) override;
    
    std::string baseName; 
    Range baseNameRange;

    std::vector<int> arraySizes; // Supports multi-dim like float[4][4] if needed
    std::string precision;       // highp, mediump, lowp
    bool isVoid = false;

    std::string toString() const {
        std::string s = precision.empty() ? "" : (precision + " ");
        s += baseName;
        for (int size : arraySizes) s += "[" + std::to_string(size) + "]";
        return s;
    }
};

// -------------------------------------------------------------------------
// EXPRESSIONS
// -------------------------------------------------------------------------

struct ExpressionNode : public ASTNode {};

struct LiteralNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    TokenType type; // TOKEN_NUMBER, TOKEN_TRUE, TOKEN_STRING
    std::string value; // "1.0", "true", "texture.png"
};

struct IdentifierNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    std::string name;
};

struct BinaryOpNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    TokenType op; // +, -, *, /, &&, ||, etc.
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
};

struct UnaryOpNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    TokenType op; // -, !, ++, --
    std::unique_ptr<ExpressionNode> operand;
    bool isPostfix = false; // true for i++
};

struct FunctionCallNode : public ExpressionNode 
{
    void accept(ASTVisitor& visitor) override;
    std::string functionName;
    Range nameRange; // Specific range for just the "func" part
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
};

// e.g., vec3(1.0, 0.0, 0.0)
struct ConstructorNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    std::string typeName;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
};

// e.g., ALBEDO.r or light.color
struct MemberAccessNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> base; // ALBEDO
    std::string member;                   // r
};

// e.g., weights[2]
struct ArrayAccessNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> base;
    std::unique_ptr<ExpressionNode> index;
};

// e.g., condition ? trueVal : falseVal
struct TernaryNode : public ExpressionNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<ExpressionNode> trueExpr;
    std::unique_ptr<ExpressionNode> falseExpr;
};

// -------------------------------------------------------------------------
// STATEMENTS
// -------------------------------------------------------------------------

struct StatementNode : public ASTNode {};

struct BlockNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::vector<std::unique_ptr<StatementNode>> statements;
};

struct ExpressionStatementNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> expr;
};

struct VariableDeclNode : public StatementNode 
{
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<TypeNode> type;
    std::string name;
    Range nameRange;
    std::unique_ptr<ExpressionNode> initializer; // Optional assignment
    bool isConst = false;
};

struct ParameterNode : public ASTNode 
{
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<TypeNode> type;
    std::string name;
    Range nameRange;
    std::string qualifier; // in, out, inout
};

struct StructMemberNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<TypeNode> type;
    std::string name;
    Range nameRange;
};

struct IfNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<StatementNode> thenBranch;
    std::unique_ptr<StatementNode> elseBranch; // Nullable
};

struct WhileNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<StatementNode> body;
};

struct ForNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<StatementNode> init;      // int i = 0;
    std::unique_ptr<ExpressionNode> condition; // i < 10;
    std::unique_ptr<ExpressionNode> increment; // i++
    std::unique_ptr<StatementNode> body;
};

struct ReturnNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> value; // Nullable (for void)
};

struct DoWhileNode : StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<StatementNode> body;
    std::unique_ptr<ExpressionNode> condition;
};

struct CaseNode : ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> value; // nullptr if 'default'
    std::vector<std::unique_ptr<StatementNode>> statements;
    bool isDefault = false;
};

struct SwitchNode : StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<ExpressionNode> expression;
    std::vector<std::unique_ptr<CaseNode>> cases;
};

struct DiscardNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
}; // "discard;"

struct BreakNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
};
struct ContinueNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
};

// Preprocessing

struct DefineNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::string name;
    std::unique_ptr<ExpressionNode> value;
};

struct IncludeNode : public StatementNode {
    void accept(ASTVisitor& visitor) override;
    std::string path;
    std::string resolvedPath;
};

// -------------------------------------------------------------------------
// TOP LEVEL DECLARATIONS
// -------------------------------------------------------------------------

// shader_type spatial;
struct ShaderTypeNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::string shaderType; // spatial, canvas_item, particles
};

// render_mode unshaded, blend_add;
struct RenderModeNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::vector<std::string> modes;
};

// An insepctor instruction
struct GroupUniformsNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::string name;
};

// uniform float height : hint_range(0, 10) = 5.0;
struct UniformNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<TypeNode> type;
    std::string name;
    Range nameRange;
    std::string hint; // Null/Empty if none
    std::unique_ptr<ExpressionNode> defaultValue;
    bool isInstance = false;
};

// varying vec3 normal;
struct VaryingNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<TypeNode> type;
    std::string name;
    Range nameRange;
    std::string interpolation; // flat, smooth
};

// const float PI = 3.14;
struct ConstNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<TypeNode> type;
    std::string name;
    Range nameRange;
    std::unique_ptr<ExpressionNode> value;
};

// struct Light { vec3 color; };
struct StructNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::string name;
    Range nameRange;
    std::vector<std::unique_ptr<StructMemberNode>> members;
};

// void fragment() { ... }
struct FunctionNode : public ASTNode 
{
    void accept(ASTVisitor& visitor) override;
    std::unique_ptr<TypeNode> returnType;
    std::string name;
    Range nameRange;
    
    std::vector<std::unique_ptr<ParameterNode>> parameters;

    std::unique_ptr<BlockNode> body;
    bool is_function_definition = false;
};

// -------------------------------------------------------------------------
// ROOT PROGRAM
// -------------------------------------------------------------------------

struct ProgramNode : public ASTNode {
    void accept(ASTVisitor& visitor) override;
    std::vector<std::unique_ptr<ASTNode>> nodes; // Contains all the top-level decls
};

} // namespace gdshader_lsp

#endif // AST_H