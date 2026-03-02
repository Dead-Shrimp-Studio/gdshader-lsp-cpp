#include "gdshader/diagnostics.hpp"
#include "gdshader/ast/ast.h"
#include "gdshader/semantics/visitors/type_checking_visitor.hpp"

namespace gdshader_lsp {

// --- Pass Throughs ---
void TypeCheckingVisitor::visit(ProgramNode* node) {
    for (const auto& child : node->nodes) if (child) child->accept(*this);
}
void TypeCheckingVisitor::visit(BlockNode* node) {
    for (const auto& stmt : node->statements) if (stmt) stmt->accept(*this);
}
void TypeCheckingVisitor::visit(ExpressionStatementNode* node) {
    if (node->expr) node->expr->accept(*this);
}

// --- Declarations Validation ---

void TypeCheckingVisitor::visit(VariableDeclNode* node) {
    if (node->initializer) {
        node->initializer->accept(*this);
        
        // We use lookupAt to get the type we registered in Pass 1
        int line = (node->range.startLine > 0) ? node->range.startLine - 1 : 0;
        const Symbol* s = symbols.lookupAt(node->name, line);
        
        if (s) {
            TypePtr initType = resolveType(node->initializer.get());
            if (initType->kind != TypeKind::UNKNOWN && *initType != *s->type) {
                reportTypeMismatch(node, s->type->toString(), initType->toString());
            }
        }
    }
}

void TypeCheckingVisitor::visit(UniformNode* node) {
    int line = (node->range.startLine > 0) ? node->range.startLine - 1 : 0;
    const Symbol* s = symbols.lookupAt(node->name, line);
    
    if (s && node->defaultValue) {
        node->defaultValue->accept(*this);
        TypePtr valType = resolveType(node->defaultValue.get());
        if (*valType != *s->type && valType->kind != TypeKind::UNKNOWN) {
            diagnostics.push_back(reportError(node, "Type mismatch: Cannot initialize uniform '" + s->type->toString() + 
                "' with value of type '" + valType->toString() + "'"));
        }
    }
}

// --- Functions and Control Flow ---

void TypeCheckingVisitor::visit(FunctionNode* node) 
{
    int line = (node->range.startLine > 0) ? node->range.startLine - 1 : 0;
    const Symbol* funcSym = symbols.lookupAt(node->name, line);
    if (!funcSym) return;

    // Setup state for ReturnNode checking
    ShaderStage previousStage = currentProcessorFunction;
    TypePtr previousExpected = currentExpectedReturnType;

    currentExpectedReturnType = funcSym->type;

    if (node->name == "vertex") currentProcessorFunction = ShaderStage::Vertex;
    else if (node->name == "fragment") currentProcessorFunction = ShaderStage::Fragment;
    else if (node->name == "light") currentProcessorFunction = ShaderStage::Light;
    else currentProcessorFunction = ShaderStage::Global;

    // Visit the body to check expressions and returns
    if (node->body) node->body->accept(*this);

    // Restore state
    currentProcessorFunction = previousStage;
    currentExpectedReturnType = previousExpected;
}

void TypeCheckingVisitor::visit(ReturnNode* node) {
    TypePtr actualType = typeRegistry.getType("void");
    
    if (node->value) {
        node->value->accept(*this);
        actualType = resolveType(node->value.get());
    }

    if (!currentExpectedReturnType) return;

    if (currentExpectedReturnType->name == "void") {
        if (actualType->name != "void") diagnostics.push_back(reportError(node, "Void function cannot return a value"));
    } else {
        if (actualType->name == "void") {
            diagnostics.push_back(reportError(node, "Function must return a value of type '" + currentExpectedReturnType->toString() + "'"));
        } else if (actualType->kind != TypeKind::UNKNOWN && *actualType != *currentExpectedReturnType) {
            diagnostics.push_back(reportError(node, "Type mismatch: Expected return type '" + currentExpectedReturnType->toString() + 
                "' but found '" + actualType->toString() + "'"));
        }
    }
}

// --- Expressions ---

void TypeCheckingVisitor::visit(IdentifierNode* node) {
    int line = (node->range.startLine > 0) ? node->range.startLine - 1 : 0;
    const Symbol* s = symbols.lookupAt(node->name, line); // Magic happens here
    
    if (!s) {
        diagnostics.push_back(reportError(node, "Undefined identifier '" + node->name + "'"));
    }
    // Semantic highlighting can be added here
}

void TypeCheckingVisitor::visit(BinaryOpNode* node) 
{
    bool isAssignment = (node->op == TokenType::TOKEN_EQUAL || node->op == TokenType::TOKEN_PLUS_EQUAL ||
                        node->op == TokenType::TOKEN_MINUS_EQUAL || node->op == TokenType::TOKEN_STAR_EQUAL || 
                        node->op == TokenType::TOKEN_SLASH_EQUAL || node->op == TokenType::TOKEN_PERCENT_EQUAL);

    if (isAssignment) {
        visitAssignment(node); // Route to assignment logic
        return;
    }
    
    if (node->left) node->left->accept(*this);
    if (node->right) node->right->accept(*this);

    TypePtr l = resolveType(node->left.get());
    TypePtr r = resolveType(node->right.get());

    if (l->kind == TypeKind::UNKNOWN || r->kind == TypeKind::UNKNOWN) return;

    TypePtr result = getBinaryOpResultType(l, r, node->op);
    if (result->kind == TypeKind::UNKNOWN) {
        diagnostics.push_back(reportError(node, "Invalid binary operation '" + l->toString() + "' and '" + r->toString() + "'"));
    }
}

void TypeCheckingVisitor::visitAssignment(const BinaryOpNode* node) 
{
    if (node->right) node->right->accept(*this);

    const Symbol* s = getRootSymbol(node->left.get());
    TypePtr lType = typeRegistry.getUnknownType();

    if (auto id = dynamic_cast<const IdentifierNode*>(node->left.get())) {
        if (s) lType = s->type;
        else diagnostics.push_back(reportError(node, "Undefined '" + id->name + "'"));
    } else if (dynamic_cast<const MemberAccessNode*>(node->left.get())) {
        node->left->accept(*this);
        lType = resolveType(node->left.get());
    } else {
        diagnostics.push_back(reportError(node->left.get(), "Expression is not assignable."));
        return;
    }

    // Validate Mutability
    if (s) {
        if (s->mutability == Mutability::ReadOnly) {
            diagnostics.push_back(reportError(node->left.get(), "Cannot assign to read-only variable '" + s->name + "'"));
        } else if (s->category == SymbolType::Varying && currentProcessorFunction != ShaderStage::Vertex) {
            diagnostics.push_back(reportError(node->left.get(), "Varyings are read-only in the fragment processor."));
        }
    }

    TypePtr rType = resolveType(node->right.get());
    if (lType->kind == TypeKind::UNKNOWN || rType->kind == TypeKind::UNKNOWN) return;

    if (node->op == TokenType::TOKEN_EQUAL) {
        if (getConversionCost(rType, lType) == -1) {
            diagnostics.push_back(reportError(node, "Type mismatch: Cannot assign '" + rType->toString() + "' to '" + lType->toString() + "'"));
        }
    } else {
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
            diagnostics.push_back(reportError(node, "Invalid operation for compound assignment."));
            return;
        }

        // 2. Check if the result can be assigned back to LHS
        // e.g. int += float -> (int + float) is float. float cannot be assigned to int. Error.
        // e.g. vec2 *= float -> (vec2 * float) is vec2. vec2 can be assigned to vec2. OK.
        int cost = getConversionCost(resultType, lType);
        if (cost == -1) {
            diagnostics.push_back(reportError(node, "Type mismatch: Result of compound assignment '" + resultType->toString() + 
                        "' cannot be assigned to '" + lType->toString() + "'"));
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
                            diagnostics.push_back(reportError(node, "Invalid Write Mask: Component '" + 
                            std::string(1, s[i]) + "' is assigned twice."));
                        }
                    }
                }
            }
        }
    }
}

