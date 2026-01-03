
#ifndef GDSHADER_SERVER_HPP 
#define GDSHADER_SERVER_HPP

#include <iostream>
#include <thread>
#include <memory>
#include <vector>
#include <unordered_map>

#include <lsp/io/socket.h>
#include <lsp/connection.h>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>
#include <lsp/types.h>

#include "gdshader/parser/parser.hpp" // parser includes ast.h and lexer.h
#include "gdshader/semantics/symbol_table.hpp"
#include "gdshader/semantics/type_registry.hpp"

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
// This is where your compiler state lives.
// -------------------------------------------------------------------------

class GdShaderServer {

private:

    lsp::io::Socket socket;
    lsp::Connection connection;
    lsp::MessageHandler handler;

    void registerHandlers();
    void compileAndPublish(const lsp::DocumentUri& uri, const std::string& code);

    // Helper

    std::pair<std::string, int> getFunctionCallContext(const std::string& source, int line, int col);
    std::string getWordAtPosition(const std::string& source, int line, int col);
    std::string getWordBeforeDot(const std::string& lineText, int dotPos);
    std::string getLine(const std::string& source, int targetLine);

    std::vector<lsp::DocumentSymbol> getDocumentSymbols(const ASTNode* node);
    lsp::DocumentSymbol createSymbol(const std::string& name, lsp::SymbolKind kind, int line, const std::string& detail, const std::vector<lsp::DocumentSymbol>& children);

    std::vector<u_int> encodeTokens(std::vector<RawToken>& raw);

public:

    GdShaderServer(lsp::io::Socket s);
    ~GdShaderServer();

    void run();

};

}

#endif // GDSHADER_SERVER_HPP