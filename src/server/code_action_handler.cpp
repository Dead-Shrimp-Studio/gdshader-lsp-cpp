
#include "server/code_action_handler.hpp"

#include "gdshader/semantics/visitors/call_finder_visitor.hpp"
#include "gdshader/semantics/visitors/node_finder_visitor.hpp"
#include "gdshader/ast/ast.h"

#include "utils/misc.hpp"

#include <spdlog/spdlog.h>

namespace gdshader_lsp {

// --- Implementations ---

std::optional<lsp::CodeAction> CodeActionHandler::fixMissingArguments(const lsp::Diagnostic& diag, const CodeActionContext& context)
{
    if (!context.ast || !context.symbols) return std::nullopt;

    CallFinderVisitor finder(diag.range.start.line, diag.range.start.character);
    context.ast->accept(finder);

    if (!finder.activeCall) return std::nullopt;

    std::string funcName = finder.activeCall->functionName;
    std::vector<const Symbol*> overloads = context.symbols->lookupFunctions(funcName);
    
    if (overloads.empty()) return std::nullopt;

    const Symbol* targetFunc = overloads[0]; 
    size_t expectedCount = targetFunc->parameterNames.size();
    size_t providedCount = finder.activeCall->arguments.size();

    // If they already have enough (or too many), this specific fix doesn't apply
    if (providedCount >= expectedCount) return std::nullopt;

    std::string snippetInsert = "";
    int snippetIndex = 1;

    for (size_t i = providedCount; i < expectedCount; ++i) {

        if (i > 0) {
            snippetInsert += ", ";
        }
        
        std::string paramName = targetFunc->parameterNames[i];
        snippetInsert += "${" + std::to_string(snippetIndex++) + ":" + paramName + "}";
    }

    lsp::Position insertPos = {
        (unsigned int)finder.activeCall->range.endLine, 
        (unsigned int)finder.activeCall->range.endCol - 1 // -1 to get inside the ')'
    };

    lsp::CodeAction action;
    action.title = "Add missing arguments to '" + funcName + "'";
    action.kind = lsp::CodeActionKind::QuickFix;
    action.diagnostics = { diag };

    lsp::TextEdit edit;
    edit.range = lsp::Range{insertPos, insertPos};
    edit.newText = snippetInsert;

    lsp::WorkspaceEdit wsEdit;
    wsEdit.changes.emplace();
    (*wsEdit.changes)[context.params.textDocument.uri].push_back(edit);
    
    action.edit = wsEdit;
    
    return action;
}

std::optional<lsp::CodeAction> gdshader_lsp::CodeActionHandler::extractMagicNumber(const CodeActionContext &context)
{
    if (!context.ast) {
        SPDLOG_WARN("AST for CodeActionContext is null.");
        return std::nullopt;
    }

    auto pos = context.params.range.start;
    NodeFinderVisitor finder(pos.line, pos.character);
    context.ast->accept(finder);

    if (!finder.deepestNode) {
        SPDLOG_ERROR("Could not find a node at the given position.");
        return std::nullopt;
    }

    auto literalNode = dynamic_cast<const LiteralNode*>(finder.deepestNode);
    if (!literalNode || literalNode->type != TokenType::TOKEN_NUMBER) {
        SPDLOG_DEBUG("Node found at cursor position is not LiteralNode");
        return std::nullopt; 
    }

    bool isFloat = literalNode->value.find('.') != std::string::npos;
    std::string typeStr = isFloat ? "float" : "int";
    std::string varName = "extracted_" + typeStr; 

    lsp::CodeAction action;
    action.title = "Extract to Local Variable";
    action.kind = lsp::CodeActionKind::RefactorExtract;

    lsp::WorkspaceEdit wsEdit;
    wsEdit.changes.emplace();

    lsp::TextEdit replaceEdit;
    replaceEdit.range = literalNode->range.toLspRange();
    replaceEdit.newText = varName;
    (*wsEdit.changes)[context.params.textDocument.uri].push_back(replaceEdit);

    EnclosingFunctionVisitor funcFinder(pos.line);
    context.ast->accept(funcFinder);

    lsp::Position insertPos = {0, 0}; // Fallback to top of file
    std::string indent = "";

    if (funcFinder.enclosingFunc && funcFinder.enclosingFunc->body) {
        insertPos.line = funcFinder.enclosingFunc->body->range.startLine;
        insertPos.character = funcFinder.enclosingFunc->body->range.startCol + 1;
        indent = "\n\t"; // Basic indentation for the new line
    }

    lsp::TextEdit insertEdit;
    insertEdit.range = lsp::Range{insertPos, insertPos};
    insertEdit.newText = indent + typeStr + " " + varName + " = " + literalNode->value + ";";
    
    (*wsEdit.changes)[context.params.textDocument.uri].push_back(insertEdit);
    action.edit = wsEdit;

    return action;

}

std::string gdshader_lsp::CodeActionHandler::findClosestSymbolName(const std::string &typo, const std::shared_ptr<SymbolTable> &symbols)
{
    if (!symbols) return "";

    std::string bestMatch = "";
    int bestDistance = 9999;

    const int threshold = 3;

    for (const auto& symbol : symbols->getAllSymbols())
    {
        int dist = levenshteinDistance(typo, symbol.name);
        if (dist < bestDistance && dist <= threshold)
        {
            bestDistance = dist;
            bestMatch = symbol.name;
        }
    }

    return bestMatch;
}

std::vector<std::variant<lsp::Command, lsp::CodeAction>> 
CodeActionHandler::getActions(const CodeActionContext& context) 
{
    std::vector<std::variant<lsp::Command, lsp::CodeAction>> actions;
    const auto& uri = context.params.textDocument.uri;

    for (const auto& diag : context.params.context.diagnostics) 
    {
        std::string code = ""; 
        if (diag.code.has_value()) {
            if (std::holds_alternative<std::string>(diag.code.value())) {
                code = std::get<std::string>(diag.code.value());
            } else if (std::holds_alternative<int>(diag.code.value())) {
                code = std::to_string(std::get<int>(diag.code.value()));
            }
        }

        if (code == "GDS1001") { // MissingSemicolon
            actions.push_back(createInsertFix("Insert missing ';'", uri, diag.range.end, ";", diag));
        } 
        else if (code == "GDS1002") { // UnmatchedParen
            actions.push_back(createInsertFix("Insert missing ')'", uri, diag.range.end, ")", diag));
        }
        else if (code == "GDS1003") { // UnmatchedBracket
            actions.push_back(createInsertFix("Insert missing ']'", uri, diag.range.end, "]", diag));
        }
        else if (code == "GDS1004") { // UnmatchedBrace
            actions.push_back(createInsertFix("Insert missing '}'", uri, diag.range.end, "}", diag));
        }
        else if (code == "GDS1009") { // ExpectedColon
            actions.push_back(createInsertFix("Insert missing ':'", uri, diag.range.end, ":", diag));
        }

        else if (code == "GDS2000" || code == "GDS2017") { 
            // UndefinedIdentifier or UnknownFunction

            NodeFinderVisitor finder(diag.range.start.line, diag.range.start.character);
            context.ast->accept(finder);

            if (finder.deepestNode) {
                std::string typoName = "";
                
                if (auto idNode = dynamic_cast<const IdentifierNode*>(finder.deepestNode)) {
                    typoName = idNode->name;
                } else if (auto callNode = dynamic_cast<const FunctionCallNode*>(finder.deepestNode)) {
                    typoName = callNode->functionName;
                }

                if (!typoName.empty()) {

                    std::string suggestion = findClosestSymbolName(typoName, context.symbols);
                    
                    if (!suggestion.empty()) {

                        std::string fixTitle = "Change to '" + suggestion + "'";
                        
                        actions.push_back(createReplaceFix(
                            fixTitle, 
                            uri, 
                            finder.deepestNode->range.toLspRange(), 
                            suggestion, 
                            diag
                        ));
                    }
                }
            }
        }

        else if (code == "GDS2005" || code == "GDS2006") { // BreakOutsideLoop or ContinueOutsideLoop
            actions.push_back(createReplaceFix("Remove invalid control flow statement", uri, diag.range, "", diag));
        }
        else if (code == "GDS2012") {
            // VoidCannotReturnValue
            // Replaces `return x;` with `return;`
            actions.push_back(createReplaceFix("Change to empty return", uri, diag.range, "return;", diag));
        }
        else if (code == "GDS2019") { // InvalidArgumentCount
            if (auto action = fixMissingArguments(diag, context)) {
                if (action.has_value()) {
                    actions.push_back(action.value());
                }
            }
        }

        else if (code == "GDS3010") { 
            // InvalidDiscardUsage
            actions.push_back(createReplaceFix("Remove invalid 'discard'", uri, diag.range, "", diag));
        }
        else if (code == "GDS3012") { 
            // MissingOrUnknownShaderType
            lsp::Position topOfFile = {0, 0};
            // Offer the two most common options to the user
            actions.push_back(createInsertFix("Add 'shader_type spatial;' at top", uri, topOfFile, "shader_type spatial;\n", diag));
            actions.push_back(createInsertFix("Add 'shader_type canvas_item;' at top", uri, topOfFile, "shader_type canvas_item;\n", diag));
        }

        else if (code == "GDS4000") { 
            // UnusedVariable
            // Instead of deleting it and risking formatting issues, just comment it out.
            actions.push_back(createInsertFix("Comment out unused variable", uri, diag.range.start, "// ", diag));
        }

        else if (code == "GDS5000") { // EmptyStatement
            actions.push_back(createReplaceFix("Remove unnecessary ';'", uri, diag.range, "", diag));
        }
    }

    if (auto extractActions = extractMagicNumber(context)) {
        if (extractActions.has_value()) {
            actions.push_back(extractActions.value());
        }
    }

    return actions;
}

// --- Helper Implementations ---

lsp::CodeAction CodeActionHandler::createInsertFix(
    const std::string& title, 
    const lsp::DocumentUri& uri, 
    const lsp::Position& position, 
    const std::string& textToInsert, 
    const lsp::Diagnostic& diag) 
{
    lsp::CodeAction action;
    action.title = title;
    action.kind = lsp::CodeActionKind::QuickFix;
    action.diagnostics = { diag }; 
    
    lsp::TextEdit edit;
    edit.range = lsp::Range{position, position}; // Start and end are the same for an insertion
    edit.newText = textToInsert;
    
    lsp::WorkspaceEdit wsEdit;
    wsEdit.changes.emplace(); 
    (*wsEdit.changes)[uri].push_back(edit);
    action.edit = wsEdit;
    
    return action;
}

lsp::CodeAction CodeActionHandler::createReplaceFix(
    const std::string& title, 
    const lsp::DocumentUri& uri, 
    const lsp::Range& range, 
    const std::string& replacementText, 
    const lsp::Diagnostic& diag) 
{
    lsp::CodeAction action;
    action.title = title;
    action.kind = lsp::CodeActionKind::QuickFix;
    action.diagnostics = { diag };
    
    lsp::TextEdit edit;
    edit.range = range; 
    edit.newText = replacementText;       
    
    lsp::WorkspaceEdit wsEdit;
    wsEdit.changes.emplace();
    (*wsEdit.changes)[uri].push_back(edit);
    action.edit = wsEdit;
    
    return action;
}

} // namespace gdshader_lsp