// -------------------------------------------------------------------------
// CONTROL FLOW AND DECLARATIONS
// -------------------------------------------------------------------------

void TypeCheckingVisitor::visit(ConstNode* node) {
    int line = (node->range.startLine > 0) ? node->range.startLine - 1 : 0;
    const Symbol* s = symbols.lookupAt(node->name, line);
    
    if (s && node->value) {
        node->value->accept(*this);
        TypePtr valType = resolveType(node->value.get());
        if (*valType != *s->type && valType->kind != TypeKind::UNKNOWN) {
            diagnostics.push_back(reportError(node, "Type mismatch in const declaration."));
        }
    }
}

void TypeCheckingVisitor::visit(IfNode* node) {
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            diagnostics.push_back(reportError(node, "If condition must be bool"));
        }
    }
    if (node->thenBranch) node->thenBranch->accept(*this);
    if (node->elseBranch) node->elseBranch->accept(*this);
}

void TypeCheckingVisitor::visit(ForNode* node) {
    if (node->init) node->init->accept(*this);
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            diagnostics.push_back(reportError(node, "For loop condition must be bool"));
        }
    }
    if (node->increment) node->increment->accept(*this);
    if (node->body) node->body->accept(*this);
}

void TypeCheckingVisitor::visit(WhileNode* node) {
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            diagnostics.push_back(reportError(node, "While condition must be bool"));
        }
    }
    if (node->body) node->body->accept(*this);
}

