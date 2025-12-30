
#include "server/project_manager.hpp"
#include "gdshader/lexer/lexer.h"
#include "gdshader/parser/parser.hpp"
#include "gdshader/semantics/semantic_analyzer.hpp"

#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace gdshader_lsp;

std::string ProjectManager::resolvePath(const std::string& currentPath, const std::string& includePath) 
{
    // 1. Handle Godot "res://" paths
    if (includePath.rfind("res://", 0) == 0) {
        std::string relative = includePath.substr(6);
        fs::path p = fs::path(rootPath) / relative;
        return p.lexically_normal().string();
    }

    // 2. Handle Relative paths ("../utils.gdshaderinc")
    fs::path currentDir = fs::path(currentPath).parent_path();
    fs::path p = currentDir / includePath;
    return p.lexically_normal().string();
}

std::string ProjectManager::loadSource(const std::string& path) 
{
    // Check if we have an in-memory version (dirty buffer from editor)
    if (units.count(path) && !units[path]->source_code.empty()) {
        return units[path]->source_code;
    }

    // Fallback to Disk
    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    return "";
}

// Update file content (called by didOpen/didChange)
void ProjectManager::updateFile(const std::string& uri, const std::string& code) 
{
    // Convert URI to path if necessary, or just use path key
    // assuming 'uri' here is the file system path
    if (units.find(uri) == units.end()) {
        units[uri] = std::make_shared<ShaderUnit>();
        units[uri]->path = uri;
    }
    units[uri]->source_code = code;
    // Invalidate AST so it gets re-parsed on next request
    units[uri]->ast = nullptr; 
    units[uri]->symbols = nullptr;
}

std::shared_ptr<ShaderUnit> ProjectManager::getUnit(const std::string& path) 
{
    if (units.find(path) == units.end()) {
        units[path] = std::make_shared<ShaderUnit>();
        units[path]->path = path;
    }
    return units[path];
}

std::shared_ptr<SymbolTable> ProjectManager::getExports(const std::string& path) 
{
    auto unit = getUnit(path);

    // If we already parsed and analyzed this file, return the cached exports
    if (unit->symbols) {
        return unit->symbols;
    }

    // CYCLE DETECTION
    if (std::find(includeStack.begin(), includeStack.end(), path) != includeStack.end()) {
        // Detected recursion (A -> B -> A). Stop here.
        return nullptr; 
    }
    includeStack.push_back(path);

    // 1. Load Source
    if (unit->source_code.empty()) {
        unit->source_code = loadSource(path);
    }

    // 2. Parse (if needed)
    if (!unit->ast && !unit->source_code.empty()) {
        Lexer lexer(unit->source_code);
        Parser parser(lexer, path);
        unit->ast = parser.parse();
        unit->defines = parser.getDefines();
    }

    // 3. Analyze (to generate Symbol Table)
    if (unit->ast) {

        SemanticAnalyzer analyzer; 
        analyzer.setFilePath(path); 
        
        AnalysisResult result = analyzer.analyze(unit->ast.get());
        unit->symbols = std::make_shared<SymbolTable>(std::move(result.symbols));
        unit->types = std::move(result.types);

    } else {
        // Create empty table if parsing failed
        unit->symbols = std::make_shared<SymbolTable>();
    }

    includeStack.pop_back();
    return unit->symbols;
}