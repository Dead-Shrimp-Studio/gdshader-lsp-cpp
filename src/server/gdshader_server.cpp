
#include "server/gdshader_server.hpp"

#include "gdshader/semantics/semantic_analyzer.hpp"
#include "gdshader/semantics/visitors/node_finder_visitor.hpp"
#include "gdshader/semantics/visitors/call_finder_visitor.hpp"
#include "gdshader/semantics/visitors/inlay_hint_visitor.hpp"
#include "gdshader/semantics/visitors/color_visitor.hpp"
#include "gdshader/semantics/visitors/semantic_token_visitor.hpp"

#include "server/project_manager.hpp"
#include "gdshader/ast/ast.h"
#include "utils/logger.hpp"

#include <unordered_set>
#include <vector>

using namespace gdshader_lsp;

GdShaderServer::GdShaderServer(std::unique_ptr<lsp::Connection> conn) : connection(std::move(conn)), handler(*connection)
{
    registerHandlers();
    compilerThread = std::thread(&GdShaderServer::compilerLoop, this);
}

GdShaderServer::~GdShaderServer()
{
    running = false;
    if (compilerThread.joinable())
    {
        compilerThread.join();
    }
}

void GdShaderServer::run() 
{
    try {
        while (running) {
            handler.processIncomingMessages();
        }
    } catch (const std::exception& e) {
        SPDLOG_INFO("Session ended: ", e.what());
    }
}

