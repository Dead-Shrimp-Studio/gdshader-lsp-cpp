
#include "gdshader/semantics/visitors/preprocessor_visitor.hpp"

#include "gdshader/ast/ast.h"
#include "server/project_manager.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

void PreprocessorVisitor::visit(ProgramNode* node) 
{
    GDSHADER_RETURN_IF(!node, "Program node is null.");
    for (const auto& child : node->nodes) {
        if (child) child->accept(*this);
    }
}

void PreprocessorVisitor::visit(BlockNode* node) 
{
    GDSHADER_RETURN_IF(!node, "Block node is null.");
    for (const auto& stmt : node->statements) {
        if (stmt) stmt->accept(*this);
    }
}

void PreprocessorVisitor::visit(IncludeNode* node) 
{
    GDSHADER_RETURN_IF(!node, "Include node is null.");

    auto pm = ProjectManager::get_singleton();
    std::string absPath = pm->resolvePath(currentFilePath, node->path);

    // Prevent circular dependencies
    if (processedFiles.count(absPath)) {
        SPDLOG_TRACE("Skipping already processed include: {}", absPath);
        return;
    }
    processedFiles.insert(absPath);

    auto current_unit = pm->getUnit(currentFilePath);
    auto included_unit = pm->getUnit(absPath);

    // Add the included file to the current file's includes
    if (std::find(current_unit->includedPaths.begin(), current_unit->includedPaths.end(), absPath) == current_unit->includedPaths.end()) {
        current_unit->includedPaths.push_back(absPath);
    }

    // Add the current file to the included file's "imported by" list
    if (std::find(included_unit->importedBy.begin(), included_unit->importedBy.end(), currentFilePath) == included_unit->importedBy.end()) {
        included_unit->importedBy.push_back(currentFilePath);
    }

    auto exportedSymbols = pm->getExports(absPath);

    if (!exportedSymbols) {
        diagnostics.push_back(reportError(node, "Could not load include: " + node->path + " (not found or cyclic include)"));
        return;
    }

    // Import globals into our current symbol table
    for (const auto& [name, overloadList] : exportedSymbols->getGlobals()) {
        for (const auto& sym : overloadList) {
            
            if (sym.category == SymbolType::Builtin || sym.source_path == "builtin" || sym.source_path.empty()) continue;

            if (!symbols.add(sym)) {
                diagnostics.push_back(reportError(node, "Symbol '" + name + "' imported from " + node->path + " conflicts with existing symbol."));
            }

            if (sym.category == SymbolType::Struct) {
                typeRegistry.registerStruct(sym.name, sym.type->members);
            }
        }
    }
}

void PreprocessorVisitor::visit(DefineNode* node) 
{
    GDSHADER_RETURN_IF(!node, "Define node is null.");
    if (node->value) {
        TypePtr type = resolveType(node->value.get());
        
        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Variable, node->range, Mutability::ReadOnly);
        s.hint = "Macro definition";
        
        if (!symbols.add(s)) {
            diagnostics.push_back(reportWarning(node, "Macro redefinition"));
        }

    } else {
        TypePtr type = typeRegistry.getType("bool");

        Symbol s = symbols.createSymbol(node->name, type, SymbolType::Variable, node->range, Mutability::ReadOnly);
        s.hint = "Preprocessor flag";
        symbols.add(s);
    }
}

TypePtr PreprocessorVisitor::resolveType(const ExpressionNode* node) 
{
    GDSHADER_RETURN_VAL_IF(!node, typeRegistry.getUnknownType(), "Expression node is null.");

    if (auto lit = dynamic_cast<const LiteralNode*>(node)) {
        if (lit->type == TokenType::TOKEN_NUMBER) 
            return (lit->value.find('.') != std::string::npos) ? typeRegistry.getType("float") : typeRegistry.getType("int");
        
        if (lit->type == TokenType::KEYWORD_TRUE || lit->type == TokenType::KEYWORD_FALSE) 
            return typeRegistry.getType("bool");
    }
    return typeRegistry.getUnknownType();
}

} // namespace gdshader_lsp