void TypeCheckingVisitor::visit(DoWhileNode* node) {
    if (node->body) node->body->accept(*this);
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            diagnostics.push_back(reportError(node, "Do-while condition must be bool"));
        }
    }
}

void TypeCheckingVisitor::visit(SwitchNode* node) {
    TypePtr exprType = typeRegistry.getUnknownType();
    
    if (node->expression) {
        node->expression->accept(*this);
        exprType = resolveType(node->expression.get());
        
        if (exprType->name != "int" && exprType->name != "uint" && exprType->name != "unknown") {
            diagnostics.push_back(reportError(node->expression.get(), "Switch expression must be 'int' or 'uint', found '" + exprType->name + "'"));
        }
    }

    for (const auto& c : node->cases) {
        if (!c->isDefault && c->value) {
            c->value->accept(*this);
            TypePtr cType = resolveType(c->value.get());
            if (*cType != *exprType && cType->kind != TypeKind::UNKNOWN) {
                diagnostics.push_back(reportError(c->value.get(), "Case Type mismatch"));
            }
        }
        for (const auto& stmt : c->statements) {
            if (stmt) stmt->accept(*this);
        }
    }
}

// -------------------------------------------------------------------------
// EXPRESSIONS
// -------------------------------------------------------------------------

void TypeCheckingVisitor::visit(UnaryOpNode* node) 
{
    if (node->operand) node->operand->accept(*this);
}

