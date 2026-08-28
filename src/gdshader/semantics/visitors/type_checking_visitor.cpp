#include "gdshader/diagnostics.hpp"
#include "gdshader/ast/ast.h"
#include "gdshader/semantics/visitors/type_checking_visitor.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

// --- Pass Throughs ---
void TypeCheckingVisitor::visit(ProgramNode* node) {
    GDSHADER_RETURN_IF(!node, "ProgramNode is null");
    for (const auto& child : node->nodes) if (child) child->accept(*this);
}
void TypeCheckingVisitor::visit(BlockNode* node) {
    GDSHADER_RETURN_IF(!node, "BlockNode is null");
    for (const auto& stmt : node->statements) if (stmt) stmt->accept(*this);
}
void TypeCheckingVisitor::visit(ExpressionStatementNode* node) {
    GDSHADER_RETURN_IF(!node, "ExpressionStatementNode is null");
    if (node->expr) node->expr->accept(*this);
}

// --- Declarations Validation ---

void TypeCheckingVisitor::visit(VariableDeclNode* node) 
{
    GDSHADER_RETURN_IF(!node, "VariableDeclNode is null");
    if (node->initializer) {
        node->initializer->accept(*this);

        GDSHADER_RETURN_IF(node->name.empty(), "node name is empty");
        
        int line = node->range.startLine;
        const Symbol* s = symbols.lookupAt(node->name, line);

        if (s && s->type && s->type->kind == TypeKind::STRUCT) {
            const Symbol* structSym = symbols.lookupAt(s->type->name, node->range.startLine);
            if (structSym) {
                Range typeRange = node->range;
                typeRange.endCol = typeRange.startCol + s->type->name.length();
                symbols.addReference(const_cast<Symbol*>(structSym), typeRange);
            }
        }

        if (s) {
            TypePtr initType = resolveType(node->initializer.get());
            
            if (!initType) {
                GDSHADER_ERROR_IF(true, "resolveType returned a nullptr for initializer of '{}'", node->name);
                return;
            }
            if (!s->type) {
                GDSHADER_ERROR_IF(true, "Symbol type is nullptr for variable '{}'", node->name);
                return;
            }

            if (initType->kind != TypeKind::UNKNOWN && *initType != *s->type) {
                reportTypeMismatch(node, s->type->toString(), initType->toString());
            }
        }
    }
}

void TypeCheckingVisitor::visit(UniformNode* node) 
{
    GDSHADER_RETURN_IF(!node, "UniformNode is null");
    int line = node->range.startLine;
    const Symbol* s = symbols.lookupAt(node->name, line);
    
    if (s && node->defaultValue) {
        node->defaultValue->accept(*this);
        TypePtr valType = resolveType(node->defaultValue.get());
        if (*valType != *s->type && valType->kind != TypeKind::UNKNOWN) {
            GDSHADER_ERROR_IF(true, "Uniform type mismatch: '{}' vs '{}'", s->type->toString(), valType->toString());
            diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Type mismatch: Cannot initialize uniform '" + s->type->toString() + 
                "' with value of type '" + valType->toString() + "'."));
        }
    }
}

// --- Functions and Control Flow ---

void TypeCheckingVisitor::visit(FunctionNode* node) 
{
    GDSHADER_RETURN_IF(!node, "FunctionNode is null");
    int line = node->range.startLine;
    const Symbol* funcSym = symbols.lookupAt(node->name, line);
    if (!funcSym) return;

    ShaderStage previousStage = currentProcessorFunction;
    TypePtr previousExpected = currentExpectedReturnType;

    currentExpectedReturnType = funcSym->returnType;

    if (node->name == "vertex") currentProcessorFunction = ShaderStage::Vertex;
    else if (node->name == "fragment") currentProcessorFunction = ShaderStage::Fragment;
    else if (node->name == "light") currentProcessorFunction = ShaderStage::Light;
    else currentProcessorFunction = ShaderStage::Global;

    if (node->body) node->body->accept(*this);

    currentProcessorFunction = previousStage;
    currentExpectedReturnType = previousExpected;
}

void TypeCheckingVisitor::visit(ReturnNode* node) {
    GDSHADER_RETURN_IF(!node, "ReturnNode is null");
    TypePtr actualType = typeRegistry.getType("void");
    
    if (node->value) {
        node->value->accept(*this);
        actualType = resolveType(node->value.get());
    }

    if (!currentExpectedReturnType) return;

    if (currentExpectedReturnType->name == "void") {
        if (actualType->name != "void") {
            GDSHADER_ERROR_IF(true, "Void function attempting to return a value");
            diagnostics.push_back(reportError(node, DiagnosticCode::VoidCannotReturnValue, "Void function cannot return a value."));
        }
    } else {
        if (actualType->name == "void") {
            GDSHADER_ERROR_IF(true, "Function expected to return '{}', but returned void", currentExpectedReturnType->toString());
            diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Function must return a value of type '" + currentExpectedReturnType->toString() + "'."));
        } else if (actualType->kind != TypeKind::UNKNOWN && *actualType != *currentExpectedReturnType) {
            GDSHADER_ERROR_IF(true, "Return type mismatch: Expected '{}', found '{}'", currentExpectedReturnType->toString(), actualType->toString());
            diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Type mismatch: Expected return type '" + currentExpectedReturnType->toString() + 
                "' but found '" + actualType->toString() + "'."));
        }
    }
}

