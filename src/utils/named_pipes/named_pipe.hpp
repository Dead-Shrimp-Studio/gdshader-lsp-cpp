#ifndef GDSHADER_LSP_NAMED_PIPES_HPP
#define GDSHADER_LSP_NAMED_PIPES_HPP

#include <memory>
#include <string>
#include <lsp/io/stream.h>

namespace lsp::io {

class NamedPipeStream : public Stream {
public:

    NamedPipeStream(NamedPipeStream&&);
    NamedPipeStream& operator=(NamedPipeStream&&);
    ~NamedPipeStream() override;

    void read(char* buffer, std::size_t size) override;
    void write(const char* buffer, std::size_t size) override;

private:
    friend class NamedPipeListener;
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    NamedPipeStream(std::unique_ptr<Impl> impl);
};

class NamedPipeListener {
public:
    // pipe_name is a filesystem path on Linux/macOS (e.g., /tmp/gdshader.sock)
    NamedPipeListener(const std::string& pipe_name);
    ~NamedPipeListener();

    [[nodiscard]] NamedPipeStream listen();
    [[nodiscard]] bool isReady() const;
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lsp::io

#endif