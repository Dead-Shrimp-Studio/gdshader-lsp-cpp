#include <string>
#include <string_view>
#include <stdexcept>
#include <iostream>

namespace gdshader_lsp
{
    struct ServerConfig {
        std::string pipe_name = "";
        int port = 6007;
        bool use_stdio = false;
        bool show_help = false;
    };

    ServerConfig parse_cli(int argc, char* argv[]) {
        ServerConfig config;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);

            if (arg == "--help" || arg == "-h") {
                config.show_help = true;
            } 
            else if (arg == "--stdio") {
                config.use_stdio = true;
            } 
            else if (arg.rfind("--pipe=", 0) == 0) {
                // Handles: --pipe=/tmp/gdshader.sock
                arg.remove_prefix(7); 
                config.pipe_name = std::string(arg);
            }
            else if (arg == "--pipe") {
                // Handles: --pipe /tmp/gdshader.sock
                if (i + 1 < argc) {
                    config.pipe_name = argv[++i];
                } else {
                    throw std::invalid_argument("--pipe requires a pipe name or path argument");
                }
            }
            else if (arg.rfind("--port=", 0) == 0) {
                // Handles: --port=6007
                arg.remove_prefix(7); 
                try {
                    config.port = std::stoi(std::string(arg));
                } catch (...) {
                    throw std::invalid_argument("Invalid port number provided via --port=");
                }
            } 
            else if (arg == "--port") {
                // Handles: --port 6007
                if (i + 1 < argc) {
                    try {
                        config.port = std::stoi(argv[++i]);
                    } catch (...) {
                        throw std::invalid_argument("Invalid port number provided after --port");
                    }
                } else {
                    throw std::invalid_argument("--port requires a port number argument");
                }
            } 
            else {
                throw std::invalid_argument("Unknown argument: " + std::string(arg));
            }
        }

        return config;
    }

    void print_help(const char* executable_name) {
        std::cout << "Usage: " << executable_name << " [options]\n"
                << "Options:\n"
                << "  --stdio        Run the LSP server over standard input/output.\n"
                << "  --pipe <name>  Run the LSP server over a named pipe / Unix domain socket.\n"
                << "  --port <num>   Run the LSP server on a TCP socket (default: 6007).\n"
                << "  -h, --help     Show this help message.\n";
    }
}