void TypeCheckingVisitor::visit(FunctionCallNode* node) 
{
    std::string name = node->functionName;

    std::vector<TypePtr> argTypes;
    for (const auto& arg : node->arguments) {
        arg->accept(*this);
        argTypes.push_back(resolveType(arg.get()));
    }

    if (typeRegistry.getType(name)->kind != TypeKind::UNKNOWN) {
        validateConstructor(node, name);
        return; 
    }

    std::vector<const Symbol*> candidates = symbols.lookupFunctions(name);

    if (candidates.empty()) {
        diagnostics.push_back(reportError(node, "Unknown function '" + name + "'"));
        return;
    }

    const Symbol* bestMatch = findBestOverload(node, argTypes);
    
    if (!bestMatch) {
        std::vector<const Symbol*> arityMatches;
        for (const auto* s : candidates) {
            if (s->parameterTypes.size() == argTypes.size()) {
                arityMatches.push_back(s);
            }
        }

        if (arityMatches.empty()) {
            size_t expected = candidates[0]->parameterTypes.size();
            diagnostics.push_back(reportError(node, "Invalid argument count for '" + name + "'. Expected " + 
                        std::to_string(expected) + ", but got " + std::to_string(argTypes.size()) + "."));
        } else {
            const Symbol* closest = arityMatches[0];
            int maxMatches = -1;

            for (const auto* cand : arityMatches) {
                int matches = 0;
                for (size_t i = 0; i < argTypes.size(); ++i) {
                    if (getConversionCost(argTypes[i], cand->parameterTypes[i]) != -1) matches++;
                }
                if (matches > maxMatches) {
                    maxMatches = matches;
                    closest = cand;
                }
            }

            for (size_t i = 0; i < argTypes.size(); ++i) {
                TypePtr expected = closest->parameterTypes[i];
                TypePtr actual = argTypes[i];
                if (getConversionCost(actual, expected) == -1) {
                    diagnostics.push_back(reportError(node->arguments[i].get(), 
                    "Invalid argument " + std::to_string(i + 1) + " for function '" + name + "'. Expected '" + 
                    expected->toString() + "', but found '" + actual->toString() + "'."));
                    break; 
                }
            }
        }
    }
}

void TypeCheckingVisitor::visit(MemberAccessNode* node) 
{
    if (node->base) node->base->accept(*this);
    TypePtr baseT = resolveType(node->base.get());

    if (baseT->kind == TypeKind::UNKNOWN) return;
    if (baseT->kind == TypeKind::ARRAY && node->member == "length") return;

    TypePtr memberT = typeRegistry.getMemberType(baseT, node->member);
    if (memberT->kind == TypeKind::UNKNOWN) {
        if (baseT->kind == TypeKind::VECTOR) {
            std::string swizzle = node->member;
            
            if (swizzle.length() > 4) {
                diagnostics.push_back(reportError(node, "Swizzle '" + swizzle + "' is too long (max 4 components)."));
                return;
            }

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
                bool hasXYZW = false, hasRGBA = false;
                for (char c : swizzle) {
                    if (std::string("xyzw").find(c) != std::string::npos) hasXYZW = true;
                    if (std::string("rgba").find(c) != std::string::npos) hasRGBA = true;
                }
                if (hasXYZW && hasRGBA) {
                    diagnostics.push_back(reportError(node, "Illegal swizzle '" + swizzle + "'. Cannot mix xyzw and rgba sets."));
                } else {
                    diagnostics.push_back(reportError(node, "Invalid swizzle component in '" + swizzle + "'."));
                }
                return;
            }

            std::string currentSet = sets[validSetIndex];
            for (char c : swizzle) {
                size_t componentIndex = currentSet.find(c);
                if (componentIndex >= (size_t)baseT->componentCount) {
                    diagnostics.push_back(reportError(node, "Swizzle component '" + std::string(1, c) + "' is out of bounds for " + baseT->toString() + "."));
                    return;
                }
            }
        }
        diagnostics.push_back(reportError(node, "Invalid member '" + node->member + "' on type '" + baseT->toString() + "'"));
    }
}

void TypeCheckingVisitor::visit(ArrayAccessNode* node) 
{
    if (node->base) node->base->accept(*this);
    if (node->index) node->index->accept(*this);

    TypePtr baseType = resolveType(node->base.get());
    TypePtr indexType = resolveType(node->index.get());

    if (baseType->kind != TypeKind::ARRAY && baseType->kind != TypeKind::VECTOR && baseType->kind != TypeKind::MATRIX && baseType->kind != TypeKind::UNKNOWN) {
        diagnostics.push_back(reportError(node, "Type '" + baseType->toString() + "' is not indexable."));
    }

    if (indexType->name != "int" && indexType->name != "uint" && indexType->kind != TypeKind::UNKNOWN) {
        diagnostics.push_back(reportError(node->index.get(), "Array index must be an integer."));
    }
}

