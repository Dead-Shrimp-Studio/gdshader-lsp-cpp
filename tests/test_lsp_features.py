import pytest # type: ignore
import sys

def test_syntax_error_reporting(lsp):
    # 1. Open a file with a deliberate error (missing semicolon)
    shader_code = """
    shader_type spatial;
    void fragment() {
        vec3 x = vec3(1.0)
    }
    """
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///test.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)

    # 2. Expect a 'textDocument/publishDiagnostics' notification
    response = lsp.read_message()
    
    assert response["method"] == "textDocument/publishDiagnostics"
    diags = response["params"]["diagnostics"]
    
    # 3. Verify the error
    assert len(diags) > 0
    assert "Expected ';'" in diags[0]["message"]
    assert diags[0]["range"]["start"]["line"] == 4  # Check line number

def test_autocomplete_builtins(lsp):
    shader_code = """
    shader_type spatial;
    void fragment() {
        ALBE 
    }
    """
    
    # Open the file
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///completion.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    
    # Wait for initial diagnostics (ignore them)
    lsp.read_message()

    # Request completion at line 3, character 12 (after "ALBE")
    msg_id = lsp.send_message("textDocument/completion", {
        "textDocument": {"uri": "file:///completion.gdshader"},
        "position": {"line": 3, "character": 12}
    })

    # Read response
    response = lsp.read_message()
    
    assert response["id"] == msg_id
    
    # --- FIX START ---
    result = response["result"]
    
    # LSP Spec says result can be Array<CompletionItem> OR CompletionList
    if isinstance(result, dict):
        items = result["items"] # Extract list from CompletionList object
    else:
        items = result # It's already a list
    # --- FIX END ---
    
    # Look for "ALBEDO"
    labels = [item["label"] for item in items]
    assert "ALBEDO" in labels
    assert "ALPHA" in labels

def test_autocomplete_builtins(lsp):
    shader_code = "shader_type spatial;\nvoid fragment() {\n    ALBE \n}\n"
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///completion.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message() # Consume diags

    msg_id = lsp.send_message("textDocument/completion", {
        "textDocument": {"uri": "file:///completion.gdshader"}, "position": {"line": 3, "character": 12}
    })
    result = lsp.read_message()["result"]
    items = result["items"] if isinstance(result, dict) else result
    labels = [item["label"] for item in items]
    
    assert "ALBEDO" in labels

def test_custom_structs_completion(lsp):
    shader_code = "shader_type spatial;\nstruct MyData { float power; };\nvoid fragment() {\nMyData d;\nd.\n}"
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///structs.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/completion", {
        "textDocument": {"uri": "file:///structs.gdshader"}, "position": {"line": 4, "character": 2}
    })
    result = lsp.read_message()["result"]
    items = result["items"] if isinstance(result, dict) else result
    
    assert "power" in [item["label"] for item in items]

def test_hover_builtin(lsp):
    shader_code = "shader_type spatial;\nvoid fragment() {\n    float x = sin(1.0);\n}\n"
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///hover.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/hover", {
        "textDocument": {"uri": "file:///hover.gdshader"}, "position": {"line": 2, "character": 15}
    })
    contents = lsp.read_message()["result"]["contents"]
    value = contents["value"] if isinstance(contents, dict) else contents
    assert "sin" in value or "sine" in value.lower()

def test_definition_lookup(lsp):
    shader_code = "shader_type spatial;\nuniform float my_uniform;\nvoid fragment() {\nfloat local = my_uniform;\n}"
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///refs.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/definition", {
        "textDocument": {"uri": "file:///refs.gdshader"}, "position": {"line": 3, "character": 16} 
    })
    result = lsp.read_message()["result"]
    assert result is not None
    
    locs = result if isinstance(result, list) else [result]
    assert locs[0]["range"]["start"]["line"] == 1

def test_document_symbols_outline(lsp):
    shader_code = "shader_type spatial;\nuniform float speed;\nvoid vertex() {}\nvoid fragment() {}\n"
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///outline.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/documentSymbol", {
        "textDocument": {"uri": "file:///outline.gdshader"}
    })
    names = [s["name"] for s in lsp.read_message()["result"]]
    assert "vertex" in names and "fragment" in names

