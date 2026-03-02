
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
    const std::vector<std::string> VEC_TYPE             = {"vec2", "vec3", "vec4"};
    const std::vector<std::string> VEC_INT_TYPE         = {"ivec2", "ivec3", "ivec4"};
    const std::vector<std::string> VEC_UINT_TYPE        = {"uvec2", "uvec3", "uvec4"};
    const std::vector<std::string> VEC_BOOL_TYPE        = {"bvec2", "bvec3", "bvec4"};
    
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
        if (generic == "vec_type") return VEC_TYPE[idx];
        if (generic == "vec_int_type") return VEC_INT_TYPE[idx];
        if (generic == "vec_uint_type") return VEC_UINT_TYPE[idx];
        if (generic == "vec_bool_type") return VEC_BOOL_TYPE[idx];
        if (generic == "gvec4_type") return GVEC4_TYPE[idx];
        
        if (generic == "gsampler2D") return GSAMPLER_2D[idx];
        if (generic == "gsampler3D") return GSAMPLER_3D[idx];
        if (generic == "gsampler2DArray") return GSAMPLER_2D_ARR[idx];
        if (generic == "gsamplerCube") return GSAMPLER_CUBE[idx];
        if (generic == "gsamplerCubeArray") return GSAMPLER_CUBE_ARR[idx];
        if (generic == "gsamplerBuffer") return GSAMPLER_BUFFER[idx];
        if (generic == "gsampler2DRect") return GSAMPLER_2D_RECT[idx];
        if (generic == "gsampler2DMS") return GSAMPLER_2DMS[idx];
        if (generic == "gsampler2DMSArray") return GSAMPLER_2DMS_ARR[idx];
        
        if (generic == "mat_type") return MAT_TYPE[idx];
        
        return generic; // Fallback
    };

    auto registerConcrete = [&](const std::string& name, const std::string& ret, const std::vector<BuiltinArg>& args, const std::string& doc) -> void
    {
        TypePtr returnType = typeRegistry.getType(ret);

        std::vector<std::string> argNames;
        std::vector<TypePtr> argTypes;
        for (const auto& a : args) {
            argTypes.push_back(typeRegistry.getType(a.type));
            argNames.push_back(a.name);
        }

        Symbol s = symbols.createSymbol(name, typeRegistry.getUnknownType(), SymbolType::Function, {}, Mutability::ReadOnly, returnType, argTypes, argNames, true);
        s.doc_string = doc;
        symbols.add(s);
    };

    for (const auto& func : gdshader_lsp::generated::GLOBAL_FUNCTIONS) {
        
        bool isGeneric = false;

        for (const BuiltinArg& arg : func.args) {
            if (arg.type.find("vec_") != std::string::npos || arg.type.find("gvec") != std::string::npos) isGeneric = true;
            if (arg.type.find("gsampler") != std::string::npos) isGeneric = true;
            if (arg.type.find("mat_type") != std::string::npos) isGeneric = true;
        }

        if (func.returnProperties.type.find("vec_") != std::string::npos || func.returnProperties.type.find("gvec") != std::string::npos) isGeneric = true;
        else if (func.returnProperties.type.find("mat_type") != std::string::npos) isGeneric = true;
        else if (func.returnProperties.type.find("gsampler") != std::string::npos) isGeneric = true;

        if (isGeneric) 
        {
            // Safety measure: keep track of generated signatures to prevent duplicate registrations
            // if a new generic type bypasses our resolver.
            std::set<std::string> generatedSignatures;

            for (int i = 0; i < 3; i++) 
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