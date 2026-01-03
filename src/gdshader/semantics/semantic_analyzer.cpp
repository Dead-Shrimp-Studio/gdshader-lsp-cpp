
#include "gdshader/semantics/semantic_analyzer.hpp"
#include "server/project_manager.hpp"

namespace gdshader_lsp {

AnalysisResult SemanticAnalyzer::analyze(const ProgramNode* ast) 
{
    symbols = SymbolTable(); // Clear
    diagnostics.clear();
    typeRegistry = TypeRegistry();

    currentShaderType = ShaderType::Spatial; // Reset default
    currentProcessorFunction = ShaderStage::Global;

    registerGlobalFunctions();
    
    if (ast) {
        visitProgram(ast);
    }
    
    return { std::move(symbols), std::move(typeRegistry), diagnostics, tokens };
}

// -------------------------------------------------------------------------
// TOP LEVEL
// -------------------------------------------------------------------------

void SemanticAnalyzer::visitShaderType(const ShaderTypeNode* node) 
{
    if (node->shaderType == "canvas_item") currentShaderType = ShaderType::CanvasItem;
    else if (node->shaderType == "particles") currentShaderType = ShaderType::Particles;
    else if (node->shaderType == "sky") currentShaderType = ShaderType::Sky;
    else if (node->shaderType == "fog") currentShaderType = ShaderType::Fog;
    else if (node->shaderType == "spatial") currentShaderType = ShaderType::Spatial;
    else {
        reportError(node, "Unknown shader type, must be on of: canvas_item, particles, sky, fog, or spatial.");
        currentShaderType = ShaderType::Spatial;
    }
}

void SemanticAnalyzer::visitUniform(const UniformNode* node) 
{
    TypePtr type = resolveTypeFromNode(node->type.get());
    Symbol s{node->name, type, {type}, SymbolType::Uniform, node->range.startLine, node->range.startCol, node->hint, {}};

    if (!symbols.add(s)) reportError(node, "Redefinition of uniform '" + s.name + "'");
    if (node->defaultValue) {
        visit(node->defaultValue.get());
        TypePtr valType = resolveType(node->defaultValue.get());
        if (*valType != *type && valType->kind != TypeKind::UNKNOWN) {
            reportError(node, "Type mismatch: Cannot initialize uniform '" + node->type->toString() + 
                "' with value of type '" + valType->toString() + "'");
        }
    }

    if (!node->hint.empty()) {
        
        std::string hintName = node->hint;
        size_t parenPos = hintName.find('(');
        if (parenPos != std::string::npos) {
            hintName = hintName.substr(0, parenPos);
        }

        // Validate based on the Godot Docs table
        bool valid = true;
        if (hintName == "source_color") {
            // Valid for: vec3, vec4, sampler2D
            valid = (type->name == "vec3" || type->name == "vec4" || type->name == "sampler2D");
        } 
        else if (hintName == "hint_range") {
            // Valid for: int, float
            size_t start = node->hint.find('(');
            size_t end = node->hint.find(')');
            
            if (start != std::string::npos && end != std::string::npos) {
                std::string args = node->hint.substr(start + 1, end - start - 1);
                
                // Simple comma splitting
                std::vector<std::string> parts;
                std::string current;
                for(char c : args) {
                    if (c == ',') { parts.push_back(current); current=""; }
                    else current += c;
                }
                parts.push_back(current);

                if (parts.size() < 2 || parts.size() > 3) {
                     reportError(node, "Invalid hint_range arguments. Expected (min, max) or (min, max, step).");
                } else {
                    // Try to parse values (basic check)
                    try {
                        float minVal = std::stof(parts[0]);
                        float maxVal = std::stof(parts[1]);
                        if (minVal >= maxVal) {
                            reportWarning(node, "hint_range min value (" + parts[0] + ") is >= max value (" + parts[1] + ").");
                        }
                    } catch (...) {
                        // Parsing failed (maybe expressions used), ignore for now
                    }
                }
            }
        }
        else if (hintName == "hint_normal" || 
                 hintName == "hint_default_white" || 
                 hintName == "hint_default_black" ||
                 hintName == "hint_default_transparent" ||
                 hintName == "hint_screen_texture" || 
                 hintName == "hint_depth_texture") {
            // Valid for: sampler2D
            valid = (type->name == "sampler2D");
        }

        if (!valid) {
            reportError(node, "Hint '" + hintName + "' is not valid for uniform type '" + type->name + "'");
        }
    }
    
    addToken(node->nameRange.startLine, node->nameRange.startCol, 
             node->name.length(), 0, (1 << 1) | (1 << 2));
}

void SemanticAnalyzer::visitVarying(const VaryingNode* node) 
{
    TypePtr type = resolveTypeFromNode(node->type.get());
    Symbol s{node->name, type, {type}, SymbolType::Varying, node->range.startLine, node->range.startCol, "", {}};
    if (!symbols.add(s)) reportError(node, "Redefinition of varying '" + s.name + "'");
    
    addToken(node->nameRange.startLine, node->nameRange.startCol, 
             node->name.length(), 0, (1 << 1) | (1 << 2));
}

void SemanticAnalyzer::visitConst(const ConstNode* node) 
{
    TypePtr type = resolveTypeFromNode(node->type.get());
    Symbol s{node->name, type, {type}, SymbolType::Const, node->range.startLine, node->range.startCol, "", {}};
    if (!symbols.add(s)) reportError(node, "Redefinition of const '" + s.name + "'");

    addToken(node->nameRange.startLine, node->nameRange.startCol, 
             node->name.length(), 0, (1 << 1) | (1 << 2));

    if (node->value) {
        visit(node->value.get());
        TypePtr valType = resolveType(node->value.get());
        if (*valType != *type && valType->kind != TypeKind::UNKNOWN) {
            reportError(node, "Type mismatch in const declaration.");
        }
    }
}

void SemanticAnalyzer::visitStruct(const StructNode* node) 
{
    addToken(node->nameRange.startLine, node->nameRange.startCol, node->name.length(), 2, 1);
    std::vector<std::pair<std::string, TypePtr>> members;

    for(const auto& m : node->members) {

        TypePtr memberType = resolveTypeFromNode(m->type.get());

        if (memberType->kind == TypeKind::UNKNOWN) {
            reportError(m.get(), "Unknown type '" + m->type->toString() + "' in struct member");
        }
        
        // Prevent recursive definition
        if (memberType->name == node->name) {
            reportError(m.get(), "Struct '" + node->name + "' cannot contain itself.");
        }

        members.push_back({m->name, memberType});
        addToken(m->nameRange.startLine, m->nameRange.startCol, m->name.length(), 6, 1);
    }

    typeRegistry.registerStruct(node->name, members);

    // Register the Struct itself as a Symbol so it can be used later
    TypePtr structType = typeRegistry.getType(node->name);
    Symbol s{node->name, structType, {structType}, SymbolType::Struct, node->range.startLine, node->range.startCol, "", {}};
    
    if (!symbols.add(s)) reportError(node, "Redefinition of struct '" + s.name + "'");
}

void SemanticAnalyzer::visitFunction(const FunctionNode* node) 
{
    // 1. Resolve Return Type (Uses the bridge to handle highlighting & array types)
    TypePtr returnType = resolveTypeFromNode(node->returnType.get());
    
    // 2. Resolve Parameter Types
    std::vector<TypePtr> paramTypes;
    for (const auto& param : node->parameters) {
        paramTypes.push_back(resolveTypeFromNode(param->type.get()));
    }

    // 3. Highlight Function Name (using the precise name range from Parser)
    addToken(node->nameRange.startLine, node->nameRange.startCol, node->name.length(), 1, 1);

    // 4. Register Function Symbol
    Symbol funcSym{
        node->name, 
        returnType, 
        paramTypes,
        SymbolType::Function, 
        node->range.startLine, 
        node->range.startCol,
        "",
        {}
    };

    if (!symbols.add(funcSym)) {
        reportError(node, "Redefinition of function '" + node->name + "' with same signature");
    }
    
    // 5. Push Scope (Parameters live here)
    symbols.pushScope(node->range.startLine);

    // --- State Management ---
    ShaderStage previousStage = currentProcessorFunction;
    bool previousReturnFlag = currentFunctionHasReturn;
    TypePtr previousExpected = currentExpectedReturnType;

    if (node->name == "vertex") currentProcessorFunction = ShaderStage::Vertex;
    else if (node->name == "fragment") currentProcessorFunction = ShaderStage::Fragment;
    else if (node->name == "light") currentProcessorFunction = ShaderStage::Light;
    else currentProcessorFunction = ShaderStage::Global;

    currentExpectedReturnType = returnType;
    currentFunctionHasReturn = false;

    // Load built-ins (e.g., ALBEDO, UV)
    loadBuiltinsForFunction(node->name);

    // Processor Validation (vertex/fragment must be void)
    if (isProcessorFunction(node->name)) {
        if (returnType->name != "void") {
            reportError(node, "Processor function '" + node->name + "' must return 'void'.");
        }
        if (!node->parameters.empty()) {
            reportError(node, "Processor function '" + node->name + "' must not have arguments.");
        }
    }
 
    // 6. Register Parameters
    for (size_t i = 0; i < node->parameters.size(); i++) {
        const auto& param = node->parameters[i]; // param is unique_ptr<ParameterNode>
        TypePtr t = paramTypes[i];
        
        Symbol argSym{
            param->name, 
            t, 
            {t}, 
            SymbolType::Variable, 
            param->range.startLine, 
            param->range.startCol, 
            "", 
            {}
        };
        symbols.add(argSym);

        // Highlight Parameter Name
        // (Note: The Type was already highlighted by resolveTypeFromNode)
        addToken(param->nameRange.startLine, param->nameRange.startCol, param->name.length(), 5, 1);
    }

    // 7. Visit Body
    if (node->body) {
        visit(node->body.get());
    }

    // 8. Return Validation
    if (returnType->name != "void" && !currentFunctionHasReturn) {
        reportError(node, "Function '" + node->name + "' must return a value");
    }
    
    // Restore State
    currentProcessorFunction = previousStage;
    currentFunctionHasReturn = previousReturnFlag;
    currentExpectedReturnType = previousExpected;

    // Pop Scope
    int endLine = node->range.endLine; 
    if (node->body) endLine = node->body->range.endLine;
    symbols.popScope(endLine);

    // Control Flow Analysis (All paths return)
    if (returnType->name != "void" && !allPathsReturn(node->body.get())) {
        reportError(node, "Not all code paths return a value in function '" + node->name + "'");
    }
}


void SemanticAnalyzer::visit(const ASTNode* node) 
{
    if (!node) return;

    if (auto p = dynamic_cast<const ProgramNode*>(node)) visitProgram(p);
    else if (auto s = dynamic_cast<const ShaderTypeNode*>(node)) visitShaderType(s);
    else if (auto u = dynamic_cast<const UniformNode*>(node)) visitUniform(u);
    else if (auto v = dynamic_cast<const VaryingNode*>(node)) visitVarying(v);
    else if (auto c = dynamic_cast<const ConstNode*>(node)) visitConst(c);
    else if (auto st = dynamic_cast<const StructNode*>(node)) visitStruct(st);
    else if (auto f = dynamic_cast<const FunctionNode*>(node)) visitFunction(f);
    
    else if (auto b = dynamic_cast<const BlockNode*>(node)) visitBlock(b);
    else if (auto vd = dynamic_cast<const VariableDeclNode*>(node)) visitVarDecl(vd);
    else if (auto i = dynamic_cast<const IfNode*>(node)) visitIf(i);
    else if (auto fr = dynamic_cast<const ForNode*>(node)) visitFor(fr);
    else if (auto w = dynamic_cast<const WhileNode*>(node)) visitWhile(w);
    else if (auto ret = dynamic_cast<const ReturnNode*>(node)) visitReturn(ret);
    else if (auto es = dynamic_cast<const ExpressionStatementNode*>(node)) visitExpressionStatement(es);
    else if (auto dw = dynamic_cast<const DoWhileNode*>(node)) visitDoWhile(dw);
    else if (auto sw = dynamic_cast<const SwitchNode*>(node)) visitSwitch(sw);
    else if (auto d = dynamic_cast<const DiscardNode*>(node)) visitDiscard(d);

    else if (auto inc = dynamic_cast<const IncludeNode*>(node)) visitInclude(inc);
    else if (auto def = dynamic_cast<const DefineNode*>(node)) visitDefine(def);

    else if (auto expr = dynamic_cast<const ExpressionNode*>(node)) visitExpression(expr);
}

void SemanticAnalyzer::visitProgram(const ProgramNode* node) 
{
    // First pass: Find shader_type (it affects everything else)
    for (const auto& child : node->nodes) {
        if (auto s = dynamic_cast<const ShaderTypeNode*>(child.get())) {
            visitShaderType(s);
            break; 
        }
    }

    // Second pass: Visit everything
    for (const auto& child : node->nodes) {
        // Skip ShaderType as we already handled it
        if (dynamic_cast<const ShaderTypeNode*>(child.get())) continue;
        visit(child.get());
    }
}

void gdshader_lsp::SemanticAnalyzer::visitInclude(const IncludeNode *node)
{
    auto pm = ProjectManager::get_singleton();
    std::string absPath = pm->resolvePath(currentFilePath, node->path);

    // 2. Ask PM for exports (this triggers recursion if needed)
    auto exportedSymbols = pm->getExports(absPath);

    if (!exportedSymbols) {
        reportError(node, "Could not load include: " + node->path + " (Cycle or Not Found)");
        return;
    }

    const auto& globals = exportedSymbols->getGlobals();
    
    for (const auto& [name, overloadList] : globals) {
        for (const auto& sym : overloadList) {
            // Check for collisions in current file?
            // Usually, includes act like copy-paste, so redefinition errors are valid.
            if (!symbols.add(sym)) {
                reportError(node, "Symbol '" + name + "' imported from " + node->path + " conflicts with existing symbol.");
            }

            if (sym.category == SymbolType::Struct) {
                typeRegistry.registerStruct(sym.name, sym.type->members);
            }

        }
    }
}

void gdshader_lsp::SemanticAnalyzer::visitDefine(const DefineNode *node)
{
    if (node->value) {
        visit(node->value.get());
        TypePtr type = resolveType(node->value.get());
        
        // Register as a Const Symbol
        // This allows it to be used in array sizes (float x[MAX_LIGHTS]) and math!
        Symbol s{node->name, type, {type}, SymbolType::Const, node->range.startLine, 0, "Macro Definition", {}};
        
        if (!symbols.add(s)) {
            // Optional: Warn about macro redefinition?
        }
    } 
    // Case 2: #define USE_FOG (Flag only)
    else {
        // We register flags as boolean constants so they appear in autocomplete
        // but typically they aren't used in expressions (unless inside #ifdef).
        TypePtr type = typeRegistry.getType("bool");
        Symbol s{node->name, type, {type}, SymbolType::Const, node->range.startLine, 0, "Preprocessor Flag", {}};
        symbols.add(s);
    }

    addToken(node, 3, 1);
}

// -------------------------------------------------------------------------
// STATEMENTS
// -------------------------------------------------------------------------

void SemanticAnalyzer::visitBlock(const BlockNode* node) 
{
    symbols.pushScope(node->range.startLine);
    bool unreachable = false;
    for (const auto& stmt : node->statements) {
        if (unreachable) {
            reportWarning(stmt.get(), "Unreachable code detected.");
            // Only report once per block to avoid spam
            break; 
        }

        visit(stmt.get());

        // Check if this statement terminates control flow
        if (dynamic_cast<const ReturnNode*>(stmt.get()) || 
            dynamic_cast<const DiscardNode*>(stmt.get()) ||
            dynamic_cast<const BreakNode*>(stmt.get()) ||
            dynamic_cast<const ContinueNode*>(stmt.get())) {
            unreachable = true;
        }
    }
    symbols.popScope(node->range.endLine);
}

void SemanticAnalyzer::visitVarDecl(const VariableDeclNode* node) 
{
    TypePtr type = resolveTypeFromNode(node->type.get());

    if (type->kind == TypeKind::UNKNOWN) reportError(node, "Unknown type '" + node->type->toString() + "'");

    if (symbols.lookup(node->name)) 
    {
        // If it exists, but add() succeeds, it means it wasn't in the *current* immediate scope
        // (because add() checks strict redefinition). Therefore, it must be shadowing.
        // We defer the warning until we know add() succeeds.
        
        Symbol s{node->name, type, {type}, SymbolType::Variable, node->range.startLine, node->range.startCol, "", {}};
        if (symbols.add(s)) {
            reportWarning(node, "Variable '" + node->name + "' shadows an existing declaration.");
        } else {
            reportError(node, "Redefinition of variable '" + node->name + "' in the same scope.");
        }
    } else {
        Symbol s{node->name, type, {type}, SymbolType::Variable, node->range.startLine, node->range.startCol, "", {}};
        symbols.add(s);
    }
    
    addToken(node, 0, 1);

    if (node->initializer) {
        visit(node->initializer.get());
        TypePtr initType = resolveType(node->initializer.get());
        
        // Smart Comparison using TypePtr
        if (initType->kind != TypeKind::UNKNOWN && *initType != *type) {
            reportTypeMismatch(node, type->toString(), initType->toString());
        }
    }
}

void SemanticAnalyzer::visitIf(const IfNode* node) 
{
    if (node->condition) visit(node->condition.get());
    TypePtr condT = resolveType(node->condition.get());

    if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) reportError(node, "If condition must be bool");

