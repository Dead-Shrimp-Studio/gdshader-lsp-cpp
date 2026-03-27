import pytest # type: ignore

# -------------------------------------------------------------------------
# TEST 7: HOVER DOCUMENTATION
# -------------------------------------------------------------------------
def test_hover_builtin(lsp):
    # Test that hovering over a built-in function returns documentation
    shader_code = """
    shader_type spatial;
    void fragment() {
        float x = sin(1.0);
    }
    """
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///hover.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    lsp.read_message() # Consume diagnostics

    # Hover over 'sin' (Line 3, Char 19 approx)
    msg_id = lsp.send_message("textDocument/hover", {
        "textDocument": {"uri": "file:///hover.gdshader"},
        "position": {"line": 3, "character": 19}
    })
    
    res = lsp.read_message()
    assert res["id"] == msg_id
    
    # We expect a result containing "sin" or description
    # Result format: { contents: "markdown string" } or { contents: { kind: "markdown", value: "..." } }
    result = res["result"]
    assert result is not None
    
    contents = result["contents"]
    assert contents is not None
    value = contents["value"] if isinstance(contents, dict) else contents
    assert value is not None

    assert "sin" in value or "sine" in value.lower()

# -------------------------------------------------------------------------
# TEST 8: DOCUMENT SYMBOLS (OUTLINE)
# -------------------------------------------------------------------------
def test_document_symbols(lsp):
    # Does the LSP provide a list of functions/structs in the file?
    shader_code = """
    shader_type spatial;
    uniform float speed;
    void vertex() {}
    void fragment() {}
    """
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///outline.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/documentSymbol", {
        "textDocument": {"uri": "file:///outline.gdshader"}
    })
    
    res = lsp.read_message()
    symbols = res["result"]
    
    names = [s["name"] for s in symbols]
    assert "vertex" in names
    assert "fragment" in names
    # Optional: Check if 'speed' uniform is listed
    # assert "speed" in names

# -------------------------------------------------------------------------
# TEST 9: SCOPE VALIDATION
# -------------------------------------------------------------------------
def test_variable_scope_rules(lsp):
    # Variables defined inside a block {} should not be visible outside
    shader_code = """
    shader_type spatial;
    void fragment() {
        if (true) {
            float hidden = 1.0;
        }
        ALBEDO = vec3(hidden); // Error expected here
    }
    """
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///scope.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    
    res = lsp.read_message()
    diags = res["params"]["diagnostics"]
    
    assert len(diags) > 0
    # Error should be about undefined identifier 'hidden'
    assert "hidden" in diags[0]["message"] or "undeclared" in diags[0]["message"]