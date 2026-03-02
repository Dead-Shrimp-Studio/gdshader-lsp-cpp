#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"
#include "gdshader/parser/parser.hpp"

#include "gdshader/builtins.hpp"

namespace gdshader_lsp {

struct AnalysisResult 
{
    SymbolTable symbols;
    TypeRegistry types;
    std::vector<Diagnostic> diagnostics;
    std::vector<RawToken> tokens;
};

class SemanticAnalyzer 
{

private:

    SymbolTable symbols;
    TypeRegistry typeRegistry;

    std::vector<Diagnostic> diagnostics;
    std::vector<RawToken> tokens;

    // State
    std::unordered_set<std::string> processedFiles;
    std::string currentFilePath;

    void registerGlobalFunctions();

public:

    /**
     * @brief The main function for semantic anlysis. Return an AnalysisResult object containing a symbol table, type registry and diagnostic message array.
     * 
     * @param ast 
     * @return AnalysisResult 
     */
    AnalysisResult analyze(const ProgramNode* ast);
    
    void setFilePath(const std::string& path) { currentFilePath = path; }

};

}

#endif