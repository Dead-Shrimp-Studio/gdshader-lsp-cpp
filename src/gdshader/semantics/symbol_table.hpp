#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

#include "gdshader/semantics/types.hpp"

namespace gdshader_lsp {

enum class SymbolType 
{
    Variable,
    Uniform,
    Varying,
    Function,
    Struct,
    Builtin
};

enum class Mutability {
    Mutable,
    ReadOnly,
    WriteOnly
};

struct Symbol 
{
    std::string name;
    
    TypePtr type;
    std::vector<TypePtr> parameterTypes;
    
    SymbolType category;

    int line;
    int column;

    std::string doc_string;
    std::string hint; 
    std::vector<std::string> hintArgs; // e.g. hint_range(0, 1)

    struct Usage {
        int line;
        int column;
    };

    mutable std::vector<Usage> usages;
    bool is_definition = false;
    Mutability mutability = Mutability::Mutable;
};

struct Scope 
{
    Scope* parent = nullptr;
    std::vector<std::unique_ptr<Scope>> children;
    
    std::unordered_map<std::string, std::vector<Symbol>> symbols;
    
    // Range this scope covers (0-based)
    int startLine = 0;
    int endLine = 0;

    Scope(Scope* p = nullptr) : parent(p) {}

    bool has_children() {
        return !children.empty();
    }
};

class SymbolTable {

public:

    SymbolTable();

    // Scope Management
    void pushScope(int startLine);
    void popScope(int endLine);

    bool add(const Symbol& symbol);

    /**
     * @brief Adds a usage notifiies to the appropiate symbol, in the current scope.
     * 
     * @param sym Pointer to the symbol 
     * @param line Line
     * @param col Column
     */
    void addReference(const Symbol* sym, int line, int col);

    /**
     * @brief With overloading, this is mainly used for local variable lookup, where we retrieve the first symbol matched.
     * @param name 
     * @return const Symbol* 
     */
    const Symbol* lookup(const std::string& name, const int depth = 0) const;

    /**
     * @brief Returns all symbols that match the symbol name across ALL scopes.
     * @param name 
     * @return const std::vector<Symbol*> 
     */
    const std::vector<Symbol*> lookup_all(const std::string& name) const;

    std::vector<const Symbol*> lookupFunctions(const std::string& name) const;

    const Scope* findScopeAt(int line) const;
    const Symbol* lookupAt(const std::string& name, int line) const;
    
    /**
     * @brief Assembles a vector of all visible symbols relative to the current scope. Traverses upwards.
     * @param scope 
     * @return std::vector<Symbol> 
     */
    std::vector<Symbol> getVisibleSymbolsAt(int line) const;

    /**
     * @brief Get the Global symbol map from the current ShaderUnit. this is for file linking.
     * @return const std::unordered_map<std::string, std::vector<Symbol>>& 
     */
    const std::unordered_map<std::string, std::vector<Symbol>>& getGlobals() const 
    {
        return root->symbols;
    }

    const std::vector<Symbol> getAllSymbols();

private:
   
    std::unique_ptr<Scope> root = nullptr;
    Scope* current;

};

}

#endif