import json
import sys
import os

def main():
    if len(sys.argv) < 3:
        print("Usage: python generate_builtins.py [input_jsons...] out.hpp out.cpp")
        sys.exit(1)

    source_files = sys.argv[1:-2]
    out_hpp = sys.argv[-2]
    out_cpp = sys.argv[-1]

    os.makedirs(os.path.dirname(out_hpp), exist_ok=True)

    # 1. Start writing the Header (.hpp)
    hpp_content = [
        "// AUTO-GENERATED. DO NOT EDIT.\n",
        "#pragma once",
        "namespace gdshader_lsp::generated {",
        ""
    ]
    
    # 2. Start writing the Source (.cpp)
    cpp_content = [
        "// AUTO-GENERATED. DO NOT EDIT.\n",
        '#include "gdshader/builtins.hpp"',
        f'#include "generated/{os.path.basename(out_hpp)}"', 
        "namespace gdshader_lsp::generated {",
        ""
    ]

    for json_file in source_files:
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            
        if not data:
            continue
            
        var_name = os.path.basename(json_file).replace('.json', '').upper()
        
        # Detect if this JSON contains Variables or Functions
        is_function_list = "return" in data[0]
        
        list_type = "BuiltinFuncList" if is_function_list else "BuiltinList"
        
        # Declare in header
        hpp_content.append(f"extern const {list_type} {var_name};")
        
        # Define in CPP
        cpp_content.append(f"const {list_type} {var_name} = {{")
        
        for item in data:
            if is_function_list:
                # --- PARSE FUNCTION ---
                name = item.get("name", "")
                doc = item.get("doc", "").replace('"', '\\"')
                
                ret = item.get("return", {})
                ret_type = ret.get("type", "")
                ret_doc = ret.get("doc", "").replace('"', '\\"')
                
                args_cpp = []
                
                for arg in item.get("arguments", []):
                    a_name = arg.get("name", "")
                    a_type = arg.get("type", "")
                    a_doc = arg.get("doc", "").replace('"', '\\"')
                    args_cpp.append(f'{{"{a_name}", "{a_type}", "{a_doc}"}}')
                
                args_str = "{" + ", ".join(args_cpp) + "}"

                # Struct layout: name, returnProperties, args, doc, argTypes (dep), returnType (dep)
                cpp_content.append(f'    {{"{name}", {{"{ret_type}", "{ret_doc}"}}, {args_str}, "{doc}"}},')
                
            else:
                # --- PARSE VARIABLE ---
                name = item.get("name", "")
                v_type = item.get("type", "")
                doc = item.get("doc", "").replace('"', '\\"')
                qualifier = item.get("qualifier", "in")
                
                # Struct layout: name, type, doc, qualifier
                cpp_content.append(f'    {{"{name}", "{v_type}", "{doc}", "{qualifier}"}},')
            
        cpp_content.append("};\n")

    hpp_content.append("\n}")
    cpp_content.append("\n}")

    # 3. Write to disk
    with open(out_hpp, 'w', encoding='utf-8') as f:
        f.write("\n".join(hpp_content))

    with open(out_cpp, 'w', encoding='utf-8') as f:
        f.write("\n".join(cpp_content))

if __name__ == "__main__":
    main()