#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace gdshader_lsp {

enum class TypeKind {
    SCALAR,     // int, float, bool, void
    VECTOR,     // vec3, ivec2
    MATRIX,     // mat4
    STRUCT,     // User defined structs
    ARRAY,      // float[5]
    SAMPLER,    // sampler2D
    UNKNOWN     // Error state
};

struct Type;
using TypePtr = std::shared_ptr<Type>;

struct Type 
{
    TypeKind kind = TypeKind::UNKNOWN;
    std::string name; // "vec3", "MyStruct", "int"
    
    // For Arrays: "float[5]" -> baseType="float", arraySize=5
    // For Vectors: "vec3" -> baseType="float", componentCount=3
    TypePtr baseType = nullptr; 
    int arraySize = 0; 
    int componentCount = 0; // 1 for scalar, 2-4 for vector, 4/9/16 for matrix

    // For Structs: Members (Ordered for constructor validation)
    std::vector<std::pair<std::string, TypePtr>> members;

    bool operator!=(const Type& other) const { return !(*this == other); }
    bool operator==(const Type& other) const {
        // 1. Simple checks
        if (this == &other) return true;
        if (kind != other.kind) return false;
        if (name != other.name) return false;
        
        // 2. Kind-specific checks
        
        // For Arrays and Vectors, check the dimensions and the inner type
        if (kind == TypeKind::ARRAY || kind == TypeKind::VECTOR || kind == TypeKind::MATRIX) {
            if (componentCount != other.componentCount) return false;
            
            // Recursive check for the base type (e.g., 'float' inside 'vec3')
            // We must handle nullptrs safely, though baseType should valid.
            if (baseType && other.baseType) {
                if (*baseType != *other.baseType) return false;
            } else if (baseType != other.baseType) {
                return false; // One is null, the other isn't
            }
        }

        // For Structs, check members
        if (kind == TypeKind::STRUCT) {
            if (members.size() != other.members.size()) return false;
            for (size_t i = 0; i < members.size(); ++i) {
                // Check member name (optional, but good for strictness)
                if (members[i].first != other.members[i].first) return false;
                
                // Check member type recursively
                if (*members[i].second != *other.members[i].second) return false;
            }
        }

        return true;
    }
    
    // Helper for diagnostics
    std::string toString() const {
        if (kind == TypeKind::ARRAY) {
            return (baseType ? baseType->toString() : "unknown") + 
                   "[" + std::to_string(arraySize) + "]";
        }
        return name;
    }
};

struct RawToken {
    int line;
    int col;
    int length;

    uint32_t type;
    uint32_t modifiers;
    
    // Helper for sorting
    bool operator<(const RawToken& other) const {
        if (line != other.line) return line < other.line;
        return col < other.col;
    }

};

} // namespace gdshader_lsp

#endif