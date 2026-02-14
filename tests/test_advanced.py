import pytest # type: ignore
import sys
import json

# -------------------------------------------------------------------------
# TEST 4: GO TO DEFINITION & REFERENCES
# -------------------------------------------------------------------------
def test_definition_and_references(lsp):

    shader_code = "shader_type spatial;\n" \
                  "uniform float my_uniform;\n" \
                  "void fragment() {\n" \
                  "float local = my_uniform;\n" \
                  "}"
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///refs.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    
    lsp.read_message() # Consume diagnostics

    # A. TEST DEFINITION
    # Request definition for 'my_uniform' used on Line 3.
    msg_id = lsp.send_message("textDocument/definition", {
        "textDocument": {"uri": "file:///refs.gdshader"},
        "position": {"line": 3, "character": 16} 
    })
    
    res = lsp.read_message()
    assert res["id"] == msg_id
    
    result = res["result"]
    if result is None:
        pytest.fail("Server returned null for definition lookup")

    locs = result if isinstance(result, list) else [result]
    target_line = locs[0]["range"]["start"]["line"]
    
    # STRICT CHECK: LSP is 0-based. Line 2 in editor is index 1.
    if target_line != 1:
        print(f"\n[DEBUG] Wrong definition line. Expected 1, got {target_line}", file=sys.stderr)
    
    assert target_line == 1

# -------------------------------------------------------------------------
# TEST 5: CUSTOM STRUCTS (Type Registry)
# -------------------------------------------------------------------------
def test_custom_structs(lsp):
    shader_code = "shader_type spatial;\n" \
                  "struct MyData { float power; };\n" \
                  "void fragment() {\n" \
                  "MyData d;\n" \
                  "d.\n" \
                  "}"
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///structs.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    lsp.read_message()

    msg_id = lsp.send_message("textDocument/completion", {
        "textDocument": {"uri": "file:///structs.gdshader"},
        "position": {"line": 4, "character": 2}
    })
    
    res = lsp.read_message()
    result = res["result"]
    items = result["items"] if isinstance(result, dict) else result
    labels = [item["label"] for item in items]
    
    assert "power" in labels

# -------------------------------------------------------------------------
# TEST 6: VECTOR SWIZZLE ERROR
# -------------------------------------------------------------------------
def test_vector_swizzle_validation(lsp):
    shader_code = "shader_type spatial;\n" \
                  "void fragment() {\n" \
                  "vec3 v = vec3(1.0);\n" \
                  "float f = v.xyzw;\n" \
                  "}"
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": "file:///swizzle.gdshader",
            "languageId": "gdshader",
            "version": 1,
            "text": shader_code
        }
    }, is_notification=True)
    
    res = lsp.read_message()
    diags = res["params"]["diagnostics"]
    
    error_messages = [d["message"] for d in diags]
    assert len(diags) > 0
    assert any("Swizzle" in m or "member" in m or "type" in m for m in error_messages)