import pytest # type: ignore

def test_preprocessor_defines(lsp):
    shader_code = """
    shader_type spatial;
    #define MY_COLOR vec3(1.0)
    void fragment() {
        ALBEDO = MY_COLOR;
    }
    """
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///macro.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    
    # Consume diagnostics
    response = lsp.read_message()
    
    # If the preprocessor works, we should NOT get an "Unknown identifier" error for MY_COLOR
    diags = response["params"]["diagnostics"]
    
    # Filter for errors related to MY_COLOR
    errors = [d for d in diags if "MY_COLOR" in d["message"]]
    assert len(errors) == 0

def test_incremental_sync_error_injection(lsp):
    # 1. Start with perfectly valid code
    initial_code = (
        "shader_type spatial;\n"
        "void fragment() {\n"
        "    int a = 5;\n"
        "}\n"
    )
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///inc_error.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": initial_code
        }
    }, is_notification=True)

    # Consume the initial diagnostics (should be 0 errors)
    response = lsp.read_message()
    assert response["method"] == "textDocument/publishDiagnostics"
    assert len(response["params"]["diagnostics"]) == 1 # We expect an unused variable warning...

    # 2. Send an incremental change: Delete the semicolon
    # We replace the range spanning just the ';' with an empty string
    lsp.send_message("textDocument/didChange", {
        "textDocument": {
            "uri": "file:///inc_error.gdshader",
            "version": 2
        },
        "contentChanges": [
            {
                "range": {
                    "start": {"line": 2, "character": 13},
                    "end": {"line": 2, "character": 14}
                },
                "text": ""
            }
        ]
    }, is_notification=True)

    # 3. Read the new diagnostics triggered by the change
    response = lsp.read_message()
    assert response["method"] == "textDocument/publishDiagnostics"
    diags = response["params"]["diagnostics"]

    # The compiler should now complain about the missing semicolon
    assert len(diags) > 0
    assert "Expected ';'" in diags[0]["message"]
    assert diags[0]["range"]["start"]["line"] == 3

def test_incremental_sync_symbol_update(lsp):
    # 1. Start with an empty fragment function
    initial_code = (
        "shader_type spatial;\n"
        "void fragment() {\n"
        "    \n" # Line 2, character 4 is our insertion point
        "}\n"
    )
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///inc_symbols.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": initial_code
        }
    }, is_notification=True)

    # Consume initial diagnostics
    lsp.read_message()

    # 2. Incrementally insert a new struct variable
    # We insert "vec3 my_color = vec3(1.0);" at line 2, char 4
    inserted_text = "vec3 my_color = vec3(1.0);"
    lsp.send_message("textDocument/didChange", {
        "textDocument": {
            "uri": "file:///inc_symbols.gdshader",
            "version": 2
        },
        "contentChanges": [
            {
                "range": {
                    "start": {"line": 2, "character": 4},
                    "end": {"line": 2, "character": 4} # Start == End means INSERT
                },
                "text": inserted_text
            }
        ]
    }, is_notification=True)

    # Consume the diagnostics from the update
    lsp.read_message()

    # 3. Incrementally insert a dot to trigger completion
    # Our line is now: "    vec3 my_color = vec3(1.0);" (Length = 4 + 26 = 30)
    # Let's add a new line to type "my_color."
    lsp.send_message("textDocument/didChange", {
        "textDocument": {
            "uri": "file:///inc_symbols.gdshader",
            "version": 3
        },
        "contentChanges": [
            {
                "range": {
                    "start": {"line": 2, "character": 30},
                    "end": {"line": 2, "character": 30}
                },
                "text": "\n    my_color."
            }
        ]
    }, is_notification=True)

    # Consume the diagnostics from this update
    lsp.read_message()

    # 4. Request autocomplete immediately after the dot
    msg_id = lsp.send_message("textDocument/completion", {
        "textDocument": {"uri": "file:///inc_symbols.gdshader"},
        "position": {"line": 3, "character": 13} # After "    my_color."
    })

    # Read the completion response
    response = lsp.read_message()
    assert response["id"] == msg_id
    
    result = response["result"]
    items = result["items"] if isinstance(result, dict) else result
    
    # Verify that the symbol table recognizes 'my_color' as a vector 
    # and offers vector swizzling (x, y, z, etc.)
    labels = [item["label"] for item in items]
    assert "x" in labels
    assert "y" in labels