// --- Expressions ---

void TypeCheckingVisitor::visit(IdentifierNode* node) 
{
    GDSHADER_RETURN_IF(!node, "IdentifierNode is null");
    int line = node->range.startLine;
    const Symbol* s = symbols.lookupAt(node->name, line);
    
    if (!s) {
        GDSHADER_ERROR_IF(true, "Undefined identifier '{}'", node->name);
        diagnostics.push_back(reportError(node, DiagnosticCode::UndefinedIdentifier, "Undefined identifier '" + node->name + "'."));
    } else {
        node->resolvedSymbol = s;
        symbols.addReference(const_cast<Symbol*>(s), node->range);
    }
}

void gdshader_lsp::TypeCheckingVisitor::visit(TernaryNode *node)
{
    GDSHADER_RETURN_IF(!node, "TernaryNode is null");
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            diagnostics.push_back(reportError(node->condition.get(), DiagnosticCode::ConditionMustBeBool, "Ternary condition must be bool."));
        }
    }
    if (node->trueExpr) node->trueExpr->accept(*this);
    if (node->falseExpr) node->falseExpr->accept(*this);

    TypePtr tTrue = resolveType(node->trueExpr.get());
    TypePtr tFalse = resolveType(node->falseExpr.get());
    
    if (tTrue->kind != TypeKind::UNKNOWN && tFalse->kind != TypeKind::UNKNOWN && *tTrue != *tFalse) {
        diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Type mismatch in ternary operator branches."));
    }
}

void TypeCheckingVisitor::visit(BinaryOpNode* node) 
{
    GDSHADER_RETURN_IF(!node, "BinaryOpNode is null");
    bool isAssignment = (
        node->op == TokenType::TOKEN_EQUAL || node->op == TokenType::TOKEN_PLUS_EQUAL ||
        node->op == TokenType::TOKEN_MINUS_EQUAL || node->op == TokenType::TOKEN_STAR_EQUAL || 
        node->op == TokenType::TOKEN_SLASH_EQUAL || node->op == TokenType::TOKEN_PERCENT_EQUAL ||
        node->op == TokenType::TOKEN_LESS_LESS_EQUAL || node->op == TokenType::TOKEN_GREATER_GREATER_EQUAL ||
        node->op == TokenType::TOKEN_AMPERSAND_EQUAL || node->op == TokenType::TOKEN_PIPE_EQUAL || node->op == TokenType::TOKEN_CARET_EQUAL
    );

    if (isAssignment) {
        visitAssignment(node); 
        return;
    }
    
    if (node->left) node->left->accept(*this);
    if (node->right) node->right->accept(*this);

    if (node->op == TokenType::TOKEN_SLASH) {
        if (auto lit = dynamic_cast<const LiteralNode*>(node->right.get())) {
            if (lit->value == "0" || lit->value == "0.0") {
                diagnostics.push_back(reportError(node->right.get(), DiagnosticCode::DivisionByZero, "Division by zero."));
            }
        }
    }

    TypePtr l = resolveType(node->left.get());
    TypePtr r = resolveType(node->right.get());

    if (l->kind == TypeKind::UNKNOWN || r->kind == TypeKind::UNKNOWN) return;

    TypePtr result = getBinaryOpResultType(l, r, node->op);
    if (result->kind == TypeKind::UNKNOWN) {
        GDSHADER_ERROR_IF(true, "Invalid binary operation between '{}' and '{}'", l->toString(), r->toString());
        diagnostics.push_back(reportError(node, DiagnosticCode::InvalidOperation, "Invalid binary operation between '" + l->toString() + "' and '" + r->toString() + "'."));
    }
}

