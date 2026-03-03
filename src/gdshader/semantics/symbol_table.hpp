#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

#include "gdshader/semantics/types.hpp"
#include "gdshader/semantics/symbol.hpp"

namespace gdshader_lsp 
{

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
     * @brief Adds a usage notification to the appropriate symbol, in the current scope.
     * * @param sym Pointer to the symbol 
     * @param range The full range of the reference
     */
    void addReference(Symbol* sym, const Range& range);

    /**
     * @brief With overloading, this is mainly used for local variable lookup, where we retrieve the first symbol matched.
     * @param name 
     * @return const Symbol* 
     */
    const Symbol* lookup(const std::string& name) const;

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
    Symbol createSymbol(const std::string& name, TypePtr type, SymbolType category, const Range& nodeRange, 
        Mutability mutability = Mutability::Mutable, TypePtr returnType = nullptr, const std::vector<TypePtr>& paramterTypes = {}, const std::vector<std::string>& paramterNames = {}, bool is_func_def = true);

private:
   
    std::unique_ptr<Scope> root = nullptr;
    Scope* current;

};

}

#endif