def test_rename_symbol(lsp):
    shader_code = (
        "shader_type spatial;\n"
        "uniform float my_val;\n"
        "void fragment() {\n"
        "    float x = my_val;\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///rename.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    # Rename 'my_val' at line 1, char 16 to 'new_val'
    msg_id = lsp.send_message("textDocument/rename", {
        "textDocument": {"uri": "file:///rename.gdshader"},
        "position": {"line": 3, "character": 16},
        "newName": "new_val"
    })
    
    res = lsp.read_message()["result"]
    assert res is not None
    assert "changes" in res
    
    changes = res["changes"]["file:///rename.gdshader"]
    assert len(changes) == 2 # Should change the declaration AND the usage in fragment()
    
    new_texts = [c["newText"] for c in changes]
    assert all(t == "new_val" for t in new_texts)

def test_document_highlight(lsp):
    shader_code = (
        "shader_type spatial;\n"
        "uniform float speed;\n"
        "void fragment() {\n"
        "    float x = speed * 2.0;\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///highlight.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/documentHighlight", {
        "textDocument": {"uri": "file:///highlight.gdshader"},
        "position": {"line": 3, "character": 16} # Hovering over 'speed' usage
    })
    
    res = lsp.read_message()["result"]
    assert res is not None
    assert len(res) == 2 # 1 definition + 1 usage

def test_signature_help(lsp):
    shader_code = (
        "shader_type spatial;\n"
        "void fragment() {\n"
        "    float x = mix(0.0, 1.0, 0.5);\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///sighelp.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    # Trigger inside the mix() parentheses
    msg_id = lsp.send_message("textDocument/signatureHelp", {
        "textDocument": {"uri": "file:///sighelp.gdshader"},
        "position": {"line": 2, "character": 25} 
    })
    
    res = lsp.read_message()["result"]
    assert res is not None
    assert len(res["signatures"]) > 0
    
    # Check that 'mix' is recognized and parameters are returned
    sig = res["signatures"][res["activeSignature"]]
    assert "mix" in sig["label"]
    assert len(sig["parameters"]) == 3

def test_folding_ranges(lsp):
    shader_code = (
        "shader_type spatial;\n"
        "void vertex() {\n"
        "    // some code\n"
        "}\n"
        "void fragment() {\n"
        "    if (true) {\n"
        "    }\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///folding.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/foldingRange", {
        "textDocument": {"uri": "file:///folding.gdshader"}
    })
    
    res = lsp.read_message()["result"]
    assert res is not None
    assert len(res) >= 3 # vertex block, fragment block, if block

def test_semantic_tokens(lsp):
    shader_code = "shader_type spatial;\nuniform float my_float;\n"
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///semantic.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/semanticTokens/full", {
        "textDocument": {"uri": "file:///semantic.gdshader"}
    })
    
    res = lsp.read_message()["result"]
    assert res is not None
    assert "data" in res
    assert len(res["data"]) > 0 # Should contain encoded integer array for the tokens

def test_inlay_hints(lsp):
    shader_code = (
        "shader_type spatial;\n"
        "void fragment() {\n"
        "    float x = smoothstep(0.1, 0.9, 0.5);\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///inlay.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/inlayHint", {
        "textDocument": {"uri": "file:///inlay.gdshader"},
        "range": {
            "start": {"line": 0, "character": 0},
            "end": {"line": 4, "character": 0}
        }
    })
    
    res = lsp.read_message()["result"]
    assert res is not None
    
    # We expect 3 hints for the 3 arguments of smoothstep
    labels = [hint["label"] for hint in res]
    assert len(labels) == 3
    assert all(label.endswith(":") for label in labels)

