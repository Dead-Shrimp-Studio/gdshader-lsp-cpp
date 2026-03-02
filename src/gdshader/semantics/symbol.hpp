#ifndef GSHADER_LSP_SYMBOL_HPP
#define GSHADER_LSP_SYMBOL_HPP

#include "gdshader/semantics/types.hpp"
#include "gdshader/diagnostics.hpp"

namespace gdshader_lsp
{
    enum class SymbolType 
    {
        Variable,
        Uniform,
        Varying,
        Function,
        Struct,
        Builtin
    };

    enum class Mutability 
    {
        Mutable,
        ReadOnly,
        WriteOnly
    };

    struct Symbol 
    {
        std::string name;
        
        TypePtr type;
        SymbolType category;
        Mutability mutability = Mutability::Mutable;

        // Positional data

        Range definition;
        mutable std::vector<Range> references;

        // Function related

        TypePtr returnType;
        std::vector<TypePtr> parameterTypes;
        std::vector<std::string> parameterNames;
        
        std::string doc_string;
        std::string hint; 
        std::vector<std::string> hintArgs; // e.g. hint_range(0, 1)+

        bool is_function_definition;

    };
}

#endif