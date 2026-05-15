
#include <iostream>
#include <thread>
#include <memory>
#include <vector>

#include <lsp/io/standardio.h>
#include <lsp/connection.h>

#include "utils/logger.hpp"
#include "utils/cli.hpp"
#include "utils/named_pipes/named_pipe.hpp"

#include "server/gdshader_server.hpp"

int main(int argc, char* argv[]) 
{
    gdshader_lsp::ServerConfig config;

    try
    {
        config = gdshader_lsp::parse_cli(argc, argv);
    }
    catch(const std::exception& e)
    {
        std::cerr << "CLI Error: " << e.what() << "\n";
        gdshader_lsp::print_help(argv[0]);
        return 1;
    }

    if (config.show_help) {
        gdshader_lsp::print_help(argv[0]);
        return 0;
    }
    
    gdshader_lsp::Logger::init();

    try {

        if (config.use_stdio) {
            SPDLOG_INFO("Starting gdshader lsp via STDIO.");
            auto connection = std::make_unique<lsp::Connection>(lsp::io::standardIO());
            gdshader_lsp::GdShaderServer server(std::move(connection));
            server.run(); // Blocks on the main thread, handling stdin/stdout
        }

        else if (!config.pipe_name.empty()) {
#if defined(__APPLE__) || defined(__linux__)
            SPDLOG_INFO("Starting gdshader lsp on pipe: {}", config.pipe_name);
            auto listener = lsp::io::NamedPipeListener(config.pipe_name);

            while (listener.isReady())
            {
                auto pipe_stream = listener.listen();
                SPDLOG_INFO("Client connected via pipe!");

                std::thread([stream = std::move(pipe_stream)]() mutable {
                    auto connection = std::make_unique<lsp::Connection>(stream); 
                    gdshader_lsp::GdShaderServer server(std::move(connection));
                    server.run();
                }).detach();
            }
#else
            SPDLOG_ERROR("Named pipes are not currently supported on Windows. Please use --stdio or --port.");
            gdshader_lsp::Logger::shutdown();
            return 1;
#endif
        }

        else {
            SPDLOG_INFO("Starting gdshader lsp on port {}.", config.port);
            auto listener = lsp::io::SocketListener(config.port);

            while (listener.isReady())
            {
                auto socket = listener.listen();
                if (!socket.isOpen()) {
                    SPDLOG_ERROR("Could not open socket.");
                    break;
                }

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