def test_document_colors(lsp):
    shader_code = (
        "shader_type spatial;\n"
        "uniform vec4 my_color : source_color = vec4(1.0, 0.5, 0.25, 1.0);\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///colors.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()

    # 1. Test finding the color
    msg_id = lsp.send_message("textDocument/documentColor", {
        "textDocument": {"uri": "file:///colors.gdshader"}
    })
    
    res = lsp.read_message()["result"]
    assert res is not None
    assert len(res) == 1
    
    color = res[0]["color"]
    assert color["red"] == 1.0
    assert color["green"] == 0.5
    assert color["blue"] == 0.25
    assert color["alpha"] == 1.0

    # 2. Test formatting a color presentation
    msg_id2 = lsp.send_message("textDocument/colorPresentation", {
        "textDocument": {"uri": "file:///colors.gdshader"},
        "color": {"red": 0.0, "green": 1.0, "blue": 0.0, "alpha": 1.0},
        "range": res[0]["range"]
    })
    
    res2 = lsp.read_message()["result"]
    assert res2 is not None
    labels = [p["label"] for p in res2]
    
    # The client should be offered standard string formats to inject back into the code
    assert any("vec4(0.0, 1.0, 0.0, 1.0)" in label for label in labels) or \
           any("vec3(0.0, 1.0, 0.0)" in label for label in labels)

def test_document_formatting(lsp):
    # Setup horribly formatted code
    unformatted_code = (
        "shader_type  spatial;\n"
        "uniform float my_val : hint_range(0.0, 1.0) = 0.5;\n"
        "// My Function Comment\n"
        "void fragment( ){\n"
        "if(my_val>0.0){\n"
        "ALBEDO=vec3(1.0,0.0,0.0);\n"
        "}else{\n"
        "  ALBEDO=vec3(0.0);\n"
        "}\n"
        "}\n"
    )
    
    # What the FormatterVisitor SHOULD output
    expected_code = (
        "shader_type spatial;\n"
        "\n"
        "uniform float my_val : hint_range(0.0, 1.0) = 0.5;\n"
        "\n"
        "// My Function Comment\n"
        "void fragment() {\n"
        "    if (my_val > 0.0) {\n"
        "        ALBEDO = vec3(1.0, 0.0, 0.0);\n"
        "    } else {\n"
        "        ALBEDO = vec3(0.0);\n"
        "    }\n"
        "}\n"
    )

    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///format_test.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": unformatted_code
        }
    }, is_notification=True)
    
    # Consume the compile diagnostics
    lsp.read_message()

    # Trigger Formatting Request
    msg_id = lsp.send_message("textDocument/formatting", {
        "textDocument": {"uri": "file:///format_test.gdshader"},
        "options": {
            "tabSize": 4,
            "insertSpaces": True
        }
    })

    # Read and Verify Response
    response = lsp.read_message()
    assert response["id"] == msg_id
    
    result = response.get("result")
    assert result is not None, "Formatting request returned None."
    assert len(result) == 1, "Expected exactly 1 text edit (full document replacement)."
    
    # Check if the output perfectly matches
    actual_code = result[0]["newText"]
    assert actual_code.strip() == expected_code.strip(), f"Formatting failed! \nExpected:\n{expected_code}\n\nGot:\n{actual_code}"

def test_call_hierarchy(lsp):
    # Setup a file with a clear call chain: fragment -> compute_lighting -> sin
    shader_code = (
        "shader_type spatial;\n"                      # Line 0
        "\n"                                          # Line 1
        "float compute_lighting(float x) {\n"         # Line 2
        "    return sin(x);\n"                        # Line 3
        "}\n"                                         # Line 4
        "\n"                                          # Line 5
        "void fragment() {\n"                         # Line 6
        "    float light = compute_lighting(1.0);\n"  # Line 7
        "}\n"                                         # Line 8
    )
    uri = "file:///call_hierarchy.gdshader"

    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": uri,
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    
    # Consume diagnostics
    lsp.read_message() 

    # ---------------------------------------------------------
    # STEP 1: PREPARE CALL HIERARCHY
    # ---------------------------------------------------------
    # Place the cursor right in the middle of 'compute_lighting' on Line 2
    msg_id_prep = lsp.send_message("textDocument/prepareCallHierarchy", {
        "textDocument": {"uri": uri},
        "position": {"line": 2, "character": 10} 
    })

    res_prep = lsp.read_message()
    assert res_prep["id"] == msg_id_prep
    items = res_prep.get("result")
    
    assert items is not None and len(items) == 1, "PrepareCallHierarchy should return exactly 1 item."
    target_item = items[0]
    assert target_item["name"] == "compute_lighting", f"Expected 'compute_lighting', got '{target_item['name']}'"
    
    # ---------------------------------------------------------
    # STEP 2: INCOMING CALLS
    # ---------------------------------------------------------
    # Ask the server: "Who calls compute_lighting?"
    msg_id_in = lsp.send_message("callHierarchy/incomingCalls", {
        "item": target_item
    })

    res_in = lsp.read_message()
    assert res_in["id"] == msg_id_in
    incoming = res_in.get("result")
    
    assert incoming is not None and len(incoming) == 1, "Expected exactly 1 incoming call."
    
    # Verify the caller is 'fragment'
    assert incoming[0]["from"]["name"] == "fragment", "Expected caller to be 'fragment'."
    
    # Verify the exact range where the call happened inside fragment() (Line 7)
    assert len(incoming[0]["fromRanges"]) == 1
    call_range = incoming[0]["fromRanges"][0]
    assert call_range["start"]["line"] == 7, "Call range line mapping is incorrect."

    # ---------------------------------------------------------
    # STEP 3: OUTGOING CALLS
    # ---------------------------------------------------------
    msg_id_out = lsp.send_message("callHierarchy/outgoingCalls", {
        "item": target_item
    })

    res_out = lsp.read_message()
    assert res_out["id"] == msg_id_out
    outgoing = res_out.get("result")
    
    assert outgoing is not None and len(outgoing) == 1, "Expected exactly 1 outgoing call."
    
    # Verify it calls the built-in 'sin' function
    assert outgoing[0]["to"]["name"] == "sin", "Expected outgoing call to be 'sin'."
    
    # Verify the exact range where 'sin' was called inside compute_lighting() (Line 3)
    assert len(outgoing[0]["fromRanges"]) == 1
    out_range = outgoing[0]["fromRanges"][0]
    assert out_range["start"]["line"] == 3, "Outgoing call range line mapping is incorrect."

