
#ifndef GDSHADER_SERVER_HPP 
#define GDSHADER_SERVER_HPP

#include "gdshader/parser/parser.hpp"
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"

#include <lsp/io/socket.h>
#include <lsp/connection.h>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>
#include <lsp/types.h>

#include <iostream>
#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <unordered_map>

namespace gdshader_lsp {

struct Document {
    std::string text;
    std::unique_ptr<ProgramNode> ast;
    SymbolTable symbols;
    TypeRegistry types;
};

// -------------------------------------------------------------------------
// SERVER SESSION
// Each client connection spawns an instance of this class.
// This is where compiler state lives.
// -------------------------------------------------------------------------

class GdShaderServer {

public:

    GdShaderServer(std::unique_ptr<lsp::Connection> conn);
    ~GdShaderServer();

    void run();

private:

    // Threading

    std::unique_ptr<lsp::Connection> connection;
    lsp::MessageHandler handler;
    
    std::atomic<bool> running{true};
    std::thread compilerThread;
    std::mutex debounceMutex;
    
    // Maps a file URI to the exact time it was last modified
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> dirtyFiles;

    void compilerLoop();

    void registerHandlers();
    void compileAndPublish(const lsp::DocumentUri& uri, const std::string& code);

    // Helper

    size_t positionToOffset(const std::string& text, int line, int character);
    void collectFoldingRanges(const ASTNode* node, std::vector<lsp::FoldingRange>& ranges);
    std::string getLine(const std::string& source, int targetLine);

    std::vector<lsp::DocumentSymbol> getDocumentSymbols(const ASTNode* node);
    lsp::DocumentSymbol createSymbol(const std::string& name, lsp::SymbolKind kind, int line, const std::string& detail, const std::vector<lsp::DocumentSymbol>& children);

};

}

#endif // GDSHADER_SERVER_HPP