    if (node->thenBranch) visit(node->thenBranch.get());
    if (node->elseBranch) visit(node->elseBranch.get());
}

void SemanticAnalyzer::visitFor(const ForNode* node) 
{
    symbols.pushScope(node->range.startLine); // 'for' creates a scope for init variable
    if (node->init) visit(node->init.get());
    if (node->condition) visit(node->condition.get());
    if (node->increment) visit(node->increment.get());
    if (node->body) visit(node->body.get());
    symbols.popScope(node->range.endLine);
}

void SemanticAnalyzer::visitWhile(const WhileNode* node) 
{
    if (node->condition) visit(node->condition.get());
    if (node->body) visit(node->body.get());
}

void SemanticAnalyzer::visitReturn(const ReturnNode* node) 
{
    TypePtr actualType = typeRegistry.getType("void");
    currentFunctionHasReturn = true;
    
    if (node->value) {
        visit(node->value.get());
        actualType = resolveType(node->value.get());
    }

    if (!currentExpectedReturnType) return; // Should not happen

    if (currentExpectedReturnType->name == "void") {
        if (actualType->name != "void") reportError(node, "Void function cannot return a value");
    } else {
        if (actualType->name == "void") {
            reportError(node, "Function must return a value of type '" + currentExpectedReturnType->toString() + "'");
        } else if (actualType->kind != TypeKind::UNKNOWN && *actualType != *currentExpectedReturnType) {
            reportError(node, "Type mismatch: Expected return type '" + currentExpectedReturnType->toString() + 
                "' but found '" + actualType->toString() + "'");
        }
    }
}

