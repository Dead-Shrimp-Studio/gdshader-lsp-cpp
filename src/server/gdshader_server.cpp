
#include "server/gdshader_server.hpp"

#include "gdshader/semantics/semantic_analyzer.hpp"
#include "server/project_manager.hpp"
#include "gdshader/ast/ast.h"

#include <unordered_set>

using namespace gdshader_lsp;

GdShaderServer::GdShaderServer(lsp::io::Socket s) : socket(std::move(s)), connection(socket), handler(connection)
{
    registerHandlers();
}

GdShaderServer::~GdShaderServer()
{
}

void GdShaderServer::run() 
{
    try {
        while (socket.isOpen()) {
            handler.processIncomingMessages();
        }
    } catch (const std::exception& e) {
        std::cerr << "Session ended: " << e.what() << std::endl;
    }
}

void GdShaderServer::registerHandlers() {
    
    // --- LIFECYCLE: INITIALIZE ---
    // The client sending its capabilities and asking for yours.
    handler.add<lsp::requests::Initialize>(
        [this](lsp::requests::Initialize::Params&& params) {
            
            if (!params.rootUri.isNull()) {
                std::string root = std::string(params.rootUri->path());
                #ifdef _WIN32
                    if (root.size() > 2 && root[0] == '/' && root[2] == ':' ) {
                        root = root.substr(1);
                    } 
                #endif

                ProjectManager::get_singleton()->setRootPath(root);
                std::cout << "Project Root set to: " << root << std::endl;
            }

            return lsp::requests::Initialize::Result{
                .capabilities = {
                    .textDocumentSync = lsp::TextDocumentSyncOptions{
                        .openClose = true,
                        .change = lsp::TextDocumentSyncKind::Full 
                    },
                    .completionProvider = lsp::CompletionOptions{
                        .triggerCharacters = std::vector<std::string>{".", ":"} 
                    },
                    .semanticTokensProvider = lsp::SemanticTokensOptions {
                        .legend = lsp::SemanticTokensLegend {
                            .tokenTypes = {
                                "variable",     // 0
                                "function",     // 1
                                "struct",       // 2
                                "macro",        // 3
                                "type",         // 4
                                "parameter",    // 5
                                "property"      // 6 (members)
                            },
                            .tokenModifiers = {
                                "declaration",  // 1
                                "readonly",     // 2
                                "static"        // 4
                            }
                        },
                        .full = true
                    }
                },
                .serverInfo = lsp::InitializeResultServerInfo{
                    .name = "gdshader-lsp",
                    .version = "0.1.0"
                }
            };
        }
    );

    // --- SYNCHRONIZATION: DID OPEN ---
    handler.add<lsp::notifications::TextDocument_DidOpen>(
        [this](lsp::notifications::TextDocument_DidOpen::Params&& params) {
            std::cout << "Opened document " << std::string(params.textDocument.uri.path()) << "..." << std::endl;
            compileAndPublish(params.textDocument.uri, params.textDocument.text);
        }
    );

    // --- SYNCHRONIZATION: DID CHANGE ---
    handler.add<lsp::notifications::TextDocument_DidChange>(
        [this](lsp::notifications::TextDocument_DidChange::Params&& params) {
            
            // Safety check: The vector should strictly have 1 element in Full Sync,
            // but it is good practice to check.
            if (params.contentChanges.empty() || params.contentChanges.size() > 1) return;

            auto uri = params.textDocument.uri;
            const auto& changeEvent = params.contentChanges.back();

            std::visit([this, &uri](auto&& change) {
                compileAndPublish(uri, change.text);
            }, changeEvent);
        }
    );

    // --- SYNCHRONIZATION: DID CLOSE ---
    handler.add<lsp::notifications::TextDocument_DidClose>(
        [this](lsp::notifications::TextDocument_DidClose::Params&& params) {
            lsp::DocumentUri uri = params.textDocument.uri;
        }
    );

    // --- FEATURE: HOVER ---
    handler.add<lsp::requests::TextDocument_Hover>(
        [this](lsp::requests::TextDocument_Hover::Params&& params) -> lsp::requests::TextDocument_Hover::Result
        {
            lsp::Hover hover;
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            if (!su->symbols) return hover;

            int line = params.position.line;
            int col = params.position.character;

            std::string word = getWordAtPosition(su->source_code, line, col);
            if (word.empty()) return hover;

            bool isMember = (col > 0 && getWordBeforeDot(su->source_code, col) != ""); 

            if (isMember) {
                // 1. Find the base variable (e.g. "my_instance" from "my_instance.test")
                std::string baseName = getWordBeforeDot(su->source_code, col); // You need to implement/expose this helper
                // 2. Look up base
                const Symbol* baseSym = su->symbols->lookupAt(baseName, line);
                if (baseSym && baseSym->type) {
                    // 3. Look up member in the base type
                    TypePtr memberT = su->types.getMemberType(baseSym->type, word);
                    if (memberT->kind != TypeKind::UNKNOWN) {
                        std::string content = "**" + baseSym->name + "**\n\n";
                        content += "Type: `" + baseSym->type->toString() + "`\n";
                        if (!baseSym->doc_string.empty()) {
                            content += "\n" + baseSym->doc_string;
                        }
                        
                        hover.contents = {
                            lsp::MarkupContent {
                                .kind = lsp::MarkupKind::Markdown,
                                .value = content
                            }
                        };
                    }
                }
            } else {

                const Symbol* sym = su->symbols->lookupAt(word, line);

                if (sym) {
                    std::string content = "**" + sym->name + "**\n\n";
                    content += "Type: `" + sym->type->toString() + "`\n";
                    if (!sym->doc_string.empty()) {
                        content += "\n" + sym->doc_string;
                    }
                    
                    hover.contents = {
                        lsp::MarkupContent {
                            .kind = lsp::MarkupKind::Markdown,
                            .value = content
                        }
                    };
                }
            }
            return hover;
        }
    );

    // --- FEATURE: COMPLETION ---
    handler.add<lsp::requests::TextDocument_Completion>(
        [this](lsp::requests::TextDocument_Completion::Params&& params) -> lsp::requests::TextDocument_Completion::Result
        {            
            lsp::CompletionList result;
            result.isIncomplete = false;

            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            if (!su->symbols) return result;

            int line = params.position.line;
            int col = params.position.character;

            // 1. Get context (Line content)
            std::string lineText = getLine(su->source_code, line);
            if (col > (int)lineText.length()) col = lineText.length();
            
            // Check trigger char
            bool isDotTrigger = false;
            if (col > 0 && lineText[col-1] == '.') isDotTrigger = true;

            // --- CASE A: DOT COMPLETION ---
            if (isDotTrigger) 
            {
                std::string varName = getWordBeforeDot(lineText, col-1);
                const Symbol* sym = su->symbols->lookupAt(varName, line);

                if (sym && sym->type) {
                    TypePtr t = sym->type;
                    
                    // 1. Vector Swizzling (Basic)
                    if (t->kind == TypeKind::VECTOR) {
                        std::vector<std::string> swizzles = {"x", "y", "z", "w", "r", "g", "b", "a"};
                        for(int i=0; i<t->componentCount * 2; ++i) { // Crude limit to valid comps
                            if (i >= (int)swizzles.size()) break;
                             result.items.push_back(lsp::CompletionItem{
                                .label = swizzles[i],
                                .kind = lsp::CompletionItemKind::Field,
                                .detail = "float"
                            });
                        }
                    }
                    // 2. Struct Members
                    else if (t->kind == TypeKind::STRUCT) {
                        for(const auto& member : t->members) {
                            result.items.push_back(lsp::CompletionItem{
                                .label = member.first,
                                .kind = lsp::CompletionItemKind::Field,
                                .detail = member.second->toString()
                            });
                        }
                    }
                }
                return result;
            }

            std::vector<Symbol> visible = su->symbols->getVisibleSymbolsAt(line);
            std::unordered_set<std::string> seen_functions; 

            for (const auto& s : visible) {
                
                // DEDUPLICATION CHECK
                if (s.category == SymbolType::Function || s.category == SymbolType::Builtin) {
                    if (seen_functions.count(s.name)) {
                        continue; // We already added 'max', don't add 'max' again
                    }
                    seen_functions.insert(s.name);
                }

                lsp::CompletionItem item;
                item.label = s.name;
                
                if (s.category == SymbolType::Function || s.category == SymbolType::Builtin) {
                    item.kind = lsp::CompletionItemKind::Function;
                    
                    // Indicate overloading in the detail text ?
                    item.detail = s.type->toString(); 
                    
                    item.insertTextFormat = lsp::InsertTextFormat::Snippet;
                    std::string insertionText = s.name + "(";
                    
                    for(size_t i=0; i<s.parameterTypes.size(); ++i) {
                        if (i > 0) insertionText += ", ";
                        insertionText += "${" + std::to_string(i+1) + ":" + s.parameterTypes[i]->toString() + "}";
                    }
                    insertionText += ")";
                    item.insertText = insertionText;
                } 
                else if (s.category == SymbolType::Struct) {
                    item.kind = lsp::CompletionItemKind::Struct;
                    item.insertText = s.name; 
                }
                else {
                    item.kind = lsp::CompletionItemKind::Variable;
                    item.detail = s.type->toString();
                    item.insertText = s.name;
                }

                result.items.push_back(item);
            }

            return result;
        }
    );
    
    // --- FEATURE: DEFINITION ---
    handler.add<lsp::requests::TextDocument_Definition>(
        [this](lsp::requests::TextDocument_Definition::Params&& params) -> lsp::requests::TextDocument_Definition::Result
        {
            lsp::Location loc;
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            if (!su->symbols) return loc;
            int line = params.position.line;
            int col = params.position.character;

            std::string word = getWordAtPosition(su->source_code, line, col);
            if (word.empty()) return nullptr;

            const Symbol* sym = su->symbols->lookupAt(word, line);

            // If found and it's not a built-in (line -1)
            if (sym && sym->line >= 0) {
                loc.uri = params.textDocument.uri;
                
                loc.range.start = lsp::Position{(unsigned)sym->line, (unsigned)sym->column};
                loc.range.end   = lsp::Position{(unsigned)sym->line, (unsigned)sym->column + (unsigned)sym->name.length()};
                
                return loc;
            }

            return nullptr;
        }
    );

    // --- FEATURE: SIGNATURE HELP ---
    handler.add<lsp::requests::TextDocument_SignatureHelp>(
        [this](lsp::requests::TextDocument_SignatureHelp::Params&& params) -> lsp::requests::TextDocument_SignatureHelp::Result
        {
            lsp::SignatureHelp help;
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            if (!su->symbols) return help;

            int line = params.position.line;
            int col = params.position.character;

            // 1. Find Context (Name + Arg Index)
            auto [funcName, argIndex] = getFunctionCallContext(su->source_code, line, col);
            
            if (funcName.empty()) return nullptr;

            // 2. Lookup Overloads
            std::vector<const Symbol*> overloads = su->symbols->lookupFunctions(funcName);
            
            if (overloads.empty()) return nullptr;

            // 3. Construct LSP Result
            help.activeParameter = argIndex;
            help.activeSignature = 0; // Default to first, or try to match best fit based on arg count

            // Optional: Try to set activeSignature to the one with matching arg count
            // (If multiple have same count, we just pick the first one)
            for (size_t i = 0; i < overloads.size(); ++i) {
                if ((int)overloads[i]->parameterTypes.size() > argIndex) {
                    help.activeSignature = i;
                    // Don't break, sometimes we want the *closest* fit, but this is a simple heuristic
                    // For exact match we'd need to analyze types of previous args, which is hard here.
                    break; 
                }
            }

            for (const auto* sym : overloads) {
                lsp::SignatureInformation sigInfo;
                
                // Label: "float mix(float a, float b, float t)"
                std::string label = sym->type->toString() + " " + sym->name + "(";
                
                std::vector<lsp::ParameterInformation> paramsInfo;
                
                for (size_t i = 0; i < sym->parameterTypes.size(); ++i) {
                    TypePtr pType = sym->parameterTypes[i];
                    
                    // Construct param label: "float arg1"
                    // Since Builtins don't store arg names in SymbolTable (yet?), we generate generic ones
                    // or use the type as the label.
                    std::string paramLabel = pType->toString(); 
                    
                    // Add comma for display label
                    if (i > 0) label += ", ";
                    
                    // We need to record the start/end index of this parameter within the 'label' string
                    // so the client can highlight it. 
                    // Simple approach: Store the string representation.
                    lsp::ParameterInformation pInfo;
                    pInfo.label = paramLabel; 
                    paramsInfo.push_back(pInfo);
                    
                    label += paramLabel;
                }
                label += ")";

                sigInfo.label = label;
                if (!sym->doc_string.empty()) {
                    sigInfo.documentation = lsp::MarkupContent{
                        .kind = lsp::MarkupKind::Markdown,
                        .value = sym->doc_string
                    };
                }
                sigInfo.parameters = paramsInfo;
                
                help.signatures.push_back(sigInfo);
            }

            return help;
        }
    );

    // --- FEATURE: DOCUMENT SYMBOL ---
    handler.add<lsp::requests::TextDocument_DocumentSymbol>(
        [this](lsp::requests::TextDocument_DocumentSymbol::Params&& params) -> lsp::requests::TextDocument_DocumentSymbol::Result
        {
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            if (!su->symbols) return nullptr;
            
            // Generate symbol tree from the AST
            return getDocumentSymbols(su->ast.get());
        }
    );

    // --- FEATURE: SEMANTIC TOKENS ---
    handler.add<lsp::requests::TextDocument_SemanticTokens_Full>(
        [this](lsp::requests::TextDocument_SemanticTokens_Full::Params&& params) -> lsp::requests::TextDocument_SemanticTokens_Full::Result 
        {
            lsp::SemanticTokens result;
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);
            if (su->tokens.empty()) return result;

            result.data = encodeTokens(su->tokens); 
            return result;
        }
    );

    // --- FEATURE: DOCUMENT HIGHLIGHT ---
    handler.add<lsp::requests::TextDocument_DocumentHighlight>(
        [this](lsp::requests::TextDocument_DocumentHighlight::Params&& params) -> lsp::requests::TextDocument_DocumentHighlight::Result 
        {
            std::vector<lsp::DocumentHighlight> result;
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);
            if (!su->symbols) return result;

            int line = params.position.line;
            int column = params.position.character;

            std::string name = getWordAtPosition(su->source_code, line, column);
            if (name.empty()) return result;

            const Symbol* sym = su->symbols->lookupAt(name, line);

            if (sym) {

                if (sym->line >= 0) {
                    result.push_back(lsp::DocumentHighlight{
                        .range = lsp::Range{
                            .start = { (unsigned)sym->line, (unsigned)sym->column },
                            .end   = { (unsigned)sym->line, (unsigned)(sym->column + sym->name.length()) }
                        },
                        .kind = lsp::DocumentHighlightKind::Text
                    });
                }

                for (const auto& usage : sym->usages) {
                    result.push_back(lsp::DocumentHighlight{
                        .range = lsp::Range{
                            .start = { (unsigned)usage.line, (unsigned)usage.column },
                            .end   = { (unsigned)usage.line, (unsigned)(usage.column + sym->name.length()) }
                        },
                        .kind = lsp::DocumentHighlightKind::Read
                    });
                }
            }

            return result;
        }
    );

    // --- LIFECYCLE: SHUTDOWN/EXIT ---
    handler.add<lsp::requests::Shutdown>([]() { return nullptr; });
    handler.add<lsp::notifications::Exit>([]() { exit(0); });
}

