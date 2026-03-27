#ifndef SEMANTIC_TOKEN_VISITOR_HPP
#define SEMANTIC_TOKEN_VISITOR_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/ast/ast_visitor.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include <vector>
#include <algorithm>

namespace gdshader_lsp {

    // These indices MUST match the legend sent in your initialize response
    enum SemanticTokenType {
        Type = 0,
        Struct = 1,
        Parameter = 2,
        Variable = 3,
        Property = 4,
        Function = 5,
        Keyword = 6,
        Number = 7,
        Macro = 8,
        EnumMember = 9 
    };

    struct SemToken {
        int line;
        int col;
        int length;
        int type;
        int modifiers;
        
        // LSP requires tokens to be strictly sorted by line, then column
        bool operator<(const SemToken& other) const {
            if (line != other.line) return line < other.line;
            return col < other.col;
        }
    };

    class SemanticTokenVisitor : public ASTVisitor {
    public:
        std::vector<SemToken> tokens;

        void addToken(int line, int col, int length, int type, int modifiers = 0) {
            if (line < 0 || col < 0 || length <= 0) return;
            tokens.push_back({line, col, length, type, modifiers});
        }

        // --- ROOT ---
        void visit(ProgramNode* n) override { for(auto& c:n->nodes) if(c) c->accept(*this); }
        void visit(BlockNode* n) override { for(auto& s:n->statements) if(s) s->accept(*this); }
        
        // --- DECLARATIONS ---
        void visit(FunctionNode* n) override { 
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Function);
            for(auto& p:n->parameters) if(p) p->accept(*this);
            if(n->body) n->body->accept(*this);
        }
        void visit(VariableDeclNode* n) override {
            if (n->type) n->type->accept(*this);
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Variable);
            if (n->initializer) n->initializer->accept(*this);
        }
        void visit(UniformNode* n) override {
            if (n->type) n->type->accept(*this);
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Variable, 1); 
            if (n->defaultValue) n->defaultValue->accept(*this);
        }
        void visit(VaryingNode* n) override {
            if (n->type) n->type->accept(*this);
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Variable);
        }
        void visit(ConstNode* n) override {
            if (n->type) n->type->accept(*this);
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Variable, 1);
            if (n->value) n->value->accept(*this);
        }
        void visit(StructNode* n) override {
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Struct);
            for(auto& m:n->members) if(m) m->accept(*this);
        }
        void visit(StructMemberNode* n) override {
            if (n->type) n->type->accept(*this);
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Property);
        }
        void visit(ParameterNode* n) override {
            if (n->type) n->type->accept(*this);
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->name.length(), SemanticTokenType::Parameter);
        }

        // --- USAGES ---
        void visit(TypeNode* n) override {
            addToken(n->baseNameRange.startLine, n->baseNameRange.startCol, n->baseName.length(), SemanticTokenType::Type);
        }
        void visit(FunctionCallNode* n) override {
            addToken(n->nameRange.startLine, n->nameRange.startCol, n->functionName.length(), SemanticTokenType::Function);
            for(auto& a:n->arguments) if(a) a->accept(*this);
        }
        void visit(ConstructorNode* n) override {
            addToken(n->range.startLine, n->range.startCol, n->typeName.length(), SemanticTokenType::Type);
            for(auto& a:n->arguments) if(a) a->accept(*this);
        }
        void visit(IdentifierNode* n) override {
            // Because TypeCheckingVisitor bounds resolvedSymbol, we can use it to perfectly color usages!
            int type = SemanticTokenType::Variable;
            if (n->resolvedSymbol) {
                if (n->resolvedSymbol->category == SymbolType::Struct) type = SemanticTokenType::Struct;
            }
            addToken(n->range.startLine, n->range.startCol, n->name.length(), type);
        }
        void visit(MemberAccessNode* n) override {
            if (n->base) n->base->accept(*this);
            addToken(n->range.endLine, n->range.endCol - n->member.length(), n->member.length(), SemanticTokenType::Property);
        }
        
        // --- PASS THROUGHS ---
        void visit(ExpressionStatementNode* n) override { if(n->expr) n->expr->accept(*this); }
        void visit(IfNode* n) override { if(n->condition) n->condition->accept(*this); if(n->thenBranch) n->thenBranch->accept(*this); if(n->elseBranch) n->elseBranch->accept(*this); }
        void visit(WhileNode* n) override { if(n->condition) n->condition->accept(*this); if(n->body) n->body->accept(*this); }
        void visit(DoWhileNode* n) override { if(n->body) n->body->accept(*this); if(n->condition) n->condition->accept(*this); }
        void visit(ForNode* n) override { if(n->init) n->init->accept(*this); if(n->condition) n->condition->accept(*this); if(n->increment) n->increment->accept(*this); if(n->body) n->body->accept(*this); }
        void visit(ReturnNode* n) override { if(n->value) n->value->accept(*this); }
        void visit(BinaryOpNode* n) override { if(n->left) n->left->accept(*this); if(n->right) n->right->accept(*this); }
        void visit(UnaryOpNode* n) override { if(n->operand) n->operand->accept(*this); }
        void visit(TernaryNode* n) override { if(n->condition) n->condition->accept(*this); if(n->trueExpr) n->trueExpr->accept(*this); if(n->falseExpr) n->falseExpr->accept(*this); }
        void visit(ArrayAccessNode* n) override { if(n->base) n->base->accept(*this); if(n->index) n->index->accept(*this); }
        void visit(SwitchNode* n) override { if(n->expression) n->expression->accept(*this); for(auto& c:n->cases) if(c) c->accept(*this); }
        void visit(CaseNode* n) override { if(n->value) n->value->accept(*this); for(auto& s:n->statements) if(s) s->accept(*this); }
        
        // Ignore the rest
        void visit(LiteralNode*) override {} void visit(DefineNode*) override {} void visit(IncludeNode*) override {}
        void visit(ShaderTypeNode*) override {} void visit(RenderModeNode*) override {} void visit(GroupUniformsNode*) override {}
        void visit(DiscardNode*) override {} void visit(BreakNode*) override {} void visit(ContinueNode*) override {}
    };
}
#endif