void SemanticAnalyzer::visitExpressionStatement(const ExpressionStatementNode* node) {
    if (node->expr) visit(node->expr.get());
}

void SemanticAnalyzer::visitDoWhile(const DoWhileNode* node) 
{
    if (node->body) visit(node->body.get());
    
    if (node->condition) {
        visit(node->condition.get());
    }
}

void SemanticAnalyzer::visitSwitch(const SwitchNode* node) 
{
    TypePtr exprType = typeRegistry.getUnknownType();
    
    if (node->expression) {
        visit(node->expression.get());
        exprType = resolveType(node->expression.get());
        
        // GLSL/Godot Switch only allows integers
        if (exprType->name != "int" && exprType->name != "uint" && exprType->name != "unknown") {
            reportError(node->expression.get(), "Switch expression must be 'int' or 'uint', found '" + exprType->name + "'");
        }
    }

    for(const auto& c : node->cases) {
        if (!c->isDefault && c->value) {
            visit(c->value.get());
            TypePtr cType = resolveType(c->value.get());
            if (*cType != *exprType && cType->kind != TypeKind::UNKNOWN) {
                reportError(c->value.get(), "Case Type mismatch");
            }
        }
        for (const auto& stmt : c->statements) visit(stmt.get());
    }
}

void gdshader_lsp::SemanticAnalyzer::visitDiscard(const DiscardNode *node)
{
    if (currentProcessorFunction == ShaderStage::Vertex) {
        reportError(node, "'discard' cannot be used in the vertex processor.");
    }
}

// -------------------------------------------------------------------------
// EXPRESSIONS
// -------------------------------------------------------------------------