void gdshader_lsp::GdShaderServer::compileAndPublish(const lsp::DocumentUri& uri, const std::string &code)
{
    std::string path = std::string(uri.path());
    #ifdef _WIN32
    if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
    #endif

    auto pm = ProjectManager::get_singleton();
    pm->updateFile(path, code);

    auto su = pm->getUnit(path);

    SemanticAnalyzer analyzer;
    analyzer.setFilePath(path);

    // 1. Run Lexer & Parser
    Lexer lexer(code);
    Parser parser(lexer, path);
    
    auto ast = parser.parse();
    auto errors = parser.getDiagnostics();

    su->defines = parser.getDefines();

    if (ast) {
        auto result = analyzer.analyze(ast.get());
        
        su->ast = std::move(ast);
        su->symbols = std::make_shared<SymbolTable>(std::move(result.symbols));
        su->types   = std::move(result.types);
        su->tokens = result.tokens;

        auto semanticErrors = result.diagnostics;
        errors.insert(errors.end(), semanticErrors.begin(), semanticErrors.end());
    }

    su->diagnostics = errors;

    // 3. Convert Diagnostics
    std::vector<lsp::Diagnostic> lspDiagnostics;
    for (const auto& err : errors) {

        int len = (err.length > 0) ? err.length : 1;

        lsp::DiagnosticSeverity severity;
        if (err.level == DiagnosticLevel::Warning) {
            severity = lsp::DiagnosticSeverity::Warning;
        } else {
            severity = lsp::DiagnosticSeverity::Error;
        }

        lspDiagnostics.push_back(lsp::Diagnostic{
            .range = lsp::Range{
                .start = lsp::Position{(unsigned)err.line, (unsigned)err.column},
                .end   = lsp::Position{(unsigned)err.line, (unsigned)(err.column + len)}
            },
            .message = err.message,
            .severity = severity,
            .source = "gdshader"
        });
    }

    // 4. Send to Editor
    lsp::notifications::TextDocument_PublishDiagnostics::Params params;

    params.uri = uri;     
    params.diagnostics = lspDiagnostics;

    handler.sendNotification<lsp::notifications::TextDocument_PublishDiagnostics>(std::move(params));
}

