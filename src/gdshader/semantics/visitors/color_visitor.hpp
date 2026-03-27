#ifndef COLOR_VISITOR_HPP
#define COLOR_VISITOR_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/ast/ast_visitor.hpp"
#include <lsp/types.h>
#include <string>

namespace gdshader_lsp 
{
    class ColorVisitor : public ASTVisitor 
    {
        public:
            std::vector<lsp::ColorInformation>& colors;

            ColorVisitor(std::vector<lsp::ColorInformation>& c) : colors(c) {}

            void visit(ProgramNode* n) override { 
                for(auto& c : n->nodes) if(c) c->accept(*this); 
            }
            
            void visit(UniformNode* n) override {
                // Godot uses source_color (or hint_color in older versions)
                if (n->hint.find("source_color") != std::string::npos || n->hint.find("hint_color") != std::string::npos) {
                    
                    if (auto call = dynamic_cast<const FunctionCallNode*>(n->defaultValue.get())) {   
                        if (call->functionName == "vec3" || call->functionName == "vec4") {
                            lsp::ColorInformation colorInfo;
                            colorInfo.range = lsp::Range{
                                .start = {(unsigned)call->range.startLine, (unsigned)call->range.startCol},
                                .end = {(unsigned)call->range.endLine, (unsigned)call->range.endCol}
                            };
                            
                            lsp::Color c{0.0, 0.0, 0.0, 1.0};
                            
                            auto extractFloat = [](const ExpressionNode* expr) -> double {
                                if (auto lit = dynamic_cast<const LiteralNode*>(expr)) {
                                    try { return std::stod(lit->value); } catch(...) {}
                                }
                                return 0.0;
                            };

                            if (call->arguments.size() >= 1) c.red   = extractFloat(call->arguments[0].get());
                            if (call->arguments.size() >= 2) c.green = extractFloat(call->arguments[1].get());
                            if (call->arguments.size() >= 3) c.blue  = extractFloat(call->arguments[2].get());
                            if (call->arguments.size() == 4) c.alpha = extractFloat(call->arguments[3].get());

                            colorInfo.color = c;
                            colors.push_back(colorInfo);
                        }
                    }
                }
            }

            // Ignore all other nodes. Uniforms only exist at the top level!
            void visit(BlockNode*) override {} void visit(FunctionNode*) override {} void visit(ExpressionStatementNode*) override {}
            void visit(VariableDeclNode*) override {} void visit(ConstNode*) override {} void visit(IfNode*) override {} 
            void visit(WhileNode*) override {} void visit(DoWhileNode*) override {} void visit(ForNode*) override {} 
            void visit(ReturnNode*) override {} void visit(BinaryOpNode*) override {} void visit(UnaryOpNode*) override {} 
            void visit(TernaryNode*) override {} void visit(ArrayAccessNode*) override {} void visit(MemberAccessNode*) override {} 
            void visit(SwitchNode*) override {} void visit(CaseNode*) override {} void visit(FunctionCallNode*) override {} 
            void visit(ConstructorNode*) override {} void visit(TypeNode*) override {} void visit(LiteralNode*) override {} 
            void visit(IdentifierNode*) override {} void visit(ParameterNode*) override {} void visit(StructMemberNode*) override {} 
            void visit(DiscardNode*) override {} void visit(BreakNode*) override {} void visit(ContinueNode*) override {} 
            void visit(DefineNode*) override {} void visit(IncludeNode*) override {} void visit(ShaderTypeNode*) override {} 
            void visit(RenderModeNode*) override {} void visit(GroupUniformsNode*) override {} void visit(VaryingNode*) override {} 
            void visit(StructNode*) override {}
    };
}

#endif