// -------------------------------------------------------------------------
// HELPERS
// -------------------------------------------------------------------------

TypePtr TypeCheckingVisitor::resolveType(const ExpressionNode* node) 
{
    if (!node) return typeRegistry.getUnknownType();

    if (auto lit = dynamic_cast<const LiteralNode*>(node)) {
        if (lit->type == TokenType::TOKEN_NUMBER) 
            return (lit->value.find('.') != std::string::npos) ? typeRegistry.getType("float") : typeRegistry.getType("int");
        if (lit->type == TokenType::KEYWORD_TRUE || lit->type == TokenType::KEYWORD_FALSE) 
            return typeRegistry.getType("bool");
        return typeRegistry.getUnknownType();
    }

    if (auto id = dynamic_cast<const IdentifierNode*>(node)) {
        int line = (id->range.startLine > 0) ? id->range.startLine - 1 : 0;
        if (const Symbol* s = symbols.lookupAt(id->name, line)) {
            return s->type;
        }
        return typeRegistry.getUnknownType();
    }

    if (auto bin = dynamic_cast<const BinaryOpNode*>(node)) {
        TypePtr l = resolveType(bin->left.get());
        TypePtr r = resolveType(bin->right.get());
        return getBinaryOpResultType(l, r, bin->op);
    }

    if (auto idx = dynamic_cast<const ArrayAccessNode*>(node)) {
        TypePtr base = resolveType(idx->base.get());
        if (base->kind == TypeKind::ARRAY) return base->baseType;
        if (base->kind == TypeKind::VECTOR) return base->baseType;
        if (base->kind == TypeKind::MATRIX) return typeRegistry.getType("vec" + std::to_string(base->componentCount));
        return typeRegistry.getUnknownType();
    }

    if (auto call = dynamic_cast<const FunctionCallNode*>(node)) {
        TypePtr t = typeRegistry.getType(call->functionName);
        if (t->kind != TypeKind::UNKNOWN) return t;

        std::vector<TypePtr> argTypes;
        for (const auto& arg : call->arguments) {
            argTypes.push_back(resolveType(arg.get()));
        }

        const Symbol* match = findBestOverload(call, argTypes);
        if (match) return match->type;
        return typeRegistry.getUnknownType();
    }
    
    if (auto mem = dynamic_cast<const MemberAccessNode*>(node)) {
        if (mem->member == "length") return typeRegistry.getType("int");
        TypePtr base = resolveType(mem->base.get());
        return typeRegistry.getMemberType(base, mem->member);
    }

    if (auto un = dynamic_cast<const UnaryOpNode*>(node)) {
        return resolveType(un->operand.get());
    }

    if (auto tern = dynamic_cast<const TernaryNode*>(node)) {
        return resolveType(tern->trueExpr.get());
    }

    return typeRegistry.getUnknownType();
}