//////////////////////////////////////////////////
// Helper
//////////////////////////////////////////////////

std::pair<std::string, int> gdshader_lsp::GdShaderServer::getFunctionCallContext(const std::string &source, int line, int col)
{
    std::string lineText = getLine(source, line);
    
    // Safety check
    if (col > (int)lineText.length()) col = lineText.length();

    int balance = 0; // Parenthesis balance
    int argIndex = 0;
    
    // Scan backwards from cursor
    for (int i = col - 1; i >= 0; i--) {
        char c = lineText[i];

        if (c == ')') {
            balance++;
        }
        else if (c == '(') {
            if (balance > 0) {
                balance--;
            } else {
                // Found the opening parenthesis for OUR function!
                // The word immediately before this index is the function name.
                return { getWordBeforeDot(lineText, i), argIndex };
            }
        }
        else if (c == ',' && balance == 0) {
            // Comma at the current level means we moved to the next argument
            argIndex++;
        }
    }

    return { "", -1 }; // Not inside a function call
}

std::string gdshader_lsp::GdShaderServer::getWordAtPosition(const std::string &source, int line, int col)
{
    size_t currentLine = 0;
    size_t start = 0;
    size_t end = source.find('\n');
    
    while (currentLine < (size_t)line && end != std::string::npos) {
        start = end + 1;
        end = source.find('\n', start);
        currentLine++;
    }
    
    if (currentLine != (size_t)line) return "";

    std::string lineText = source.substr(start, end - start);
    
    // 2. Expand outwards from col to find start/end of identifier
    if ((size_t)col >= lineText.size()) return "";

    // Check if cursor is on a valid identifier char
    auto isIdChar = [](char c) { return isalnum(c) || c == '_'; };
    
    if (!isIdChar(lineText[col])) return "";

    int wordStart = col;
    while (wordStart > 0 && isIdChar(lineText[wordStart - 1])) {
        wordStart--;
    }

    int wordEnd = col;
    while ((size_t)wordEnd < lineText.size() && isIdChar(lineText[wordEnd])) {
        wordEnd++;
    }

    return lineText.substr(wordStart, wordEnd - wordStart);
}

