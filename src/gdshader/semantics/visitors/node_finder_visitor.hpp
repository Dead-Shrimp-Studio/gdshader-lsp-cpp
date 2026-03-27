#ifndef NODE_FINDER_VISITOR_HPP
#define NODE_FINDER_VISITOR_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/ast/ast_visitor.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include "gdshader/diagnostics.hpp"

namespace gdshader_lsp 
{
    class NodeFinderVisitor : public ASTVisitor {
    public:
        int targetLine, targetCol;
        const ASTNode* deepestNode = nullptr;

        NodeFinderVisitor(int line, int col) : targetLine(line), targetCol(col) {}

        // Check if the cursor is inside this node's range
        bool isInside(const ASTNode* node) {
            if (!node) return false;
            if (targetLine < node->range.startLine || targetLine > node->range.endLine) return false;
            if (targetLine == node->range.startLine && targetCol < node->range.startCol) return false;
            if (targetLine == node->range.endLine && targetCol > node->range.endCol) return false;
            
            deepestNode = node; // Found a tighter fit!
            return true;
        }

        // -------------------------------------------------------------------------
        // ROOT AND TOP LEVEL NODES
        // -------------------------------------------------------------------------
        void visit(ProgramNode* node) override { for (auto& n : node->nodes) if (n) n->accept(*this); }
        void visit(ShaderTypeNode* node) override { isInside(node); }
        void visit(RenderModeNode* node) override { isInside(node); }
        void visit(GroupUniformsNode* node) override { isInside(node); }

        void visit(UniformNode* node) override {
            if (isInside(node)) {
                if (node->type) node->type->accept(*this);
                if (node->defaultValue) node->defaultValue->accept(*this);
            }
        }
        void visit(VaryingNode* node) override {
            if (isInside(node)) {
                if (node->type) node->type->accept(*this);
            }
        }
        void visit(ConstNode* node) override {
            if (isInside(node)) {
                if (node->type) node->type->accept(*this);
                if (node->value) node->value->accept(*this);
            }
        }
        void visit(StructNode* node) override {
            if (isInside(node)) {
                for (auto& m : node->members) if (m) m->accept(*this);
            }
        }
        void visit(FunctionNode* node) override {
            if (isInside(node)) {
                if (node->returnType) node->returnType->accept(*this);
                for (auto& param : node->parameters) if (param) param->accept(*this);
                if (node->body) node->body->accept(*this);
            }
        }

        // -------------------------------------------------------------------------
        // STRUCTURAL & HELPERS
        // -------------------------------------------------------------------------
        void visit(TypeNode* node) override { isInside(node); }
        void visit(ParameterNode* node) override {
            if (isInside(node)) {
                if (node->type) node->type->accept(*this);
            }
        }
        void visit(StructMemberNode* node) override {
            if (isInside(node)) {
                if (node->type) node->type->accept(*this);
            }
        }

        // -------------------------------------------------------------------------
        // STATEMENTS
        // We cannot if-guard here due to the parser being slightly incorrect when it comes to range tracking...
        // -------------------------------------------------------------------------
        void visit(BlockNode* node) override {
            isInside(node);
            for (auto& stmt : node->statements) if (stmt) stmt->accept(*this);
        }
        void visit(ExpressionStatementNode* node) override {
            isInside(node);
            if (node->expr) node->expr->accept(*this);
        }
        void visit(VariableDeclNode* node) override {
            isInside(node);
            if (node->type) node->type->accept(*this);
            if (node->initializer) node->initializer->accept(*this);
        }
        void visit(IfNode* node) override {
            isInside(node);
            if (node->condition) node->condition->accept(*this);
            if (node->thenBranch) node->thenBranch->accept(*this);
            if (node->elseBranch) node->elseBranch->accept(*this);
        }
        void visit(WhileNode* node) override {
            isInside(node);
            if (node->condition) node->condition->accept(*this);
            if (node->body) node->body->accept(*this);
        }
        void visit(DoWhileNode* node) override {
            isInside(node);
            if (node->body) node->body->accept(*this);
            if (node->condition) node->condition->accept(*this);
        }
        void visit(ForNode* node) override {
            isInside(node);
            if (node->init) node->init->accept(*this);
            if (node->condition) node->condition->accept(*this);
            if (node->increment) node->increment->accept(*this);
            if (node->body) node->body->accept(*this);
        }
        void visit(ReturnNode* node) override {
            isInside(node);
            if (node->value) node->value->accept(*this);
        }
        void visit(SwitchNode* node) override {
            isInside(node);
            if (node->expression) node->expression->accept(*this);
            for (auto& c : node->cases) if (c) c->accept(*this);
        }
        void visit(CaseNode* node) override {
            isInside(node);
            if (node->value) node->value->accept(*this);
            for (auto& stmt : node->statements) if (stmt) stmt->accept(*this);
        }
        void visit(DiscardNode* node) override { isInside(node); }
        void visit(BreakNode* node) override { isInside(node); }
        void visit(ContinueNode* node) override { isInside(node); }
        
        // -------------------------------------------------------------------------
        // PREPROCESSOR
        // -------------------------------------------------------------------------
        void visit(DefineNode* node) override {
            if (isInside(node)) {
                if (node->value) node->value->accept(*this);
            }
        }
        void visit(IncludeNode* node) override { isInside(node); }

        // -------------------------------------------------------------------------
        // EXPRESSION NODES (Where the magic happens)
        // -------------------------------------------------------------------------
        void visit(BinaryOpNode* node) override {
            if (isInside(node)) {
                if (node->left) node->left->accept(*this);
                if (node->right) node->right->accept(*this);
            }
        }
        void visit(UnaryOpNode* node) override {
            if (isInside(node)) {
                if (node->operand) node->operand->accept(*this);
            }
        }
        void visit(MemberAccessNode* node) override {
            if (isInside(node)) {
                if (node->base) node->base->accept(*this);
            }
        }
        void visit(ArrayAccessNode* node) override {
            if (isInside(node)) {
                if (node->base) node->base->accept(*this);
                if (node->index) node->index->accept(*this);
            }
        }
        void visit(FunctionCallNode* node) override {
            if (isInside(node)) {
                for (auto& arg : node->arguments) if (arg) arg->accept(*this);
            }
        }
        void visit(ConstructorNode* node) override {
            if (isInside(node)) {
                for (auto& arg : node->arguments) if (arg) arg->accept(*this);
            }
        }
        void visit(TernaryNode* node) override {
            if (isInside(node)) {
                if (node->condition) node->condition->accept(*this);
                if (node->trueExpr) node->trueExpr->accept(*this);
                if (node->falseExpr) node->falseExpr->accept(*this);
            }
        }
        
        // -------------------------------------------------------------------------
        // LEAF EXPRESSION NODES
        // -------------------------------------------------------------------------
        void visit(IdentifierNode* node) override { isInside(node); }
        void visit(LiteralNode* node) override { isInside(node); }
    };
} // namespace gdshader_lsp

#endif // NODE_FINDER_VISITOR_HPP