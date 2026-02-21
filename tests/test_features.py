import pytest # type: ignore

# -------------------------------------------------------------------------
# TEST 1: DIAGNOSTICS (Does it catch errors?)
# -------------------------------------------------------------------------
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

# -------------------------------------------------------------------------
# TEST 2: AUTOCOMPLETE (Does it know built-ins?)
# -------------------------------------------------------------------------
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

# -------------------------------------------------------------------------
# TEST 3: PREPROCESSOR (Does #define work?)
# -------------------------------------------------------------------------
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

# -------------------------------------------------------------------------
# TEST 4: INCREMENTAL SYNC (Does it patch the string and find an error?)
# -------------------------------------------------------------------------
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


# -------------------------------------------------------------------------
# TEST 5: INCREMENTAL SYNC (Does it update the symbol table?)
# -------------------------------------------------------------------------
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