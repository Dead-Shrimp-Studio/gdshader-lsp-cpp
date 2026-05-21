#include "linting_visitor.hpp"
#include "gdshader/ast/ast.h"
#include "utils/logger.hpp"

namespace gdshader_lsp {

// -------------------------------------------------------------------------
// ACTIVE CHECKERS
// -------------------------------------------------------------------------

void LintingVisitor::visit(ProgramNode* node) 
{
    GDSHADER_RETURN_IF(!node, "ProgramNode is null");
    for (const auto& child : node->nodes) {
        if (child) child->accept(*this);
    }
}

void LintingVisitor::visit(FunctionNode* node) 
{
    GDSHADER_RETURN_IF(!node, "FunctionNode is null");

    // Save previous state
    std::string prevFunc = currentFunctionName;
    ShaderStage prevStage = currentProcessorFunction;
    bool prevReturn = currentFunctionHasReturn;

    currentFunctionName = node->name;
    currentFunctionHasReturn = false;

    // Determine Processor Stage
    if (node->name == "vertex") currentProcessorFunction = ShaderStage::Vertex;
    else if (node->name == "fragment") currentProcessorFunction = ShaderStage::Fragment;
    else if (node->name == "light") currentProcessorFunction = ShaderStage::Light;
    else if (node->name == "start") currentProcessorFunction = ShaderStage::Start;
    else if (node->name == "process") currentProcessorFunction = ShaderStage::Process;
    else if (node->name == "sky") currentProcessorFunction = ShaderStage::Sky;
    else if (node->name == "fog") currentProcessorFunction = ShaderStage::Fog;
    else currentProcessorFunction = ShaderStage::Global;

    // 1. Processor Rules
    if (currentProcessorFunction != ShaderStage::Global) {
        if (node->returnType && node->returnType->baseName != "void") {
            GDSHADER_ERROR_IF(true, "Processor function '{}' does not return void", node->name);
            diagnostics.push_back(reportError(node, DiagnosticCode::ProcessorMustReturnVoid, "Processor function '" + node->name + "' must return 'void'."));
        }
        if (!node->parameters.empty()) {
            GDSHADER_ERROR_IF(true, "Processor function '{}' has arguments", node->name);
            diagnostics.push_back(reportError(node, DiagnosticCode::ProcessorCannotHaveArgs, "Processor function '" + node->name + "' must not have arguments."));
        }
    }

    // Traverse body
    if (node->body) node->body->accept(*this);

    // 2. Return Validation
    if (node->returnType && node->returnType->baseName != "void") {
        if (!currentFunctionHasReturn) {
            GDSHADER_ERROR_IF(true, "Function '{}' missing return value", node->name);
            diagnostics.push_back(reportError(node, DiagnosticCode::MissingReturnValue, "Function '" + node->name + "' must return a value."));
        } else if (!allPathsReturn(node->body.get())) {
            GDSHADER_ERROR_IF(true, "Not all paths return a value in '{}'", node->name);
            diagnostics.push_back(reportError(node, DiagnosticCode::NotAllPathsReturn, "Not all code paths return a value in function '" + node->name + "'."));
        }
    }

    // Restore previous state
    currentFunctionName = prevFunc;
    currentProcessorFunction = prevStage;
    currentFunctionHasReturn = prevReturn;
}

void LintingVisitor::visit(BlockNode* node) 
{
    GDSHADER_RETURN_IF(!node, "BlockNode is null");
    bool unreachable = false;
    
    for (const auto& stmt : node->statements) 
    {
        if (unreachable) {
            GDSHADER_WARN_IF(true, "Unreachable code detected in block");
            diagnostics.push_back(reportWarning(stmt.get(), DiagnosticCode::UnreachableCode, "Unreachable code detected."));
        }

        if (stmt) stmt->accept(*this);

        if (dynamic_cast<const ReturnNode*>(stmt.get()) || 
            dynamic_cast<const DiscardNode*>(stmt.get()) ||
            dynamic_cast<const BreakNode*>(stmt.get()) ||
            dynamic_cast<const ContinueNode*>(stmt.get())) {
            unreachable = true;
        }
    }

    for (const auto& stmt : node->statements) 
    {
        if (auto varDecl = dynamic_cast<const VariableDeclNode*>(stmt.get())) 
        {
            GDSHADER_RETURN_IF(varDecl->name.empty(), "node name empty");
            int line = varDecl->range.startLine;
            const Symbol* sym = symbols.lookupAt(varDecl->name, line);
            
            if (!sym) continue;
            if (sym->category == SymbolType::Variable && sym->references.empty()) {
                GDSHADER_WARN_IF(true, "Unused variable '{}'", varDecl->name);
                diagnostics.push_back(reportWarning(varDecl, DiagnosticCode::UnusedVariable, "Unused variable '" + varDecl->name + "'."));
            }
        }
    }
}

void LintingVisitor::visit(ReturnNode* node) 
{
    GDSHADER_RETURN_IF(!node, "ReturnNode is null");
    currentFunctionHasReturn = true;

    if (node->value) node->value->accept(*this);
}

void LintingVisitor::visit(DiscardNode* node) 
{
    GDSHADER_RETURN_IF(!node, "DiscardNode is null");
    if (currentProcessorFunction == ShaderStage::Vertex || currentProcessorFunction == ShaderStage::Light) {
        GDSHADER_ERROR_IF(true, "Invalid discard in vertex/light processor");
        diagnostics.push_back(reportError(node, DiagnosticCode::InvalidDiscardUsage, "'discard' cannot be used in the vertex or light processor."));
    }
}

void gdshader_lsp::LintingVisitor::visit(BreakNode *node)
{
    if (loopDepth == 0) diagnostics.push_back(reportError(node, DiagnosticCode::BreakOutsideLoop, "Break statement outside of loop."));
}

void LintingVisitor::visit(ContinueNode* node) 
{
    if (loopDepth == 0) diagnostics.push_back(reportError(node, DiagnosticCode::ContinueOutsideLoop, "Continue statement outside of loop."));
}

void LintingVisitor::visit(FunctionCallNode* node) 
{
    GDSHADER_RETURN_IF(!node, "FunctionCallNode is null");

    if (node->functionName == currentFunctionName) 
    {
        GDSHADER_ERROR_IF(true, "Recursion detected: '{}' calls itself", node->functionName);
        diagnostics.push_back(reportError(node, DiagnosticCode::RecursionNotAllowed, "Recursion is not allowed in shaders (function '" + node->functionName + "' calls itself)."));
    }

    for (const auto& arg : node->arguments) 
    {
        if (arg) arg->accept(*this);
    }
}

// -------------------------------------------------------------------------
// STRUCTURAL PASS-THROUGHS
// -------------------------------------------------------------------------

void LintingVisitor::visit(IfNode* node) 
{
    GDSHADER_RETURN_IF(!node, "IfNode is null");

    if (node->condition) node->condition->accept(*this);

    if (node->thenBranch)
    {
        node->thenBranch->accept(*this);
    } else 
    {
        GDSHADER_ERROR_IF(true, "If statement missing execution branch");
        diagnostics.push_back(reportError(node, DiagnosticCode::MissingExecutionBranch, "If statements require at least one execution path (missing branch)."));
    }
    if (node->elseBranch) node->elseBranch->accept(*this);
}

void LintingVisitor::visit(WhileNode* node) 
{
    GDSHADER_RETURN_IF(!node, "WhileNode is null");

    if (node->condition) node->condition->accept(*this);

    if (node->body)
    {
        loopDepth++;
        node->body->accept(*this);
        loopDepth--;
    } else 
    {
        GDSHADER_ERROR_IF(true, "While statement missing body");
        diagnostics.push_back(reportError(node, DiagnosticCode::MissingExecutionBranch, "While statements require a body."));
    }
}

void LintingVisitor::visit(ForNode* node) 
{
    GDSHADER_RETURN_IF(!node, "ForNode is null");
    
    if (node->init) node->init->accept(*this);
    if (node->condition) node->condition->accept(*this);
    if (node->increment) node->increment->accept(*this);

    if (node->body) 
    {
        loopDepth++;
        node->body->accept(*this);
        loopDepth--;
    } else 
    {
        GDSHADER_ERROR_IF(true, "For statement missing body");
        diagnostics.push_back(reportError(node, DiagnosticCode::MissingExecutionBranch, "For statements require a body."));
    }
}

void LintingVisitor::visit(DoWhileNode* node) 
{
    GDSHADER_RETURN_IF(!node, "DoWhileNode is null");

    if (node->condition) node->condition->accept(*this);
    
    if (node->body) 
    {
        loopDepth++;
        node->body->accept(*this);
        loopDepth--;
    } else
    {
        GDSHADER_ERROR_IF(true, "Do-while statement missing body");
        diagnostics.push_back(reportError(node, DiagnosticCode::MissingExecutionBranch, "Do-while statements require a body."));
    }
}

void LintingVisitor::visit(SwitchNode* node) 
{
    GDSHADER_RETURN_IF(!node, "SwitchNode is null");

    if (node->expression) node->expression->accept(*this);

    if (node->cases.empty())
    {
        GDSHADER_ERROR_IF(true, "Switch statement missing cases");
        diagnostics.push_back(reportError(node, DiagnosticCode::MissingExecutionBranch, "Switch statements require at least one case block."));
    }

    for (const auto& c : node->cases) {
        for (const auto& stmt : c->statements) {
            if (stmt) stmt->accept(*this);
        }
    }
}

void LintingVisitor::visit(ExpressionStatementNode* node) 
{
    GDSHADER_RETURN_IF(!node, "ExpressionStatementNode is null");
    if (node->expr) node->expr->accept(*this);
}

void LintingVisitor::visit(VariableDeclNode* node) 
{
    GDSHADER_RETURN_IF(!node, "VariableDeclNode is null");
    if (node->initializer) node->initializer->accept(*this);
}

void LintingVisitor::visit(BinaryOpNode* node) 
{
    GDSHADER_RETURN_IF(!node, "BinaryOpNode is null");
    if (node->left) node->left->accept(*this);
    if (node->right) node->right->accept(*this);
}

void LintingVisitor::visit(UnaryOpNode* node) 
{
    GDSHADER_RETURN_IF(!node, "UnaryOpNode is null");
    if (node->operand) node->operand->accept(*this);
}

void LintingVisitor::visit(TernaryNode* node) 
{
    GDSHADER_RETURN_IF(!node, "TernaryNode is null");
    if (node->condition) node->condition->accept(*this);
    if (node->trueExpr) node->trueExpr->accept(*this);
    if (node->falseExpr) node->falseExpr->accept(*this);
}

void LintingVisitor::visit(ConstructorNode* node) 
{
    GDSHADER_RETURN_IF(!node, "ConstructorNode is null");
    for (const auto& arg : node->arguments) {
        if (arg) arg->accept(*this);
    }
}

void LintingVisitor::visit(ArrayAccessNode* node) 
{
    GDSHADER_RETURN_IF(!node, "ArrayAccessNode is null");
    if (node->base) node->base->accept(*this);
    if (node->index) node->index->accept(*this);
}

void LintingVisitor::visit(MemberAccessNode* node) 
{
    GDSHADER_RETURN_IF(!node, "MemberAccessNode is null");
    if (node->base) node->base->accept(*this);
}

// -------------------------------------------------------------------------
// HELPERS
// -------------------------------------------------------------------------

bool LintingVisitor::allPathsReturn(const ASTNode* node) 
{
    if (!node)
    {
        SPDLOG_ERROR("ASTNode in return path check is nullptr");
        return false;
    }

    if (dynamic_cast<const ReturnNode*>(node)) return true;
    if (dynamic_cast<const DiscardNode*>(node)) return true;
    
    if (auto block = dynamic_cast<const BlockNode*>(node)) {
        for (const auto& stmt : block->statements) {
            if (stmt) {
                if (allPathsReturn(stmt.get())) return true;
            }
        }
        return false;
    }

    if (auto ifNode = dynamic_cast<const IfNode*>(node)) 
    {
        if (ifNode) {
            return ifNode->elseBranch && 
               allPathsReturn(ifNode->thenBranch.get()) && 
               allPathsReturn(ifNode->elseBranch.get());
        }
    }

    return false;
}

} // namespace gdshader_lsp