#ifndef TYPE_REGISTRY_HPP
#define TYPE_REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

#include "gdshader/semantics/types.hpp"
#include "gdshader/builtins.hpp"
#include "utils/logger.hpp"

namespace gdshader_lsp {

class TypeRegistry {

private:

    std::unordered_map<std::string, TypePtr> types;
    TypePtr unknownType;

    bool isValidSwizzle(const std::string& swizzle, int vectorSize) const {
        if (swizzle.length() > 4) return false;
        
        const std::string xyzw = "xyzw";
        const std::string rgba = "rgba";
        const std::string stpq = "stpq"; // Texture coords

        int setIdx = -1; // 0=xyzw, 1=rgba, 2=stpq

        for (char c : swizzle) {
            // Find which set this char belongs to and its index (0-3)
            size_t idx = -1;
            int currentSet = -1;

            if ((idx = xyzw.find(c)) != std::string::npos) currentSet = 0;
            else if ((idx = rgba.find(c)) != std::string::npos) currentSet = 1;
            else if ((idx = stpq.find(c)) != std::string::npos) currentSet = 2;

            if (currentSet == -1) {
                SPDLOG_TRACE("[TypeReg] Invalid swizzle char: '{}'", c);
                return false; 
            }
            if (setIdx != -1 && setIdx != currentSet) {
                SPDLOG_TRACE("[TypeReg] Mixed swizzle sets in '{}'", swizzle);
                return false; 
            }
            if (idx >= (size_t)vectorSize) {
                SPDLOG_TRACE("[TypeReg] Swizzle component out of bounds: '{}' on size {}", c, vectorSize);
                return false; 
            }
            setIdx = currentSet;
        }
        return true;
    }

    void registerBuiltins() 
    {
        SPDLOG_TRACE("[TypeReg] Registering Built-in types...");

        unknownType = std::make_shared<Type>();
        unknownType->kind = TypeKind::UNKNOWN;
        unknownType->name = "unknown";

        // Helper
        auto add = [&](const std::string& n, TypeKind k, int comps = 1, TypePtr base = nullptr) {
            auto t = std::make_shared<Type>();
            t->kind = k;
            t->name = n;
            t->componentCount = comps;
            t->baseType = base;
            types[n] = t;
            return t;
        };

        add("void", TypeKind::SCALAR);

        auto floatT = add("float", TypeKind::SCALAR);
        auto intT   = add("int", TypeKind::SCALAR);
        auto uintT   = add("uint", TypeKind::SCALAR);
        auto boolT = add("bool", TypeKind::SCALAR);

        auto vec2T = add("vec2", TypeKind::VECTOR, 2, floatT);
        auto vec3T = add("vec3", TypeKind::VECTOR, 3, floatT);
        auto vec4T = add("vec4", TypeKind::VECTOR, 4, floatT);

        add("ivec2", TypeKind::VECTOR, 2, intT);
        add("ivec3", TypeKind::VECTOR, 3, intT);
        add("ivec4", TypeKind::VECTOR, 4, intT);

        add("uvec2", TypeKind::VECTOR, 2, uintT);
        add("uvec3", TypeKind::VECTOR, 3, uintT);
        add("uvec4", TypeKind::VECTOR, 4, uintT);

        add("bvec2", TypeKind::VECTOR, 2, boolT);
        add("bvec3", TypeKind::VECTOR, 3, boolT);
        add("bvec4", TypeKind::VECTOR, 4, boolT);

        add("mat2", TypeKind::MATRIX, 2, vec2T);
        add("mat3", TypeKind::MATRIX, 3, vec3T);
        add("mat4", TypeKind::MATRIX, 4, vec4T);

        add("sampler2D", TypeKind::SAMPLER);
        add("isampler2D", TypeKind::SAMPLER);
        add("usampler2D", TypeKind::SAMPLER);

        add("sampler3D", TypeKind::SAMPLER);
        add("isampler3D", TypeKind::SAMPLER);
        add("usampler3D", TypeKind::SAMPLER);

        add("samplerCube", TypeKind::SAMPLER);
        add("samplerCubeArray", TypeKind::SAMPLER);
    }

public:

    TypeRegistry() {
        registerBuiltins();
    }

    /**
     * @brief Get the Type object ptr (flyweight pattern).
     * 
     * @param name 
     * @return TypePtr 
     */
    TypePtr getType(const std::string& name) 
    {
        if (types.count(name)) return types[name];
        size_t bracketStart = name.find('[');
        size_t bracketEnd = name.rfind(']');
        
        if (bracketStart != std::string::npos && bracketEnd != std::string::npos && bracketEnd > bracketStart) {
            
            std::string baseName = name.substr(0, bracketStart);
            std::string sizeStr = name.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
            
            try {
                int size = std::stoi(sizeStr);
                TypePtr base = getType(baseName); // Recursively get base (e.g. "vec3")
                
                if (base->kind != TypeKind::UNKNOWN) {
                    SPDLOG_TRACE("[TypeReg] Resolving Array Type: {}[{}]", baseName, size);
                    return getArrayType(base, size); // This caches and returns the array type
                }
            } catch (...) {
                SPDLOG_WARN("[TypeReg] Invalid array size parsing: {}", sizeStr);
            }
        }

        return unknownType;
    }

    TypePtr getUnknownType() const { return unknownType; }

    TypePtr getArrayType(TypePtr base, int size) 
    {
        // Construct a unique name for caching: "float[5]"
        std::string key = base->name + "[" + std::to_string(size) + "]";
        if (types.count(key)) return types[key];

        auto t = std::make_shared<Type>();
        t->kind = TypeKind::ARRAY;
        t->name = key;
        t->baseType = base;
        t->arraySize = size;
        
        types[key] = t;
        return t;
    }

    void registerStruct(const std::string& name, const std::vector<std::pair<std::string, TypePtr>>& members) 
    {
        SPDLOG_INFO("[TypeReg] Registering Struct: {}", name);

        auto t = std::make_shared<Type>();
        t->kind = TypeKind::STRUCT;
        t->name = name;
        t->members = members;
        types[name] = t;
    }

    TypePtr getMemberType(TypePtr base, const std::string& member) {
        
        if (base->kind == TypeKind::STRUCT) {
            for (const auto& m : base->members) {
                if (m.first == member) return m.second;
            }
            SPDLOG_TRACE("[TypeReg] Member '{}' not found in struct '{}'", member, base->name);
            return unknownType;
        }

        if (base->kind == TypeKind::VECTOR) {
            int len = member.length();
            if (isValidSwizzle(member, base->componentCount)) {
                // 1. Determine the prefix based on the base type's scalar type
                std::string prefix;
                if (base->baseType->name == "int") prefix = "i";
                else if (base->baseType->name == "uint") prefix = "u";
                else if (base->baseType->name == "bool") prefix = "b";

                // 2. Return the correct vector size
                if (len == 1) return base->baseType;
                if (len >= 2 && len <= 4) {
                    return getType(prefix + "vec" + std::to_string(len));
                }
            } else {
                SPDLOG_TRACE("[TypeReg] Invalid Swizzle on {}: .{}", base->name, member);
            }
        }
        return unknownType;
    }

};

}


#endif