#ifndef INLAY_HINT_VISITOR_HPP
#define INLAY_HINT_VISITOR_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/ast/ast_visitor.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include <lsp/types.h>

namespace gdshader_lsp {
    class InlayHintVisitor : public ASTVisitor {
    public:
        std::vector<lsp::InlayHint>& hints;
        const SymbolTable* symbols;
        const TypeRegistry* types;
        lsp::Range targetRange;

        InlayHintVisitor(std::vector<lsp::InlayHint>& h, const SymbolTable* s, const TypeRegistry* t, lsp::Range r)
            : hints(h), symbols(s), types(t), targetRange(r) {}

        bool isOverlapping(const Range& r) {
            if ((u_int)r.endLine < targetRange.start.line || (u_int)r.startLine > targetRange.end.line) return false;
            return true;
        }

        // --- Structural Pass-Throughs ---
        void visit(ProgramNode* n) override { for(auto& c:n->nodes) if(c) c->accept(*this); }
        void visit(FunctionNode* n) override { if(!isOverlapping(n->range)) return; if(n->body) n->body->accept(*this); }
        void visit(BlockNode* n) override { if(!isOverlapping(n->range)) return; for(auto& s:n->statements) if(s) s->accept(*this); }
        void visit(ExpressionStatementNode* n) override { if(!isOverlapping(n->range)) return; if(n->expr) n->expr->accept(*this); }
        void visit(VariableDeclNode* n) override { if(!isOverlapping(n->range)) return; if(n->initializer) n->initializer->accept(*this); }
        void visit(UniformNode* n) override { if(!isOverlapping(n->range)) return; if(n->defaultValue) n->defaultValue->accept(*this); }
        void visit(ConstNode* n) override { if(!isOverlapping(n->range)) return; if(n->value) n->value->accept(*this); }
        void visit(IfNode* n) override { if(!isOverlapping(n->range)) return; if(n->condition) n->condition->accept(*this); if(n->thenBranch) n->thenBranch->accept(*this); if(n->elseBranch) n->elseBranch->accept(*this); }
        void visit(WhileNode* n) override { if(!isOverlapping(n->range)) return; if(n->condition) n->condition->accept(*this); if(n->body) n->body->accept(*this); }
        void visit(DoWhileNode* n) override { if(!isOverlapping(n->range)) return; if(n->condition) n->condition->accept(*this); if(n->body) n->body->accept(*this); }
        void visit(ForNode* n) override { if(!isOverlapping(n->range)) return; if(n->init) n->init->accept(*this); if(n->condition) n->condition->accept(*this); if(n->increment) n->increment->accept(*this); if(n->body) n->body->accept(*this); }
        void visit(ReturnNode* n) override { if(!isOverlapping(n->range)) return; if(n->value) n->value->accept(*this); }
        void visit(BinaryOpNode* n) override { if(!isOverlapping(n->range)) return; if(n->left) n->left->accept(*this); if(n->right) n->right->accept(*this); }
        void visit(UnaryOpNode* n) override { if(!isOverlapping(n->range)) return; if(n->operand) n->operand->accept(*this); }
        void visit(TernaryNode* n) override { if(!isOverlapping(n->range)) return; if(n->condition) n->condition->accept(*this); if(n->trueExpr) n->trueExpr->accept(*this); if(n->falseExpr) n->falseExpr->accept(*this); }
        void visit(ArrayAccessNode* n) override { if(!isOverlapping(n->range)) return; if(n->base) n->base->accept(*this); if(n->index) n->index->accept(*this); }
        void visit(MemberAccessNode* n) override { if(!isOverlapping(n->range)) return; if(n->base) n->base->accept(*this); }
        void visit(SwitchNode* n) override { if(!isOverlapping(n->range)) return; if(n->expression) n->expression->accept(*this); for(auto& c:n->cases) if(c) c->accept(*this); }
        void visit(CaseNode* n) override { if(!isOverlapping(n->range)) return; if(n->value) n->value->accept(*this); for(auto& s:n->statements) if(s) s->accept(*this); }

        // --- Core Logic: Functions and Constructors ---
        void visit(FunctionCallNode* n) override {
            if (!isOverlapping(n->range)) return;
            for (auto& a : n->arguments) if (a) a->accept(*this);

            auto overloads = symbols->lookupFunctions(n->functionName);
            if (overloads.empty()) return;

            // Simple arity match heuristic
            const Symbol* bestMatch = overloads[0];
            for (const auto* sym : overloads) {
                if (sym->parameterTypes.size() == n->arguments.size()) {
                    bestMatch = sym;
                    break;
                }
            }

            for (size_t i = 0; i < n->arguments.size(); ++i) {
                if (i < bestMatch->parameterNames.size() && n->arguments[i]) {
                    lsp::InlayHint hint;
                    hint.position = lsp::Position{(unsigned)n->arguments[i]->range.startLine, (unsigned)n->arguments[i]->range.startCol};
                    hint.label = bestMatch->parameterNames[i] + ":";
                    hint.kind = lsp::InlayHintKind::Parameter;
                    hint.paddingRight = true;
                    hints.push_back(hint);
                }
            }
        }

        void visit(ConstructorNode* n) override {
            if (!isOverlapping(n->range)) return;
            for (auto& a : n->arguments) if (a) a->accept(*this);

            TypePtr t = const_cast<TypeRegistry*>(types)->getType(n->typeName); // this is ugly
            if (t && t->kind == TypeKind::STRUCT) {
                for (size_t i = 0; i < n->arguments.size(); ++i) {
                    if (i < t->members.size() && n->arguments[i]) {
                        lsp::InlayHint hint;
                        hint.position = lsp::Position{(unsigned)n->arguments[i]->range.startLine, (unsigned)n->arguments[i]->range.startCol};
                        hint.label = t->members[i].first + ":";
                        hint.kind = lsp::InlayHintKind::Parameter;
                        hint.paddingRight = true;
                        hints.push_back(hint);
                    }
                }
            }
        }

        // Ignored leaves
        void visit(TypeNode*) override {} void visit(LiteralNode*) override {} void visit(IdentifierNode*) override {} 
        void visit(ParameterNode*) override {} void visit(StructMemberNode*) override {} void visit(DiscardNode*) override {} 
        void visit(BreakNode*) override {} void visit(ContinueNode*) override {} void visit(DefineNode*) override {} 
        void visit(IncludeNode*) override {} void visit(ShaderTypeNode*) override {} void visit(RenderModeNode*) override {} 
        void visit(GroupUniformsNode*) override {} void visit(VaryingNode*) override {} void visit(StructNode*) override {}
    };
}
#endif