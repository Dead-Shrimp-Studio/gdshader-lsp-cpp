#include "linting_visitor.hpp"
#include "gdshader/ast/ast.h"

namespace gdshader_lsp {

// -------------------------------------------------------------------------
// ACTIVE CHECKERS
// -------------------------------------------------------------------------

void LintingVisitor::visit(ProgramNode* node) {
    for (const auto& child : node->nodes) {
        if (child) child->accept(*this);
    }
}

void LintingVisitor::visit(FunctionNode* node) 
{
    // Save previous state
    std::string prevFunc = currentFunctionName;
    ShaderStage prevStage = currentProcessorFunction;
    bool prevReturn = currentFunctionHasReturn;

    currentFunctionName = node->name;
    currentFunctionHasReturn = false;

    if (currentFunctionName == prevFunc) {
        diagnostics.push_back(reportError(node, "Recursion is not allowed in shader functions (function '" + node->name + "' called itself)"));
    }

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
        if (node->returnType->baseName != "void") {
            diagnostics.push_back(reportError(node, "Processor function '" + node->name + "' must return 'void'."));
        }
        if (!node->parameters.empty()) {
            diagnostics.push_back(reportError(node, "Processor function '" + node->name + "' must not have arguments."));
        }
    }

    // Traverse body
    if (node->body) node->body->accept(*this);

    // 2. Return Validation
    if (node->returnType->baseName != "void") {
        if (!currentFunctionHasReturn) {
            diagnostics.push_back(reportError(node, "Function '" + node->name + "' must return a value."));
        } else if (!allPathsReturn(node->body.get())) {
            diagnostics.push_back(reportError(node, "Not all code paths return a value in function '" + node->name + "'."));
        }
    }

    // Restore previous state
    currentFunctionName = prevFunc;
    currentProcessorFunction = prevStage;
    currentFunctionHasReturn = prevReturn;
}

void LintingVisitor::visit(BlockNode* node) {
    bool unreachable = false;
    
    // 3. Unreachable Code Check
    for (const auto& stmt : node->statements) {
        if (unreachable) {
            diagnostics.push_back(reportWarning(stmt.get(), "Unreachable code detected."));
            break; 
        }

        if (stmt) stmt->accept(*this);

        if (dynamic_cast<const ReturnNode*>(stmt.get()) || 
            dynamic_cast<const DiscardNode*>(stmt.get()) ||
            dynamic_cast<const BreakNode*>(stmt.get()) ||
            dynamic_cast<const ContinueNode*>(stmt.get())) {
            unreachable = true;
        }
    }

    // 4. Unused Variables Check
    for (const auto& stmt : node->statements) {
        if (auto varDecl = dynamic_cast<const VariableDeclNode*>(stmt.get())) {
            int line = (varDecl->range.startLine > 0) ? varDecl->range.startLine - 1 : 0;
            const Symbol* sym = symbols.lookupAt(varDecl->name, line);
            
            // Ignore globals, only warn for local variables
            if (sym && sym->category == SymbolType::Variable && sym->references.empty()) {
                diagnostics.push_back(reportWarning(varDecl, "Unused variable '" + varDecl->name + "'"));
            }
        }
    }
}

void LintingVisitor::visit(ReturnNode* node) {
    currentFunctionHasReturn = true;
}

void LintingVisitor::visit(DiscardNode* node) {
    if (currentProcessorFunction == ShaderStage::Vertex) {
        diagnostics.push_back(reportError(node, "'discard' cannot be used in the vertex processor."));
    }
}

void LintingVisitor::visit(FunctionCallNode* node) 
{
    if (node->functionName == currentFunctionName) {
        diagnostics.push_back(reportError(node, "Recursion is not allowed in shaders (function '" + node->functionName + "' calls itself)."));
    }
}

// -------------------------------------------------------------------------
// STRUCTURAL PASS-THROUGHS
// -------------------------------------------------------------------------

void LintingVisitor::visit(IfNode* node) 
{
    // Checked the condition validity in type checking already
    if (!node->condition)
    {
        diagnostics.push_back(reportError(node, "If statement is missing condition."));
    }

    if (node->thenBranch)
    {
        node->thenBranch->accept(*this);
    } else 
    {
        diagnostics.push_back(reportError(node, "If statements require at least one execution path (missing branch)."));
    }
    if (node->elseBranch) node->elseBranch->accept(*this);
}

void LintingVisitor::visit(WhileNode* node) 
{
    if (node->body) 
    {
        node->body->accept(*this);
    } else 
    {
        diagnostics.push_back(reportError(node, "while statement require at least one execution path (missing branch)."));
    }
}

void LintingVisitor::visit(ForNode* node) 
{
    if (node->body) 
    {
        node->body->accept(*this);
    } else 
    {
        diagnostics.push_back(reportError(node, "for statement require at least one execution path (missing branch)."));
    }
}

void LintingVisitor::visit(DoWhileNode* node) 
{
    if (node->body) 
    {
        node->body->accept(*this);
    } else
    {
        diagnostics.push_back(reportError(node, "do-while statement require at least one execution path (missing branch)."));
    }
}

void LintingVisitor::visit(SwitchNode* node) 
{
    if (node->cases.empty())
    {
        diagnostics.push_back(reportError(node, "switch statement require at least one execution path (missing branch)."));
    }

    for (const auto& c : node->cases) {
        for (const auto& stmt : c->statements) {
            if (stmt) stmt->accept(*this);
        }
    }
}

// -------------------------------------------------------------------------
// HELPERS
// -------------------------------------------------------------------------

bool LintingVisitor::allPathsReturn(const ASTNode* node) 
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
    
    return false;
}

} // namespace gdshader_lsp