void TypeCheckingVisitor::visitAssignment(const BinaryOpNode* node) 
{
    GDSHADER_RETURN_IF(!node, "BinaryOpNode (Assignment) is null");
    if (node->right) node->right->accept(*this);
    GDSHADER_RETURN_IF(!node->left, "BinaryOpNode (Assignment) has no left side.");

    if (node->left) node->left->accept(*this);

    const Symbol* s = getRootSymbol(node->left.get());
    TypePtr lType = typeRegistry.getUnknownType();

    if (auto id = dynamic_cast<const IdentifierNode*>(node->left.get())) 
    {
        if (s && s->type) lType = s->type; 
        else if (s && !s->type) {
            GDSHADER_ERROR_IF(true, "Attempted assignment to function symbol '{}'", id->name);
            diagnostics.push_back(reportError(node, DiagnosticCode::NotAssignable, "Cannot assign to function '" + id->name + "'."));
            return;
        }
        else {
            GDSHADER_ERROR_IF(true, "Undefined identifier '{}' in assignment", id->name);
            diagnostics.push_back(reportError(node, DiagnosticCode::UndefinedIdentifier, "Undefined identifier '" + id->name + "'."));
            return;
        }
        
    } else if (dynamic_cast<const MemberAccessNode*>(node->left.get())) 
    {
        node->left->accept(*this);
        lType = resolveType(node->left.get());
    } else 
    {
        GDSHADER_ERROR_IF(true, "Expression on LHS is not assignable");
        diagnostics.push_back(reportError(node->left.get(), DiagnosticCode::NotAssignable, "Expression is not assignable."));
        return;
    }

    if (s) {
        if (s->mutability == Mutability::ReadOnly) {
            GDSHADER_ERROR_IF(true, "Attempted assignment to read-only variable '{}'", s->name);
            diagnostics.push_back(reportError(node->left.get(), DiagnosticCode::CannotAssignToReadOnly, "Cannot assign to read-only variable '" + s->name + "'."));
        } else if (s->category == SymbolType::Varying && currentProcessorFunction != ShaderStage::Vertex) {
            GDSHADER_ERROR_IF(true, "Attempted to write to varying in fragment processor");
            diagnostics.push_back(reportError(node->left.get(), DiagnosticCode::VaryingReadOnlyInFragment, "Varyings are read-only in the fragment processor."));
        }
    }

    TypePtr rType = resolveType(node->right.get());
    if (lType->kind == TypeKind::UNKNOWN || rType->kind == TypeKind::UNKNOWN) return;

    if (node->op == TokenType::TOKEN_EQUAL) {
        if (getConversionCost(rType, lType) == -1) {
            GDSHADER_ERROR_IF(true, "Type mismatch on assignment: cannot assign '{}' to '{}'", rType->toString(), lType->toString());
            diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Type mismatch: Cannot assign '" + rType->toString() + "' to '" + lType->toString() + "'."));
        }
    } else {
        TokenType mathOp;
        switch (node->op) {
            case TokenType::TOKEN_PLUS_EQUAL:                   mathOp = TokenType::TOKEN_PLUS; break;
            case TokenType::TOKEN_MINUS_EQUAL:                  mathOp = TokenType::TOKEN_MINUS; break;
            case TokenType::TOKEN_STAR_EQUAL:                   mathOp = TokenType::TOKEN_STAR; break;
            case TokenType::TOKEN_SLASH_EQUAL:                  mathOp = TokenType::TOKEN_SLASH; break;
            case TokenType::TOKEN_PERCENT_EQUAL:                mathOp = TokenType::TOKEN_PERCENT; break;
            case TokenType::TOKEN_LESS_LESS_EQUAL:              mathOp = TokenType::TOKEN_LESS_LESS; break;
            case TokenType::TOKEN_GREATER_GREATER_EQUAL:        mathOp = TokenType::TOKEN_GREATER_GREATER; break;
            case TokenType::TOKEN_AMPERSAND_EQUAL:              mathOp = TokenType::TOKEN_AMPERSAND; break;
            case TokenType::TOKEN_PIPE_EQUAL:                   mathOp = TokenType::TOKEN_PIPE; break;
            case TokenType::TOKEN_CARET_EQUAL:                  mathOp = TokenType::TOKEN_CARET; break;
            default:                                            mathOp = TokenType::TOKEN_ERROR; break;
        }

        TypePtr resultType = getBinaryOpResultType(lType, rType, mathOp);
        
        if (resultType->kind == TypeKind::UNKNOWN) {
            GDSHADER_ERROR_IF(true, "Invalid operation for compound assignment");
            diagnostics.push_back(reportError(node, DiagnosticCode::InvalidOperation, "Invalid operation for compound assignment."));
            return;
        }

        int cost = getConversionCost(resultType, lType);
        if (cost == -1) {
            GDSHADER_ERROR_IF(true, "Type mismatch on compound assignment: '{}' cannot be assigned to '{}'", resultType->toString(), lType->toString());
            diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Type mismatch: Result of compound assignment '" + resultType->toString() + 
                        "' cannot be assigned to '" + lType->toString() + "'."));
        }
    }

    const ExpressionNode* lhs = node->left.get();
    
    if (auto mem = dynamic_cast<const MemberAccessNode*>(lhs)) {
        std::string s = mem->member;
        if (s.length() <= 4) { 
            
            bool isSwizzleChars = true;
            const std::string validSet = "xyzwrgbastpq"; 
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
                            GDSHADER_ERROR_IF(true, "Invalid Write Mask: Component '{}' assigned twice", std::string(1, s[i]));
                            diagnostics.push_back(reportError(node, DiagnosticCode::InvalidSwizzle, "Invalid Write Mask: Component '" + 
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
    GDSHADER_RETURN_IF(!node, "ConstNode is null");
    int line = node->range.startLine;
    const Symbol* s = symbols.lookupAt(node->name, line);
    
    if (s && node->value) {
        node->value->accept(*this);
        TypePtr valType = resolveType(node->value.get());
        if (*valType != *s->type && valType->kind != TypeKind::UNKNOWN) {
            GDSHADER_ERROR_IF(true, "Type mismatch in const declaration");
            diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Type mismatch in const declaration."));
        }
    }
}

void TypeCheckingVisitor::visit(IfNode* node) {
    GDSHADER_RETURN_IF(!node, "IfNode is null");
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            GDSHADER_ERROR_IF(true, "If condition is not bool");
            diagnostics.push_back(reportError(node, DiagnosticCode::ConditionMustBeBool, "If condition must be bool."));
        }
    }
    if (node->thenBranch) node->thenBranch->accept(*this);
    if (node->elseBranch) node->elseBranch->accept(*this);
}

void TypeCheckingVisitor::visit(ForNode* node) {
    GDSHADER_RETURN_IF(!node, "ForNode is null");
    if (node->init) node->init->accept(*this);
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            GDSHADER_ERROR_IF(true, "For loop condition is not bool");
            diagnostics.push_back(reportError(node, DiagnosticCode::ConditionMustBeBool, "For loop condition must be bool."));
        }
    }
    if (node->increment) node->increment->accept(*this);
    if (node->body) node->body->accept(*this);
}