void GdShaderServer::registerHandlers() {
    
    // --- LIFECYCLE: INITIALIZE ---
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
                SPDLOG_INFO("Project Root set to: {} from recevied path {}.", root, params.rootUri->path());
            }

            lsp::ServerCapabilities caps;

            // Text Document Sync
            lsp::TextDocumentSyncOptions syncOps;
            syncOps.openClose = true;
            syncOps.change = lsp::TextDocumentSyncKind::Incremental;
            caps.textDocumentSync = syncOps;

            // Completion
            lsp::CompletionOptions compOps;
            compOps.triggerCharacters = std::vector<std::string>{".", ":"};
            caps.completionProvider = compOps;

            // Semantic Tokens
            lsp::SemanticTokensOptions semOps;
            semOps.legend = lsp::SemanticTokensLegend{
                .tokenTypes = { "type", "struct", "parameter", "variable", "property", "function", "keyword", "number", "macro", "enumMember" },
                .tokenModifiers = { "declaration", "readonly" }
            };
            semOps.full = true;
            caps.semanticTokensProvider = semOps;

            // Others
            caps.referencesProvider = true;
            caps.renameProvider = true;
            caps.foldingRangeProvider = true;
            caps.inlayHintProvider = true;
            caps.colorProvider = true;

            return lsp::requests::Initialize::Result{
                .capabilities = caps,
                .serverInfo = lsp::InitializeResultServerInfo{
                    .name = "gdshader-lsp",
                    .version = "0.4.2"
                }
            };
        }
    );

    // --- SYNCHRONIZATION: DID OPEN ---
    handler.add<lsp::notifications::TextDocument_DidOpen>(
        [this](lsp::notifications::TextDocument_DidOpen::Params&& params) 
        {
            auto uri = params.textDocument.uri;
            std::string path = std::string(uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            SPDLOG_DEBUG("Opening file at path '{}'", path.c_str());

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            su->unitMutex.lock();
            pm->updateFile(path, params.textDocument.text);
            su->unitMutex.unlock();
            {
                std::lock_guard<std::mutex> lock(debounceMutex);
                dirtyFiles[path] = std::chrono::steady_clock::now();
            }
        }
    );

    // --- SYNCHRONIZATION: DID CHANGE ---
    handler.add<lsp::notifications::TextDocument_DidChange>(
        [this](lsp::notifications::TextDocument_DidChange::Params&& params) {
            
            if (params.contentChanges.empty()) return;

            auto uri = params.textDocument.uri;
            std::string path = std::string(uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            su->unitMutex.lock();
            std::string currentText = su->source_code;

            for (const auto& changeEvent : params.contentChanges) {
            
                std::visit([this, &currentText](auto&& change) {
                    using T = std::decay_t<decltype(change)>;
                    
                    if constexpr (std::is_same_v<T, lsp::TextDocumentContentChangeEvent_Range_Text>) {
                        
                        size_t startOff = positionToOffset(currentText, change.range.start.line, change.range.start.character);
                        size_t endOff   = positionToOffset(currentText, change.range.end.line, change.range.end.character);
                        
                        if (startOff <= currentText.length() && endOff <= currentText.length() && startOff <= endOff) {
                            currentText.replace(startOff, endOff - startOff, change.text);
                        }
                    } 
                    // Full doc sync fallback
                    else if constexpr (std::is_same_v<T, lsp::TextDocumentContentChangeEvent_Text>) {
                        currentText = change.text;
                    }
                    
                }, changeEvent);
            }

            pm->updateFile(path, currentText);
            su->unitMutex.unlock();
            {
                std::lock_guard<std::mutex> lock(debounceMutex);
                dirtyFiles[path] = std::chrono::steady_clock::now();
            }
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

            su->unitMutex.lock();
            if (!su->symbols) {
                su->unitMutex.unlock();
                return hover;                
            }

            int line = params.position.line;
            int col = params.position.character;

            NodeFinderVisitor finder(line, col);
            su->ast->accept(finder);
            const ASTNode* target = finder.deepestNode;

            if (!target) {
                su->unitMutex.unlock();
                return hover;
            }

            std::string content = "";

            if (auto id = dynamic_cast<const IdentifierNode*>(target)) 
            {
                if (id->resolvedSymbol) {
                    content = "**" + id->name + "**\n\n";
                    if (id->resolvedSymbol->category == SymbolType::Function) {
                        content += "Return Type: `" + id->resolvedSymbol->returnType->toString() + "`\n";
                    } else if (id->evaluatedType) {
                        content += "Type: `" + id->evaluatedType->toString() + "`\n";
                    }
                    if (!id->resolvedSymbol->doc_string.empty()) content += "\n" + id->resolvedSymbol->doc_string;
                }
            } 

            else if (auto call = dynamic_cast<const FunctionCallNode*>(target)) 
            {
                std::vector<const Symbol*> overloads = su->symbols->lookupFunctions(call->functionName);
                if (!overloads.empty()) {
                    const Symbol* sym = overloads[0]; // Display the base function doc
                    content = "**" + sym->name + "**\n\n";
                    content += "Return Type: `" + (sym->returnType ? sym->returnType->toString() : "void") + "`\n";
                    if (!sym->doc_string.empty()) content += "\n" + sym->doc_string;
                }
            }

            else if (auto mem = dynamic_cast<const MemberAccessNode*>(target)) 
            {
                if (mem->evaluatedType) {
                    content = "**" + mem->member + "**\n\n";
                    content += "Type: `" + mem->evaluatedType->toString() + "`";
                }
            }

            else if (auto typeNode = dynamic_cast<const TypeNode*>(target)) 
            {
                content = "**Type**\n\n`" + typeNode->toString() + "`";
            }

            else
            {
                // FALLBACK: If we hovered over a Literal (like 1.0) or an operator, 
                // check if we are inside a function call's arguments
                CallFinderVisitor callFinder(line, col);
                su->ast->accept(callFinder);

                if (callFinder.activeCall) {
                    std::vector<const Symbol*> overloads = su->symbols->lookupFunctions(callFinder.activeCall->functionName);
                    if (!overloads.empty()) {
                        const Symbol* sym = overloads[0]; 
                        content = "**" + sym->name + "**\n\n";
                        content += "Return Type: `" + (sym->returnType ? sym->returnType->toString() : "void") + "`\n";
                        if (!sym->doc_string.empty()) content += "\n" + sym->doc_string;
                    }
                }
            }

            if (!content.empty()) {
                hover.contents = {
                    lsp::MarkupContent {
                        .kind = lsp::MarkupKind::Markdown,
                        .value = content
                    }
                };
            }

            su->unitMutex.unlock();
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

            su->unitMutex.lock();
            if (!su->symbols || !su->ast) {
                su->unitMutex.unlock();
                return result;                
            }

            int line = params.position.line;
            int col = params.position.character;

            // We still use the text buffer just to check if the trigger was a '.'
            std::string lineText = getLine(su->source_code, line);
            if (col > (int)lineText.length()) col = lineText.length();
            
            bool isDotTrigger = (col > 0 && lineText[col-1] == '.');

            // --- CASE A: DOT COMPLETION (Semantic) ---
            if (isDotTrigger) 
            {
                int targetCol = col - 2;
                if (targetCol < 0) targetCol = 0;

                NodeFinderVisitor finder(line, targetCol);
                su->ast->accept(finder);

                TypePtr t = nullptr;

                if (auto expr = dynamic_cast<const ExpressionNode*>(finder.deepestNode)) 
                {
                    if (expr->evaluatedType && expr->evaluatedType->kind != TypeKind::UNKNOWN) {
                        t = expr->evaluatedType;
                    }
                }
                
                // FALLBACK: If AST bounds were broken or type checker failed, extract string manually
                if (!t) {
                    std::string varName = "";
                    int i = col - 2;
                    while (i >= 0 && (std::isalnum(lineText[i]) || lineText[i] == '_')) {
                        varName = lineText[i] + varName;
                        i--;
                    }
                    if (!varName.empty()) {
                        const Symbol* sym = su->symbols->lookupAt(varName, line);
                        if (!sym) sym = su->symbols->lookupAt(varName, 99999);
                        if (sym && sym->type) t = sym->type;
                    }
                }

                // If the node before the dot is an expression, we already know its exact type!
                if (t && t->kind != TypeKind::UNKNOWN) 
                {
                    // 1. Vector Swizzling
                    if (t->kind == TypeKind::VECTOR) {
                        std::vector<std::string> swizzles = {"x", "y", "z", "w", "r", "g", "b", "a", "s", "t", "p", "q"};
                        
                        // Provide valid single-character swizzles for this vector size
                        for(int i = 0; i < t->componentCount; ++i) {
                            result.items.push_back(lsp::CompletionItem{
                                .label = swizzles[i],
                                .kind = lsp::CompletionItemKind::Field,
                                .detail = "float"
                            });
                            // Add rgba/stpq equivalents
                            result.items.push_back(lsp::CompletionItem{
                                .label = swizzles[i + 4],
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
                
                su->unitMutex.unlock();
                return result;
            }

            // --- CASE B: GLOBAL COMPLETION ---
            
            std::vector<Symbol> visible = su->symbols->getVisibleSymbolsAt(line);
            std::unordered_set<std::string> seen_functions; 

            for (const auto& s : visible) {
                
                // DEDUPLICATION CHECK
                if (s.category == SymbolType::Function || s.category == SymbolType::Builtin) {
                    if (seen_functions.count(s.name)) continue; 
                    seen_functions.insert(s.name);
                }

                lsp::CompletionItem item;
                item.label = s.name;
                
                if (s.category == SymbolType::Function || s.category == SymbolType::Builtin) {
                    item.kind = lsp::CompletionItemKind::Function;
                    item.detail = s.returnType ? s.returnType->toString() : "void";
                    
                    item.insertTextFormat = lsp::InsertTextFormat::Snippet;
                    std::string insertionText = s.name + "(";
                    
                    for(size_t i=0; i<s.parameterTypes.size(); ++i) {
                        if (i > 0) insertionText += ", ";
                        insertionText += "${" + std::to_string(i+1) + ":" + s.parameterNames[i] + "}";
                    }
                    insertionText += ")";
                    item.insertText = insertionText;
                } 
                else if (s.category == SymbolType::Struct) {
                    item.kind = lsp::CompletionItemKind::Struct;
                    item.insertText = s.name; 
                }
                else {
                    item.kind = (s.category == SymbolType::Uniform || s.mutability == Mutability::ReadOnly)
                                ? lsp::CompletionItemKind::Constant 
                                : lsp::CompletionItemKind::Variable;
                                
                    item.detail = s.type ? s.type->toString() : "unknown";
                    item.insertText = s.name;
                }

                result.items.push_back(item);
            }

            su->unitMutex.unlock();
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

            su->unitMutex.lock();
            if (!su->symbols) {
                su->unitMutex.unlock();
                return loc;                
            }

            NodeFinderVisitor finder(params.position.line, params.position.character);
            su->ast->accept(finder);

            if (auto id = dynamic_cast<const IdentifierNode*>(finder.deepestNode)) 
            {
                const Symbol* sym = id->resolvedSymbol;

                if (!sym) sym = su->symbols->lookupAt(id->name, params.position.line);
                if (!sym) sym = su->symbols->lookupAt(id->name, 99999);

                if (sym) {

                    loc.uri = params.textDocument.uri;
                    loc.range.start = lsp::Position{(unsigned)sym->definition.startLine, (unsigned)sym->definition.startCol};
                    loc.range.end   = lsp::Position{(unsigned)sym->definition.startLine, (unsigned)sym->definition.endCol};
                    
                    su->unitMutex.unlock();
                    return loc;
                }
            }

            else if (auto call = dynamic_cast<const FunctionCallNode*>(finder.deepestNode)) 
            {
                const Symbol* sym = su->symbols->lookupAt(call->functionName, params.position.line);
                if (!sym) sym = su->symbols->lookupAt(call->functionName, 99999);

                if (sym && sym->definition.startLine >= 0) {
                    loc.uri = params.textDocument.uri;
                    loc.range.start = lsp::Position{(unsigned)sym->definition.startLine, (unsigned)sym->definition.startCol};
                    loc.range.end   = lsp::Position{(unsigned)sym->definition.endLine, (unsigned)sym->definition.endCol};
                    su->unitMutex.unlock();
                    return loc;
                }
            }

            else if (auto varDecl = dynamic_cast<const VariableDeclNode*>(finder.deepestNode)) {
                loc.uri = params.textDocument.uri;
                loc.range.start = lsp::Position{(unsigned)varDecl->nameRange.startLine, (unsigned)varDecl->nameRange.startCol};
                loc.range.end   = lsp::Position{(unsigned)varDecl->nameRange.endLine, (unsigned)varDecl->nameRange.endCol};
                su->unitMutex.unlock();
                return loc;
            }

            su->unitMutex.unlock();
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

            su->unitMutex.lock();
            if (!su->symbols) {
                su->unitMutex.unlock();
                return help;                
            }

            int line = params.position.line;
            int col = params.position.character;

            CallFinderVisitor finder(line, col);
            su->ast->accept(finder);

            if (!finder.activeCall && !finder.activeConstructor) {
                su->unitMutex.unlock();
                return nullptr;
            }

            std::string funcName = "";
            const std::vector<std::unique_ptr<ExpressionNode>>* args = nullptr;

            if (finder.activeCall) {
                funcName = finder.activeCall->functionName;
                args = &finder.activeCall->arguments;
            } else if (finder.activeConstructor) {
                funcName = finder.activeConstructor->typeName;
                args = &finder.activeConstructor->arguments;
            }

            // 2. Calculate Active Argument Index semantically
            int argIndex = 0;
            if (args) {
                for (size_t i = 0; i < args->size(); i++) {
                    const auto& arg = (*args)[i];
                    if (!arg) continue;
                    
                    // If the cursor is before or inside this argument's end bound
                    if (line < arg->range.endLine || (line == arg->range.endLine && col <= arg->range.endCol)) {
                        argIndex = i;
                        break;
                    }
                    // Default to the next argument if we have passed this one
                    argIndex = i + 1; 
                }
            }

            help.activeParameter = argIndex;
            help.activeSignature = 0; 

            // 3A. Handle Standard Function Calls
            if (finder.activeCall) 
            {
                std::vector<const Symbol*> overloads = su->symbols->lookupFunctions(funcName);
                if (overloads.empty()) {
                    su->unitMutex.unlock();
                    return nullptr;
                }

                // Best-fit heuristic based on argument count
                for (size_t i = 0; i < overloads.size(); ++i) {
                    if ((int)overloads[i]->parameterTypes.size() > argIndex) {
                        help.activeSignature = i;
                        break; 
                    }
                }

                for (const auto* sym : overloads) {
                    lsp::SignatureInformation sigInfo;
                    
                    std::string returnStr = sym->returnType ? sym->returnType->toString() : "void";
                    std::string label = returnStr + " " + sym->name + "(";
                    
                    std::vector<lsp::ParameterInformation> paramsInfo;
                    
                    for (size_t i = 0; i < sym->parameterTypes.size(); ++i) {
                        std::string pType = sym->parameterTypes[i]->toString();
                        
                        // Use actual parsed argument names, fallback to arg0, arg1
                        std::string pName = (i < sym->parameterNames.size()) ? sym->parameterNames[i] : ("arg" + std::to_string(i));
                        std::string paramLabel = pType + " " + pName;
                        
                        if (i > 0) label += ", ";
                        
                        paramsInfo.push_back(lsp::ParameterInformation{ .label = paramLabel });
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
            }

            else if (finder.activeConstructor) 
            {
                TypePtr t = su->types.getType(funcName);
                if (t->kind == TypeKind::STRUCT) {
                    lsp::SignatureInformation sigInfo;
                    std::string label = funcName + "(";
                    std::vector<lsp::ParameterInformation> paramsInfo;

                    for (size_t i = 0; i < t->members.size(); ++i) {
                        std::string pName = t->members[i].first;
                        std::string pType = t->members[i].second->toString();
                        std::string paramLabel = pType + " " + pName;

                        if (i > 0) label += ", ";
                        
                        paramsInfo.push_back(lsp::ParameterInformation{ .label = paramLabel });
                        label += paramLabel;
                    }
                    label += ")";
                    
                    sigInfo.label = label;
                    sigInfo.parameters = paramsInfo;
                    help.signatures.push_back(sigInfo);
                }
            }

            su->unitMutex.unlock();
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

            su->unitMutex.lock();
            if (!su->symbols) {
                su->unitMutex.unlock();
                return nullptr;                
            }
            
            // Generate symbol tree from the AST
            std::vector<lsp::DocumentSymbol> symbols = getDocumentSymbols(su->ast.get());
            su->unitMutex.unlock();
            return symbols;
        }
    );

    // --- FEATURE: SEMANTIC TOKENS ---
    handler.add<lsp::requests::TextDocument_SemanticTokens_Full>(
        [this](lsp::requests::TextDocument_SemanticTokens_Full::Params&& params) -> lsp::requests::TextDocument_SemanticTokens_Full::Result 
        {
            lsp::requests::TextDocument_SemanticTokens_Full::Result result;
            
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            su->unitMutex.lock();
            if (!su->ast) {
                su->unitMutex.unlock();
                return result;                
            }

            // 1. Traverse AST to collect context-aware tokens
            SemanticTokenVisitor visitor;
            su->ast->accept(visitor);

            // 2. Sort tokens by position (LSP requires strict ordering)
            std::sort(visitor.tokens.begin(), visitor.tokens.end());

            // 3. Compute Line & Char Deltas
            std::vector<unsigned int> data;
            int prevLine = 0;
            int prevCol = 0;

            for (const auto& token : visitor.tokens) {
                int deltaLine = token.line - prevLine;
                int deltaCol = deltaLine == 0 ? token.col - prevCol : token.col;
                
                // Safety check: LSP will crash the editor if deltas are negative
                if (deltaLine < 0 || deltaCol < 0) continue; 

                data.push_back(deltaLine);
                data.push_back(deltaCol);
                data.push_back(token.length);
                data.push_back(token.type);
                data.push_back(token.modifiers);

                prevLine = token.line;
                prevCol = token.col;
            }

            lsp::SemanticTokens semantic_tokens_obj;
            semantic_tokens_obj.data = data;

            result = semantic_tokens_obj;
            
            su->unitMutex.unlock();
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
            
            su->unitMutex.lock();
            if (!su->symbols) {
                su->unitMutex.unlock();
                return result;                
            }

            NodeFinderVisitor finder(params.position.line, params.position.character);
            su->ast->accept(finder);

            if (auto id = dynamic_cast<const IdentifierNode*>(finder.deepestNode)) 
            {
                const Symbol* sym = id->resolvedSymbol;

                if (!sym) sym = su->symbols->lookupAt(id->name, params.position.line);
                if (!sym) sym = su->symbols->lookupAt(id->name, 99999);

                if (sym) {
                    if (sym->definition.startLine >= 0) {
                        int defLine = sym->definition.startLine;
                        result.push_back(lsp::DocumentHighlight{
                            .range = lsp::Range{
                                .start = { (unsigned)defLine, (unsigned)sym->definition.startCol },
                                .end   = { (unsigned)defLine, (unsigned)(sym->definition.endCol) }
                            },
                            .kind = lsp::DocumentHighlightKind::Text
                        });
                    }

                    for (const auto& usage : sym->references) {
                        int useLine = usage.startLine;
                        result.push_back(lsp::DocumentHighlight{
                            .range = lsp::Range{
                                .start = { (unsigned)useLine, (unsigned)usage.startCol },
                                .end   = { (unsigned)useLine, (unsigned)(usage.endCol) }
                            },
                            .kind = lsp::DocumentHighlightKind::Read
                        });
                    }
                }
            }

            su->unitMutex.unlock();
            return result;
        }
    );

    // --- FEATURE: TEXTDOCUMENT_RENAME ---
    handler.add<lsp::requests::TextDocument_Rename>(
        [this](lsp::requests::TextDocument_Rename::Params&& params) -> lsp::requests::TextDocument_Rename::Result
        {
            lsp::WorkspaceEdit result;

            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);
            
            su->unitMutex.lock();
            if (!su->symbols) {
                su->unitMutex.unlock();
                return result;                
            }

            NodeFinderVisitor finder(params.position.line, params.position.character);
            su->ast->accept(finder);

            if (auto id = dynamic_cast<const IdentifierNode*>(finder.deepestNode)) 
            {
                const Symbol* sym = id->resolvedSymbol;

                if (!sym) sym = su->symbols->lookupAt(id->name, params.position.line);
                if (!sym) sym = su->symbols->lookupAt(id->name, 99999);

                if (sym) {
                    // Safety: Don't rename built-ins
                    if (sym->category == SymbolType::Builtin) {
                        SPDLOG_WARN("Cannot rename builtins!");
                        su->unitMutex.unlock();
                        return result;
                    }

                    std::vector<lsp::TextEdit> edits;

                    if (sym->definition.startLine >= 0) {
                        int defLine = sym->definition.startLine;
                        edits.push_back(lsp::TextEdit{
                            .range = lsp::Range{
                                .start = { (unsigned)defLine, (unsigned)sym->definition.startCol },
                                .end   = { (unsigned)defLine, (unsigned)(sym->definition.endCol) }
                            },
                            .newText = params.newName
                        });
                    }

                    for (const auto& usage : sym->references) {
                        int useLine = usage.startLine;
                        edits.push_back(lsp::TextEdit{
                            .range = lsp::Range{
                                .start = { (unsigned)useLine, (unsigned)usage.startCol },
                                .end   = { (unsigned)useLine, (unsigned)(usage.endCol) }
                            },
                            .newText = params.newName
                        });
                    }

                    result.changes = {
                        { params.textDocument.uri, edits }
                    };
                }
            }

            su->unitMutex.unlock();
            return result;
        }
    );

    // --- FEATURE: REFERENCES ---
    handler.add<lsp::requests::TextDocument_References>(
        [this](lsp::requests::TextDocument_References::Params&& params) -> lsp::requests::TextDocument_References::Result
        {
            std::vector<lsp::Location> result;
            
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            su->unitMutex.lock();
            if (!su->symbols) {
                su->unitMutex.unlock();
                return result;                
            }

            NodeFinderVisitor finder(params.position.line, params.position.character);
            su->ast->accept(finder);

            if (auto id = dynamic_cast<const IdentifierNode*>(finder.deepestNode)) 
            {
                const Symbol* sym = id->resolvedSymbol;

                if (!sym) sym = su->symbols->lookupAt(id->name, params.position.line);
                if (!sym) sym = su->symbols->lookupAt(id->name, 99999);

                if (sym) {

                    // 1. Add Definition
                    if (params.context.includeDeclaration && sym->definition.startLine >= 0) {
                        int defLine = sym->definition.startLine;
                        result.push_back(lsp::Location{
                            .uri = params.textDocument.uri,
                            .range = lsp::Range{
                                .start = { (unsigned)defLine, (unsigned)sym->definition.startCol },
                                .end   = { (unsigned)defLine, (unsigned)(sym->definition.endCol) }
                            }
                        });
                    }

                    // 2. Add Usages
                    for (const auto& usage : sym->references) {
                        int useLine = usage.startLine;
                        result.push_back(lsp::Location{
                            .uri = params.textDocument.uri,
                            .range = lsp::Range{
                                .start = { (unsigned)useLine, (unsigned)usage.startCol },
                                .end   = { (unsigned)useLine, (unsigned)(usage.endCol) }
                            }
                        });
                    }
                }
            }

            su->unitMutex.unlock();
            return result;
        }
    );

    // --- FEATURE: FOLDING RANGES ---
    handler.add<lsp::requests::TextDocument_FoldingRange>(
        [this](lsp::requests::TextDocument_FoldingRange::Params&& params) -> lsp::requests::TextDocument_FoldingRange::Result 
        {
            std::vector<lsp::FoldingRange> result;

            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            su->unitMutex.lock();
            if (su->ast) {
                collectFoldingRanges(su->ast.get(), result);
            }
            su->unitMutex.unlock();

            return result;
        }
    );

    // --- FEATURE: INLAY HINTS ---
    handler.add<lsp::requests::TextDocument_InlayHint>(
        [this](lsp::requests::TextDocument_InlayHint::Params&& params) -> lsp::requests::TextDocument_InlayHint::Result
        {
            std::vector<lsp::InlayHint> hints;
            
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            su->unitMutex.lock();
            if (!su->ast || !su->symbols) 
            {
                su->unitMutex.unlock();
                return hints;
            }

            InlayHintVisitor visitor(hints, su->symbols.get(), &su->types, params.range);
            su->ast->accept(visitor);

            su->unitMutex.unlock();

            return hints;
        }
    );

    handler.add<lsp::requests::TextDocument_DocumentColor>(
        [this](lsp::requests::TextDocument_DocumentColor::Params&& params) -> lsp::requests::TextDocument_DocumentColor::Result
        {
            std::vector<lsp::ColorInformation> colors;
            
            std::string path = std::string(params.textDocument.uri.path());
            #ifdef _WIN32
            if (path.size() > 2 && path[0] == '/' && path[2] == ':') path = path.substr(1);
            #endif

            auto pm = ProjectManager::get_singleton();
            auto su = pm->getUnit(path);

            su->unitMutex.lock();
            if (!su->ast) {
                su->unitMutex.unlock();
                return colors;
            }

            ColorVisitor visitor(colors);
            su->ast->accept(visitor);
            
            su->unitMutex.unlock();
            return colors;
        }
    );

    // --- FEATURE: COLOR PRESENTATION ---
    handler.add<lsp::requests::TextDocument_ColorPresentation>(
        [this](lsp::requests::TextDocument_ColorPresentation::Params&& params) -> lsp::requests::TextDocument_ColorPresentation::Result
        {
            std::vector<lsp::ColorPresentation> presentations;
            
            // Format the floats
            std::string r = std::to_string(params.color.red);
            std::string g = std::to_string(params.color.green);
            std::string b = std::to_string(params.color.blue);
            std::string a = std::to_string(params.color.alpha);

            // Helper to strip trailing zeros, but keep the decimal (e.g., 1.0)
            auto trim = [](std::string& s) {
                s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                if (s.back() == '.') s += "0";
            };
            trim(r); trim(g); trim(b); trim(a);

            // Suggest both vec3 and vec4 formats. The editor will let the user pick, 
            // or default to the first one depending on the client.
            lsp::ColorPresentation vec4Pres;
            vec4Pres.label = "vec4(" + r + ", " + g + ", " + b + ", " + a + ")";
            vec4Pres.textEdit = lsp::TextEdit{params.range, vec4Pres.label};
            presentations.push_back(vec4Pres);

            lsp::ColorPresentation vec3Pres;
            vec3Pres.label = "vec3(" + r + ", " + g + ", " + b + ")";
            vec3Pres.textEdit = lsp::TextEdit{params.range, vec3Pres.label};
            presentations.push_back(vec3Pres);

            return presentations;
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

    Lexer lexer(code);
    Parser parser(lexer, path);
    
    auto ast = parser.parse();
    auto errors = parser.getDiagnostics();

    su->defines = parser.getDefines();

    SemanticAnalyzer analyzer;
    analyzer.setFilePath(path);

    if (ast) {
        auto result = analyzer.analyze(ast.get());
        
        su->unitMutex.lock();

        su->ast = std::move(ast);
        su->symbols = std::make_shared<SymbolTable>(std::move(result.symbols));
        su->types   = std::move(result.types);
        su->tokens = result.tokens;

        su->unitMutex.unlock();

        auto semanticErrors = result.diagnostics;
        errors.insert(errors.end(), semanticErrors.begin(), semanticErrors.end());
    }

    su->diagnostics = errors;

    // 3. Convert Diagnostics
    std::vector<lsp::Diagnostic> lspDiagnostics;
    for (const auto& err : errors) {

        lsp::DiagnosticSeverity severity;
        if (err.level == DiagnosticLevel::Warning) {
            severity = lsp::DiagnosticSeverity::Warning;
        } else {
            severity = lsp::DiagnosticSeverity::Error;
        }

        lspDiagnostics.push_back(lsp::Diagnostic{
            .range = lsp::Range{
                .start = lsp::Position{(unsigned)err.range.startLine, (unsigned)err.range.startCol},
                .end   = lsp::Position{(unsigned)err.range.endLine, (unsigned)(err.range.endCol)}
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
// Thread loop
//////////////////////////////////////////////////

void GdShaderServer::compilerLoop() 
{
    while (running) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::vector<std::string> toCompile;
        auto now = std::chrono::steady_clock::now();
        
        // Safely check which files have "settled" (no keystrokes for > 250ms)
        {
            std::lock_guard<std::mutex> lock(debounceMutex);
            for (auto it = dirtyFiles.begin(); it != dirtyFiles.end(); ) 
            {
                if (now - it->second > std::chrono::milliseconds(250)) 
                {
                    toCompile.push_back(it->first);
                    it = dirtyFiles.erase(it); // Remove from dirty list
                } else {
                    ++it;
                }
            }
        }
        
        for (const auto& uri_str : toCompile) 
        {
            auto pm = ProjectManager::get_singleton();            
            auto unit = pm->getUnit(uri_str);
            SPDLOG_DEBUG("Compiling file at path '{}'.", uri_str.c_str());
            compileAndPublish(lsp::DocumentUri::fromPath(uri_str), unit->source_code);
        }
    }
}

//////////////////////////////////////////////////
// Helper
//////////////////////////////////////////////////

size_t gdshader_lsp::GdShaderServer::positionToOffset(const std::string& text, int line, int character) 
{
    size_t offset = 0;
    int currentLine = 0;

    while (offset < text.length() && currentLine < line) {
        if (text[offset] == '\n') {
            currentLine++;
        }
        offset++;
    }
    offset += character;
    
    return std::min(offset, text.length()); // Clamp just in case
}

void gdshader_lsp::GdShaderServer::collectFoldingRanges(const ASTNode* node, std::vector<lsp::FoldingRange>& ranges) 
{
    if (!node) return;

    // 1. BlockNode
    if (auto block = dynamic_cast<const BlockNode*>(node)) {
        if (block->range.endLine > block->range.startLine) {
            lsp::FoldingRange fr;
            fr.startLine = block->range.startLine;
            fr.startCharacter = block->range.startCol;
            fr.endLine = block->range.endLine;
            fr.endCharacter = block->range.endCol;
            fr.kind = lsp::FoldingRangeKind::Region;
            ranges.push_back(fr);
        }
        
        for (const auto& stmt : block->statements) {
            collectFoldingRanges(stmt.get(), ranges);
        }
    }
    // 2. StructNode (struct MyStruct { ... };)
    else if (auto str = dynamic_cast<const StructNode*>(node)) {
        if (str->range.endLine > str->range.startLine) {
            lsp::FoldingRange fr;
            fr.startLine = str->range.startLine;
            fr.startCharacter = str->range.startCol;
            fr.endLine = str->range.endLine;
            fr.endCharacter = str->range.endCol;
            fr.kind = lsp::FoldingRangeKind::Region;
            ranges.push_back(fr);
        }
        // Struct members are leaves, no recursion needed inside them
    }
    // 3. SwitchNode (switch (...) { ... })
    //    Our AST stores cases directly, so we fold the switch statement itself.
    else if (auto sw = dynamic_cast<const SwitchNode*>(node)) {
        if (sw->range.endLine > sw->range.startLine) {
            lsp::FoldingRange fr;
            fr.startLine = sw->range.startLine;
            fr.startCharacter = sw->range.startCol;
            fr.endLine = sw->range.endLine;
            fr.endCharacter = sw->range.endCol;
            fr.kind = lsp::FoldingRangeKind::Region;
            ranges.push_back(fr);
        }
        for (const auto& c : sw->cases) {
            collectFoldingRanges(c.get(), ranges);
        }
    }
    // 4. CaseNode (case X: ... break;)
    else if (auto c = dynamic_cast<const CaseNode*>(node)) {
        if (c->range.endLine > c->range.startLine) {
            lsp::FoldingRange fr;
            fr.startLine = c->range.startLine;
            fr.startCharacter = c->range.startCol;
            fr.endLine = c->range.endLine;
            fr.endCharacter = c->range.endCol;
            fr.kind = lsp::FoldingRangeKind::Region;
            ranges.push_back(fr);
        }
        for (const auto& stmt : c->statements) {
            collectFoldingRanges(stmt.get(), ranges);
        }
    }
    // 5. Recursion Boilerplate (Traverse children to find nested blocks)
    else if (auto prog = dynamic_cast<const ProgramNode*>(node)) {
        for (const auto& n : prog->nodes) collectFoldingRanges(n.get(), ranges);
    }
    else if (auto func = dynamic_cast<const FunctionNode*>(node)) {
        if (func->body) collectFoldingRanges(func->body.get(), ranges);
    }
    else if (auto ifNode = dynamic_cast<const IfNode*>(node)) {
        if (ifNode->thenBranch) collectFoldingRanges(ifNode->thenBranch.get(), ranges);
        if (ifNode->elseBranch) collectFoldingRanges(ifNode->elseBranch.get(), ranges);
    }
    else if (auto forNode = dynamic_cast<const ForNode*>(node)) {
        if (forNode->body) collectFoldingRanges(forNode->body.get(), ranges);
    }
    else if (auto whileNode = dynamic_cast<const WhileNode*>(node)) {
        if (whileNode->body) collectFoldingRanges(whileNode->body.get(), ranges);
    }
    else if (auto doNode = dynamic_cast<const DoWhileNode*>(node)) {
        if (doNode->body) collectFoldingRanges(doNode->body.get(), ranges);
    }
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

    int lspLine = line;

    sym.range = lsp::Range{
        .start = lsp::Position{(unsigned)lspLine, 0},
        .end = lsp::Position{(unsigned)lspLine, 0}
    };
    sym.selectionRange = sym.range;
    
    sym.children = children;
    return sym;
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