std::string gdshader_lsp::GdShaderServer::getWordBeforeDot(const std::string& lineText, int dotPos) 
{
    // Safety check
    if (dotPos <= 0 || (size_t)dotPos >= lineText.size()) return "";

    // Start looking immediately before the dot
    int i = dotPos - 1;

    // 1. Skip optional whitespace (handle "test . x")
    while (i >= 0 && std::isspace(lineText[i])) {
        i--;
    }

    if (i < 0) return ""; // Found nothing before the dot

    // 2. Mark the end of the identifier
    int end = i;

    // 3. Scan backwards capturing valid identifier chars (alnum + _)
    while (i >= 0) {
        char c = lineText[i];
        if (std::isalnum(c) || c == '_') {
            i--;
        } else {
            // Hit a non-identifier char (space, operator, etc.)
            break;
        }
    }

    // 'i' is now at the character *before* the word.
    int start = i + 1;
    if (start > end) return "";

    return lineText.substr(start, end - start + 1);
}

std::string gdshader_lsp::GdShaderServer::getLine(const std::string &source, int targetLine)
{
    if (source.empty()) return "";

    size_t start = 0;
    size_t end = source.find('\n');
    int currentLine = 0;

    // Iterate until we find the start of our target line
    while (end != std::string::npos && currentLine < targetLine) {
        start = end + 1;
        end = source.find('\n', start);
        currentLine++;
    }

    // If we reached EOF before the line index, return empty
    if (currentLine != targetLine) return "";

    // Extract the line
    size_t count = (end == std::string::npos) ? std::string::npos : (end - start);
    std::string line = source.substr(start, count);

    // Trim carriage return '\r' if present (Windows line endings)
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    return line;
}

