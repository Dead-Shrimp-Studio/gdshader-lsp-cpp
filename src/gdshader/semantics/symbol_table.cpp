
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/types.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

namespace 
{
    bool signaturesMatch(const std::vector<TypePtr>& a, const std::vector<TypePtr>& b) 
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            // Dereference pointers to compare the actual Type objects
            if (*a[i] != *b[i]) return false;
        }
        return true;
    }

    void collectSymbolsRecursive(const Scope* scope, std::vector<Symbol>& results) 
    {
        GDSHADER_RETURN_IF(!scope, "Attempted to collect symbols from a null scope.");

        for (const auto& pair : scope->symbols) {
            for (const auto& sym : pair.second) {
                results.push_back(sym);
            }
        }

        for (const auto& child : scope->children) {
            collectSymbolsRecursive(child.get(), results);
        }
    }
}

SymbolTable::SymbolTable() 
{
    root = std::make_unique<Scope>(nullptr);
    GDSHADER_ALWAYS_ASSERT(root != nullptr, "Failed to allocate memory for root scope.");
    current = root.get();
}

void SymbolTable::pushScope(int startLine) 
{
    SPDLOG_TRACE("[SymTable] Push Scope (Start Line: {})", startLine);

    auto newScope = std::make_unique<Scope>(current);
    newScope->startLine = startLine;
    
    // Store raw pointer before moving ownership
    Scope* next = newScope.get();
    current->children.push_back(std::move(newScope));
    current = next;
}

void SymbolTable::popScope(int endLine) 
{
    if (current->parent) {
        SPDLOG_TRACE("[SymTable] Pop Scope (End Line: {})", endLine);
        current->endLine = endLine;
        current = current->parent;
    } else {
        SPDLOG_ERROR("[SymTable] Attempted to pop Root Scope!");
    }
}

bool SymbolTable::add(const Symbol& symbol) 
{
    GDSHADER_RETURN_VAL_IF(symbol.name.empty(), false, "Attempted to add a symbol with an empty name.");
    GDSHADER_ASSERT(current != nullptr, "Current scope is null while adding symbol '{}'", symbol.name);

    if (!current->symbols.contains(symbol.name))
    {
        current->symbols[symbol.name] = std::vector<Symbol>();
        SPDLOG_TRACE("[SymTable]: Created store for symbol with name '{}'.", symbol.name);
    }

    std::vector<Symbol>& store = current->symbols[symbol.name];

    if (!store.empty()) 
    {
        for (Symbol& storeSymbol : store)
        {
            GDSHADER_ASSERT(symbol.name == storeSymbol.name, 
                "Symbol store broken. Expected '{}', found '{}'", symbol.name, storeSymbol.name);

            if (symbol.is_function_definition && storeSymbol.is_function_definition)
            {
                GDSHADER_RETURN_VAL_IF(
                    signaturesMatch(symbol.parameterTypes, storeSymbol.parameterTypes), 
                    false, 
                    "[SymTable] Error: Body of function '{}' already defined.", symbol.name
                );

                SPDLOG_TRACE("[SymTable] Added Overload for '{}'.", symbol.name);

            } else if (symbol.is_function_definition && !storeSymbol.is_function_definition){
                auto oldUsages = storeSymbol.references;
                storeSymbol = symbol; 
                storeSymbol.references = oldUsages;
            }
        }
    }
    store.push_back(symbol);
    return true;
}

void SymbolTable::addReference(Symbol* sym, const Range& range)
{
    GDSHADER_RETURN_IF(!sym, "Attempted to add reference to a null symbol pointer at line {}", range.startLine);
    sym->references.push_back(range);
}

const Symbol* SymbolTable::lookup(const std::string& name) const 
{
    GDSHADER_WARN_IF(name.empty(), "Looking up a symbol with an empty name.");

    Scope* walker = current;
    int rdepth = 0;
    while (walker) {
        auto it = walker->symbols.find(name);
        if (it != walker->symbols.end() && !it->second.empty()) {
            return &it->second[0];
        }
        
        rdepth++;
        
        walker = walker->parent;
    }
    SPDLOG_DEBUG("Lookup Failed: '{}' with depth {}", name, rdepth);
    return nullptr;
}

