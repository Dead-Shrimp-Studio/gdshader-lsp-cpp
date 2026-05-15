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