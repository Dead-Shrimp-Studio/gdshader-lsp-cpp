
#include "gdshader/ast/ast.h"
#include "gdshader/semantics/visitors/symbol_declaration_visitor.hpp"

namespace gdshader_lsp {

    // Helper

    void SymbolDeclarationVisitor::loadBuiltinsForFunction(const std::string& funcName) 
    {
        ShaderStage scope = ShaderStage::Global;
        if (funcName == "vertex") scope = ShaderStage::Vertex;
        else if (funcName == "fragment") scope = ShaderStage::Fragment;
        else if (funcName == "light") scope = ShaderStage::Light;
        else if (funcName == "start") scope = ShaderStage::Start;
        else if (funcName == "process") scope = ShaderStage::Process;
        else if (funcName == "sky") scope = ShaderStage::Sky;
        else if (funcName == "fog") scope = ShaderStage::Fog;

        if (scope != ShaderStage::Global) {
            const auto& builtins = get_builtins(currentShaderType, scope); //
            for (const auto& b : builtins) {
                TypePtr t = typeRegistry.getType(b.type);
                Mutability m = (b.qualifier == "in" || b.qualifier == "const") ? Mutability::ReadOnly : Mutability::Mutable;

                Symbol s = symbols.createSymbol(b.name, t, SymbolType::Builtin, {}, m);
                symbols.add(s);
            }
        }
    }

    TypePtr SymbolDeclarationVisitor::resolveTypeFromNode(const TypeNode *node)
    {
        if (!node) return typeRegistry.getUnknownType(); // or unknown

        TypePtr currentType = typeRegistry.getType(node->baseName);
        for (int size : node->arraySizes) {
            currentType = typeRegistry.getArrayType(currentType, size);
        }

        return currentType;
    }

    // End Helper

    void SymbolDeclarationVisitor::visit(ProgramNode* node) 
    {
        for (const auto& child : node->nodes) {
            if (child) child->accept(*this);
        }
    }

    void SymbolDeclarationVisitor::visit(BlockNode* node) 
    {
        int startLine = (node->range.startLine > 0) ? node->range.startLine - 1 : 0;
        symbols.pushScope(startLine); // Open Scope

        for (const auto& stmt : node->statements) {
            if (stmt) stmt->accept(*this);
        }

        symbols.popScope(node->range.endLine); // Close Scope
    }

    void SymbolDeclarationVisitor::visit(ForNode* node) 
    {
        // 'for' creates a scope for its init variable
        int startLine = (node->range.startLine > 0) ? node->range.startLine - 1 : 0;
        symbols.pushScope(startLine); 

        if (node->init) node->init->accept(*this);
        // Condition and Increment don't declare symbols, so we can skip them in this pass!
        if (node->body) node->body->accept(*this);

        symbols.popScope(node->range.endLine);
    }

    void SymbolDeclarationVisitor::visit(VariableDeclNode* node) 
    {
        TypePtr type = resolveTypeFromNode(node->type.get());
        Mutability mut = node->isConst ? Mutability::ReadOnly : Mutability::Mutable;

        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Variable, node->range, mut);        

        if (symbols.lookup(node->name)) {
            if (symbols.add(s)) {
                diagnostics.push_back(reportWarning(node, "Variable '" + node->name + "' shadows an existing declaration.")); 
            } else {
                diagnostics.push_back(reportError(node, "Redefinition of variable '" + node->name + "' in the same scope."));
            }
        } else {
            symbols.add(s);
        }

        // We do NOT check if initializer matches the type here. 
        // But we DO visit it, in case it contains something we need (usually expressions don't).
        if (node->initializer) node->initializer->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(FunctionNode* node) 
    {
        TypePtr returnType = resolveTypeFromNode(node->returnType.get());
        
        std::vector<std::string> paramNames;
        std::vector<TypePtr> paramTypes;
        for (const auto& param : node->parameters) {
            paramTypes.push_back(resolveTypeFromNode(param->type.get()));
            paramNames.push_back(param->name);
        }

        Symbol s = symbols.createSymbol(node->name, {}, SymbolType::Function, node->range);
        s.is_function_definition = node->is_function_definition;
        s.parameterTypes = paramTypes;
        s.parameterNames = paramNames;
        s.returnType = returnType;

        if (!symbols.add(s)) {
            diagnostics.push_back(reportError(node, "Redefinition of function '" + node->name + "'"));
        }
        
        // Functions create a scope for their parameters
        symbols.pushScope(node->range.startLine);
        loadBuiltinsForFunction(node->name);

        for (size_t i = 0; i < node->parameters.size(); i++) {
            const auto& param = node->parameters[i];
            TypePtr t = paramTypes[i];
            
            Symbol argSym = symbols.createSymbol(param->name, t, SymbolType::Variable, param->range, Mutability::Mutable);
            symbols.add(argSym);
        }

        if (node->body) node->body->accept(*this);

        int endLine = node->body ? node->body->range.endLine : node->range.endLine;
        symbols.popScope(endLine);
    }

