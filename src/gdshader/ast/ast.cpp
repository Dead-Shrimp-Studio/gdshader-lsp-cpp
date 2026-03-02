#include "gdshader/ast/ast.h"
#include "gdshader/ast/ast_visitor.hpp"

namespace gdshader_lsp {

// Base Types
void TypeNode::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Expressions
void LiteralNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void IdentifierNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void BinaryOpNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void UnaryOpNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void FunctionCallNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ConstructorNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void MemberAccessNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ArrayAccessNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void TernaryNode::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Statements
void BlockNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ExpressionStatementNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void VariableDeclNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ParameterNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void StructMemberNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void IfNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void WhileNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ForNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ReturnNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DoWhileNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void CaseNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void SwitchNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void DiscardNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void BreakNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ContinueNode::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Preprocessing
void DefineNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void IncludeNode::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Top Level Declarations
void ShaderTypeNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void RenderModeNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void GroupUniformsNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void UniformNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void VaryingNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void ConstNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void StructNode::accept(ASTVisitor& visitor) { visitor.visit(this); }
void FunctionNode::accept(ASTVisitor& visitor) { visitor.visit(this); }

// Root
void ProgramNode::accept(ASTVisitor& visitor) { visitor.visit(this); }

} // namespace gdshader_lsp