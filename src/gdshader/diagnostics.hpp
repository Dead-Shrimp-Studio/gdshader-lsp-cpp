
#ifndef DIAGNOSTICS_HPP
#define DIAGNOSTICS_HPP

#include <string>

namespace gdshader_lsp
{
    
enum class DiagnosticLevel {
    Error,
    Warning
};

struct Diagnostic {
    int line;
    int column;
    std::string message;
    DiagnosticLevel level = DiagnosticLevel::Error;

    int length = 0;
};

} // namespace gdshader_lsp


#endif