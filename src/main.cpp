
#include <iostream>
#include <thread>
#include <memory>
#include <vector>

#include "utils/logger.hpp"
#include "server/gdshader_server.hpp"

int main(int argc, char* argv[]) 
{
    gdshader_lsp::Logger::init();

    int port = 6005;
    
    // Simple argument parsing for port
    if (argc > 1 && std::string(argv[1]).rfind("--port=", 0) == 0) {
        port = std::stoi(std::string(argv[1]).substr(7));
    }

    SPDLOG_INFO("Starting gdshader lsp on port {}.", port);

    try {
        auto listener = lsp::io::SocketListener(port);

        while (listener.isReady()) 
        {
            auto socket = listener.listen();
            if (!socket.isOpen()) break;

            SPDLOG_INFO("Client connected!");

            // Spawn a thread to handle this connection independently
            std::thread([socket = std::move(socket)]() mutable {
                gdshader_lsp::GdShaderServer server(std::move(socket));
                server.run();
            }).detach();
        }

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Fatal error: {}.", e.what());
        gdshader_lsp::Logger::shutdown();
        return 1;
    }

    gdshader_lsp::Logger::shutdown();
    return 0;
}