def test_cross_file_rename(lsp):
    # 1. Setup Origin File (The included file)
    utils_code = (
        "shader_type spatial;\n"
        "float calculate_lighting(float x) {\n"
        "    return x * 2.0;\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///utils.gdshaderinc",
            "languageId": "gdshader",
            "version": 1,
            "text": utils_code
        }
    }, is_notification=True)
    
    # Consume diagnostics for utils
    lsp.read_message()

    # 2. Setup Dependent File (The main shader)
    main_code = (
        "shader_type spatial;\n"
        "#include \"utils.gdshaderinc\"\n"
        "void fragment() {\n"
        "    float final_light = calculate_lighting(1.0);\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///main.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": main_code
        }
    }, is_notification=True)
    
    # Consume diagnostics for main
    lsp.read_message()

    # 3. Trigger Rename from the Dependent file
    # 'calculate_lighting' on Line 3 starts at character 24.
    # We place the cursor inside the function name (e.g., character 26).
    msg_id = lsp.send_message("textDocument/rename", {
        "textDocument": {"uri": "file:///main.gdshader"},
        "position": {"line": 3, "character": 26},
        "newName": "compute_lighting"
    })

    # 4. Read and Verify Response
    response = lsp.read_message()
    assert response["id"] == msg_id
    
    result = response.get("result")
    assert result is not None, "Rename request returned None. Symbol lookup might have failed."
    assert "changes" in result, "Result does not contain a 'changes' map."
    
    changes = result["changes"]
    
    # Verify that edits were mapped to BOTH files
    assert "file:///utils.gdshaderinc" in changes, "Edits missing for the origin file."
    assert "file:///main.gdshader" in changes, "Edits missing for the dependent file."

    # Verify origin file edits (Declaration)
    utils_edits = changes["file:///utils.gdshaderinc"]
    assert len(utils_edits) == 1, "Expected exactly 1 edit in the origin file."
    assert utils_edits[0]["newText"] == "compute_lighting"
    assert utils_edits[0]["range"]["start"]["line"] == 1 # Line 0 in utils.gdshaderinc

    # Verify dependent file edits (Usage)
    main_edits = changes["file:///main.gdshader"]
    assert len(main_edits) == 1, "Expected exactly 1 edit in the dependent file."
    assert main_edits[0]["newText"] == "compute_lighting"
    assert main_edits[0]["range"]["start"]["line"] == 3 # Line 3 in main.gdshader

def test_workspace_symbol_search(lsp):
    # 1. Setup File 1
    file1_code = (
        "shader_type spatial;\n"
        "uniform float unique_global_uniform;\n"
        "void test_function_one() {}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///workspace_file1.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": file1_code
        }
    }, is_notification=True)
    lsp.read_message()

    # 2. Setup File 2
    file2_code = (
        "shader_type spatial;\n"
        "struct UniqueStruct { float x; };\n"
        "void test_function_two() {}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///workspace_file2.gdshaderinc",
            "languageId": "gdshader",
            "version": 1,
            "text": file2_code
        }
    }, is_notification=True)
    lsp.read_message()

    # 3. Query for "unique"
    msg_id = lsp.send_message("workspace/symbol", {
        "query": "unique"
    })

    response = lsp.read_message()
    assert response["id"] == msg_id
    
    result = response.get("result")
    assert result is not None, "Workspace symbol request returned None"
    
    # We expect 'unique_global_uniform' and 'UniqueStruct' (testing case-insensitivity if implemented)
    names = [sym["name"] for sym in result]
    assert "unique_global_uniform" in names
    assert "UniqueStruct" in names
    assert "test_function_one" not in names # Shouldn't match query

    # 4. Query for "test_function" (should pull from both files)
    msg_id2 = lsp.send_message("workspace/symbol", {
        "query": "test_function"
    })
    
    response2 = lsp.read_message()
    result2 = response2.get("result")
    
    names2 = [sym["name"] for sym in result2]
    assert "test_function_one" in names2
    assert "test_function_two" in names2
    
    # 5. Verify the location URIs are correct
    func_two_sym = next(sym for sym in result2 if sym["name"] == "test_function_two")
    assert "workspace_file2.gdshaderinc" in func_two_sym["location"]["uri"], "Symbol returned incorrect file URI!"