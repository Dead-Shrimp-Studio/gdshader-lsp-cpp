
#ifndef GDSHADER_SERVER_TYPES_HPP
#define GDSHADER_SERVER_TYPES_HPP

#include "gdshader/ast/ast.h"
#include "gdshader/diagnostics.hpp"
#include "gdshader/semantics/types.hpp"
#include "gdshader/semantics/type_registry.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <cstdint>

namespace gdshader_lsp
{

class SymbolTable;
    
struct ShaderUnit {

    std::string path;
    std::string source_code;

    // Document versioning and thread synchronization.
    // contentVersion is bumped for every accepted edit (didOpen/didChange).
    // compiledVersion is set to the contentVersion whose text ast/symbols reflect.
    // A reader blocks until compiledVersion >= contentVersion before reading ast.
    std::uint64_t contentVersion  = 0;
    std::uint64_t compiledVersion = 0;
    std::condition_variable compiledCV;

    std::unordered_set<std::string> defines;

    std::unique_ptr<ProgramNode> ast;
    std::shared_ptr<SymbolTable> symbols;
    TypeRegistry types;

    std::vector<std::string> includedPaths;
    std::vector<std::string> importedBy;

    std::vector<Diagnostic> diagnostics;
    std::vector<RawToken> tokens;

    // Snychronization primitive for locking the ast and symbols
    std::mutex unitMutex;
};

} // namespace gdshader_lsp


#endif // GDSHADER_SERVER_TYPES_HPP