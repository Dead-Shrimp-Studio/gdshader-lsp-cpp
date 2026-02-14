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

enum class ShaderType {
    Spatial,
    CanvasItem,
    Particles,
    Sky,
    Fog,
    Unknown
};

enum class ShaderStage {
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
    std::string type; // e.g., "vec3", "float"
    std::string doc;
};

struct BuiltinFunction {
    std::string name;
    std::string returnType;
    std::vector<std::string> argTypes;
    std::string doc;
};

// Helper to define built-ins quickly
using BuiltinList = std::vector<BuiltinVariable>;
using BuiltinFuncList = std::vector<BuiltinFunction>;

// -------------------------------------------------------------------------
// BUILT-IN VARIABLES
// -------------------------------------------------------------------------

// SPATIAL according to the Godot documentation

static const BuiltinList SPATIAL_VERTEX = {

    // Global
    {"TIME", "float", "Global time in seconds."},
    {"PI", "float", "PI constant (3.141592)."},
    {"TAU", "float", "TAU constant (6.283185)."},
    {"E", "float", "E constant (2.718281)."},
    {"OUTPUT_IS_SRGB", "bool", "True if output is sRGB."},
    {"CLIP_SPACE_FAR", "float", "Clip space far Z value."},

    // Specific

    {"VIEWPORT_SIZE", "vec2", "Size of viewport in pixels."},
    {"VIEW_MATRIX", "mat4", "World space to view space transform."},
    {"INV_VIEW_MATRIX", "mat4", "View space to world space transform."},
    {"MAIN_CAM_INV_VIEW_MATRIX", "mat4", "View space to world space transform of the main camera."},
    {"INV_PROJECTION_MATRIX", "mat4", "Clip space to view space transform."},
    {"NODE_POSITION_WORLD", "vec3", "Node position in world space."},
    {"NODE_POSITION_VIEW", "vec3", "Node position in view space."},
    {"CAMERA_POSITION_WORLD", "vec3", "Camera position in world space."},
    {"CAMERA_DIRECTION_WORLD", "vec3", "Camera direction in world space."},
    {"CAMERA_VISIBLE_LAYERS", "uint", "Cull layers of the camera."},
    {"INSTANCE_ID", "int", "Instance ID."},
    {"INSTANCE_CUSTOM", "vec4", "Instance custom data."},
    {"VIEW_INDEX", "int", "View index (0 for mono/left, 1 for right)."},
    {"VIEW_MONO_LEFT", "int", "Constant 0."},
    {"VIEW_RIGHT", "int", "Constant 1."},
    {"EYE_OFFSET", "vec3", "Position offset for the eye being rendered."},

    {"VERTEX", "vec3", "Vertex position in model space (or world if using world_vertex_coords)."},
    {"VERTEX_ID", "int", "Index of the current vertex."},
    {"NORMAL", "vec3", "Normal in model space."},
    {"TANGENT", "vec3", "Tangent in model space."},
    {"BINORMAL", "vec3", "Binormal in model space."},
    {"POSITION", "vec4", "Override final vertex position in clip space."},
    {"UV", "vec2", "UV main channel."},
    {"UV2", "vec2", "UV secondary channel."},
    {"COLOR", "vec4", "Vertex color."},
    {"ROUGHNESS", "float", "Roughness for vertex lighting."},
    {"POINT_SIZE", "float", "Point size for point rendering."},

    {"MODELVIEW_MATRIX", "mat4", "Model space to view space transform."},
    {"MODELVIEW_NORMAL_MATRIX", "mat3", "Model space to view space normal transform."},
    {"MODEL_MATRIX", "mat4", "Model space to world space transform."},
    {"MODEL_NORMAL_MATRIX", "mat3", "Model space to world space normal transform."},
    {"PROJECTION_MATRIX", "mat4", "View space to clip space transform."},

    {"BONE_INDICES", "uvec4", "Bone indices."},
    {"BONE_WEIGHTS", "vec4", "Bone weights."},
    {"CUSTOM0", "vec4", "Custom value 0 (UV3/UV4)."},
    {"CUSTOM1", "vec4", "Custom value 1 (UV5/UV6)."},
    {"CUSTOM2", "vec4", "Custom value 2 (UV7/UV8)."},
    {"CUSTOM3", "vec4", "Custom value 3."}
};