    void SymbolDeclarationVisitor::visit(StructNode* node) 
    {
        std::vector<std::pair<std::string, TypePtr>> members;

        for(const auto& m : node->members) {
            TypePtr memberType = resolveTypeFromNode(m->type.get());
            members.push_back({m->name, memberType});
        }

        typeRegistry.registerStruct(node->name, members);
        TypePtr structType = typeRegistry.getType(node->name);

        Symbol s = symbols.createSymbol(node->name, structType, SymbolType::Struct, node->range, Mutability::Mutable);        
        if (!symbols.add(s)) diagnostics.push_back(reportError(node, "Redefinition of struct '" + s.name + "'"));
    }

    void SymbolDeclarationVisitor::visit(ShaderTypeNode* node) 
    {
        if (node->shaderType == "canvas_item") currentShaderType = ShaderType::CanvasItem;
        else if (node->shaderType == "spatial") currentShaderType = ShaderType::Spatial;
        else if (node->shaderType == "particles") currentShaderType = ShaderType::Particles;
        else if (node->shaderType == "sky") currentShaderType = ShaderType::Sky;
        else if (node->shaderType == "fog") currentShaderType = ShaderType::Fog;
        else {
            currentShaderType = ShaderType::Unknown;
            reportError(node, "shader_type missing. Must be one of spatial, canvas_item, particles, sky, or fog.");
        }
    }

    void gdshader_lsp::SymbolDeclarationVisitor::visit(RenderModeNode *node)
    {
        if (currentShaderType != ShaderType::Spatial)
        {
            reportError(node, "render_mode declarations are only valid in spatial type shaders.");
        }

        for (const std::string& mode : node->modes)
        {
            if (stringToRenderMode(mode) == RenderMode::UNKNOWN)
            {
                reportError(node, "Unknown render_mode: " + mode);
            }
        }
    }

    void SymbolDeclarationVisitor::visit(UniformNode* node) 
    {
        TypePtr type = resolveTypeFromNode(node->type.get());

        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Uniform, node->range, Mutability::ReadOnly);
        s.hint = node->hint;

        if (!symbols.add(s)) {
            diagnostics.push_back(reportError(node, "Redefinition of uniform '" + s.name + "'"));
        }
        
        // Visit the default value in case it contains nested expressions we need to traverse.
        // Type checking (does the value match the uniform type?) is deferred to TypeCheckingVisitor.
        if (node->defaultValue) {
            node->defaultValue->accept(*this);
        }
    }

    void SymbolDeclarationVisitor::visit(VaryingNode* node) 
    {
        TypePtr type = resolveTypeFromNode(node->type.get());
        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Varying, node->range, Mutability::ReadOnly);        
        if (!symbols.add(s)) {
            diagnostics.push_back(reportError(node, "Redefinition of varying '" + s.name + "'"));
        }
    }

    void SymbolDeclarationVisitor::visit(ConstNode* node) 
    {
        TypePtr type = resolveTypeFromNode(node->type.get());
        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Variable, node->range, Mutability::ReadOnly);         
        if (!symbols.add(s)) {
            diagnostics.push_back(reportError(node, "Redefinition of const '" + s.name + "'"));
        }

        if (node->value) {
            node->value->accept(*this);
        }
    }

    // We only need to traverse these to ensure any declarations inside their 
    // branches (e.g., inside block nodes) are reached and registered.

    void SymbolDeclarationVisitor::visit(IfNode* node) 
    {
        if (node->condition) node->condition->accept(*this);
        if (node->thenBranch) node->thenBranch->accept(*this);
        if (node->elseBranch) node->elseBranch->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(WhileNode* node) 
    {
        if (node->condition) node->condition->accept(*this);
        if (node->body) node->body->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(DoWhileNode* node) 
    {
        if (node->body) node->body->accept(*this);
        if (node->condition) node->condition->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(SwitchNode* node) 
    {
        if (node->expression) node->expression->accept(*this);

        for (const auto& c : node->cases) {
            if (!c->isDefault && c->value) {
                c->value->accept(*this);
            }
            for (const auto& stmt : c->statements) {
                if (stmt) stmt->accept(*this);
            }
        }
    }

} // namespace
