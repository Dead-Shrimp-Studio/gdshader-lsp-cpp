#ifndef AST_H
#define AST_H

#include <vector>
#include <string>
#include <memory>
#include <optional>

#include "gdshader/lexer/lexer_types.h"

namespace gdshader_lsp {

struct SourceRange {
    int startLine = 0;
    int startCol = 0;
    int endLine = 0;
    int endCol = 0;
};

// -------------------------------------------------------------------------
// BASE NODE
// -------------------------------------------------------------------------
struct ASTNode {
    virtual ~ASTNode() = default;
    SourceRange range;
};

struct TypeNode : public ASTNode 
{
    std::string baseName; 
    SourceRange baseNameRange;

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
    TokenType type; // TOKEN_NUMBER, TOKEN_TRUE, TOKEN_STRING
    std::string value; // "1.0", "true", "texture.png"
};

struct IdentifierNode : public ExpressionNode {
    std::string name;
};

struct BinaryOpNode : public ExpressionNode {
    TokenType op; // +, -, *, /, &&, ||, etc.
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
};

struct UnaryOpNode : public ExpressionNode {
    TokenType op; // -, !, ++, --
    std::unique_ptr<ExpressionNode> operand;
    bool isPostfix = false; // true for i++
};

struct FunctionCallNode : public ExpressionNode {
    std::string functionName;
    SourceRange nameRange; // Specific range for just the "func" part
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
};

// e.g., vec3(1.0, 0.0, 0.0)
struct ConstructorNode : public ExpressionNode {
    std::string typeName;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
};

// e.g., ALBEDO.r or light.color
struct MemberAccessNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> base; // ALBEDO
    std::string member;                   // r
};

// e.g., weights[2]
struct ArrayAccessNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> base;
    std::unique_ptr<ExpressionNode> index;
};

// e.g., condition ? trueVal : falseVal
struct TernaryNode : public ExpressionNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<ExpressionNode> trueExpr;
    std::unique_ptr<ExpressionNode> falseExpr;
};

// -------------------------------------------------------------------------
// STATEMENTS
// -------------------------------------------------------------------------

struct StatementNode : public ASTNode {};

struct BlockNode : public StatementNode {
    std::vector<std::unique_ptr<StatementNode>> statements;
};

struct ExpressionStatementNode : public StatementNode {
    std::unique_ptr<ExpressionNode> expr;
};

struct VariableDeclNode : public StatementNode 
{
    std::unique_ptr<TypeNode> type;
    std::string name;
    SourceRange nameRange;
    std::unique_ptr<ExpressionNode> initializer; // Optional assignment
    bool isConst = false;
};

struct ParameterNode : public ASTNode 
{
    std::unique_ptr<TypeNode> type;
    std::string name;
    SourceRange nameRange;
    std::string qualifier; // in, out, inout
};

struct StructMemberNode : public ASTNode {
    std::unique_ptr<TypeNode> type;
    std::string name;
    SourceRange nameRange;
};

struct IfNode : public StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<StatementNode> thenBranch;
    std::unique_ptr<StatementNode> elseBranch; // Nullable
};

struct WhileNode : public StatementNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<StatementNode> body;
};

struct ForNode : public StatementNode {
    std::unique_ptr<StatementNode> init;      // int i = 0;
    std::unique_ptr<ExpressionNode> condition; // i < 10;
    std::unique_ptr<ExpressionNode> increment; // i++
    std::unique_ptr<StatementNode> body;
};

struct ReturnNode : public StatementNode {
    std::unique_ptr<ExpressionNode> value; // Nullable (for void)
};

struct DoWhileNode : StatementNode {
    std::unique_ptr<StatementNode> body;
    std::unique_ptr<ExpressionNode> condition;
};

struct CaseNode : ASTNode {
    std::unique_ptr<ExpressionNode> value; // nullptr if 'default'
    std::vector<std::unique_ptr<StatementNode>> statements;
    bool isDefault = false;
};

struct SwitchNode : StatementNode {
    std::unique_ptr<ExpressionNode> expression;
    std::vector<std::unique_ptr<CaseNode>> cases;
};

struct DiscardNode : public StatementNode {}; // "discard;"

struct BreakNode : public StatementNode {};
struct ContinueNode : public StatementNode {};

// Preprocessing

struct DefineNode : public StatementNode {
    std::string name;
    std::unique_ptr<ExpressionNode> value;
};

struct IncludeNode : public StatementNode {
    std::string path;
    std::string resolvedPath;
};

// -------------------------------------------------------------------------
// TOP LEVEL DECLARATIONS
// -------------------------------------------------------------------------

// shader_type spatial;
struct ShaderTypeNode : public ASTNode {
    std::string shaderType; // spatial, canvas_item, particles
};

// render_mode unshaded, blend_add;
struct RenderModeNode : public ASTNode {
    std::vector<std::string> modes;
};

// uniform float height : hint_range(0, 10) = 5.0;
struct UniformNode : public ASTNode {
    std::unique_ptr<TypeNode> type;
    std::string name;
    SourceRange nameRange;
    std::string hint; // Null/Empty if none
    std::unique_ptr<ExpressionNode> defaultValue;
};

// varying vec3 normal;
struct VaryingNode : public ASTNode {
    std::unique_ptr<TypeNode> type;
    std::string name;
    SourceRange nameRange;
    std::string interpolation; // flat, smooth
};

// const float PI = 3.14;
struct ConstNode : public ASTNode {
    std::unique_ptr<TypeNode> type;
    std::string name;
    SourceRange nameRange;
    std::unique_ptr<ExpressionNode> value;
};

// struct Light { vec3 color; };
struct StructNode : public ASTNode {
    std::string name;
    SourceRange nameRange;
    std::vector<std::unique_ptr<StructMemberNode>> members;
};

// void fragment() { ... }
struct FunctionNode : public ASTNode {
    std::unique_ptr<TypeNode> returnType;
    std::string name;
    SourceRange nameRange;
    
    std::vector<std::unique_ptr<ParameterNode>> parameters;

    std::unique_ptr<BlockNode> body;
};

// -------------------------------------------------------------------------
// ROOT PROGRAM
// -------------------------------------------------------------------------

struct ProgramNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> nodes; // Contains all the top-level decls
};

} // namespace gdshader_lsp

#endif // AST_H