def test_builtin_optional_arguments(lsp):
    # Setup test with valid bounds (2 and 3 args) and invalid bounds (1 and 4 args)
    shader_code = (
        "shader_type spatial;\n"
        "uniform sampler2D my_tex;\n"
        "void fragment() {\n"
        "    vec2 my_uv = vec2(0.5);\n"
        "    vec4 col1 = texture(my_tex, my_uv);\n"           # Line 4: Valid (2 args, omitted bias)
        "    vec4 col2 = texture(my_tex, my_uv, 1.5);\n"      # Line 5: Valid (3 args, provided bias)
        "    vec4 col3 = texture(my_tex);\n"                  # Line 6: Invalid (1 arg, too few)
        "    vec4 col4 = texture(my_tex, my_uv, 1.5, 0.0);\n" # Line 7: Invalid (4 args, too many)
        "}\n"
    )
    
    # Open the document via LSP
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///test_optional_args.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    
    # Read the diagnostics published by the server
    response = lsp.read_message()
    assert response["method"] == "textDocument/publishDiagnostics"
    
    # Filter for actual errors (severity 1), ignoring warnings like unused variables
    diags = [d for d in response["params"]["diagnostics"] if d.get("severity", 1) == 1]

    # We rigorously expect exactly 2 errors for the invalid calls on Lines 6 and 7
    assert len(diags) == 2, f"Expected exactly 2 errors, got {len(diags)}: {diags}"

    # Extract the line numbers where errors occurred
    error_lines = {d["range"]["start"]["line"] for d in diags}

    # Verify the invalid calls throw errors
    assert 6 in error_lines, "Expected 'Invalid argument count' error on line 6 (too few args)"
    assert 7 in error_lines, "Expected 'Invalid argument count' error on line 7 (too many args)"
    
    # Verify the valid calls do NOT throw errors (ensures our overload logic works)
    assert 4 not in error_lines, "False positive error on line 4 (valid 2-argument texture call)"
    assert 5 not in error_lines, "False positive error on line 5 (valid 3-argument texture call)"
    
    # Ensure the errors are specifically argument count errors
    for d in diags:
        assert "Invalid argument count" in d["message"], f"Unexpected error message: {d['message']}"

def test_blist_shader_support(lsp):
    # Valid blist shader should produce no diagnostics
    shader_code = (
        "shader_type blist;\n"
        "render_mode blend_add;\n"
        "uniform sampler2D source_tex : hint_blit_source0;\n"
        "void blit() {\n"
        "    COLOR0 = texture(source_tex, UV) * MODULATE;\n"
        "}\n"
    )
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///blist.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)

    response = lsp.read_message()
    assert response["method"] == "textDocument/publishDiagnostics"
    errors = [d for d in response["params"]["diagnostics"] if d.get("severity", 1) == 1]
    assert len(errors) == 0, f"Expected no errors for valid blist shader, got: {errors}"

def test_blist_autocomplete_builtins(lsp):
    shader_code = "shader_type blist;\nvoid blit() {\n    COL \n}\n"
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": "file:///blist_completion.gdshader", "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)
    lsp.read_message()  # Consume diags

    msg_id = lsp.send_message("textDocument/completion", {
        "textDocument": {"uri": "file:///blist_completion.gdshader"}, "position": {"line": 2, "character": 7}
    })
    result = lsp.read_message()["result"]
    items = result["items"] if isinstance(result, dict) else result
    labels = [item["label"] for item in items]

    assert "COLOR0" in labels
    assert "COLOR1" in labels
    assert "COLOR2" in labels
    assert "COLOR3" in labels
    assert "MODULATE" in labels
    assert "FRAGCOORD" in labels
