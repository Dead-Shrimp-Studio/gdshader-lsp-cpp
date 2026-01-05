
#include "gdshader/semantics/symbol_table.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

namespace { // Anonymous namespace for local helper
    bool signaturesMatch(const std::vector<TypePtr>& a, const std::vector<TypePtr>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            // Dereference pointers to compare the actual Type objects
            if (*a[i] != *b[i]) return false;
        }
        return true;
    }
}

SymbolTable::SymbolTable() 
{
    SPDLOG_TRACE("[SymTable] Initializing Global Scope.");
    root = std::make_unique<Scope>(nullptr);
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
    auto& store = current->symbols[symbol.name];
    
    if (!store.empty()) 
    {
        const Symbol& existing = store[0];
        bool isFunc = (symbol.category == SymbolType::Function || symbol.category == SymbolType::Builtin);
        bool existingIsFunc = (store[0].category == SymbolType::Function || store[0].category == SymbolType::Builtin);

        // Rule 1: Collision Check
        // If either is NOT a function/builtin, it's a variable collision.
        if (!isFunc || !existingIsFunc) {

            if (existing.category == SymbolType::Builtin && symbol.category == SymbolType::Builtin) {
                return true; // Silent success: It's the same built-in.
            }

            SPDLOG_WARN("[SymTable] Redefinition Error: '{}' already exists in this scope.", symbol.name);
            return false; // Redefinition Error
        }

        // Rule 2: Function Overloading
        // We must check if a function with the SAME signature already exists.
        for (auto& existing : store) {
            if (signaturesMatch(existing.parameterTypes, symbol.parameterTypes)) 
            {
                // SCENARIO A: Two Definitions (ERROR)
                // "void foo() {}" AND "void foo() {}" -> BAD
                if (existing.is_definition && symbol.is_definition) {
                    SPDLOG_ERROR("[SymTable] Error: Body of function '{}' already defined.", symbol.name);
                    return false;
                }

                // SCENARIO B: Declaration followed by Definition (UPGRADE)
                // "void foo();" AND "void foo() {}" -> OK (Upgrade the symbol)
                if (!existing.is_definition && symbol.is_definition) {
                    SPDLOG_TRACE("[SymTable] Upgrading declaration of '{}' to definition.", symbol.name);
                    
                    // We replace the existing prototype with the full definition
                    // (You might want to preserve the 'usages' from the old symbol)
                    auto oldUsages = existing.usages;
                    existing = symbol; 
                    existing.usages = oldUsages; // Keep references pointing to it valid
                    return true;
                }

                // SCENARIO C: Definition followed by Declaration (REDUNDANT)
                // "void foo() {}" AND "void foo();" -> OK (Ignore new)
                if (existing.is_definition && !symbol.is_definition) {
                    return true;
                }
                
                // SCENARIO D: Declaration followed by Declaration (IDEMPOTENT)
                // "void foo();" AND "void foo();" -> OK (Ignore new)
                return true;
            }
        }
        SPDLOG_TRACE("[SymTable] Added Overload: '{}'", symbol.name);
    } else {
        SPDLOG_TRACE("[SymTable] Added Symbol: '{}' ({})", symbol.name, (int)symbol.category);
    }

    // Safe to add
    store.push_back(symbol);
    return true;
}

void gdshader_lsp::SymbolTable::addReference(const Symbol* sym, int line, int col)
{
    // Note: We cast away constness because 'lookup' returns const, 
    // but we are in the analysis phase populating data.
    if (sym) {
        const_cast<Symbol*>(sym)->usages.push_back({line, col});
    }
}

const Symbol* SymbolTable::lookup(const std::string& name) const 
{
    Scope* walker = current;
    int depth = 0;
    while (walker) {
        auto it = walker->symbols.find(name);
        if (it != walker->symbols.end() && !it->second.empty()) {
            // Return the first one found.
            // For variables, this is the only one. 
            // For functions, this returns one of the overloads (usually the first defined).
            return &it->second[0];
        }
        walker = walker->parent;
        depth++;
    }
    SPDLOG_TRACE("[SymTable] Lookup Failed: '{}' (checked {} scopes)", name, depth);
    return nullptr;
}

std::vector<const Symbol*> SymbolTable::lookupFunctions(const std::string& name) const 
{
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
    const Scope* candidate = root.get();
    
    // Drill down as deep as possible
    while (true) {
        bool foundChild = false;
        for (const auto& child : candidate->children) {
            // Check if line is inside child's range
            // Note: We might need precise column checks later, but line is usually enough
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

const Symbol* SymbolTable::lookupAt(const std::string& name, int line) const {
    const Scope* searchScope = findScopeAt(line);
    
    // Walk up the scope tree
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

namespace {
    // Helper to recursively collect symbols
    void collectSymbolsRecursive(const Scope* scope, std::vector<Symbol>& results) 
    {
        if (!scope) return;

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

const std::vector<Symbol> gdshader_lsp::SymbolTable::getAllSymbols()
{
    std::vector<Symbol> all_symbols;
    
    if (root) {
        collectSymbolsRecursive(root.get(), all_symbols);
    }
    
    return all_symbols;
}

}