lsp::DocumentSymbol GdShaderServer::createSymbol(const std::string& name, lsp::SymbolKind kind, int line, const std::string& detail, const std::vector<lsp::DocumentSymbol>& children) 
{
    lsp::DocumentSymbol sym;
    sym.name = name;
    sym.kind = kind;
    sym.detail = detail;

    // Range: For now, we select the whole line. 
    // Ideally, you'd add 'endLine' to your ASTNodes for precise block ranges.
    sym.range = lsp::Range{
        .start = lsp::Position{(unsigned)line, 0},
        .end = lsp::Position{(unsigned)line, 0}
    };
    sym.selectionRange = sym.range; // The text to highlight when clicked
    
    sym.children = children;
    return sym;
}

std::vector<u_int> gdshader_lsp::GdShaderServer::encodeTokens(std::vector<RawToken> &raw)
{
    std::sort(raw.begin(), raw.end());

    std::vector<u_int> encoded;
    u_int prevLine = 0;
    u_int prevCol = 0;

    for (const auto& t : raw) {
        u_int deltaLine = t.line - prevLine;
        u_int deltaCol = (deltaLine == 0) ? (t.col - prevCol) : t.col;

        encoded.push_back(deltaLine);
        encoded.push_back(deltaCol);
        encoded.push_back(t.length);
        encoded.push_back(t.type);
        encoded.push_back(t.modifiers);

        prevLine = t.line;
        prevCol = t.col;
    }

    return encoded;
}