static const BuiltinList SPATIAL_FRAGMENT = {

    // Global
    {"TIME", "float", "Global time in seconds."},
    {"PI", "float", "PI constant (3.141592)."},
    {"TAU", "float", "TAU constant (6.283185)."},
    {"E", "float", "E constant (2.718281)."},
    {"OUTPUT_IS_SRGB", "bool", "True if output is sRGB."},
    {"CLIP_SPACE_FAR", "float", "Clip space far Z value."},

    // Sepcific

    {"VIEWPORT_SIZE", "vec2", "Size of viewport in pixels."},
    {"FRAGCOORD", "vec4", "Fragment coordinates in window space."},
    {"FRONT_FACING", "bool", "True if front face."},
    {"VIEW", "vec3", "Vector from fragment to camera (view space)."},
    {"UV", "vec2", "UV coordinates."},
    {"UV2", "vec2", "UV2 coordinates."},
    {"COLOR", "vec4", "Interpolated vertex color."},
    {"POINT_COORD", "vec2", "Point coordinates."},

    {"MODEL_MATRIX", "mat4", "Model to world transform."},
    {"MODEL_NORMAL_MATRIX", "mat3", "Model to world normal transform."},
    {"VIEW_MATRIX", "mat4", "World to view transform."},
    {"INV_VIEW_MATRIX", "mat4", "View to world transform."},
    {"PROJECTION_MATRIX", "mat4", "View to clip transform."},
    {"INV_PROJECTION_MATRIX", "mat4", "Clip to view transform."},
    {"NODE_POSITION_WORLD", "vec3", "Node position (world)."},
    {"NODE_POSITION_VIEW", "vec3", "Node position (view)."},
    {"CAMERA_POSITION_WORLD", "vec3", "Camera position (world)."},
    {"CAMERA_DIRECTION_WORLD", "vec3", "Camera direction (world)."},
    {"CAMERA_VISIBLE_LAYERS", "uint", "Camera cull layers."},

    {"VERTEX", "vec3", "Fragment position in view space."},
    {"LIGHT_VERTEX", "vec3", "Writable VERTEX for lighting calculations (does not move pixel)."},
    {"VIEW_INDEX", "int", "View index."},
    {"VIEW_MONO_LEFT", "int", "Constant 0."},
    {"VIEW_RIGHT", "int", "Constant 1."},
    {"EYE_OFFSET", "vec3", "Eye offset."},
    {"SCREEN_UV", "vec2", "Screen UV coordinates."},

    {"DEPTH", "float", "Custom depth value."},
    {"NORMAL", "vec3", "Normal in view space."},
    {"TANGENT", "vec3", "Tangent in view space."},
    {"BINORMAL", "vec3", "Binormal in view space."},
    {"NORMAL_MAP", "vec3", "Normal map value."},
    {"NORMAL_MAP_DEPTH", "float", "Normal map depth (default 1.0)."},
    {"ALBEDO", "vec3", "Base color."},
    {"ALPHA", "float", "Opacity."},
    {"ALPHA_SCISSOR_THRESHOLD", "float", "Discard threshold."},
    {"ALPHA_HASH_SCALE", "float", "Alpha hash scale."},
    {"ALPHA_ANTIALIASING_EDGE", "float", "Alpha AA edge."},
    {"ALPHA_TEXTURE_COORDINATE", "vec2", "Texture coord for Alpha AA."},
    {"PREMUL_ALPHA_FACTOR", "float", "Premultiplied alpha factor."},
    {"METALLIC", "float", "Metallic (0.0 - 1.0)."},
    {"SPECULAR", "float", "Specular (0.0 - 1.0)."},
    {"ROUGHNESS", "float", "Roughness (0.0 - 1.0)."},
    {"RIM", "float", "Rim lighting."},
    {"RIM_TINT", "float", "Rim tint."},
    {"CLEARCOAT", "float", "Clearcoat intensity."},
    {"CLEARCOAT_GLOSS", "float", "Clearcoat gloss."},
    {"ANISOTROPY", "float", "Anisotropy strength."},
    {"ANISOTROPY_FLOW", "vec2", "Anisotropy direction."},
    {"SSS_STRENGTH", "float", "Subsurface scattering strength."},
    {"SSS_TRANSMITTANCE_COLOR", "vec4", "SSS Transmittance color."},
    {"SSS_TRANSMITTANCE_DEPTH", "float", "SSS Transmittance depth."},
    {"SSS_TRANSMITTANCE_BOOST", "float", "SSS Transmittance boost."},
    {"BACKLIGHT", "vec3", "Backlight color."},
    {"AO", "float", "Ambient occlusion."},
    {"AO_LIGHT_AFFECT", "float", "AO light affect."},
    {"EMISSION", "vec3", "Emission color."},
    {"FOG", "vec4", "Fog blend."},
    {"RADIANCE", "vec4", "Radiance blend."},
    {"IRRADIANCE", "vec4", "Irradiance blend."}
};