void SemanticAnalyzer::visitExpression(const ExpressionNode* node) 
{
    if (auto id = dynamic_cast<const IdentifierNode*>(node)) visitIdentifier(id);
    else if (auto bin = dynamic_cast<const BinaryOpNode*>(node)) visitBinaryOp(bin);
    else if (auto un = dynamic_cast<const UnaryOpNode*>(node)) visitUnaryOp(un);
    else if (auto call = dynamic_cast<const FunctionCallNode*>(node)) visitFunctionCall(call);
    else if (auto mem = dynamic_cast<const MemberAccessNode*>(node)) visitMemberAccess(mem);
    else if (auto tern = dynamic_cast<const TernaryNode*>(node)) {
        visit(tern->condition.get());
        visit(tern->trueExpr.get());
        visit(tern->falseExpr.get());
    }
    // LiteralNode needs no checking
}

void SemanticAnalyzer::visitIdentifier(const IdentifierNode* node) 
{
    const Symbol* s = symbols.lookup(node->name);
    
    if (s)
    {
        uint32_t type = 0;
        uint32_t mod = 0;

        if (s->category == SymbolType::Function || s->category == SymbolType::Builtin) {
            type = 1; // Function
        } 
        else if (s->category == SymbolType::Struct) {
            type = 2; // Struct
        }
        else if (s->category == SymbolType::Const) {
            type = 0; 
            mod |= (1 << 1); // ReadOnly
        }
        else if (s->category == SymbolType::Uniform) {
            type = 0;
            mod |= (1 << 1) | (1 << 2); // ReadOnly + Static
        }

        addToken(node, type, mod);
        symbols.addReference(s, node->range.startLine, node->range.startCol);

    } else {
        reportError(node, "Undefined identifier '" + node->name + "'");
    }
}

void SemanticAnalyzer::visitBinaryOp(const BinaryOpNode* node) 
{
    // Check for assignment to const/read-only
    bool isAssignment = (node->op == TokenType::TOKEN_EQUAL || node->op == TokenType::TOKEN_PLUS_EQUAL ||
                        node->op == TokenType::TOKEN_MINUS_EQUAL || node->op == TokenType::TOKEN_STAR_EQUAL || 
                        node->op == TokenType::TOKEN_SLASH_EQUAL || node->op == TokenType::TOKEN_PERCENT_EQUAL);

    if (isAssignment)
    {
        visitAssignment(node);
        return;
    }
    
    if (node->left) visit(node->left.get());
    if (node->right) visit(node->right.get());

    TypePtr l = resolveType(node->left.get());
    TypePtr r = resolveType(node->right.get());

    // If children are already "unknown", an error was likely already reported 
    // (e.g., "Undefined identifier"). Stop here to prevent error cascading.
    if (l->kind == TypeKind::UNKNOWN || r->kind == TypeKind::UNKNOWN) return;

    TypePtr result = getBinaryOpResultType(l, r, node->op);
    if (result->kind == TypeKind::UNKNOWN) {
        reportError(node, "Invalid binary operation '" + l->toString() + "' and '" + r->toString() + "'");
    }
}

void SemanticAnalyzer::visitAssignment(const BinaryOpNode* node) 
{
    if (node->right) visit(node->right.get());

    const Symbol* s = getRootSymbol(node->left.get(), symbols);
    TypePtr lType = typeRegistry.getUnknownType();

    if (auto id = dynamic_cast<const IdentifierNode*>(node->left.get())) {
        s = symbols.lookup(id->name);
        if (s) lType = s->type;
        else reportError(node, "Undefined '" + id->name + "'");
    } else if (dynamic_cast<const MemberAccessNode*>(node->left.get())) {
        visit(node->left.get());
        lType = resolveType(node->left.get());
    }

    // 3. Permission Checks (Const, Uniform, Varying)
    if (s) {
        if (s->category == SymbolType::Const) {
            reportError(node->left.get(), "Cannot assign to constant '" + s->name + "'");
        } 
        else if (s->category == SymbolType::Uniform) {
            reportError(node->left.get(), "Cannot assign to uniform '" + s->name + "'");
        }
        else if (s->category == SymbolType::Varying && currentProcessorFunction != ShaderStage::Vertex) {
            reportError(node->left.get(), "Varyings are read-only in the fragment processor.");
        }
    }

    TypePtr rType = resolveType(node->right.get());
    if (lType->kind == TypeKind::UNKNOWN || rType->kind == TypeKind::UNKNOWN) return;

    // 3. Logic for Assignment vs Compound Assignment
    if (node->op == TokenType::TOKEN_EQUAL) {
        // Simple Assignment (=)
        // Strict match or safe implicit conversion (e.g. float = int)
        int cost = getConversionCost(rType, lType);
        if (cost == -1) {
             reportError(node, "Type mismatch: Cannot assign '" + rType->toString() + 
                         "' to '" + lType->toString() + "'");
        }
    }
    else {
        // Compound Assignment (+=, *=, etc.)
        // We must check if the MATH operation is valid, and if the RESULT fits in LHS.
        
        TokenType mathOp;
        switch (node->op) {
            case TokenType::TOKEN_PLUS_EQUAL:    mathOp = TokenType::TOKEN_PLUS; break;
            case TokenType::TOKEN_MINUS_EQUAL:   mathOp = TokenType::TOKEN_MINUS; break;
            case TokenType::TOKEN_STAR_EQUAL:    mathOp = TokenType::TOKEN_STAR; break;
            case TokenType::TOKEN_SLASH_EQUAL:   mathOp = TokenType::TOKEN_SLASH; break;
            case TokenType::TOKEN_PERCENT_EQUAL: mathOp = TokenType::TOKEN_PERCENT; break;
            default: mathOp = TokenType::TOKEN_ERROR; break;
        }

        // 1. Check if the math is valid (e.g. vec2 * float -> vec2)
        TypePtr resultType = getBinaryOpResultType(lType, rType, mathOp);
        
        if (resultType->kind == TypeKind::UNKNOWN) {
            reportError(node, "Invalid operation for compound assignment.");
            return;
        }

        // 2. Check if the result can be assigned back to LHS
        // e.g. int += float -> (int + float) is float. float cannot be assigned to int. Error.
        // e.g. vec2 *= float -> (vec2 * float) is vec2. vec2 can be assigned to vec2. OK.
        int cost = getConversionCost(resultType, lType);
        if (cost == -1) {
            reportError(node, "Type mismatch: Result of compound assignment '" + resultType->toString() + 
                        "' cannot be assigned to '" + lType->toString() + "'");
        }
    }

    const ExpressionNode* lhs = node->left.get();
    
    if (auto mem = dynamic_cast<const MemberAccessNode*>(lhs)) {
        // CHECK: Swizzle duplication (e.g. v.xx = vec2(1.0) is INVALID)
        // Only applies if the member is a swizzle (len <= 4 and chars are xyzw/rgba/stpq)
        // We assume valid chars because visitMemberAccess checked that.
        
        std::string s = mem->member;
        if (s.length() <= 4) { // Heuristic: it's likely a swizzle
            
            bool isSwizzleChars = true;
            const std::string validSet = "xyzwrugbstpq"; // Combined sets
            for (char c : s) {
                if (validSet.find(c) == std::string::npos) {
                    isSwizzleChars = false;
                    break;
                }
            }

            if (isSwizzleChars && s.length() <= 4) {
                for(size_t i=0; i<s.length(); ++i) {
                    for(size_t j=i+1; j<s.length(); ++j) {
                        if (s[i] == s[j]) {
                            reportError(node, "Invalid Write Mask: Component '" + 
                            std::string(1, s[i]) + "' is assigned twice.");
                        }
                    }
                }
            }
        }
    }

    
}