std::vector<lsp::DocumentSymbol> GdShaderServer::getDocumentSymbols(const ASTNode* node) 
{
    std::vector<lsp::DocumentSymbol> symbols;
    if (!node) return symbols;

    // 1. PROGRAM ROOT
    if (auto p = dynamic_cast<const ProgramNode*>(node)) {
        for (const auto& child : p->nodes) {
            auto childSyms = getDocumentSymbols(child.get());
            symbols.insert(symbols.end(), childSyms.begin(), childSyms.end());
        }
    }
    // 2. FUNCTIONS
    else if (auto f = dynamic_cast<const FunctionNode*>(node)) {
        std::vector<lsp::DocumentSymbol> children;
        
        // Add Arguments
        for (const auto& arg : f->parameters) {
            children.push_back(createSymbol(arg->name, lsp::SymbolKind::Variable, f->range.startLine, arg->type->toString(), {}));
        }
        
        // Add Local Variables (Recurse into body)
        if (f->body) {
            auto bodySyms = getDocumentSymbols(f->body.get());
            children.insert(children.end(), bodySyms.begin(), bodySyms.end());
        }
        
        symbols.push_back(createSymbol(f->name, lsp::SymbolKind::Function, f->range.startLine, f->returnType->toString(), children));
    }
    // 3. STRUCTS
    else if (auto s = dynamic_cast<const StructNode*>(node)) {
        std::vector<lsp::DocumentSymbol> members;
        for (const auto& m : s->members) {
            members.push_back(createSymbol(m->name, lsp::SymbolKind::Field, s->range.startLine, m->type->toString(), {}));
        }
        symbols.push_back(createSymbol(s->name, lsp::SymbolKind::Struct, s->range.startLine, "", members));
    }
    // 4. UNIFORMS
    else if (auto u = dynamic_cast<const UniformNode*>(node)) {
        symbols.push_back(createSymbol(u->name, lsp::SymbolKind::Constant, u->range.startLine, u->type->toString(), {}));
    }
    // 5. VARYINGS
    else if (auto v = dynamic_cast<const VaryingNode*>(node)) {
        symbols.push_back(createSymbol(v->name, lsp::SymbolKind::Variable, v->range.startLine, v->type->toString(), {}));
    }
    // 6. CONSTS
    else if (auto c = dynamic_cast<const ConstNode*>(node)) {
        symbols.push_back(createSymbol(c->name, lsp::SymbolKind::Constant, c->range.startLine, c->type->toString(), {}));
    }
    // 7. BLOCKS (Pass-through to find locals)
    else if (auto b = dynamic_cast<const BlockNode*>(node)) {
        for (const auto& stmt : b->statements) {
            auto stmtSyms = getDocumentSymbols(stmt.get());
            symbols.insert(symbols.end(), stmtSyms.begin(), stmtSyms.end());
        }
    }
    // 8. LOCAL VARIABLES
    else if (auto v = dynamic_cast<const VariableDeclNode*>(node)) {
        symbols.push_back(createSymbol(v->name, lsp::SymbolKind::Variable, v->range.startLine, v->type->toString(), {}));
    }

    // Note: We intentionally skip If/While/For nodes here to keep the Outline clean.
    // If you want to show variables declared inside 'if' blocks, add handlers for them here.

    return symbols;
}