static const BuiltinList SPATIAL_LIGHT = {

    // Global
    {"TIME", "float", "Global time in seconds."},
    {"PI", "float", "PI constant."},
    {"TAU", "float", "TAU constant."},
    {"E", "float", "E constant."},
    {"OUTPUT_IS_SRGB", "bool", "Output is sRGB."},
    {"CLIP_SPACE_FAR", "float", "Clip space far."},

    // Specific

    {"VIEWPORT_SIZE", "vec2", "Viewport size."},
    {"FRAGCOORD", "vec4", "Fragment coordinates."},
    {"MODEL_MATRIX", "mat4", "Model matrix."},
    {"INV_VIEW_MATRIX", "mat4", "Inverse view matrix."},
    {"VIEW_MATRIX", "mat4", "View matrix."},
    {"PROJECTION_MATRIX", "mat4", "Projection matrix."},
    {"INV_PROJECTION_MATRIX", "mat4", "Inverse projection matrix."},
    {"NORMAL", "vec3", "Normal in view space."},
    {"SCREEN_UV", "vec2", "Screen UV."},
    {"UV", "vec2", "UV."},
    {"UV2", "vec2", "UV2."},
    {"VIEW", "vec3", "View vector in view space."},
    {"LIGHT", "vec3", "Light vector in view space."},
    {"LIGHT_COLOR", "vec3", "Color of the light."},
    {"SPECULAR_AMOUNT", "float", "Specular multiplier (2.0 or 1.0)."},
    {"LIGHT_IS_DIRECTIONAL", "bool", "True if directional light."},
    {"ATTENUATION", "float", "Shadow and distance attenuation."},
    {"ALBEDO", "vec3", "Base albedo from fragment."},
    {"BACKLIGHT", "vec3", "Backlight from fragment."},
    {"METALLIC", "float", "Metallic from fragment."},
    {"ROUGHNESS", "float", "Roughness from fragment."},

    {"DIFFUSE_LIGHT", "vec3", "Output diffuse light."},
    {"SPECULAR_LIGHT", "vec3", "Output specular light."},
    {"ALPHA", "float", "Alpha (transparency)."}
};

// CANVAS ITEM (2D)
static const BuiltinList CANVAS_VERTEX = {
    
    // Global
    {"TIME", "float", "Global time in seconds."},
    {"PI", "float", "PI constant (3.141592)."},
    {"TAU", "float", "TAU constant (6.283185)."},
    {"E", "float", "E constant (2.718281)."},

    // Specific

    {"MODEL_MATRIX", "mat4", "Local space to world space transform."},
    {"CANVAS_MATRIX", "mat4", "World space to canvas space transform."},
    {"SCREEN_MATRIX", "mat4", "Canvas space to clip space transform."},
    {"INSTANCE_ID", "int", "Instance ID for instancing."},
    {"INSTANCE_CUSTOM", "vec4", "Instance custom data."},
    {"AT_LIGHT_PASS", "bool", "Always false in vertex."},
    {"TEXTURE_PIXEL_SIZE", "vec2", "Normalized pixel size of default 2D texture."},

    {"VERTEX", "vec2", "Vertex position in local space."},
    {"VERTEX_ID", "int", "Index of the current vertex."},
    {"UV", "vec2", "Normalized texture coordinates."},
    {"COLOR", "vec4", "Color from vertex primitive."},
    {"POINT_SIZE", "float", "Point size for point drawing."},

    {"CUSTOM0", "vec4", "Custom value from vertex primitive."},
    {"CUSTOM1", "vec4", "Custom value from vertex primitive."}

};

static const BuiltinList CANVAS_FRAGMENT = {

    // Global
    {"TIME", "float", "Global time in seconds."},
    {"PI", "float", "PI constant."},
    {"TAU", "float", "TAU constant."},
    {"E", "float", "E constant."},

    // Specific

    {"FRAGCOORD", "vec4", "Coordinate of pixel center in screen space."},
    {"SCREEN_PIXEL_SIZE", "vec2", "Size of individual pixels (inverse of resolution)."},
    {"REGION_RECT", "vec4", "Visible area of the sprite region."},
    {"POINT_COORD", "vec2", "Coordinate for drawing points."},
    {"TEXTURE", "sampler2D", "Default 2D texture."},
    {"TEXTURE_PIXEL_SIZE", "vec2", "Normalized pixel size of default 2D texture."},
    {"AT_LIGHT_PASS", "bool", "Always false in fragment."},
    {"SPECULAR_SHININESS_TEXTURE", "sampler2D", "Specular shininess texture."},
    {"SPECULAR_SHININESS", "vec4", "Specular shininess color."},
    {"UV", "vec2", "UV from vertex function."},
    {"SCREEN_UV", "vec2", "Screen UV coordinate."},
    {"NORMAL_TEXTURE", "sampler2D", "Default 2D normal texture."},

    {"NORMAL", "vec3", "Normal read from NORMAL_TEXTURE (writable)."},
    {"NORMAL_MAP", "vec3", "Configures normal maps meant for 3D (overrides NORMAL)."},
    {"NORMAL_MAP_DEPTH", "float", "Normal map depth for scaling."},
    {"VERTEX", "vec2", "Pixel position in screen space."},
    {"SHADOW_VERTEX", "vec2", "Same as VERTEX but can be written to for shadows."},
    {"LIGHT_VERTEX", "vec3", "Same as VERTEX but can be written to for lighting (Z is height)."},
    {"COLOR", "vec4", "Output color (Vertex color * Texture color)."}
};