const std::vector<Symbol*> SymbolTable::lookup_all(const std::string &name) const
{
    GDSHADER_WARN_IF(name.empty(), "lookup_all called with an empty name.");

    std::vector<Symbol*> symbols;
    Scope* walker = current;
    while (walker) {
        auto it = walker->symbols.find(name);
        if (it != walker->symbols.end() && !it->second.empty()) {
            symbols.push_back(&it->second[0]);
        }
        walker = walker->parent;
    }
    return symbols;
}

std::vector<const Symbol*> SymbolTable::lookupFunctions(const std::string& name) const 
{
    GDSHADER_WARN_IF(name.empty(), "lookupFunctions called with an empty name.");

    Scope* walker = current;
    while (walker) {
        auto it = walker->symbols.find(name);
        if (it != walker->symbols.end() && !it->second.empty()) {
            // Found the scope where this function is defined. 
            // Return pointers to ALL overloads in this scope.
            std::vector<const Symbol*> results;
            results.reserve(it->second.size());
            
            for (const auto& s : it->second) {
                results.push_back(&s);
            }
            SPDLOG_TRACE("[SymTable] Found {} overloads for '{}'", results.size(), name);
            return results;
        }
        walker = walker->parent;
    }
    return {};
}

const Scope* SymbolTable::findScopeAt(int line) const 
{
    GDSHADER_ASSERT(root != nullptr, "Root scope is null during findScopeAt");
    const Scope* candidate = root.get();
    
    // Drill down as deep as possible
    while (true) {
        bool foundChild = false;
        for (const auto& child : candidate->children) {
            GDSHADER_ASSERT(child != nullptr, "Encountered null child scope!");
            if (line >= child->startLine && line <= child->endLine) {
                candidate = child.get();
                foundChild = true;
                break;
            }
        }
        if (!foundChild) break;
    }
    return candidate;
}

const Symbol* SymbolTable::lookupAt(const std::string& name, int line) const 
{
    GDSHADER_WARN_IF(name.empty(), "lookupAt called with empty name at line {}", line);
    const Scope* searchScope = findScopeAt(line);

    while (searchScope) {
        auto it = searchScope->symbols.find(name);
        if (it != searchScope->symbols.end() && !it->second.empty()) {
            return &it->second[0];
        }
        searchScope = searchScope->parent;
    }
    return nullptr;
}

std::vector<Symbol> SymbolTable::getVisibleSymbolsAt(int line) const 
{
    std::vector<Symbol> results;
    const Scope* walker = findScopeAt(line);
    
    // We use a set to avoid duplicates when shadowing variables
    std::unordered_map<std::string, bool> seen;

    while (walker) {
        for (const auto& pair : walker->symbols) {
            if (!seen.count(pair.first)) {
                // Add all overloads
                for(const auto& s : pair.second) {
                    results.push_back(s);
                }
                seen[pair.first] = true;
            }
        }
        walker = walker->parent;
    }
    return results;
}

const std::vector<Symbol> SymbolTable::getAllSymbols()
{
    GDSHADER_RETURN_VAL_IF(!root, {}, "Root scope is null, cannot get all symbols");
    std::vector<Symbol> all_symbols;
    
    if (root) {
        collectSymbolsRecursive(root.get(), all_symbols);
    }
    
    return all_symbols;
}

Symbol SymbolTable::createSymbol(const std::string& name, TypePtr type, SymbolType category, const Range& nodeRange, 
                                            Mutability mutability, TypePtr returnType, const std::vector<TypePtr>& paramterTypes, const std::vector<std::string>& paramterNames, bool is_func_def) 
{
    GDSHADER_WARN_IF(name.empty(), "Creating a symbol with an empty name!");

    Symbol s;
    s.name = name;
    s.type = type;
    s.category = category;
    s.mutability = mutability;
    
    s.definition = nodeRange;
    s.definition.startLine = nodeRange.startLine;
    s.definition.endLine = nodeRange.endLine;
    
    s.returnType = returnType;
    s.parameterTypes = paramterTypes;
    s.parameterNames = paramterNames;
    s.is_function_definition = is_func_def;
    return s;
}

}