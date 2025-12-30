
#ifndef GDSHADER_SERVER_TYPES_HPP
#define GDSHADER_SERVER_TYPES_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/diagnostics.hpp"
#include "gdshader/semantics/type_registry.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>

namespace gdshader_lsp
{

class SymbolTable;
    
struct ShaderUnit {

    std::string path;
    std::string source_code;
    int version = 0;

    std::unordered_set<std::string> defines;

    std::unique_ptr<ProgramNode> ast;
    std::shared_ptr<SymbolTable> symbols;
    TypeRegistry types;

    std::vector<std::string> includedPaths;
    std::vector<std::string> importedBy;

    std::vector<Diagnostic> diagnostics;
};

} // namespace gdshader_lsp


#endif // GDSHADER_SERVER_TYPES_HPP