void gdshader_lsp::SemanticAnalyzer::visitArrayAccess(const ArrayAccessNode *node)
{
    if (node->base) visit(node->base.get());
    if (node->index) visit(node->index.get());

    TypePtr baseType = resolveType(node->base.get());
    TypePtr indexType = resolveType(node->index.get());

    // 1. Validate Base is Array or Vector
    if (baseType->kind != TypeKind::ARRAY && baseType->kind != TypeKind::VECTOR && baseType->kind != TypeKind::MATRIX) {
        reportError(node, "Type '" + baseType->toString() + "' is not indexable.");
    }

    // 2. Validate Index is Integer
    if (indexType->name != "int" && indexType->name != "uint") {
        reportError(node->index.get(), "Array index must be an integer.");
    }
}

void SemanticAnalyzer::visitUnaryOp(const UnaryOpNode* node) {
    if (node->operand) visit(node->operand.get());
}

/**
 * @brief Visits any function call, even builtins. We analyze the arguments first, so that we can resolve types and overloads.
 * 
 * @param node 
 */
void SemanticAnalyzer::visitFunctionCall(const FunctionCallNode* node) 
{
    std::vector<TypePtr> argTypes;
    for (const auto& arg : node->arguments) {
        visit(arg.get());
        argTypes.push_back(resolveType(arg.get()));
    }

    std::string name = node->functionName;

    // (If the name matches a known type, it's a constructor)
    if (typeRegistry.getType(name)->kind != TypeKind::UNKNOWN) {
        validateConstructor(node, name);
        addToken(node, 4, 0);
        return; 
    }

    addToken(node, 1, 0);

    // 2. Function Lookup
    // This now finds BOTH User functions AND Built-ins (because of registerGlobalFunctions)
    std::vector<const Symbol*> candidates = symbols.lookupFunctions(name);

    if (candidates.empty()) {
        reportError(node, "Unknown function '" + name + "'");
        return;
    }

    const Symbol* bestMatch = findBestOverload(node, argTypes);
    
    if (bestMatch) {
        symbols.addReference(bestMatch, node->range.startLine, node->range.startCol);            
    } else {
        
        std::vector<const Symbol*> arityMatches;
        for (const auto* s : candidates) {
            if (s->parameterTypes.size() == argTypes.size()) {
                arityMatches.push_back(s);
            }
        }

        if (arityMatches.empty()) {
            // Case: Wrong number of arguments
            // Just grab the first candidate to show expected count (usually sufficient for simple functions)
            size_t expected = candidates[0]->parameterTypes.size();
            reportError(node, "Invalid argument count for '" + name + "'. Expected " + 
                        std::to_string(expected) + ", but got " + std::to_string(argTypes.size()) + ".");
        } 
        else {
            // Case: Correct arg count, but wrong types.
            // We need to find the "closest" match to report the most relevant error.
            
            const Symbol* closest = arityMatches[0];
            int maxMatches = -1;

            for (const auto* cand : arityMatches) {
                int matches = 0;
                for(size_t i=0; i < argTypes.size(); ++i) {
                    if (getConversionCost(argTypes[i], cand->parameterTypes[i]) != -1) {
                        matches++;
                    }
                }
                if (matches > maxMatches) {
                    maxMatches = matches;
                    closest = cand;
                }
            }

            // Now report the specific mismatch in the closest candidate
            for (size_t i = 0; i < argTypes.size(); ++i) {
                TypePtr expected = closest->parameterTypes[i];
                TypePtr actual = argTypes[i];
                
                if (getConversionCost(actual, expected) == -1) {
                    reportError(node->arguments[i].get(), 
                    "Invalid argument " + std::to_string(i + 1) + " for function '" + name + "'. Expected '" + 
                    expected->toString() + "', but found '" + actual->toString() + "'.");
                    break; 
                }
            }
        }
    }
}

void SemanticAnalyzer::visitMemberAccess(const MemberAccessNode* node) 
{
    if (node->base) visit(node->base.get());
    TypePtr baseT = resolveType(node->base.get());

    if (baseT->kind == TypeKind::UNKNOWN) return;

    // FIX: Allow .length() on arrays
    if (baseT->kind == TypeKind::ARRAY && node->member == "length") {
        return; // Valid
    }

    addToken(node, 6, 0);

    TypePtr memberT = typeRegistry.getMemberType(baseT, node->member);
    if (memberT->kind == TypeKind::UNKNOWN) {
        
        if (baseT->kind == TypeKind::VECTOR) {
            std::string swizzle = node->member;
            
            // Check 1: Length
            if (swizzle.length() > 4) {
                reportError(node, "Swizzle '" + swizzle + "' is too long (max 4 components).");
                return;
            }

            // Check 2: Component Validity for this vector size
            const std::string sets[] = {"xyzw", "rgba", "stpq"};
            int validSetIndex = -1;

            for (int i = 0; i < 3; i++) {
                bool partOfSet = true;
                for (char c : swizzle) {
                    if (sets[i].find(c) == std::string::npos) {
                        partOfSet = false;
                        break;
                    }
                }
                if (partOfSet) {
                    validSetIndex = i;
                    break;
                }
            }

            if (validSetIndex == -1) {
                // Check for mixed sets (e.g. "xg")
                bool hasXYZW = false, hasRGBA = false;
                for (char c : swizzle) {
                    if (std::string("xyzw").find(c) != std::string::npos) hasXYZW = true;
                    if (std::string("rgba").find(c) != std::string::npos) hasRGBA = true;
                }
                if (hasXYZW && hasRGBA) {
                    reportError(node, "Illegal swizzle '" + swizzle + "'. Cannot mix xyzw and rgba sets.");
                } else {
                    reportError(node, "Invalid swizzle component in '" + swizzle + "'.");
                }
                return;
            }

            // Check 3: Bounds (e.g. vec2.z)
            // validSetIndex is 0(xyzw), 1(rgba), or 2(stpq)
            std::string currentSet = sets[validSetIndex];
            for (char c : swizzle) {
                size_t componentIndex = currentSet.find(c);
                if (componentIndex >= (size_t)baseT->componentCount) {
                    reportError(node, "Swizzle component '" + std::string(1, c) + "' is out of bounds for " + baseT->toString() + ".");
                    return;
                }
            }
        }
        
        reportError(node, "Invalid member '" + node->member + "' on type '" + baseT->toString() + "'");
    }
}

// -------------------------------------------------------------------------
// HELPERS
// -------------------------------------------------------------------------

