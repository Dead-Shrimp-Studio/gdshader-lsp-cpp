
#include "gdshader/semantics/semantic_analyzer.hpp"
#include "gdshader/semantics/visitors/preprocessor_visitor.hpp"
#include "gdshader/semantics/visitors/symbol_declaration_visitor.hpp"
#include "gdshader/semantics/visitors/type_checking_visitor.hpp"
#include "gdshader/semantics/visitors/linting_visitor.hpp"
#include "server/project_manager.hpp"

#include <set>

namespace gdshader_lsp {

AnalysisResult SemanticAnalyzer::analyze(const ProgramNode* ast) 
{
    symbols = SymbolTable();
    typeRegistry = TypeRegistry();

    diagnostics.clear();
    tokens.clear();

    registerGlobalFunctions();

    if (ast) {

        PreprocessorVisitor prepVisitor(symbols, typeRegistry, diagnostics, currentFilePath, processedFiles);
        const_cast<ProgramNode*>(ast)->accept(prepVisitor);

        SymbolDeclarationVisitor declVisitor(symbols, typeRegistry, diagnostics);
        const_cast<ProgramNode*>(ast)->accept(declVisitor);

        TypeCheckingVisitor typeVisitor(symbols, typeRegistry, diagnostics, tokens);
        const_cast<ProgramNode*>(ast)->accept(typeVisitor);

        LintingVisitor lintVisitor(symbols, diagnostics);
        const_cast<ProgramNode*>(ast)->accept(lintVisitor);
    }
    
    return { std::move(symbols), std::move(typeRegistry), diagnostics, tokens };
}

void SemanticAnalyzer::registerGlobalFunctions()
{
    // Added the scalar base types to the generic vector expansions
    const std::vector<std::string> VEC_TYPE             = {"float", "vec2", "vec3", "vec4"};
    const std::vector<std::string> VEC_INT_TYPE         = {"int", "ivec2", "ivec3", "ivec4"};
    const std::vector<std::string> VEC_UINT_TYPE        = {"uint", "uvec2", "uvec3", "uvec4"};
    const std::vector<std::string> VEC_BOOL_TYPE        = {"bool", "bvec2", "bvec3", "bvec4"};
    
    // Expanded Sampler definitions to catch all Godot variations
    const std::vector<std::string> GSAMPLER_2D          = {"sampler2D", "isampler2D", "usampler2D"};
    const std::vector<std::string> GSAMPLER_2D_ARR      = {"sampler2DArray", "isampler2DArray", "usampler2DArray"};
    const std::vector<std::string> GSAMPLER_3D          = {"sampler3D", "isampler3D", "usampler3D"};
    const std::vector<std::string> GSAMPLER_CUBE        = {"samplerCube", "isamplerCube", "usamplerCube"};
    const std::vector<std::string> GSAMPLER_CUBE_ARR    = {"samplerCubeArray", "isamplerCubeArray", "usamplerCubeArray"};
    const std::vector<std::string> GSAMPLER_BUFFER      = {"samplerBuffer", "isamplerBuffer", "usamplerBuffer"};
    const std::vector<std::string> GSAMPLER_2D_RECT     = {"sampler2DRect", "isampler2DRect", "usampler2DRect"};
    const std::vector<std::string> GSAMPLER_2DMS        = {"sampler2DMS", "isampler2DMS", "usampler2DMS"};
    const std::vector<std::string> GSAMPLER_2DMS_ARR    = {"sampler2DMSArray", "isampler2DMSArray", "usampler2DMSArray"};
    
    const std::vector<std::string> GVEC4_TYPE           = {"vec4", "ivec4", "uvec4"}; 
    const std::vector<std::string> MAT_TYPE             = {"mat2", "mat3", "mat4"};

    auto resolveGeneric = [&](const std::string& generic, int idx) -> std::string 
    {
        // Safety bounds check just in case variations overflow the array size
        auto safeGet = [&](const std::vector<std::string>& vec) { return vec[idx % vec.size()]; };

        if (generic == "vec_type") return safeGet(VEC_TYPE);
        if (generic == "vec_int_type") return safeGet(VEC_INT_TYPE);
        if (generic == "vec_uint_type") return safeGet(VEC_UINT_TYPE);
        if (generic == "vec_bool_type") return safeGet(VEC_BOOL_TYPE);
        if (generic == "gvec4_type") return safeGet(GVEC4_TYPE);
        
        if (generic == "gsampler2D") return safeGet(GSAMPLER_2D);
        if (generic == "gsampler3D") return safeGet(GSAMPLER_3D);
        if (generic == "gsampler2DArray") return safeGet(GSAMPLER_2D_ARR);
        if (generic == "gsamplerCube") return safeGet(GSAMPLER_CUBE);
        if (generic == "gsamplerCubeArray") return safeGet(GSAMPLER_CUBE_ARR);
        if (generic == "gsamplerBuffer") return safeGet(GSAMPLER_BUFFER);
        if (generic == "gsampler2DRect") return safeGet(GSAMPLER_2D_RECT);
        if (generic == "gsampler2DMS") return safeGet(GSAMPLER_2DMS);
        if (generic == "gsampler2DMSArray") return safeGet(GSAMPLER_2DMS_ARR);
        
        if (generic == "mat_type") return safeGet(MAT_TYPE);
        
        return generic; // Fallback
    };

    auto registerConcrete = [&](const std::string& name, const std::string& ret, const std::vector<BuiltinArg>& args, const std::string& doc) -> void
    {
        TypePtr returnType = typeRegistry.getType(ret);

        std::vector<std::string> argNames;
        std::vector<TypePtr> argTypes;
        for (const auto& a : args) {
            TypePtr argType = typeRegistry.getType(a.type);
            GDSHADER_ERROR_IF(argType->kind == TypeKind::UNKNOWN, "Function registering with unkwown argument type {}", a.type);
            argTypes.push_back(argType);
            argNames.push_back(a.name);
        }

        Symbol s = symbols.createSymbol(name, typeRegistry.getUnknownType(), SymbolType::Function, {0,0,0,0}, Mutability::ReadOnly, returnType, argTypes, argNames, true);
        s.doc_string = doc;
        symbols.add(s);
    };

    for (const auto& func : gdshader_lsp::generated::GLOBAL_FUNCTIONS) {
        
        bool isGeneric = false;
        int variations = 1;

        // Determine if we need to iterate 3 times (samplers/matrices) or 4 times (scalars + vectors)
        auto checkGeneric = [&](const std::string& type) {
            if (type.find("vec_") != std::string::npos) {
                isGeneric = true;
                variations = std::max(variations, 4); // Need 4 iterations: scalar, vec2, vec3, vec4
            } else if (type.find("gsampler") != std::string::npos || type.find("gvec4") != std::string::npos || type.find("mat_type") != std::string::npos) {
                isGeneric = true;
                variations = std::max(variations, 3); // Need 3 iterations: e.g. sampler2D, isampler2D, usampler2D
            }
        };

        checkGeneric(func.returnProperties.type);
        for (const BuiltinArg& arg : func.args) {
            checkGeneric(arg.type);
        }

        if (isGeneric) 
        {
            // Safety measure: keep track of generated signatures to prevent duplicate registrations
            std::set<std::string> generatedSignatures;

            for (int i = 0; i < variations; i++) 
            {
                std::string r = resolveGeneric(func.returnProperties.type, i);
                std::vector<BuiltinArg> argsResolved;
                
                std::string signatureKey = r + "(";
                for (const BuiltinArg& arg : func.args)
                {
                    std::string resolvedArg = resolveGeneric(arg.type, i);
                    argsResolved.push_back({arg.name, resolvedArg, arg.doc});
                    signatureKey += resolvedArg + ",";
                }
                signatureKey += ")";

                // Only register if we haven't already registered this exact signature
                if (generatedSignatures.insert(signatureKey).second) {
                    registerConcrete(func.name, r, argsResolved, func.doc);
                }
            }
        }
        else {
            registerConcrete(func.name, func.returnProperties.type, func.args, func.doc);
        }
    }
}

} // end of namespace