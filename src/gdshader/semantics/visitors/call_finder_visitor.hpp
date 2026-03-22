
#ifndef CALL_FINDER_VISITOR_HPP
#define CALL_FINDER_VISITOR_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/ast/ast_visitor.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include "gdshader/diagnostics.hpp"

namespace gdshader_lsp
{

    class CallFinderVisitor : public ASTVisitor {
    public:
        int targetLine, targetCol;
        const FunctionCallNode* activeCall = nullptr;
        const ConstructorNode* activeConstructor = nullptr;

        CallFinderVisitor(int l, int c) : targetLine(l), targetCol(c) {}

        bool isInside(const Range& r) {
            if (targetLine < r.startLine || targetLine > r.endLine) return false;
            if (targetLine == r.startLine && targetCol < r.startCol) return false;
            if (targetLine == r.endLine && targetCol > r.endCol) return false;
            return true;
        }

        // Structural traversals
        void visit(ProgramNode* n) override { if(isInside(n->range)) for(auto& c:n->nodes) if(c) c->accept(*this); }
        void visit(BlockNode* n) override { if(isInside(n->range)) for(auto& s:n->statements) if(s) s->accept(*this); }
        void visit(FunctionNode* n) override { if(isInside(n->range) && n->body) n->body->accept(*this); }
        void visit(ExpressionStatementNode* n) override { if(isInside(n->range) && n->expr) n->expr->accept(*this); }
        void visit(VariableDeclNode* n) override { if(isInside(n->range) && n->initializer) n->initializer->accept(*this); }
        void visit(UniformNode* n) override { if(isInside(n->range) && n->defaultValue) n->defaultValue->accept(*this); }
        void visit(ConstNode* n) override { if(isInside(n->range) && n->value) n->value->accept(*this); }
        void visit(IfNode* n) override { if(isInside(n->range)) { if(n->condition) n->condition->accept(*this); if(n->thenBranch) n->thenBranch->accept(*this); if(n->elseBranch) n->elseBranch->accept(*this); } }
        void visit(WhileNode* n) override { if(isInside(n->range)) { if(n->condition) n->condition->accept(*this); if(n->body) n->body->accept(*this); } }
        void visit(DoWhileNode* n) override { if(isInside(n->range)) { if(n->condition) n->condition->accept(*this); if(n->body) n->body->accept(*this); } }
        void visit(ForNode* n) override { if(isInside(n->range)) { if(n->init) n->init->accept(*this); if(n->condition) n->condition->accept(*this); if(n->increment) n->increment->accept(*this); if(n->body) n->body->accept(*this); } }
        void visit(ReturnNode* n) override { if(isInside(n->range) && n->value) n->value->accept(*this); }
        void visit(BinaryOpNode* n) override { if(isInside(n->range)) { if(n->left) n->left->accept(*this); if(n->right) n->right->accept(*this); } }
        void visit(UnaryOpNode* n) override { if(isInside(n->range) && n->operand) n->operand->accept(*this); }
        void visit(TernaryNode* n) override { if(isInside(n->range)) { if(n->condition) n->condition->accept(*this); if(n->trueExpr) n->trueExpr->accept(*this); if(n->falseExpr) n->falseExpr->accept(*this); } }
        void visit(ArrayAccessNode* n) override { if(isInside(n->range)) { if(n->base) n->base->accept(*this); if(n->index) n->index->accept(*this); } }
        void visit(MemberAccessNode* n) override { if(isInside(n->range) && n->base) n->base->accept(*this); }
        void visit(SwitchNode* n) override { if(isInside(n->range)) { if(n->expression) n->expression->accept(*this); for(auto& c:n->cases) if(c) c->accept(*this); } }
        void visit(CaseNode* n) override { if(isInside(n->range)) { if(n->value) n->value->accept(*this); for(auto& s:n->statements) if(s) s->accept(*this); } }
        
        // Target Nodes
        void visit(FunctionCallNode* n) override {
            if (isInside(n->range)) {
                activeCall = n;
                activeConstructor = nullptr; // Reset constructor if we enter a nested function call
                for (auto& a : n->arguments) if (a) a->accept(*this);
            }
        }
        void visit(ConstructorNode* n) override {
            if (isInside(n->range)) {
                activeConstructor = n;
                activeCall = nullptr; // Reset call if we enter a nested constructor
                for (auto& a : n->arguments) if (a) a->accept(*this);
            }
        }

        // Leaves (Ignored)
        void visit(TypeNode*) override {} void visit(LiteralNode*) override {} void visit(IdentifierNode*) override {} 
        void visit(ParameterNode*) override {} void visit(StructMemberNode*) override {} void visit(DiscardNode*) override {} 
        void visit(BreakNode*) override {} void visit(ContinueNode*) override {} void visit(DefineNode*) override {} 
        void visit(IncludeNode*) override {} void visit(ShaderTypeNode*) override {} void visit(RenderModeNode*) override {} 
        void visit(GroupUniformsNode*) override {} void visit(VaryingNode*) override {} void visit(StructNode*) override {}
    };

} // namespace

#endif