void SemanticAnalyzer::validateConstructor(const FunctionCallNode* node, const std::string& typeName) 
{
    TypePtr target = typeRegistry.getType(typeName);
    
    if (target->kind == TypeKind::STRUCT) {
        
        if (node->arguments.size() != target->members.size()) {
            reportError(node, "Constructor for '" + typeName + "' expects " + 
                        std::to_string(target->members.size()) + " arguments, but got " + 
                        std::to_string(node->arguments.size()));
            return;
        }

        for(size_t i = 0; i < target->members.size(); ++i) {
            TypePtr argType = resolveType(node->arguments[i].get());
            TypePtr expectedType = target->members[i].second;

            // Strict Type Check (Structs don't usually allow implicit casting in constructors)
            // This works for Arrays too because of your Type::operator==
            if (*argType != *expectedType) {
                reportError(node->arguments[i].get(), 
                    "Type mismatch in struct constructor argument " + std::to_string(i+1) + 
                    ". Expected '" + expectedType->toString() + "', found '" + argType->toString() + "'");
            }
        }
        return;
    }

    if (target->kind == TypeKind::VECTOR) {
        int expected = target->componentCount;
        int provided = 0;

        for (const auto& arg : node->arguments) {
            TypePtr argT = resolveType(arg.get());
            
            // Allow constructing vectors from any Scalar or Vector type (implicit casting)
            // e.g. vec2(ivec2) or vec2(int, float) is valid.
            // We only check if valid types are passed (no structs/arrays).
            if (argT->kind != TypeKind::SCALAR && argT->kind != TypeKind::VECTOR) {
                reportError(arg.get(), "Invalid argument for vector constructor.");
            }

            // 2. Count Components
            provided += (argT->kind == TypeKind::SCALAR) ? 1 : argT->componentCount;
        }

        // Exception: Splatting (vec3(1.0) -> 1,1,1)
        if (node->arguments.size() == 1 && provided == 1 && expected > 1) return;

        if (provided != expected) {
            reportError(node, "Invalid constructor. Expected " + std::to_string(expected) + 
                " components, but found " + std::to_string(provided));
        }
        return;
    }

    if (target->kind == TypeKind::MATRIX) {
        int expected = target->componentCount * target->componentCount; // e.g., mat2 is 2x2 = 4

        if (target->name == "mat2") expected = 4;
        else if (target->name == "mat3") expected = 9;
        else if (target->name == "mat4") expected = 16;
        
        int provided = 0;
        for (const auto& arg : node->arguments) {
            TypePtr argT = resolveType(arg.get());
            if (argT->kind == TypeKind::SCALAR) provided += 1;
            else if (argT->kind == TypeKind::VECTOR) provided += argT->componentCount;
            // Matrices can also be constructed from other matrices, which is complex to validate
            else if (argT->kind == TypeKind::MATRIX) provided += 100; // Let's just assume it's fine for now
        }

        // Only complain if we clearly don't have enough components
        // (excluding the generic matrix-from-matrix case)
        if (provided < expected && provided < 50) {
            reportError(node, "Not enough components to construct '" + typeName + "'");
        }
    }

}

bool gdshader_lsp::SemanticAnalyzer::isProcessorFunction(const std::string &name)
{
    return name == "vertex" || name == "fragment" || name == "light" || 
           name == "start" || name == "process" || name == "sky" || name == "fog";
}

bool gdshader_lsp::SemanticAnalyzer::allPathsReturn(const ASTNode *node)
{
    if (!node) return false;

    if (dynamic_cast<const ReturnNode*>(node)) return true;
    if (dynamic_cast<const DiscardNode*>(node)) return true;
    
    if (auto block = dynamic_cast<const BlockNode*>(node)) {
        for (const auto& stmt : block->statements) {
            if (allPathsReturn(stmt.get())) return true;
        }
        return false;
    }

    if (auto ifNode = dynamic_cast<const IfNode*>(node)) {
        // BOTH branches must return
        return ifNode->elseBranch && 
               allPathsReturn(ifNode->thenBranch.get()) && 
               allPathsReturn(ifNode->elseBranch.get());
    }

    // Note: Loops (For/While) are generally considered "not returning" 
    // because the loop body might never run.
    
    return false;
}

TypePtr gdshader_lsp::SemanticAnalyzer::getBinaryOpResultType(TypePtr l, TypePtr r, TokenType op)
{
    if (l->kind == TypeKind::UNKNOWN || r->kind == TypeKind::UNKNOWN) return typeRegistry.getUnknownType();

    // --- 1. Comparison Operators (Always return bool) ---
    bool isComparison = (op == TokenType::TOKEN_EQ_EQ || op == TokenType::TOKEN_NOT_EQ ||
                         op == TokenType::TOKEN_LESS || op == TokenType::TOKEN_GREATER ||
                         op == TokenType::TOKEN_LESS_EQ || op == TokenType::TOKEN_GREATER_EQ);

    if (isComparison) {
        // Types must generally be comparable (e.g., match exactly, or implicit cast)
        // For strictness: assume they must match types (vec3 == vec3) or be vector/scalar compatible.
        // For now, let's allow it if they are somewhat compatible.
        return typeRegistry.getType("bool"); 
    }

    // --- 2. Arithmetic Exact Match (int+int, vec3+vec3, mat4*mat4) ---
    if (*l == *r) {
        return l; 
    }

    // --- 3. Vector-Scalar Mixing (vec3 * 2.0) ---
    bool lVec = (l->kind == TypeKind::VECTOR);
    bool rVec = (r->kind == TypeKind::VECTOR);
    bool lScalar = (l->kind == TypeKind::SCALAR);
    bool rScalar = (r->kind == TypeKind::SCALAR);

    if (lVec && rScalar) {
        // Check if scalar type matches vector base (float * vec3 vs int * vec3)
        if (l->baseType && *l->baseType == *r) return l; 
    }
    if (rVec && lScalar) {
        if (r->baseType && *r->baseType == *l) return r;
    }

    // --- 4. Matrix Math (The Hard Part) ---
    bool lMat = (l->kind == TypeKind::MATRIX);
    bool rMat = (r->kind == TypeKind::MATRIX);

    // Matrix * Scalar (Component-wise)
    if ((lMat && rScalar) || (rMat && lScalar)) {
         // Assuming float scalar for matrices
         if ((lScalar && l->name == "float") || (rScalar && r->name == "float")) {
             return lMat ? l : r;
         }
    }

    // Matrix * Vector (Linear Algebra: Transform)
    // mat4 (4x4) * vec4 (4x1) -> vec4
    if (lMat && rVec) {
        if (l->componentCount == r->componentCount) return r; 
    }

    // Vector * Matrix (Linear Algebra: Post-multiply)
    // vec4 (1x4) * mat4 (4x4) -> vec4
    if (lVec && rMat) {
        if (l->componentCount == r->componentCount) return l;
    }

    // Matrix * Matrix 
    // mat4 * mat4 -> mat4
    if (lMat && rMat) {
        if (l->componentCount == r->componentCount) return l;
    }

    return typeRegistry.getUnknownType();
}

