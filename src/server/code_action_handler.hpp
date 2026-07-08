#ifndef CODE_ACTION_HANDLER_HPP
#define CODE_ACTION_HANDLER_HPP

#include <lsp/messages.h>
#include <lsp/types.h>

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <optional>

namespace gdshader_lsp
{
    class ProgramNode;
}

namespace gdshader_lsp
{
    class ASTNode;
    class SymbolTable;

    struct CodeActionContext
    {
        const lsp::requests::TextDocument_CodeAction::Params& params;
        const std::unique_ptr<ProgramNode>& ast;
        const std::shared_ptr<SymbolTable> symbols;
        const std::string& source_code;
        const std::string& file_path;
    };
}

namespace gdshader_lsp 
{

    class CodeActionHandler 
    {

        public:

            /**
             * @brief Parses diagnostics from the client and generates appropriate quick fixes.
             */
            static std::vector<std::variant<lsp::Command, lsp::CodeAction>> getActions(const CodeActionContext& context);

        private:
            
            static std::optional<lsp::CodeAction> fixMissingArguments(const lsp::Diagnostic& diag, const CodeActionContext& context);
            static std::optional<lsp::CodeAction> extractMagicNumber(const CodeActionContext& context);
            static std::string findClosestSymbolName(const std::string& typo, const std::shared_ptr<SymbolTable>& symbols);
            static std::optional<lsp::CodeAction> generateFunctionStub(const lsp::Diagnostic& diag, const CodeActionContext& context);

            // Inserts text at a specific position (usually the end of the diagnostic range)
            static lsp::CodeAction createInsertFix(
                const std::string& title, 
                const lsp::DocumentUri& uri, 
                const lsp::Position& position, 
                const std::string& textToInsert, 
                const lsp::Diagnostic& diag);

            // Replaces text across an entire range (used for deletions or swaps)
            static lsp::CodeAction createReplaceFix(
                const std::string& title, 
                const lsp::DocumentUri& uri, 
                const lsp::Range& range, 
                const std::string& replacementText, 
                const lsp::Diagnostic& diag);
            };

} // namespace gdshader_lsp

#endif // CODE_ACTION_HANDLER_HPP