TypePtr TypeCheckingVisitor::getBinaryOpResultType(TypePtr l, TypePtr r, TokenType op) 
{
    if (l->kind == TypeKind::UNKNOWN || r->kind == TypeKind::UNKNOWN) return typeRegistry.getUnknownType();

    bool isComparison = (op == TokenType::TOKEN_EQ_EQ || op == TokenType::TOKEN_NOT_EQ ||
                         op == TokenType::TOKEN_LESS || op == TokenType::TOKEN_GREATER ||
                         op == TokenType::TOKEN_LESS_EQ || op == TokenType::TOKEN_GREATER_EQ);

    if (isComparison) return typeRegistry.getType("bool"); 
    if (l == typeRegistry.getType("bool") || r == typeRegistry.getType("bool")) return typeRegistry.getUnknownType();
    if (*l == *r) return l; 

    bool lVec = (l->kind == TypeKind::VECTOR);
    bool rVec = (r->kind == TypeKind::VECTOR);
    bool lScalar = (l->kind == TypeKind::SCALAR);
    bool rScalar = (r->kind == TypeKind::SCALAR);

    if (lVec && rScalar) {
        if (l->baseType && *l->baseType == *r) return l; 
    }
    if (rVec && lScalar) {
        if (r->baseType && *r->baseType == *l) return r;
    }

    bool lMat = (l->kind == TypeKind::MATRIX);
    bool rMat = (r->kind == TypeKind::MATRIX);

    if ((lMat && rScalar) || (rMat && lScalar)) {
        if ((lScalar && l->name == "float") || (rScalar && r->name == "float")) {
            return lMat ? l : r;
        }
    }

    if (lMat && rVec) {
        if (l->componentCount == r->componentCount) return r; 
    }
    if (lVec && rMat) {
        if (l->componentCount == r->componentCount) return l;
    }
    if (lMat && rMat) {
        if (l->componentCount == r->componentCount) return l;
    }

    return typeRegistry.getUnknownType();
}

int TypeCheckingVisitor::getConversionCost(TypePtr from, TypePtr to) 
{
    if (*from == *to) return 0;
    if (from->name == "int" && to->name == "float") return 1;
    if (from->name == "uint" && to->name == "float") return 1;
    return -1; 
}

const Symbol* TypeCheckingVisitor::getRootSymbol(const ExpressionNode* node) 
{
    if (auto id = dynamic_cast<const IdentifierNode*>(node)) {
        int line = (id->range.startLine > 0) ? id->range.startLine - 1 : 0;
        return symbols.lookupAt(id->name, line);
    }
    
    if (auto mem = dynamic_cast<const MemberAccessNode*>(node)) {
        return getRootSymbol(mem->base.get());
    }

    if (auto arr = dynamic_cast<const ArrayAccessNode*>(node)) {
        return getRootSymbol(arr->base.get());
    }

    return nullptr;
}

const Symbol* TypeCheckingVisitor::findBestOverload(const FunctionCallNode* node, const std::vector<TypePtr>& argTypes) 
{
    std::string name = node->functionName;
    auto candidates = symbols.lookupFunctions(name);
    if (candidates.empty()) return nullptr;

    const Symbol* bestMatch = nullptr;
    int minCost = 999999;
    bool isAmbiguous = false;

    for (const auto* sym : candidates) {
        if (sym->parameterTypes.size() != argTypes.size()) continue;

        int currentCost = 0;
        bool possible = true;

        for (size_t i = 0; i < argTypes.size(); i++) {
            int cost = getConversionCost(argTypes[i], sym->parameterTypes[i]);
            if (cost == -1) {
                possible = false;
                break;
            }
            currentCost += cost;
        }

        if (possible) {
            if (currentCost < minCost) {
                minCost = currentCost;
                bestMatch = sym;
                isAmbiguous = false;
            } 
            else if (currentCost == minCost) {
                isAmbiguous = true;
            }
        }
    }
    
    if (isAmbiguous) {
        diagnostics.push_back(reportError(node, "Ambiguous function call for '" + name + "'. Multiple overloads match these arguments."));
    }
    return bestMatch;
}