const Symbol *gdshader_lsp::SemanticAnalyzer::findBestOverload(const FunctionCallNode* node, const std::vector<TypePtr>& argTypes)
{
    std::string name = node->functionName;
    auto candidates = symbols.lookupFunctions(name);
    if (candidates.empty()) return nullptr;

    // 3. Overload Resolution
    const Symbol* bestMatch = nullptr;
    int minCost = 999999;
    bool isAmbiguous = false;

    for (const auto* sym : candidates) {
        // A. Check Argument Count
        if (sym->parameterTypes.size() != argTypes.size()) continue;

        // B. Calculate Cost for this Candidate
        int currentCost = 0;
        bool possible = true;

        for (size_t i = 0; i < argTypes.size(); i++) {
            // Check conversion for each argument
            int cost = getConversionCost(argTypes[i], sym->parameterTypes[i]);
            
            if (cost == -1) {
                possible = false;
                break; // Argument mismatch
            }
            currentCost += cost;
        }

        if (possible) {
            if (currentCost < minCost) {
                // Found a better match
                minCost = currentCost;
                bestMatch = sym;
                isAmbiguous = false;
            } 
            else if (currentCost == minCost) {
                // Found another match with the exact same score -> Ambiguous
                // (Unless it's the exact same symbol, which shouldn't happen)
                isAmbiguous = true;
            }
        }
    }
    
    if (isAmbiguous) {
        reportError(node, "Ambiguous function call for '" + name + "'. Multiple overloads match these arguments.");
    }
    return bestMatch;
}

int gdshader_lsp::SemanticAnalyzer::getConversionCost(TypePtr from, TypePtr to)
{
    if (*from == *to) return 0;
    
    if (from->name == "int" && to->name == "float") return 1;
    // uint -> float is usually safe
    if (from->name == "uint" && to->name == "float") return 1;
    return -1; // No conversion
}

const Symbol *gdshader_lsp::SemanticAnalyzer::getRootSymbol(const ExpressionNode *node, const SymbolTable &symbols)
{
    if (auto id = dynamic_cast<const IdentifierNode*>(node)) {
        return symbols.lookup(id->name);
    }
    return nullptr;
}

int SemanticAnalyzer::getNodeLength(const ASTNode* node) {
    if (!node) return 1;

    // 1. Identifiers (e.g. "my_var")
    if (auto n = dynamic_cast<const IdentifierNode*>(node)) {
        return n->name.length();
    }
    // 2. Function Calls (e.g. "max(...)") - highlights "max"
    if (auto n = dynamic_cast<const FunctionCallNode*>(node)) {
        return n->functionName.length();
    }
    // 3. Literals (e.g. "1.0", "true")
    if (auto n = dynamic_cast<const LiteralNode*>(node)) {
        return n->value.length();
    }
    // 4. Types / Variables (e.g. "vec3")
    if (auto n = dynamic_cast<const VariableDeclNode*>(node)) {
        return n->name.length(); 
    }
    // 5. Member Access (e.g. ".x") - Parser usually sets loc to the dot
    if (auto n = dynamic_cast<const MemberAccessNode*>(node)) {
        return 1 + n->member.length(); // ".member"
    }
    // 6. Binary Ops (highlight the operator, e.g. +=)
    if (auto n = dynamic_cast<const BinaryOpNode*>(node)) {
        TokenType t = n->op;
        if (t == TokenType::TOKEN_PLUS_EQUAL || t == TokenType::TOKEN_MINUS_EQUAL || 
            t == TokenType::TOKEN_STAR_EQUAL || t == TokenType::TOKEN_SLASH_EQUAL ||
            t == TokenType::TOKEN_EQ_EQ || t == TokenType::TOKEN_NOT_EQ ||
            t == TokenType::TOKEN_LESS_EQ || t == TokenType::TOKEN_GREATER_EQ ||
            t == TokenType::TOKEN_AND || t == TokenType::TOKEN_OR) {
            return 2; 
        }
        return 1; // +, -, *, /
    }

    // Fallback
    return 1;
}

void SemanticAnalyzer::loadBuiltinsForFunction(const std::string& funcName) 
{
    ShaderStage scope = ShaderStage::Global;
    if (funcName == "vertex") scope = ShaderStage::Vertex;
    else if (funcName == "fragment") scope = ShaderStage::Fragment;
    else if (funcName == "light") scope = ShaderStage::Light;
    else if (funcName == "start") scope = ShaderStage::Start;
    else if (funcName == "process") scope = ShaderStage::Process;
    
    if (scope != ShaderStage::Global) {
        const auto& builtins = get_builtins(currentShaderType, scope);
        for (const auto& b : builtins) {
            TypePtr t = typeRegistry.getType(b.type);
            symbols.add({b.name, t, {t}, SymbolType::Builtin, -1, 0, b.doc, {}});
        }
    }
}

TypePtr gdshader_lsp::SemanticAnalyzer::resolveType(const ExpressionNode *node)
{
    if (!node) return typeRegistry.getType("void");

    // 1. Literal?
    if (auto lit = dynamic_cast<const LiteralNode*>(node)) {
        if (lit->type == TokenType::TOKEN_NUMBER) return (lit->value.find('.') != std::string::npos) ? typeRegistry.getType("float") : typeRegistry.getType("int");
        if (lit->type == TokenType::KEYWORD_TRUE || lit->type == TokenType::KEYWORD_FALSE) return typeRegistry.getType("bool");
        return typeRegistry.getType("void");
    }

    // 2. Identifier? Lookup in Symbol Table
    if (auto id = dynamic_cast<const IdentifierNode*>(node)) {
        if (const Symbol* s = symbols.lookup(id->name)) {
            return s->type;
        }
        return typeRegistry.getUnknownType();
    }

    // 3. Binary Op? (Simplify: float + float = float)
    if (auto bin = dynamic_cast<const BinaryOpNode*>(node)) {
        TypePtr l = resolveType(bin->left.get());
        TypePtr r = resolveType(bin->right.get());
        
        return getBinaryOpResultType(l, r, bin->op);
    }

    if (auto idx = dynamic_cast<const ArrayAccessNode*>(node)) {
        TypePtr base = resolveType(idx->base.get());
        if (base->kind == TypeKind::ARRAY) return base->baseType;
        if (base->kind == TypeKind::VECTOR) return base->baseType;
        if (base->kind == TypeKind::MATRIX) return typeRegistry.getType("vec" + std::to_string(base->componentCount)); // mat4[0] -> vec4
        return typeRegistry.getUnknownType();
    }

    // 4. Function Call? Lookup return type
    if (auto call = dynamic_cast<const FunctionCallNode*>(node)) {
        
        // A. Constructors
        TypePtr t = typeRegistry.getType(call->functionName);
        if (t->kind != TypeKind::UNKNOWN) return t;

        std::vector<TypePtr> argTypes;
        for (const auto& arg : call->arguments) {
            argTypes.push_back(resolveType(arg.get()));
        }

        const Symbol* match = findBestOverload(call, argTypes);

        if (match) {
            return match->type;
        }
        
        return typeRegistry.getUnknownType();
    }
    
    // 5. Member Access (The Tricky Part)
    if (auto mem = dynamic_cast<const MemberAccessNode*>(node)) {
        if (mem->member == "length") return typeRegistry.getType("int");
        TypePtr base = resolveType(mem->base.get());
        return typeRegistry.getMemberType(base, mem->member);
    }

    if (auto un = dynamic_cast<const UnaryOpNode*>(node)) {
        // The type of "-x" is usually just the type of "x"
        return resolveType(un->operand.get());
    }

    if (auto tern = dynamic_cast<const TernaryNode*>(node)) {
        return resolveType(tern->trueExpr.get());
    }

    return typeRegistry.getUnknownType();
}

