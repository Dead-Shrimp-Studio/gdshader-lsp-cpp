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