static const BuiltinList CANVAS_LIGHT = {

    // Global
    {"TIME", "float", "Global time in seconds."},
    {"PI", "float", "PI constant."},
    {"TAU", "float", "TAU constant."},
    {"E", "float", "E constant."},

    // Specific

    {"FRAGCOORD", "vec4", "Coordinate of pixel center in screen space."},
    {"NORMAL", "vec3", "Input normal."},
    {"COLOR", "vec4", "Input color (output of fragment function)."},
    {"UV", "vec2", "UV from vertex function."},
    {"TEXTURE", "sampler2D", "Current texture in use."},
    {"TEXTURE_PIXEL_SIZE", "vec2", "Normalized pixel size of TEXTURE."},
    {"SCREEN_UV", "vec2", "Screen UV coordinate."},
    {"POINT_COORD", "vec2", "UV for Point Sprite."},

    {"LIGHT_COLOR", "vec4", "Color of the Light2D."},
    {"LIGHT_ENERGY", "float", "Energy multiplier of the Light2D."},
    {"LIGHT_POSITION", "vec3", "Position of the Light2D in screen space."},
    {"LIGHT_DIRECTION", "vec3", "Direction of the Light2D in screen space."},
    {"LIGHT_IS_DIRECTIONAL", "bool", "True if pass is DirectionalLight2D."},
    {"LIGHT_VERTEX", "vec3", "Pixel position in screen space (modified in fragment)."},
    {"SPECULAR_SHININESS", "vec4", "Specular shininess."},

    {"LIGHT", "vec4", "Output color for this Light2D."},
    {"SHADOW_MODULATE", "vec4", "Multiply shadows cast at this point by this color."}
};

// PARTICLES
static const BuiltinList PARTICLES_PROCESS = {
    {"COLOR", "vec4", "Particle color."},
    {"VELOCITY", "vec3", "Particle velocity."},
    {"MASS", "float", "Particle mass."},
    {"ACTIVE", "bool", "If false, particle is removed."},
    {"RESTART", "bool", "If true, particle restarts."},
    {"CUSTOM", "vec4", "Custom particle data."},
    {"TRANSFORM", "mat4", "Particle transform."},
    {"LIFETIME", "float", "Particle lifetime."},
    {"DELTA", "float", "Time since last frame."},
    {"TIME", "float", "Global time."},
    {"COLLISION_NORMAL", "vec3", "Collision normal."},
    {"COLLISION_DEPTH", "float", "Collision depth."},
    {"ATTRACTOR_FORCE", "vec3", "Force from attractors."}
};

// SKY
static const BuiltinList SKY_PROCESS = {
    {"COLOR", "vec3", "Output color."},
    {"ALPHA", "float", "Output alpha."},
    {"EYEDIR", "vec3", "Eye direction."},
    {"SKY_COORDS", "vec2", "Sky spherical coordinates."},
    {"SCREEN_UV", "vec2", "Screen UV."},
    {"TIME", "float", "Global time."},
    {"PI", "float", "Const PI."},
    {"TAU", "float", "Const TAU."},
    {"E", "float", "Const E."}
};

// FOG
static const BuiltinList FOG_PROCESS = {
    {"DENSITY", "float", "Output fog density."},
    {"ALBEDO", "vec3", "Output fog color."},
    {"EMISSION", "vec3", "Output emission."},
    {"WORLD_POSITION", "vec3", "World position of the cell."},
    {"OBJECT_POSITION", "vec3", "Object position."},
    {"SDF", "vec3", "Signed Distance Field value."},
    {"UVW", "vec3", "3D Texture UVW."},
    {"SIZE", "vec3", "Size of fog volume."},
    {"TIME", "float", "Global time."}
};

// -------------------------------------------------------------------------
// GLOBAL FUNCTIONS
// -------------------------------------------------------------------------
// GENERIC KEY LEGEND (from Godot Docs):
// vec_type       : float, vec2, vec3, vec4
// vec_int_type   : int, ivec2, ivec3, ivec4
// vec_uint_type  : uint, uvec2, uvec3, uvec4
// vec_bool_type  : bool, bvec2, bvec3, bvec4
// mat_type       : mat2, mat3, mat4
// gvec4_type     : vec4, ivec4, uvec4 (Matches sampler type)
// gsampler2D     : sampler2D, isampler2D, usampler2D
// gsampler2DArray: sampler2DArray, isampler2DArray, usampler2DArray
// gsampler3D     : sampler3D, isampler3D, usampler3D

