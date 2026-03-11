
#ifndef GDSHADERLSP_PARSER_TYPES_H
#define GDSHADERLSP_PARSER_TYPES_H

#include "gdshader/ast/ast.h"
#include <memory>

namespace gdshader_lsp
{

    enum Precedence
    {
        PREC_NONE,
        PREC_ASSIGNMENT,  // = += -= *= /= %= <<= >>= &= ^= |=
        PREC_TERNARY,     // ?:
        PREC_OR,          // ||
        PREC_LOGICAL_XOR, // ^^
        PREC_AND,         // &&
        PREC_BITWISE_OR,  // |
        PREC_BITWISE_XOR, // ^
        PREC_BITWISE_AND, // &
        PREC_EQUALITY,    // == !=
        PREC_COMPARISON,  // < > <= >=
        PREC_SHIFT,       // << >>
        PREC_TERM,        // + -
        PREC_FACTOR,      // * / %
        PREC_UNARY,       // ! - ~ ++ -- (prefix)
        PREC_CALL,        // . () [] ++ -- (postfix)
        PREC_PRIMARY
    };

} // namespcae

#endif