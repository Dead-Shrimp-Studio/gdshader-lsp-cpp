
#include <iostream>
#include <thread>
#include <memory>
#include <vector>

#include <lsp/io/standardio.h>
#include <lsp/connection.h>

#include "utils/logger.hpp"
#include "server/gdshader_server.hpp"

int main(int argc, char* argv[]) 
{
    gdshader_lsp::Logger::init();

    int port = 6007;
    bool use_stdio = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--port=", 0) == 0) {
            port = std::stoi(arg.substr(7));
        }
        else if (arg == "--stdio") {
            use_stdio = true;
        }
    }

    try {

        if (use_stdio) {
            SPDLOG_INFO("Starting gdshader lsp via STDIO.");
            auto connection = std::make_unique<lsp::Connection>(lsp::io::standardIO());
            gdshader_lsp::GdShaderServer server(std::move(connection));
            server.run(); // Blocks on the main thread, handling stdin/stdout
        } 
        else {
            SPDLOG_INFO("Starting gdshader lsp on port {}.", port);
            auto listener = lsp::io::SocketListener(port);

            while (listener.isReady())
            {
                auto socket = listener.listen();
                if (!socket.isOpen()) break;

                SPDLOG_INFO("Client connected!");

                std::thread([socket = std::move(socket)]() mutable {
                    auto connection = std::make_unique<lsp::Connection>(socket);
                    gdshader_lsp::GdShaderServer server(std::move(connection));
                    server.run();
                }).detach();
            }
        }

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Fatal error: {}.", e.what());
        gdshader_lsp::Logger::shutdown();
        return 1;
    }

    gdshader_lsp::Logger::shutdown();
    return 0;
}