void TypeCheckingVisitor::visit(WhileNode* node) {
    GDSHADER_RETURN_IF(!node, "WhileNode is null");
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            GDSHADER_ERROR_IF(true, "While condition is not bool");
            diagnostics.push_back(reportError(node, DiagnosticCode::ConditionMustBeBool, "While condition must be bool."));
        }
    }
    if (node->body) node->body->accept(*this);
}

void TypeCheckingVisitor::visit(DoWhileNode* node) {
    GDSHADER_RETURN_IF(!node, "DoWhileNode is null");
    if (node->body) node->body->accept(*this);
    if (node->condition) {
        node->condition->accept(*this);
        TypePtr condT = resolveType(node->condition.get());
        if (condT->name != "bool" && condT->kind != TypeKind::UNKNOWN) {
            GDSHADER_ERROR_IF(true, "Do-while condition is not bool");
            diagnostics.push_back(reportError(node, DiagnosticCode::ConditionMustBeBool, "Do-while condition must be bool."));
        }
    }
}

void TypeCheckingVisitor::visit(SwitchNode* node) {
    GDSHADER_RETURN_IF(!node, "SwitchNode is null");
    TypePtr exprType = typeRegistry.getUnknownType();
    
    if (node->expression) {
        node->expression->accept(*this);
        exprType = resolveType(node->expression.get());
        
        if (exprType->name != "int" && exprType->name != "uint" && exprType->name != "unknown") {
            GDSHADER_ERROR_IF(true, "Switch expression must be int/uint, found '{}'", exprType->name);
            diagnostics.push_back(reportError(node->expression.get(), DiagnosticCode::SwitchExpressionMustBeInt, "Switch expression must be 'int' or 'uint', found '" + exprType->name + "'."));
        }
    }

    for (const auto& c : node->cases) {
        if (!c->isDefault && c->value) {
            c->value->accept(*this);
            TypePtr cType = resolveType(c->value.get());
            if (*cType != *exprType && cType->kind != TypeKind::UNKNOWN) {
                GDSHADER_ERROR_IF(true, "Switch case type mismatch");
                diagnostics.push_back(reportError(c->value.get(), DiagnosticCode::TypeMismatch, "Case Type mismatch."));
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
    GDSHADER_RETURN_IF(!node, "UnaryOpNode is null");
    if (node->operand) node->operand->accept(*this);

    if (node->op == TokenType::TOKEN_PLUS_PLUS || node->op == TokenType::TOKEN_MINUS_MINUS) {
        bool assignable = false;
        if (dynamic_cast<const IdentifierNode*>(node->operand.get()) || 
            dynamic_cast<const MemberAccessNode*>(node->operand.get()) || 
            dynamic_cast<const ArrayAccessNode*>(node->operand.get())) {
            assignable = true;
        }

        if (!assignable) {
            GDSHADER_ERROR_IF(true, "Expression is not assignable");
            diagnostics.push_back(reportError(node->operand.get(), DiagnosticCode::NotAssignable, "Expression is not assignable."));
        } else {
            const Symbol* s = getRootSymbol(node->operand.get());
            if (s && s->mutability == Mutability::ReadOnly) {
                GDSHADER_ERROR_IF(true, "Attempted assignment to read-only variable '{}'", s->name);
                diagnostics.push_back(reportError(node->operand.get(), DiagnosticCode::CannotAssignToReadOnly, "Cannot assign to read-only variable '" + s->name + "'."));
            } else if (s->category == SymbolType::Varying && currentProcessorFunction != ShaderStage::Vertex) {
                GDSHADER_ERROR_IF(true, "Attempted to write to varying in non-vertex processor");
                diagnostics.push_back(reportError(node->operand.get(), DiagnosticCode::VaryingReadOnlyInFragment, "Varyings are read-only outside the vertex processor."));
            }
        }
    }

    if (node->op == TokenType::TOKEN_TILDE) {
        TypePtr t = resolveType(node->operand.get());
        if (t->kind != TypeKind::UNKNOWN && t->name.find("int") == std::string::npos && 
            t->name.find("uint") == std::string::npos && t->name.find("ivec") == std::string::npos && 
            t->name.find("uvec") == std::string::npos) {
            GDSHADER_ERROR_IF(true, "Bitwise NOT on non-integer type");
            diagnostics.push_back(reportError(node, DiagnosticCode::BitwiseRequiresInteger, "Type mismatch: bitwise NOT requires integer type."));
        }
    }
}

void TypeCheckingVisitor::visit(FunctionCallNode* node) 
{
    GDSHADER_RETURN_IF(!node, "FunctionCallNode is null");
    std::string name = node->functionName;

    std::vector<TypePtr> argTypes;
    for (const auto& arg : node->arguments) 
    {
        if (!arg) {
            argTypes.push_back(typeRegistry.getUnknownType());
            continue;
        }

        arg->accept(*this);
        argTypes.push_back(resolveType(arg.get()));
    }

    if (typeRegistry.getType(name)->kind != TypeKind::UNKNOWN) {
        validateConstructor(node, name);
        return; 
    }

    std::vector<const Symbol*> candidates = symbols.lookupFunctions(name);

    if (candidates.empty()) {
        GDSHADER_ERROR_IF(true, "Call to unknown function '{}'", name);
        diagnostics.push_back(reportError(node, DiagnosticCode::UnknownFunction, "Unknown function '" + name + "'."));
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
            GDSHADER_ERROR_IF(true, "Invalid argument count for '{}'", name);
            diagnostics.push_back(reportError(node, DiagnosticCode::InvalidArgumentCount, "Invalid argument count for '" + name + "'. Expected " + 
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
                    GDSHADER_ERROR_IF(true, "Invalid argument type in function call '{}'", name);
                    diagnostics.push_back(reportError(node->arguments[i].get(), DiagnosticCode::InvalidArgumentType, 
                    "Invalid argument " + std::to_string(i + 1) + " for function '" + name + "'. Expected '" + 
                    expected->toString() + "', but found '" + actual->toString() + "'."));
                    break; 
                }
            }
        }
    } else {
        Range nameRange = node->range;
        nameRange.endCol = nameRange.startCol + node->functionName.length();
        symbols.addReference(const_cast<Symbol*>(bestMatch), nameRange);
    }
}

void TypeCheckingVisitor::visit(MemberAccessNode* node) 
{
    GDSHADER_RETURN_IF(!node, "MemberAccessNode is null");
    if (node->base) node->base->accept(*this);
    TypePtr baseT = resolveType(node->base.get());

    if (baseT->kind == TypeKind::UNKNOWN) return;
    if (baseT->kind == TypeKind::ARRAY && node->member == "length") return;

    TypePtr memberT = typeRegistry.getMemberType(baseT, node->member);
    if (memberT->kind == TypeKind::UNKNOWN) {
        if (baseT->kind == TypeKind::VECTOR) {
            std::string swizzle = node->member;
            
            if (swizzle.length() > 4) {
                GDSHADER_ERROR_IF(true, "Swizzle '{}' too long", swizzle);
                diagnostics.push_back(reportError(node, DiagnosticCode::InvalidSwizzle, "Swizzle '" + swizzle + "' is too long (max 4 components)."));
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
                bool hasXYZW = false, hasRGBA = false; bool hasSTPQ = false;
                for (char c : swizzle) {
                    if (std::string("xyzw").find(c) != std::string::npos) hasXYZW = true;
                    if (std::string("rgba").find(c) != std::string::npos) hasRGBA = true;
                    if (std::string("stpq").find(c) != std::string::npos) hasSTPQ = true;
                }
                if ( (hasXYZW && hasRGBA) || (hasXYZW && hasSTPQ) || (hasRGBA && hasSTPQ) ) 
                {
                    GDSHADER_ERROR_IF(true, "Illegal swizzle '{}', mixed sets", swizzle);
                    diagnostics.push_back(reportError(node, DiagnosticCode::InvalidSwizzle, "Illegal swizzle '" + swizzle + "'. Cannot mix xyzw, rgba and stpq sets."));
                } else {
                    GDSHADER_ERROR_IF(true, "Invalid swizzle component in '{}'", swizzle);
                    diagnostics.push_back(reportError(node, DiagnosticCode::InvalidSwizzle, "Invalid swizzle component in '" + swizzle + "'."));
                }
                return;
            }

            std::string currentSet = sets[validSetIndex];
            for (char c : swizzle) {
                size_t componentIndex = currentSet.find(c);
                if (componentIndex >= (size_t)baseT->componentCount) {
                    GDSHADER_ERROR_IF(true, "Swizzle component out of bounds");
                    diagnostics.push_back(reportError(node, DiagnosticCode::InvalidSwizzle, "Swizzle component '" + std::string(1, c) + "' is out of bounds for " + baseT->toString() + "."));
                    return;
                }
            }
        }
        GDSHADER_ERROR_IF(true, "Invalid member '{}' on type '{}'", node->member, baseT->toString());
        diagnostics.push_back(reportError(node, DiagnosticCode::InvalidMemberAccess, "Invalid member '" + node->member + "' on type '" + baseT->toString() + "'."));
    }
}

void TypeCheckingVisitor::visit(ArrayAccessNode* node) 
{
    GDSHADER_RETURN_IF(!node, "ArrayAccessNode is null");
    if (node->base) node->base->accept(*this);
    if (node->index) node->index->accept(*this);

    TypePtr baseType = resolveType(node->base.get());
    TypePtr indexType = resolveType(node->index.get());

    if (baseType->kind != TypeKind::ARRAY && baseType->kind != TypeKind::VECTOR && baseType->kind != TypeKind::MATRIX && baseType->kind != TypeKind::UNKNOWN) {
        GDSHADER_ERROR_IF(true, "Type '{}' is not indexable", baseType->toString());
        diagnostics.push_back(reportError(node, DiagnosticCode::NotIndexable, "Type '" + baseType->toString() + "' is not indexable."));
    }

    if (indexType->name != "int" && indexType->name != "uint" && indexType->kind != TypeKind::UNKNOWN) {
        GDSHADER_ERROR_IF(true, "Array index is not an integer");
        diagnostics.push_back(reportError(node->index.get(), DiagnosticCode::InvalidArrayIndex, "Array index must be an integer."));
    }
}

// -------------------------------------------------------------------------
// HELPERS
// -------------------------------------------------------------------------

TypePtr TypeCheckingVisitor::resolveType(const ExpressionNode* node) 
{
    GDSHADER_RETURN_VAL_IF(!node, typeRegistry.getUnknownType(), "ExpressionNode is null in resolveType");

    TypePtr result = typeRegistry.getUnknownType();

    if (auto lit = dynamic_cast<const LiteralNode*>(node)) {
        if (lit->type == TokenType::TOKEN_NUMBER) result = (lit->value.find('.') != std::string::npos) ? typeRegistry.getType("float") : typeRegistry.getType("int");
        else if (lit->type == TokenType::KEYWORD_TRUE || lit->type == TokenType::KEYWORD_FALSE) result =  typeRegistry.getType("bool");
        else result = typeRegistry.getUnknownType();
    }

    if (auto id = dynamic_cast<const IdentifierNode*>(node)) {
        int line = id->range.startLine;
        const Symbol* s = symbols.lookupAt(id->name, line);
        result = (s && s->type) ? s->type : typeRegistry.getUnknownType();
    }

    if (auto bin = dynamic_cast<const BinaryOpNode*>(node)) {
        TypePtr l = resolveType(bin->left.get());
        TypePtr r = resolveType(bin->right.get());
        result = getBinaryOpResultType(l, r, bin->op);
    }

    if (auto idx = dynamic_cast<const ArrayAccessNode*>(node)) {
        TypePtr base = resolveType(idx->base.get());
        if (base->kind == TypeKind::ARRAY) result = base->baseType;
        else if (base->kind == TypeKind::VECTOR) result = base->baseType;
        else if (base->kind == TypeKind::MATRIX) result = typeRegistry.getType("vec" + std::to_string(base->componentCount));
        else result = typeRegistry.getUnknownType();
    }

    if (auto call = dynamic_cast<const FunctionCallNode*>(node)) 
    {
        TypePtr t = typeRegistry.getType(call->functionName);
        if (t->kind != TypeKind::UNKNOWN) {
            result = t;
        }
        else {
            std::vector<TypePtr> argTypes;
            for (const auto& arg : call->arguments) {
                if (!arg) {
                    SPDLOG_ERROR("arg in functionCallNode is nullptr");
                    argTypes.push_back(typeRegistry.getUnknownType());
                    continue;
                }
                argTypes.push_back(resolveType(arg.get()));
            }

            const Symbol* match = findBestOverload(call, argTypes);
            if (match && match->returnType) result = match->returnType;
            else result = typeRegistry.getUnknownType();
        }
    }

    if (auto ctor = dynamic_cast<const ConstructorNode*>(node)) {
        TypePtr t = typeRegistry.getType(ctor->typeName);
        if (t->kind != TypeKind::UNKNOWN) {
            result = t;
        }
    }

    if (auto mem = dynamic_cast<const MemberAccessNode*>(node)) {
        if (mem->member == "length") return typeRegistry.getType("int");
        TypePtr base = resolveType(mem->base.get());
        result = typeRegistry.getMemberType(base, mem->member);
    }

    if (auto un = dynamic_cast<const UnaryOpNode*>(node)) {
        result = resolveType(un->operand.get());
    }

    if (auto tern = dynamic_cast<const TernaryNode*>(node)) {
        result = resolveType(tern->trueExpr.get());
    }

    const_cast<ExpressionNode*>(node)->evaluatedType = result;
    return result;
}

TypePtr TypeCheckingVisitor::getBinaryOpResultType(TypePtr l, TypePtr r, TokenType op) 
{
    GDSHADER_RETURN_VAL_IF(!l || !r, typeRegistry.getUnknownType(), "Left or right type is null in getBinaryOpResultType");
    
    if (l->kind == TypeKind::UNKNOWN || r->kind == TypeKind::UNKNOWN) return typeRegistry.getUnknownType();

    bool isComparison = (op == TokenType::TOKEN_EQ_EQ || op == TokenType::TOKEN_NOT_EQ ||
                         op == TokenType::TOKEN_LESS || op == TokenType::TOKEN_GREATER ||
                         op == TokenType::TOKEN_LESS_EQ || op == TokenType::TOKEN_GREATER_EQ);

    if (isComparison) return typeRegistry.getType("bool"); 

    if (op == TokenType::TOKEN_CARET_CARET) {
        if (l->name == "bool" && r->name == "bool") return typeRegistry.getType("bool");
        return typeRegistry.getUnknownType();
    }

    bool isBitwise = (op == TokenType::TOKEN_LESS_LESS || op == TokenType::TOKEN_GREATER_GREATER ||
                      op == TokenType::TOKEN_AMPERSAND || op == TokenType::TOKEN_PIPE || op == TokenType::TOKEN_CARET);
    if (isBitwise) {
        bool lIsInt = (l->name.find("int") != std::string::npos || l->name.find("uint") != std::string::npos || l->name.find("ivec") != std::string::npos || l->name.find("uvec") != std::string::npos);
        bool rIsInt = (r->name.find("int") != std::string::npos || r->name.find("uint") != std::string::npos || r->name.find("ivec") != std::string::npos || r->name.find("uvec") != std::string::npos);
        
        if (lIsInt && rIsInt) {
            return l->kind == TypeKind::VECTOR ? l : (r->kind == TypeKind::VECTOR ? r : l); 
        }
        return typeRegistry.getUnknownType();
    }

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
    GDSHADER_RETURN_VAL_IF(!from || !to, -1, "Source or target type is null in getConversionCost");
    if (*from == *to) return 0;
    if (from->name == "int" && to->name == "float") return 1;
    if (from->name == "uint" && to->name == "float") return 1;
    return -1; 
}

const Symbol* TypeCheckingVisitor::getRootSymbol(const ExpressionNode* node) 
{
    GDSHADER_RETURN_VAL_IF(!node, nullptr, "ExpressionNode is null in getRootSymbol");

    if (auto id = dynamic_cast<const IdentifierNode*>(node)) {
        int line = id->range.startLine;
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
    GDSHADER_RETURN_VAL_IF(!node, nullptr, "FunctionCallNode is null in findBestOverload");
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
        GDSHADER_ERROR_IF(true, "Ambiguous function call for '{}'", name);
        diagnostics.push_back(reportError(node, DiagnosticCode::AmbiguousFunctionCall, "Ambiguous function call for '" + name + "'. Multiple overloads match these arguments."));
    }
    return bestMatch;
}

void TypeCheckingVisitor::validateConstructor(const FunctionCallNode* node, const std::string& typeName) 
{
    GDSHADER_RETURN_IF(!node, "FunctionCallNode is null in validateConstructor");
    TypePtr target = typeRegistry.getType(typeName);
    
    if (target->kind == TypeKind::STRUCT) {

        const Symbol* structSym = symbols.lookupAt(typeName, node->range.startLine);
        if (structSym && structSym->category == SymbolType::Struct) {
            Range nameRange = node->range;
            nameRange.endCol = nameRange.startCol + typeName.length();
            symbols.addReference(const_cast<Symbol*>(structSym), nameRange);
        }

        if (node->arguments.size() != target->members.size()) {
            GDSHADER_ERROR_IF(true, "Struct constructor arg mismatch for '{}'", typeName);
            diagnostics.push_back(reportError(node, DiagnosticCode::ConstructorArgumentMismatch, "Constructor for '" + typeName + "' expects " + 
                        std::to_string(target->members.size()) + " arguments, but got " + 
                        std::to_string(node->arguments.size()) + "."));
            return;
        }

        for (size_t i = 0; i < target->members.size(); ++i) {
            TypePtr argType = resolveType(node->arguments[i].get());
            TypePtr expectedType = target->members[i].second;

            if (*argType != *expectedType) {
                GDSHADER_ERROR_IF(true, "Struct constructor type mismatch for arg {}", i+1);
                diagnostics.push_back(reportError(node->arguments[i].get(), DiagnosticCode::TypeMismatch, 
                    "Type mismatch in struct constructor argument " + std::to_string(i+1) + 
                    ". Expected '" + expectedType->toString() + "', found '" + argType->toString() + "'."));
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
                        GDSHADER_ERROR_IF(true, "Attempted to construct '{}' from string", typeName);
                        diagnostics.push_back(reportError(arg.get(), DiagnosticCode::CannotConstructType, "Cannot construct '" + typeName + "' from a string."));
                        continue;
                    }
                }
                GDSHADER_ERROR_IF(true, "Invalid argument type in vector constructor");
                diagnostics.push_back(reportError(arg.get(), DiagnosticCode::InvalidArgumentType, "Invalid argument type."));
                continue; 
            }

            if (argT->name == "void") {
                GDSHADER_ERROR_IF(true, "Attempted to use void in constructor");
                diagnostics.push_back(reportError(arg.get(), DiagnosticCode::CannotConstructType, "Cannot use 'void' expression in constructor."));
                continue;
            }
            
            if (argT->kind != TypeKind::SCALAR && argT->kind != TypeKind::VECTOR) {
                GDSHADER_ERROR_IF(true, "Invalid arg for vector constructor (needs scalar or vector)");
                diagnostics.push_back(reportError(arg.get(), DiagnosticCode::InvalidArgumentType, "Invalid argument for vector constructor. Expected Scalar or Vector."));
                continue; 
            }

            provided += (argT->kind == TypeKind::SCALAR) ? 1 : argT->componentCount;
        }

        if (node->arguments.size() == 1 && provided == 1 && expected > 1) return;

        if (provided != expected) {
            GDSHADER_ERROR_IF(true, "Component count mismatch in vector constructor (Expected: {}, Found: {})", expected, provided);
            diagnostics.push_back(reportError(node, DiagnosticCode::ConstructorArgumentMismatch, "Invalid constructor. Expected " + std::to_string(expected) + 
                " components, but found " + std::to_string(provided) + "."));
        }
        return;
    }

    if (target->kind == TypeKind::MATRIX) {
        // Matrix constructors are strictly typed (Godot shading language docs):
        //   matN(vecN, vecN, ...)   One column vector per column.
        //   matN(float)             Diagonal matrix, e.g. mat4(1.0) is the identity.
        //   matN(matM)              Matrix conversion, only between different dimensions.
        const int dim = target->componentCount;

        std::vector<TypePtr> argTypes;
        argTypes.reserve(node->arguments.size());
        for (const auto& arg : node->arguments) {
            argTypes.push_back(resolveType(arg.get()));
        }

        // An unresolved argument was already reported while visiting it.
        for (const auto& argT : argTypes) {
            if (argT->kind == TypeKind::UNKNOWN) return;
        }

        // Diagonal construction: matN(float), with the usual int/uint -> float conversions.
        if (argTypes.size() == 1 && argTypes[0]->kind == TypeKind::SCALAR) {
            TypePtr scalarT = argTypes[0];
            if (scalarT->name == "void") {
                GDSHADER_ERROR_IF(true, "Attempted to use void in matrix constructor '{}'", typeName);
                diagnostics.push_back(reportError(node->arguments[0].get(), DiagnosticCode::CannotConstructType, "Cannot use 'void' expression in constructor."));
            } else if (getConversionCost(scalarT, typeRegistry.getType("float")) == -1) {
                GDSHADER_ERROR_IF(true, "Invalid diagonal construction of '{}' from '{}'", typeName, scalarT->toString());
                diagnostics.push_back(reportError(node->arguments[0].get(), DiagnosticCode::InvalidArgumentType,
                    "Invalid argument for constructor '" + typeName + "'. Expected 'float', but found '" + scalarT->toString() + "'."));
            }
            return;
        }

        // Matrix conversion: matN(matM). There is no matN(matN) constructor;
        // matrices of the same dimension are copied by direct assignment.
        if (argTypes.size() == 1 && argTypes[0]->kind == TypeKind::MATRIX) {
            if (argTypes[0]->componentCount == dim) {
                GDSHADER_ERROR_IF(true, "No same-dimension matrix constructor for '{}'", typeName);
                diagnostics.push_back(reportError(node, DiagnosticCode::ConstructorArgumentMismatch,
                    "No matching constructor found for '" + typeName + "'. Matrices of the same dimension can be assigned directly."));
            }
            return;
        }

        // Column construction: every argument must be a column vector of the matrix dimension.
        if (argTypes.size() == (size_t)dim) {
            for (size_t i = 0; i < argTypes.size(); ++i) {
                if (*argTypes[i] == *target->baseType) continue;

                GDSHADER_ERROR_IF(true, "Invalid argument {} for matrix constructor '{}'", i + 1, typeName);
                diagnostics.push_back(reportError(node->arguments[i].get(), DiagnosticCode::InvalidArgumentType,
                    "Invalid argument " + std::to_string(i + 1) + " for constructor '" + typeName + "'. Expected '" +
                    target->baseType->toString() + "', but found '" + argTypes[i]->toString() + "'."));
                break;
            }
            return;
        }

        GDSHADER_ERROR_IF(true, "No matching matrix constructor for '{}' with {} arguments", typeName, argTypes.size());
        diagnostics.push_back(reportError(node, DiagnosticCode::ConstructorArgumentMismatch,
            "No matching constructor found for '" + typeName + "'."));
        return;
    }

    if (target->kind == TypeKind::SCALAR) {
        if (typeName == "void") {
            GDSHADER_ERROR_IF(true, "Attempted to construct void");
            diagnostics.push_back(reportError(node, DiagnosticCode::CannotConstructType, "Cannot construct 'void'."));
            return;
        }

        bool isBasic = (typeName == "float" || typeName == "int" || typeName == "uint" || typeName == "bool");
        if (!isBasic) {
            GDSHADER_ERROR_IF(true, "Attempted to construct opaque type '{}'", typeName);
            diagnostics.push_back(reportError(node, DiagnosticCode::CannotConstructType, "Cannot construct opaque type '" + typeName + "'."));
            return;
        }

        if (node->arguments.size() != 1) {
            GDSHADER_ERROR_IF(true, "Scalar constructor expects 1 argument");
            diagnostics.push_back(reportError(node, DiagnosticCode::ConstructorArgumentMismatch, "Scalar constructor expects exactly 1 argument."));
        }

        if (!node->arguments.empty()) {
            TypePtr argT = resolveType(node->arguments[0].get());
             if (argT->kind == TypeKind::UNKNOWN) {
                GDSHADER_ERROR_IF(true, "Invalid argument in scalar constructor");
                diagnostics.push_back(reportError(node->arguments[0].get(), DiagnosticCode::InvalidArgumentType, "Invalid argument."));
            }
        }
        return;
    }

    if (target->kind == TypeKind::SAMPLER) {
        GDSHADER_ERROR_IF(true, "Attempted to construct opaque sampler type '{}'", typeName);
        diagnostics.push_back(reportError(node, DiagnosticCode::CannotConstructType, "Samplers are opaque types and cannot be instantiated via constructors."));
        return;
    }
    
    GDSHADER_ERROR_IF(true, "Cannot construct type '{}'", typeName);
    diagnostics.push_back(reportError(node, DiagnosticCode::CannotConstructType, "Cannot construct type '" + typeName + "'."));
}

void TypeCheckingVisitor::reportTypeMismatch(const ASTNode* node, const std::string& expected, const std::string& found) 
{
    GDSHADER_ERROR_IF(true, "Type mismatch reported: Expected '{}', Found '{}'", expected, found);
    diagnostics.push_back(reportError(node, DiagnosticCode::TypeMismatch, "Type mismatch: Expected '" + expected + "', but found '" + found + "'."));
}

} // namespace gdshader_lsp