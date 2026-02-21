#pragma once
#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <vector>
#include <string>
#include <map>

namespace gdshader_lsp {

// -------------------------------------------------------------------------
// ENUMS
// -------------------------------------------------------------------------

enum class ShaderType 
{
    Spatial,
    CanvasItem,
    Particles,
    Sky,
    Fog,
    Unknown
};

enum class ShaderStage 
{
    Global,
    Vertex,
    Fragment,
    Light,
    Start,   // Particles
    Process, // Particles
    Sky,     // Sky
    Fog      // Fog
};

// -------------------------------------------------------------------------
// BUILT-IN DATA
// -------------------------------------------------------------------------

struct BuiltinVariable {
    std::string name;
    std::string type;
    std::string doc;
    std::string qualifier = "in";
};

struct BuiltinArg {
    std::string name;
    std::string type;
    std::string doc;
};

struct BuiltinReturn {
    std::string type;
    std::string doc;
};

struct BuiltinFunction {
    std::string name;
    
    BuiltinReturn returnProperties;
    std::vector<BuiltinArg> args;

    std::string doc;

    /**
     * @brief Backwards compatibilty.
     * @deprecated Do not use.
     */
    std::vector<std::string> argTypes;
    std::string returnType;
};

// Helper to define built-ins quickly
using BuiltinList = std::vector<BuiltinVariable>;
using BuiltinFuncList = std::vector<BuiltinFunction>;
}

#include "generated/builtins_data.hpp"

namespace gdshader_lsp {
// -------------------------------------------------------------------------
// LOOKUP UTILITIES
// -------------------------------------------------------------------------

static const BuiltinList EMPTY_LIST = {};

inline const BuiltinList& get_builtins(ShaderType type, ShaderStage scope) 
{
    if (type == ShaderType::Spatial) {
        if (scope == ShaderStage::Vertex) return gdshader_lsp::generated::SPATIAL_VERTEX;
        if (scope == ShaderStage::Fragment) return gdshader_lsp::generated::SPATIAL_FRAGMENT;
        if (scope == ShaderStage::Light) return gdshader_lsp::generated::SPATIAL_LIGHT;
    } 
    else if (type == ShaderType::CanvasItem) {
        if (scope == ShaderStage::Vertex) return gdshader_lsp::generated::CANVAS_VERTEX;
        if (scope == ShaderStage::Fragment) return gdshader_lsp::generated::CANVAS_FRAGMENT;
        if (scope == ShaderStage::Light) return gdshader_lsp::generated::CANVAS_LIGHT;
    }
    else if (type == ShaderType::Particles) {
        if (scope == ShaderStage::Start) return gdshader_lsp::generated::PARTICLE_START;
        if (scope == ShaderStage::Process) return gdshader_lsp::generated::PARTICLE_PROCESS;
    }
    else if (type == ShaderType::Sky) {
        if (scope == ShaderStage::Sky) return gdshader_lsp::generated::SKY;
    }
    else if (type == ShaderType::Fog) {
        if (scope == ShaderStage::Fog) return gdshader_lsp::generated::FOG;
    }
    return EMPTY_LIST;
}

inline const BuiltinFuncList& get_builtin_functions() {
    return gdshader_lsp::generated::GLOBAL_FUNCTIONS;
}

// Helper: Returns 1 for scalar, 2 for vec2, 3 for vec3, etc.
static inline int getComponentCount(const std::string& type) {
    if (type.find("vec2") != std::string::npos) return 2;
    if (type.find("vec3") != std::string::npos) return 3;
    if (type.find("vec4") != std::string::npos) return 4;
    return 1; // Scalars (int, float, bool) count as 1
}

// Helper: Returns "float" for vec3, "int" for ivec3, etc.
static inline std::string getElementBaseType(const std::string& type) 
{
    if (type.substr(0, 1) == "i") return "int";
    if (type.substr(0, 1) == "u") return "uint";
    if (type.substr(0, 1) == "b") return "bool";
    if (type == "int" || type == "uint" || type == "bool" || type == "float") return type;
    return "float"; // vec3, mat3, etc. default to float
}

} // namespace gdshader_lsp

#endif // BUILTINS_HPP