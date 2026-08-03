#pragma once
#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <vector>
#include <unordered_map>
#include <string>
#include <map>

namespace gdshader_lsp 
{

enum class ShaderType 
{
    Spatial,
    CanvasItem,
    Particles,
    Sky,
    Fog,
    Blist,
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
    Fog,     // Fog
    Blit     // Blist
};

enum class RenderMode
{
    UNKNOWN = 0,
    BLEND_MIX,
    BLEND_ADD,
    BLEND_SUB,
    BLEND_MUL,
    BLEND_PREMUL_ALPHA,
    DEPTH_DRAW_OPAQUE,
    DEPTH_DRAW_ALWAYS,
    DEPTH_DRAW_NEVER,
    DEPTH_PREPASS_ALPHA,
    DEPTH_TEST_DISABLED,
    SSS_MODE_SKIN,
    CULL_BACK,
    CULL_FRONT,
    CULL_DISABLED,
    UNSHADED,
    WIREFRAME,
    DEBUG_SHADOW_SPLITS,
    DIFFUSE_BURLEY,
    DIFFUSE_LAMBERT,
    DIFFUSE_LAMBERT_WRAP,
    DIFFUSE_TOON,
    SPECULAR_SCHLICK_GGX,
    SPECULAR_TOON,
    SPECULAR_DISABLED,
    SKIP_VERTEX_TRANSFORM,
    WORLD_VERTEX_COORDS,
    ENSURE_CORRECT_NORMALS,
    SHADOWS_DISABLED,
    AMBIENT_LIGHT_DISABLED,
    SHADOW_TO_OPACITY,
    VERTEX_LIGHTING,
    PARTICLE_TRAILS,
    ALPHA_TO_COVERAGE,
    ALPHA_TO_COVERAGE_AND_ONE,
    FOG_DISABLED,
    BLEND_DISABLED
};

struct BuiltinVariable 
{
    std::string name;
    std::string type;
    std::string doc;
    std::string qualifier = "in";
};

struct BuiltinArg 
{
    std::string name;
    std::string type;
    std::string doc;
    bool is_optional = false;
};

struct BuiltinReturn 
{
    std::string type;
    std::string doc;
};

struct BuiltinFunction 
{
    std::string name;
    
    BuiltinReturn returnProperties;
    std::vector<BuiltinArg> args;
    std::string doc;
};

// Helper to define built-ins quickly
using BuiltinList = std::vector<BuiltinVariable>;
using BuiltinFuncList = std::vector<BuiltinFunction>;
}

#include "generated/builtins_data.hpp"

namespace gdshader_lsp 
{
static const BuiltinList EMTPY_LIST = {};
static inline const BuiltinList& get_nonglobal_builtins(ShaderType type, ShaderStage scope) 
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
    else if (type == ShaderType::Blist) {
        if (scope == ShaderStage::Blit) return gdshader_lsp::generated::BLIST;
    }
    return EMTPY_LIST;
}

static inline const BuiltinList& get_global_builtins(ShaderType type)
{
    if (type == ShaderType::Spatial) {
        return gdshader_lsp::generated::SPATIAL_GLOBAL;
    }
    else if (type == ShaderType::CanvasItem) {
        return gdshader_lsp::generated::CANVAS_GLOBAL;
    }
    else if (type == ShaderType::Particles) {
        return gdshader_lsp::generated::PARTICLE_GLOBAL;
    }
    else if (type == ShaderType::Sky) {   
        return gdshader_lsp::generated::SKY_GLOBAL;
    }
    else if (type == ShaderType::Fog) {
        return gdshader_lsp::generated::FOG_GLOBAL;
    }
    else if (type == ShaderType::Blist) {
        return gdshader_lsp::generated::BLIST_GLOBAL;
    }
    return EMTPY_LIST;
}

static inline const BuiltinFuncList& get_builtin_functions()
{
    return gdshader_lsp::generated::GLOBAL_FUNCTIONS;
}

// Converts a lowercase string (e.g. "blend_mix") to the RenderMode enum
static inline RenderMode stringToRenderMode(const std::string& modeStr) 
{
    static const std::unordered_map<std::string, RenderMode> modeMap = 
    {
        {"blend_mix", RenderMode::BLEND_MIX},
        {"blend_add", RenderMode::BLEND_ADD},
        {"blend_sub", RenderMode::BLEND_SUB},
        {"blend_mul", RenderMode::BLEND_MUL},
        {"blend_premul_alpha", RenderMode::BLEND_PREMUL_ALPHA},
        {"depth_draw_opaque", RenderMode::DEPTH_DRAW_OPAQUE},
        {"depth_draw_always", RenderMode::DEPTH_DRAW_ALWAYS},
        {"depth_draw_never", RenderMode::DEPTH_DRAW_NEVER},
        {"depth_prepass_alpha", RenderMode::DEPTH_PREPASS_ALPHA},
        {"depth_test_disabled", RenderMode::DEPTH_TEST_DISABLED},
        {"sss_mode_skin", RenderMode::SSS_MODE_SKIN},
        {"cull_back", RenderMode::CULL_BACK},
        {"cull_front", RenderMode::CULL_FRONT},
        {"cull_disabled", RenderMode::CULL_DISABLED},
        {"unshaded", RenderMode::UNSHADED},
        {"wireframe", RenderMode::WIREFRAME},
        {"debug_shadow_splits", RenderMode::DEBUG_SHADOW_SPLITS},
        {"diffuse_burley", RenderMode::DIFFUSE_BURLEY},
        {"diffuse_lambert", RenderMode::DIFFUSE_LAMBERT},
        {"diffuse_lambert_wrap", RenderMode::DIFFUSE_LAMBERT_WRAP},
        {"diffuse_toon", RenderMode::DIFFUSE_TOON},
        {"specular_schlick_ggx", RenderMode::SPECULAR_SCHLICK_GGX},
        {"specular_toon", RenderMode::SPECULAR_TOON},
        {"specular_disabled", RenderMode::SPECULAR_DISABLED},
        {"skip_vertex_transform", RenderMode::SKIP_VERTEX_TRANSFORM},
        {"world_vertex_coords", RenderMode::WORLD_VERTEX_COORDS},
        {"ensure_correct_normals", RenderMode::ENSURE_CORRECT_NORMALS},
        {"shadows_disabled", RenderMode::SHADOWS_DISABLED},
        {"ambient_light_disabled", RenderMode::AMBIENT_LIGHT_DISABLED},
        {"shadow_to_opacity", RenderMode::SHADOW_TO_OPACITY},
        {"vertex_lighting", RenderMode::VERTEX_LIGHTING},
        {"particle_trails", RenderMode::PARTICLE_TRAILS},
        {"alpha_to_coverage", RenderMode::ALPHA_TO_COVERAGE},
        {"alpha_to_coverage_and_one", RenderMode::ALPHA_TO_COVERAGE_AND_ONE},
        {"fog_disabled", RenderMode::FOG_DISABLED},
        {"blend_disabled", RenderMode::BLEND_DISABLED}
    };

    auto it = modeMap.find(modeStr);
    if (it != modeMap.end()) {
        return it->second;
    }
    
    return RenderMode::UNKNOWN; 
}

// Helper: Returns 1 for scalar, 2 for vec2, 3 for vec3, etc.
static inline int getComponentCount(const std::string& type) 
{
    if (type.find("vec2") != std::string::npos || type.find("ivec2") != std::string::npos) return 2;
    if (type.find("vec3") != std::string::npos || type.find("ivec3") != std::string::npos) return 3;
    if (type.find("vec4") != std::string::npos || type.find("ivec4") != std::string::npos) return 4;
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