TypePtr gdshader_lsp::SemanticAnalyzer::resolveTypeFromNode(const TypeNode *node)
{
    if (!node) return typeRegistry.getUnknownType(); // or unknown

    // Note: We use the baseName range here, not the whole node range (which includes [5])
    addToken(node->baseNameRange.startLine, node->baseNameRange.startCol, node->baseNameRange.endCol - node->baseNameRange.startCol, 4, 0); // 4 = Type color

    TypePtr currentType = typeRegistry.getType(node->baseName);

    for (int size : node->arraySizes) {
        currentType = typeRegistry.getArrayType(currentType, size);
    }
    return currentType;
}

void gdshader_lsp::SemanticAnalyzer::addToken(const ASTNode *node, uint32_t type, uint32_t modifiers)
{
    if (!node) return;
    int len = getNodeLength(node);
    tokens.push_back({node->range.startLine, node->range.startCol, node->range.endCol - node->range.startCol, type, modifiers});
}

void gdshader_lsp::SemanticAnalyzer::addToken(int line, int col, int len, uint32_t type, uint32_t modifiers)
{
    tokens.push_back({line, col, len, type, modifiers});
}

void SemanticAnalyzer::reportError(const ASTNode* node, const std::string& msg) {
    if (node) {
        int len = getNodeLength(node);
        diagnostics.push_back({node->range.startLine, node->range.startCol, msg, DiagnosticLevel::Error, len});
    }
}

void SemanticAnalyzer::reportWarning(const ASTNode* node, const std::string& msg) {
    if (node) {
        int len = getNodeLength(node);
        diagnostics.push_back({node->range.startLine, node->range.startCol, msg, DiagnosticLevel::Warning, len});
    }
}

void SemanticAnalyzer::reportTypeMismatch(const ASTNode* node, const std::string& expected, const std::string& found) {
    reportError(node, "Type mismatch: Expected '" + expected + "', but found '" + found + "'.");
}

void SemanticAnalyzer::registerGlobalFunctions()
{
    const std::vector<std::string> VEC_TYPE      = {"float", "vec2", "vec3", "vec4"};
    const std::vector<std::string> VEC_INT_TYPE  = {"int", "ivec2", "ivec3", "ivec4"};
    const std::vector<std::string> VEC_UINT_TYPE = {"uint", "uvec2", "uvec3", "uvec4"};
    const std::vector<std::string> VEC_BOOL_TYPE = {"bool", "bvec2", "bvec3", "bvec4"};
    
    // Sampler Expansion: 0=float, 1=int, 2=uint
    const std::vector<std::string> GSAMPLER_2D      = {"sampler2D", "isampler2D", "usampler2D"};
    const std::vector<std::string> GSAMPLER_2D_ARR  = {"sampler2DArray", "isampler2DArray", "usampler2DArray"};
    const std::vector<std::string> GSAMPLER_3D      = {"sampler3D", "isampler3D", "usampler3D"};
    const std::vector<std::string> GVEC4_TYPE       = {"vec4", "ivec4", "uvec4"}; // Return types matching sampler

    // Matrix Expansion
    const std::vector<std::string> MAT_TYPE = {"mat2", "mat3", "mat4"};

    // Helper: Expands "vec_type" -> "vec3", "vec_int_type" -> "ivec3" based on index
    auto resolveGeneric = [&](const std::string& generic, int idx) -> std::string {
        if (generic == "vec_type") return VEC_TYPE[idx];
        if (generic == "vec_int_type") return VEC_INT_TYPE[idx];
        if (generic == "vec_uint_type") return VEC_UINT_TYPE[idx];
        if (generic == "vec_bool_type") return VEC_BOOL_TYPE[idx];
        // gvec4_type is handled in the sampler loop usually, but if independent:
        if (generic == "gvec4_type") return GVEC4_TYPE[std::min(idx, 2)]; 
        return generic;
    };

    // Helper: Register a single concrete function
    auto registerConcrete = [&](const std::string& name, const std::string& ret, const std::vector<std::string>& args, const std::string& doc) {
        TypePtr returnType = typeRegistry.getType(ret);
        std::vector<TypePtr> argTypes;
        for (const auto& a : args) argTypes.push_back(typeRegistry.getType(a));
        Symbol s{name, returnType, argTypes, SymbolType::Builtin, -1, 0, doc, {}};
        symbols.add(s);
    };

    for (const auto& func : GLOBAL_FUNCTIONS) {
        
        bool isVectorGeneric = false;
        bool isSamplerGeneric = false;
        bool isMatrixGeneric = false;

        for (const auto& t : func.argTypes) {
            if (t.find("vec_") == 0) isVectorGeneric = true;
            if (t.find("gsampler") == 0) isSamplerGeneric = true;
            if (t == "mat_type") isMatrixGeneric = true;
        }
        if (func.returnType.find("vec_") == 0) isVectorGeneric = true;
        if (func.returnType == "mat_type") isMatrixGeneric = true;

        if (isVectorGeneric) {
            // Expand 4 times (Scalar, Vec2, Vec3, Vec4)
            for (int i = 0; i < 4; i++) {
                std::string r = resolveGeneric(func.returnType, i);
                std::vector<std::string> a;
                for(const auto& arg : func.argTypes) a.push_back(resolveGeneric(arg, i));
                registerConcrete(func.name, r, a, func.doc);
            }
        }
        else if (isSamplerGeneric) {
            // Expand 3 times (Float, Int, Uint samplers)
            for (int i = 0; i < 3; i++) {
                std::string r = func.returnType;
                if (r == "gvec4_type") r = GVEC4_TYPE[i];

                std::vector<std::string> a;
                for (const auto& arg : func.argTypes) {
                    if (arg == "gsampler2D") a.push_back(GSAMPLER_2D[i]);
                    else if (arg == "gsampler2DArray") a.push_back(GSAMPLER_2D_ARR[i]);
                    else if (arg == "gsampler3D") a.push_back(GSAMPLER_3D[i]);
                    else a.push_back(arg);
                }
                registerConcrete(func.name, r, a, func.doc);
            }
        }
        else if (isMatrixGeneric) {
            // Expand 3 times (mat2, mat3, mat4)
            for (int i = 0; i < 3; i++) {
                std::string r = (func.returnType == "mat_type") ? MAT_TYPE[i] : func.returnType;
                std::vector<std::string> a;
                for (const auto& arg : func.argTypes) {
                    a.push_back((arg == "mat_type") ? MAT_TYPE[i] : arg);
                }
                registerConcrete(func.name, r, a, func.doc);
            }
        }
        else {
            // No generics, register as is
            registerConcrete(func.name, func.returnType, func.argTypes, func.doc);
        }
    }
}

} // end of namespace