#include "gdshader/ast/ast.h"
#include "gdshader/semantics/visitors/symbol_declaration_visitor.hpp"
#include "utils/logger.hpp"

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

        if (scope != ShaderStage::Global)
        {
            const auto& builtins = get_nonglobal_builtins(currentShaderType, scope); //
            for (const auto& b : builtins) 
            {
                TypePtr t = typeRegistry.getType(b.type);

                if (t->kind == TypeKind::UNKNOWN) {
                    GDSHADER_WARN_IF(true, "Registering builtin '{}' failed: unknown type '{}'", b.name, b.type);
                    continue; 
                }

                Mutability m = (b.qualifier == "in" || b.qualifier == "const") ? Mutability::ReadOnly : Mutability::Mutable;
                Symbol s = symbols.createSymbol(b.name, t, SymbolType::Builtin, {0,0,0,0}, m, nullptr, {}, {}, false, "builtin");
                symbols.add(s);
            }
        }
    }

    void gdshader_lsp::SymbolDeclarationVisitor::loadGlobalVariables()
    {
        GDSHADER_RETURN_IF(currentShaderType == ShaderType::Unknown, "Cannot load global variables for unknown shader type.");
        const auto& builtins = get_global_builtins(currentShaderType);
        GDSHADER_RETURN_IF(builtins.size() <= 0, "Global builtins for shader type are emtpy");

        for (const auto& b : builtins)
        {
            TypePtr t = typeRegistry.getType(b.type);

            if (t->kind == TypeKind::UNKNOWN) {
                GDSHADER_WARN_IF(t->kind == TypeKind::UNKNOWN, "Registering global builtin '{}' failed: unknown type '{}'", b.name, b.type);
                continue;
            }

            Mutability mutability = b.qualifier == "in" ? Mutability::ReadOnly : Mutability::Mutable;
            Symbol s = symbols.createSymbol(b.name, t, SymbolType::Builtin, {0,0,0,0}, mutability, nullptr, {}, {}, false, "builtin");
            symbols.add(s);
        }
    }

    TypePtr SymbolDeclarationVisitor::resolveTypeFromNode(const TypeNode *node)
    {
        GDSHADER_RETURN_VAL_IF(!node, typeRegistry.getUnknownType(), "TypeNode is null in resolveTypeFromNode");

        TypePtr currentType = typeRegistry.getType(node->baseName);
        for (int size : node->arraySizes) {
            currentType = typeRegistry.getArrayType(currentType, size);
        }

        return currentType;
    }

    // End Helper

    void SymbolDeclarationVisitor::visit(ProgramNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "ProgramNode is null");
        for (const auto& child : node->nodes) {
            if (child) child->accept(*this);
        }
    }

    void SymbolDeclarationVisitor::visit(BlockNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "BlockNode is null");
        int startLine = node->range.startLine;
        symbols.pushScope(startLine); 

        for (const auto& stmt : node->statements) {
            if (stmt) stmt->accept(*this);
        }

        symbols.popScope(node->range.endLine); 
    }

    void SymbolDeclarationVisitor::visit(ForNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "ForNode is null");
        int startLine = node->range.startLine;
        symbols.pushScope(startLine); 

        if (node->init) node->init->accept(*this);
        if (node->body) node->body->accept(*this);

        symbols.popScope(node->range.endLine);
    }

    void SymbolDeclarationVisitor::visit(VariableDeclNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "VariableDeclNode is null");
        
        if (node->name.empty()) {
            if (node->initializer) node->initializer->accept(*this);
            return;
        }

        TypePtr type = resolveTypeFromNode(node->type.get());
        if (type->kind == TypeKind::SAMPLER) {
            diagnostics.push_back(reportError(node, DiagnosticCode::LocalSamplerNotAllowed, "Samplers must be declared as uniform, cannot be local."));
        }

        Mutability mut = node->isConst ? Mutability::ReadOnly : Mutability::Mutable;

        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Variable, node->range, mut, nullptr, {}, {}, false, currentFilePath);

        if (symbols.lookup(node->name)) {
            if (symbols.add(s)) {
                diagnostics.push_back(reportWarning(node, DiagnosticCode::VariableShadowed, "Variable '" + node->name + "' shadows an existing declaration.")); 
                GDSHADER_WARN_IF(true, "Variable shadowed: {}", node->name); 
            } else {
                diagnostics.push_back(reportError(node, DiagnosticCode::SymbolRedefinition, "Redefinition of variable '" + node->name + "' in the same scope."));
            }
        } else {
            symbols.add(s);
        }

        if (node->initializer) node->initializer->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(FunctionNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "FunctionNode is null");
        TypePtr returnType = resolveTypeFromNode(node->returnType.get());
        
        std::vector<std::string> paramNames;
        std::vector<TypePtr> paramTypes;
        for (const auto& param : node->parameters) {
            paramTypes.push_back(resolveTypeFromNode(param->type.get()));
            paramNames.push_back(param->name);
        }

        Symbol s = symbols.createSymbol(node->name, {}, SymbolType::Function, node->range, Mutability::ReadOnly, returnType, paramTypes, paramNames, node->is_function_definition, currentFilePath);

        if (!symbols.add(s)) 
        {
            diagnostics.push_back(reportError(node, DiagnosticCode::SymbolRedefinition, "Redefinition of function '" + node->name + "'"));
        }
        
        symbols.pushScope(node->range.startLine);

        if (currentShaderType == ShaderType::Unknown)
        {
            diagnostics.push_back(reportError(node, DiagnosticCode::MissingOrUnknownShaderType, "shader_type missing. Must be one of spatial, canvas_item, particles, sky, or fog."));
        }

        loadBuiltinsForFunction(node->name);

        for (size_t i = 0; i < node->parameters.size(); i++) {
            const auto& param = node->parameters[i];
            TypePtr t = paramTypes[i];
            
            Symbol argSym = symbols.createSymbol(param->name, t, SymbolType::Variable, param->range, Mutability::Mutable, nullptr, {}, {}, false, currentFilePath);
            symbols.add(argSym);
        }

        if (node->body) node->body->accept(*this);

        int endLine = node->body ? node->body->range.endLine : node->range.endLine;
        symbols.popScope(endLine);
    }

    void SymbolDeclarationVisitor::visit(StructNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "StructNode is null");
        std::vector<std::pair<std::string, TypePtr>> members;

        for(const auto& m : node->members) {
            TypePtr memberType = resolveTypeFromNode(m->type.get());
            members.push_back({m->name, memberType});

            if (memberType->kind == TypeKind::UNKNOWN) {
                diagnostics.push_back(reportError(node, DiagnosticCode::UnknownType, "Undefined type in struct member '" + m->name + "'."));
            }
        }

        typeRegistry.registerStruct(node->name, members);
        TypePtr structType = typeRegistry.getType(node->name);

        Symbol s = symbols.createSymbol(node->name, structType, SymbolType::Struct, node->range, Mutability::Mutable, nullptr, {}, {}, false, currentFilePath);
        if (!symbols.add(s)) diagnostics.push_back(reportError(node, DiagnosticCode::SymbolRedefinition, "Redefinition of struct '" + s.name + "'"));
    }

    void SymbolDeclarationVisitor::visit(ShaderTypeNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "ShaderTypeNode is null");
        if (node->shaderType == "canvas_item") currentShaderType = ShaderType::CanvasItem;
        else if (node->shaderType == "spatial") currentShaderType = ShaderType::Spatial;
        else if (node->shaderType == "particles") currentShaderType = ShaderType::Particles;
        else if (node->shaderType == "sky") currentShaderType = ShaderType::Sky;
        else if (node->shaderType == "fog") currentShaderType = ShaderType::Fog;
        else {
            currentShaderType = ShaderType::Unknown;
            GDSHADER_ERROR_IF(true, "Unknown shader type encountered: {}", node->shaderType);
            diagnostics.push_back(reportError(node, DiagnosticCode::MissingOrUnknownShaderType, "shader_type missing. Must be one of spatial, canvas_item, particles, sky, or fog."));
        }
        loadGlobalVariables();
    }

    void gdshader_lsp::SymbolDeclarationVisitor::visit(RenderModeNode *node)
    {
        GDSHADER_RETURN_IF(!node, "RenderModeNode is null");
        
        bool is_not_spatial = (currentShaderType != ShaderType::Spatial);
        GDSHADER_ERROR_IF(is_not_spatial, "render_mode declared outside spatial shader");
        
        if (is_not_spatial)
        {
            diagnostics.push_back(reportError(node, DiagnosticCode::InvalidRenderModeScope, "render_mode declarations are only valid in spatial type shaders."));
        }

        for (const std::string& mode : node->modes)
        {
            bool is_unknown = (stringToRenderMode(mode) == RenderMode::UNKNOWN);
            GDSHADER_ERROR_IF(is_unknown, "Unknown render_mode: {}", mode);
            
            if (is_unknown)
            {
                diagnostics.push_back(reportError(node, DiagnosticCode::UnknownRenderMode, "Unknown render_mode: " + mode));
            }
        }
    }

    void SymbolDeclarationVisitor::visit(UniformNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "UniformNode is null");
        TypePtr type = resolveTypeFromNode(node->type.get());

        if (node->isInstance && type->kind == TypeKind::SAMPLER) {
            diagnostics.push_back(reportError(node, DiagnosticCode::InvalidInstanceUniformType, "Instance uniforms cannot be opaque types (like samplers)."));
        }

        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Uniform, node->range, Mutability::ReadOnly, nullptr, {}, {}, false, currentFilePath);
        s.hint = node->hint;

        if (!symbols.add(s)) {
            diagnostics.push_back(reportError(node, DiagnosticCode::SymbolRedefinition, "Redefinition of uniform '" + s.name + "'"));
        }
        
        if (node->defaultValue) {
            node->defaultValue->accept(*this);
        }
    }

    void SymbolDeclarationVisitor::visit(VaryingNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "VaryingNode is null");
        TypePtr type = resolveTypeFromNode(node->type.get());
        
        bool isInt = (type == typeRegistry.getType("int") || type == typeRegistry.getType("uint"));
        if (isInt && node->interpolation != "flat") {
            diagnostics.push_back(reportError(node, DiagnosticCode::IntegerVaryingNeedsFlat, "Integer varyings must use 'flat' interpolation."));
        }

        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Varying, node->range, Mutability::Mutable, nullptr, {}, {}, false, currentFilePath);        
        if (!symbols.add(s)) {
            diagnostics.push_back(reportError(node, DiagnosticCode::SymbolRedefinition, "Redefinition of varying '" + s.name + "'"));
        }
    }

    void SymbolDeclarationVisitor::visit(ConstNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "ConstNode is null");
        TypePtr type = resolveTypeFromNode(node->type.get());
        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Variable, node->range, Mutability::ReadOnly, nullptr, {}, {}, false, currentFilePath);
        
        if (!symbols.add(s)) {
            diagnostics.push_back(reportError(node, DiagnosticCode::SymbolRedefinition, "Redefinition of const '" + s.name + "'"));
        }

        if (node->value) {
            node->value->accept(*this);
        }
    }

    void SymbolDeclarationVisitor::visit(IfNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "IfNode is null");
        if (node->condition) node->condition->accept(*this);
        if (node->thenBranch) node->thenBranch->accept(*this);
        if (node->elseBranch) node->elseBranch->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(WhileNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "WhileNode is null");
        if (node->condition) node->condition->accept(*this);
        if (node->body) node->body->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(DoWhileNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "DoWhileNode is null");
        if (node->body) node->body->accept(*this);
        if (node->condition) node->condition->accept(*this);
    }

    void SymbolDeclarationVisitor::visit(SwitchNode* node) 
    {
        GDSHADER_RETURN_IF(!node, "SwitchNode is null");
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

} // namespace gdshader_lsp