static const BuiltinFuncList GLOBAL_FUNCTIONS = {

    {"radians", "vec_type", {"vec_type"}, "Convert degrees to radians."},
    {"degrees", "vec_type", {"vec_type"}, "Convert radians to degrees."},
    {"sin", "vec_type", {"vec_type"}, "Sine."},
    {"cos", "vec_type", {"vec_type"}, "Cosine."},
    {"tan", "vec_type", {"vec_type"}, "Tangent."},
    {"asin", "vec_type", {"vec_type"}, "Arc sine."},
    {"acos", "vec_type", {"vec_type"}, "Arc cosine."},
    {"atan", "vec_type", {"vec_type", "vec_type"}, "Arc tangent (y, x)."},
    {"atan", "vec_type", {"vec_type"}, "Arc tangent (y_over_x)."},
    {"sinh", "vec_type", {"vec_type"}, "Hyperbolic sine."},
    {"cosh", "vec_type", {"vec_type"}, "Hyperbolic cosine."},
    {"tanh", "vec_type", {"vec_type"}, "Hyperbolic tangent."},
    {"asinh", "vec_type", {"vec_type"}, "Arc hyperbolic sine."},
    {"acosh", "vec_type", {"vec_type"}, "Arc hyperbolic cosine."},
    {"atanh", "vec_type", {"vec_type"}, "Arc hyperbolic tangent."},

    {"pow", "vec_type", {"vec_type", "vec_type"}, "Power."},
    {"exp", "vec_type", {"vec_type"}, "Base-e exponential."},
    {"log", "vec_type", {"vec_type"}, "Natural logarithm."},
    {"exp2", "vec_type", {"vec_type"}, "Base-2 exponential."},
    {"log2", "vec_type", {"vec_type"}, "Base-2 logarithm."},
    {"sqrt", "vec_type", {"vec_type"}, "Square root."},
    {"inversesqrt", "vec_type", {"vec_type"}, "Inverse square root."},
    {"abs", "vec_type", {"vec_type"}, "Absolute value (float)."},
    {"abs", "vec_int_type", {"vec_int_type"}, "Absolute value (int)."},
    {"sign", "vec_type", {"vec_type"}, "Sign (float)."},
    {"sign", "vec_int_type", {"vec_int_type"}, "Sign (int)."},
    {"floor", "vec_type", {"vec_type"}, "Floor."},
    {"ceil", "vec_type", {"vec_type"}, "Ceiling."},
    {"trunc", "vec_type", {"vec_type"}, "Truncation."},
    {"round", "vec_type", {"vec_type"}, "Round to nearest."},
    {"roundEven", "vec_type", {"vec_type"}, "Round to even."},
    {"fract", "vec_type", {"vec_type"}, "Fractional part."},
    {"mod", "vec_type", {"vec_type", "vec_type"}, "Modulo."},
    {"mod", "vec_type", {"vec_type", "float"}, "Modulo with scalar."},
    {"modf", "vec_type", {"vec_type", "vec_type"}, "Split integer/fractional (out i)."}, // Note: 2nd arg is out
    {"min", "vec_type", {"vec_type", "vec_type"}, "Min (float)."},
    {"min", "vec_type", {"vec_type", "float"}, "Min (float scalar)."},
    {"min", "vec_int_type", {"vec_int_type", "vec_int_type"}, "Min (int)."},
    {"min", "vec_int_type", {"vec_int_type", "int"}, "Min (int scalar)."},
    {"min", "vec_uint_type", {"vec_uint_type", "vec_uint_type"}, "Min (uint)."},
    {"min", "vec_uint_type", {"vec_uint_type", "uint"}, "Min (uint scalar)."},
    {"max", "vec_type", {"vec_type", "vec_type"}, "Max (float)."},
    {"max", "vec_type", {"vec_type", "float"}, "Max (float scalar)."},
    {"max", "vec_int_type", {"vec_int_type", "vec_int_type"}, "Max (int)."},
    {"max", "vec_int_type", {"vec_int_type", "int"}, "Max (int scalar)."},
    {"max", "vec_uint_type", {"vec_uint_type", "vec_uint_type"}, "Max (uint)."},
    {"max", "vec_uint_type", {"vec_uint_type", "uint"}, "Max (uint scalar)."},
    {"clamp", "vec_type", {"vec_type", "vec_type", "vec_type"}, "Clamp (float)."},
    {"clamp", "vec_type", {"vec_type", "float", "float"}, "Clamp (float scalar)."},
    {"clamp", "vec_int_type", {"vec_int_type", "vec_int_type", "vec_int_type"}, "Clamp (int)."},
    {"clamp", "vec_int_type", {"vec_int_type", "int", "int"}, "Clamp (int scalar)."},
    {"clamp", "vec_uint_type", {"vec_uint_type", "vec_uint_type", "vec_uint_type"}, "Clamp (uint)."},
    {"clamp", "vec_uint_type", {"vec_uint_type", "uint", "uint"}, "Clamp (uint scalar)."},
    {"mix", "vec_type", {"vec_type", "vec_type", "vec_type"}, "Linear interpolate."},
    {"mix", "vec_type", {"vec_type", "vec_type", "float"}, "Linear interpolate (scalar)."},
    {"mix", "vec_type", {"vec_type", "vec_type", "vec_bool_type"}, "Select based on bool."},
    {"fma", "vec_type", {"vec_type", "vec_type", "vec_type"}, "Fused multiply-add."},
    {"step", "vec_type", {"vec_type", "vec_type"}, "Step."},
    {"step", "vec_type", {"float", "vec_type"}, "Step (scalar edge)."},
    {"smoothstep", "vec_type", {"vec_type", "vec_type", "vec_type"}, "Smoothstep."},
    {"smoothstep", "vec_type", {"float", "float", "vec_type"}, "Smoothstep (scalar edges)."},
    {"isnan", "vec_bool_type", {"vec_type"}, "Is NaN."},
    {"isinf", "vec_bool_type", {"vec_type"}, "Is infinity."},
    {"floatBitsToInt", "vec_int_type", {"vec_type"}, "Float bits to int."},
    {"floatBitsToUint", "vec_uint_type", {"vec_type"}, "Float bits to uint."},
    {"intBitsToFloat", "vec_type", {"vec_int_type"}, "Int bits to float."},
    {"uintBitsToFloat", "vec_type", {"vec_uint_type"}, "Uint bits to float."},
    {"ldexp", "vec_type", {"vec_type", "vec_int_type"}, "Assemble float from significand and exponent."},
    {"frexp", "vec_type", {"vec_type", "vec_int_type"}, "Extract significand and exponent."},
    
    {"length", "float", {"vec_type"}, "Length."},
    {"distance", "float", {"vec_type", "vec_type"}, "Distance."},
    {"dot", "float", {"vec_type", "vec_type"}, "Dot product."},
    {"cross", "vec3", {"vec3", "vec3"}, "Cross product."},
    {"normalize", "vec_type", {"vec_type"}, "Normalize."},
    {"reflect", "vec3", {"vec3", "vec3"}, "Reflect."},
    {"refract", "vec3", {"vec3", "vec3", "float"}, "Refract."},
    {"faceforward", "vec_type", {"vec_type", "vec_type", "vec_type"}, "Face forward."},

    {"matrixCompMult", "mat_type", {"mat_type", "mat_type"}, "Component-wise multiply."},
    {"outerProduct", "mat_type", {"vec_type", "vec_type"}, "Outer product."},
    {"transpose", "mat_type", {"mat_type"}, "Transpose."},
    {"determinant", "float", {"mat_type"}, "Determinant."},
    {"inverse", "mat_type", {"mat_type"}, "Inverse."},

    {"lessThan", "vec_bool_type", {"vec_type", "vec_type"}, "Less than (float)."},
    {"lessThan", "vec_bool_type", {"vec_int_type", "vec_int_type"}, "Less than (int)."},
    {"lessThan", "vec_bool_type", {"vec_uint_type", "vec_uint_type"}, "Less than (uint)."}, // Added for completeness
    {"greaterThan", "vec_bool_type", {"vec_type", "vec_type"}, "Greater than (float)."},
    {"greaterThan", "vec_bool_type", {"vec_int_type", "vec_int_type"}, "Greater than (int)."},
    {"greaterThan", "vec_bool_type", {"vec_uint_type", "vec_uint_type"}, "Greater than (uint)."},
    {"lessThanEqual", "vec_bool_type", {"vec_type", "vec_type"}, "Less equal (float)."},
    {"lessThanEqual", "vec_bool_type", {"vec_int_type", "vec_int_type"}, "Less equal (int)."},
    {"lessThanEqual", "vec_bool_type", {"vec_uint_type", "vec_uint_type"}, "Less equal (uint)."},
    {"greaterThanEqual", "vec_bool_type", {"vec_type", "vec_type"}, "Greater equal (float)."},
    {"greaterThanEqual", "vec_bool_type", {"vec_int_type", "vec_int_type"}, "Greater equal (int)."},
    {"greaterThanEqual", "vec_bool_type", {"vec_uint_type", "vec_uint_type"}, "Greater equal (uint)."},
    {"equal", "vec_bool_type", {"vec_type", "vec_type"}, "Equal (float)."},
    {"equal", "vec_bool_type", {"vec_int_type", "vec_int_type"}, "Equal (int)."},
    {"equal", "vec_bool_type", {"vec_uint_type", "vec_uint_type"}, "Equal (uint)."},
    {"equal", "vec_bool_type", {"vec_bool_type", "vec_bool_type"}, "Equal (bool)."},
    {"notEqual", "vec_bool_type", {"vec_type", "vec_type"}, "Not equal (float)."},
    {"notEqual", "vec_bool_type", {"vec_int_type", "vec_int_type"}, "Not equal (int)."},
    {"notEqual", "vec_bool_type", {"vec_uint_type", "vec_uint_type"}, "Not equal (uint)."},
    {"notEqual", "vec_bool_type", {"vec_bool_type", "vec_bool_type"}, "Not equal (bool)."},
    {"any", "bool", {"vec_bool_type"}, "Any component true."},
    {"all", "bool", {"vec_bool_type"}, "All components true."},
    {"not", "vec_bool_type", {"vec_bool_type"}, "Logical not."},

    {"textureSize", "ivec2", {"gsampler2D", "int"}, "Texture size (2D)."},
    {"textureSize", "ivec3", {"gsampler2DArray", "int"}, "Texture size (2D Array)."},
    {"textureSize", "ivec3", {"gsampler3D", "int"}, "Texture size (3D)."},
    {"textureSize", "ivec2", {"samplerCube", "int"}, "Texture size (Cube)."},
    {"textureSize", "ivec3", {"samplerCubeArray", "int"}, "Texture size (Cube Array)."}, // Assumed based on pattern, check docs if errors
    {"textureQueryLod", "vec2", {"gsampler2D", "vec2"}, "Query LOD (2D)."},
    {"textureQueryLod", "vec3", {"gsampler2DArray", "vec2"}, "Query LOD (2D Array)."},
    {"textureQueryLod", "vec2", {"gsampler3D", "vec3"}, "Query LOD (3D)."},
    {"textureQueryLod", "vec2", {"samplerCube", "vec3"}, "Query LOD (Cube)."},
    {"textureQueryLevels", "int", {"gsampler2D"}, "Query Levels (2D)."},
    {"textureQueryLevels", "int", {"gsampler2DArray"}, "Query Levels (2D Array)."},
    {"textureQueryLevels", "int", {"gsampler3D"}, "Query Levels (3D)."},
    {"textureQueryLevels", "int", {"samplerCube"}, "Query Levels (Cube)."},

    {"texture", "gvec4_type", {"gsampler2D", "vec2"}, "Sample texture."},
    {"texture", "gvec4_type", {"gsampler2D", "vec2", "float"}, "Sample texture with bias."},
    {"texture", "gvec4_type", {"gsampler2DArray", "vec3"}, "Sample array texture."},
    {"texture", "gvec4_type", {"gsampler2DArray", "vec3", "float"}, "Sample array texture with bias."},
    {"texture", "gvec4_type", {"gsampler3D", "vec3"}, "Sample 3D texture."},
    {"texture", "gvec4_type", {"gsampler3D", "vec3", "float"}, "Sample 3D texture with bias."},
    {"texture", "vec4", {"samplerCube", "vec3"}, "Sample cubemap."},
    {"texture", "vec4", {"samplerCube", "vec3", "float"}, "Sample cubemap with bias."},

    {"textureProj", "gvec4_type", {"gsampler2D", "vec3"}, "Proj texture (vec3)."},
    {"textureProj", "gvec4_type", {"gsampler2D", "vec4"}, "Proj texture (vec4)."},
    {"textureProj", "gvec4_type", {"gsampler2D", "vec3", "float"}, "Proj texture bias (vec3)."},
    {"textureProj", "gvec4_type", {"gsampler2D", "vec4", "float"}, "Proj texture bias (vec4)."},
    {"textureProj", "gvec4_type", {"gsampler3D", "vec4"}, "Proj 3D texture."},
    {"textureProj", "gvec4_type", {"gsampler3D", "vec4", "float"}, "Proj 3D texture bias."},

    {"textureLod", "gvec4_type", {"gsampler2D", "vec2", "float"}, "Sample explicit LOD."},
    {"textureLod", "gvec4_type", {"gsampler2DArray", "vec3", "float"}, "Sample array explicit LOD."},
    {"textureLod", "gvec4_type", {"gsampler3D", "vec3", "float"}, "Sample 3D explicit LOD."},
    {"textureLod", "vec4", {"samplerCube", "vec3", "float"}, "Sample cube explicit LOD."},

    {"texelFetch", "gvec4_type", {"gsampler2D", "ivec2", "int"}, "Fetch texel (2D)."},
    {"texelFetch", "gvec4_type", {"gsampler2DArray", "ivec3", "int"}, "Fetch texel (2D Array)."},
    {"texelFetch", "gvec4_type", {"gsampler3D", "ivec3", "int"}, "Fetch texel (3D)."},

    {"textureGather", "gvec4_type", {"gsampler2D", "vec2"}, "Gather."},
    {"textureGather", "gvec4_type", {"gsampler2D", "vec2", "int"}, "Gather comp."},
    {"textureGather", "gvec4_type", {"gsampler2DArray", "vec3"}, "Gather array."},
    {"textureGather", "gvec4_type", {"gsampler2DArray", "vec3", "int"}, "Gather array comp."},
    {"textureGather", "vec4", {"samplerCube", "vec3"}, "Gather cube."},
    {"textureGather", "vec4", {"samplerCube", "vec3", "int"}, "Gather cube comp."},

    {"dFdx", "vec_type", {"vec_type"}, "Derivative X."},
    {"dFdy", "vec_type", {"vec_type"}, "Derivative Y."},
    {"fwidth", "vec_type", {"vec_type"}, "Filter width."},
    {"dFdxCoarse", "vec_type", {"vec_type"}, "Derivative X Coarse."},
    {"dFdyCoarse", "vec_type", {"vec_type"}, "Derivative Y Coarse."},
    {"fwidthCoarse", "vec_type", {"vec_type"}, "Filter width Coarse."},
    {"dFdxFine", "vec_type", {"vec_type"}, "Derivative X Fine."},
    {"dFdyFine", "vec_type", {"vec_type"}, "Derivative Y Fine."},
    {"fwidthFine", "vec_type", {"vec_type"}, "Filter width Fine."},

    {"bitfieldExtract", "vec_int_type", {"vec_int_type", "int", "int"}, "Extract bits (int)."},
    {"bitfieldExtract", "vec_uint_type", {"vec_uint_type", "int", "int"}, "Extract bits (uint)."},
    {"bitfieldInsert", "vec_int_type", {"vec_int_type", "vec_int_type", "int", "int"}, "Insert bits (int)."},
    {"bitfieldInsert", "vec_uint_type", {"vec_uint_type", "vec_uint_type", "int", "int"}, "Insert bits (uint)."},
    {"bitfieldReverse", "vec_int_type", {"vec_int_type"}, "Reverse bits (int)."},
    {"bitfieldReverse", "vec_uint_type", {"vec_uint_type"}, "Reverse bits (uint)."},
    {"bitCount", "vec_int_type", {"vec_int_type"}, "Count bits (int)."},
    {"bitCount", "vec_int_type", {"vec_uint_type"}, "Count bits (uint)."},
    {"findLSB", "vec_int_type", {"vec_int_type"}, "Find LSB (int)."},
    {"findLSB", "vec_int_type", {"vec_uint_type"}, "Find LSB (uint)."},
    {"findMSB", "vec_int_type", {"vec_int_type"}, "Find MSB (int)."},
    {"findMSB", "vec_int_type", {"vec_uint_type"}, "Find MSB (uint)."},

    {"packHalf2x16", "uint", {"vec2"}, "Pack half 2x16."},
    {"unpackHalf2x16", "vec2", {"uint"}, "Unpack half 2x16."},
    {"packUnorm2x16", "uint", {"vec2"}, "Pack unorm 2x16."},
    {"unpackUnorm2x16", "vec2", {"uint"}, "Unpack unorm 2x16."},
    {"packSnorm2x16", "uint", {"vec2"}, "Pack snorm 2x16."},
    {"unpackSnorm2x16", "vec2", {"uint"}, "Unpack snorm 2x16."},
    {"packUnorm4x8", "uint", {"vec4"}, "Pack unorm 4x8."},
    {"unpackUnorm4x8", "vec4", {"uint"}, "Unpack unorm 4x8."},
    {"packSnorm4x8", "uint", {"vec4"}, "Pack snorm 4x8."},
    {"unpackSnorm4x8", "vec4", {"uint"}, "Unpack snorm 4x8."}
};

