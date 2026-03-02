
#ifndef DIAGNOSTICS_HPP
#define DIAGNOSTICS_HPP

#include <string>

namespace gdshader_lsp
{

struct Range 
{
    int startLine = 0;
    int startCol = 0;
    int endLine = 0;
    int endCol = 0;
};

enum class DiagnosticLevel 
{
    Error,
    Warning
};

struct Diagnostic 
{
    std::string message;
    DiagnosticLevel level = DiagnosticLevel::Error;

    Range range;
};

} // namespace gdshader_lsp


#endif