void TypeCheckingVisitor::validateConstructor(const FunctionCallNode* node, const std::string& typeName) 
{
    TypePtr target = typeRegistry.getType(typeName);
    
    if (target->kind == TypeKind::STRUCT) {
        if (node->arguments.size() != target->members.size()) {
            diagnostics.push_back(reportError(node, "Constructor for '" + typeName + "' expects " + 
                        std::to_string(target->members.size()) + " arguments, but got " + 
                        std::to_string(node->arguments.size())));
            return;
        }

        for (size_t i = 0; i < target->members.size(); ++i) {
            TypePtr argType = resolveType(node->arguments[i].get());
            TypePtr expectedType = target->members[i].second;

            if (*argType != *expectedType) {
                diagnostics.push_back(reportError(node->arguments[i].get(), 
                    "Type mismatch in struct constructor argument " + std::to_string(i+1) + 
                    ". Expected '" + expectedType->toString() + "', found '" + argType->toString() + "'"));
            }
        }
        return;
    }

    if (target->kind == TypeKind::VECTOR) {
        int expected = target->componentCount;
        int provided = 0;

        for (const auto& arg : node->arguments) {
            TypePtr argT = resolveType(arg.get());
            
            if (argT->kind == TypeKind::UNKNOWN) {
                if (auto lit = dynamic_cast<const LiteralNode*>(arg.get())) {
                    if (lit->type == TokenType::TOKEN_STRING) {
                        diagnostics.push_back(reportError(arg.get(), "Cannot construct '" + typeName + "' from a string."));
                        continue;
                    }
                }
                diagnostics.push_back(reportError(arg.get(), "Invalid argument type."));
                continue; 
            }

            if (argT->name == "void") {
                diagnostics.push_back(reportError(arg.get(), "Cannot use 'void' expression in constructor."));
                continue;
            }
            
            if (argT->kind != TypeKind::SCALAR && argT->kind != TypeKind::VECTOR) {
                diagnostics.push_back(reportError(arg.get(), "Invalid argument for vector constructor. Expected Scalar or Vector."));
                continue; 
            }

            provided += (argT->kind == TypeKind::SCALAR) ? 1 : argT->componentCount;
        }

        if (node->arguments.size() == 1 && provided == 1 && expected > 1) return;

        if (provided != expected) {
            diagnostics.push_back(reportError(node, "Invalid constructor. Expected " + std::to_string(expected) + 
                " components, but found " + std::to_string(provided)));
        }
        return;
    }

    if (target->kind == TypeKind::MATRIX) {
        int expected = target->componentCount * target->componentCount; 
        if (target->name == "mat2") expected = 4;
        else if (target->name == "mat3") expected = 9;
        else if (target->name == "mat4") expected = 16;
        
        int provided = 0;
        for (const auto& arg : node->arguments) {
            TypePtr argT = resolveType(arg.get());
            if (argT == typeRegistry.getUnknownType() || argT == typeRegistry.getType("void")) {
                 diagnostics.push_back(reportError(arg.get(), "Invalid matrix constructor argument."));
                 continue;
            }

            if (argT->kind == TypeKind::SCALAR) provided += 1;
            else if (argT->kind == TypeKind::VECTOR) provided += argT->componentCount;
            else if (argT->kind == TypeKind::MATRIX) provided += 100; 
        }

        if (provided < expected && provided < 50) {
            diagnostics.push_back(reportError(node, "Not enough components to construct '" + typeName + "'"));
        }
    }

    if (target->kind == TypeKind::SCALAR) {
        if (typeName == "void") {
            diagnostics.push_back(reportError(node, "Cannot construct 'void'."));
            return;
        }

        bool isBasic = (typeName == "float" || typeName == "int" || typeName == "uint" || typeName == "bool");
        if (!isBasic) {
            diagnostics.push_back(reportError(node, "Cannot construct opaque type '" + typeName + "'."));
            return;
        }

        if (node->arguments.size() != 1) {
            diagnostics.push_back(reportError(node, "Scalar constructor expects exactly 1 argument."));
        }

        if (!node->arguments.empty()) {
            TypePtr argT = resolveType(node->arguments[0].get());
             if (argT->kind == TypeKind::UNKNOWN) {
                diagnostics.push_back(reportError(node->arguments[0].get(), "Invalid argument."));
            }
        }
        return;
    }
    diagnostics.push_back(reportError(node, "Cannot construct type '" + typeName + "'."));
}

void TypeCheckingVisitor::reportTypeMismatch(const ASTNode* node, const std::string& expected, const std::string& found) {
    diagnostics.push_back(reportError(node, "Type mismatch: Expected '" + expected + "', but found '" + found + "'."));
}

} // namespace gdshader_lsp