// -------------------------------------------------------------------------
// LOOKUP UTILITIES
// -------------------------------------------------------------------------

static const BuiltinList EMPTY_LIST = {};

inline const BuiltinList& get_builtins(ShaderType type, ShaderStage scope) 
{
    if (type == ShaderType::Spatial) {
        if (scope == ShaderStage::Vertex) return SPATIAL_VERTEX;
        if (scope == ShaderStage::Fragment) return SPATIAL_FRAGMENT;
        if (scope == ShaderStage::Light) return SPATIAL_LIGHT;
    } 
    else if (type == ShaderType::CanvasItem) {
        if (scope == ShaderStage::Vertex) return CANVAS_VERTEX;
        if (scope == ShaderStage::Fragment) return CANVAS_FRAGMENT;
        if (scope == ShaderStage::Light) return CANVAS_LIGHT;
    }
    else if (type == ShaderType::Particles) {
        if (scope == ShaderStage::Start || scope == ShaderStage::Process) return PARTICLES_PROCESS;
    }
    else if (type == ShaderType::Sky) {
        if (scope == ShaderStage::Sky) return SKY_PROCESS;
    }
    else if (type == ShaderType::Fog) {
        if (scope == ShaderStage::Fog) return FOG_PROCESS;
    }
    
    return EMPTY_LIST;
}

inline const BuiltinFuncList& get_builtin_functions() {
    return GLOBAL_FUNCTIONS;
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