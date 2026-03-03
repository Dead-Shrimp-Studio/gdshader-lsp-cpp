
#ifndef GDSHADERLSP_PARSER_TYPES_H
#define GDSHADERLSP_PARSER_TYPES_H

#include "gdshader/ast/ast.h"
#include <memory>

namespace gdshader_lsp
{

    enum Precedence
    {
        PREC_NONE,
        PREC_ASSIGNMENT,  // = += -= *= /= %=
        PREC_TERNARY,     // ?:
        PREC_OR,          // ||
        PREC_AND,         // &&
        PREC_EQUALITY,    // == !=
        PREC_COMPARISON,  // < > <= >=
        PREC_TERM,        // + -
        PREC_FACTOR,      // * / %
        PREC_UNARY,       // ! -
        PREC_CALL,        // . () []
        PREC_PRIMARY
    };

} // namespcae

#endif