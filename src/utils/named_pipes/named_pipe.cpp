
#include "utils/named_pipes/named_pipe.hpp"
#include <cassert>

#if defined(__APPLE__) || defined(__linux__)

#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace lsp::io {

struct NamedPipeStream::Impl {
    int m_fd = -1;

    Impl(int fd) : m_fd(fd) {}
    ~Impl() {
        if (m_fd != -1) {
            shutdown(m_fd, SHUT_RDWR);
            ::close(m_fd);
        }
    }

    void throwError(const std::string& msg) {
        throw lsp::io::Error(msg + ": " + std::to_string(errno)); // Inherited from lsp::Exception
    }

    void read(char* buffer, std::size_t size) {
        if (size == 0) return;
        std::size_t totalBytesRead = 0;
        while (totalBytesRead < size) {
            auto bytesRead = recv(m_fd, buffer + totalBytesRead, size - totalBytesRead, 0);
            if (bytesRead < 0 && errno == EINTR) continue;
            if (bytesRead <= 0) throwError("Failed to read from Unix Domain Socket");
            totalBytesRead += bytesRead;
        }
    }

    void write(const char* buffer, std::size_t size) {
        if (size == 0) return;
        std::size_t totalBytesWritten = 0;
        while (totalBytesWritten < size) {
            auto bytesWritten = send(m_fd, buffer + totalBytesWritten, size - totalBytesWritten, 0);
            if (bytesWritten < 0 && errno == EINTR) continue;
            if (bytesWritten <= 0) throwError("Failed to write to Unix Domain Socket");
            totalBytesWritten += bytesWritten;
        }
    }
};

NamedPipeStream::NamedPipeStream(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}

NamedPipeStream::NamedPipeStream(NamedPipeStream&&) = default;
NamedPipeStream& NamedPipeStream::operator=(NamedPipeStream&&) = default;
NamedPipeStream::~NamedPipeStream() = default;

void NamedPipeStream::read(char* buffer, std::size_t size) {
    assert(m_impl);
    m_impl->read(buffer, size);
}

void NamedPipeStream::write(const char* buffer, std::size_t size) {
    assert(m_impl);
    m_impl->write(buffer, size);
}

// --- Listener Implementation ---

struct NamedPipeListener::Impl {
    int m_serverFd = -1;
    std::string m_path;

    Impl(const std::string& path) : m_path(path) {
        m_serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_serverFd == -1) {
            throw lsp::io::Error("Failed to create Unix Domain Socket");
        }

        // Ensure we don't collide with an old crashed socket file
        ::unlink(m_path.c_str()); 

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_path.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(m_serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(m_serverFd);
            throw lsp::io::Error("Failed to bind Unix Domain Socket to path");
        }

        if (::listen(m_serverFd, 1) == -1) {
            ::close(m_serverFd);
            throw lsp::io::Error("Failed to listen on Unix Domain Socket");
        }
    }

    ~Impl() {
        if (m_serverFd != -1) {
            ::close(m_serverFd);
            ::unlink(m_path.c_str()); // Clean up the socket file on exit
        }
    }
};

NamedPipeListener::NamedPipeListener(const std::string& pipe_name) 
    : m_impl(std::make_unique<Impl>(pipe_name)) {}

NamedPipeListener::~NamedPipeListener() = default;

bool NamedPipeListener::isReady() const { return m_impl && m_impl->m_serverFd != -1; }

NamedPipeStream NamedPipeListener::listen() {
    if (!isReady()) throw lsp::io::Error("Pipe listener is not ready");

    int clientFd = accept(m_impl->m_serverFd, nullptr, nullptr);
    if (clientFd == -1) throw lsp::io::Error("Failed to accept pipe connection");

    return NamedPipeStream(std::make_unique<NamedPipeStream::Impl>(clientFd));
}

void NamedPipeListener::shutdown() { m_impl.reset(); }

} // namespace lsp::io

#endif // __APPLE__ || __linux__