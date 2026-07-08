import pytest # type: ignore

def test_quick_fix_missing_semicolon(lsp):
    uri = "file:///action_semi.gdshader"
    shader_code = (
        "shader_type spatial;\n"
        "void fragment() {\n"
        "    float x = 1.0\n" # Missing semicolon here
        "}\n"
    )
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": uri, "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)

    # 1. Catch the diagnostic
    diag_res = lsp.read_message()
    diags = diag_res["params"]["diagnostics"]
    
    # Find the GDS1001 (MissingSemicolon) diagnostic
    target_diag = next((d for d in diags if str(d.get("code")) == "GDS1001"), None)
    assert target_diag is not None, "Did not receive missing semicolon diagnostic."

    # 2. Request Code Actions for that specific diagnostic
    msg_id = lsp.send_message("textDocument/codeAction", {
        "textDocument": {"uri": uri},
        "range": target_diag["range"],
        "context": {
            "diagnostics": [target_diag]
        }
    })

    # 3. Verify the Code Action Response
    res = lsp.read_message()["result"]
    assert res is not None
    assert len(res) > 0

    # Ensure our specific fix is in the list
    action = next((a for a in res if "Insert missing ';'" in a["title"]), None)
    assert action is not None
    assert action["kind"] == "quickfix"
    
    # 4. Verify the WorkspaceEdit payload
    changes = action["edit"]["changes"][uri]
    assert len(changes) == 1
    assert changes[0]["newText"] == ";"
    # Insertion should happen at the end of the diagnostic range
    assert changes[0]["range"]["start"] == target_diag["range"]["end"]

def test_quick_fix_void_return(lsp):
    uri = "file:///action_void.gdshader"
    shader_code = (
        "shader_type spatial;\n"
        "void fragment() {\n"
        "    return vec3(1.0);\n" # Invalid return
        "}\n"
    )
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": uri, "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)

    diags = lsp.read_message()["params"]["diagnostics"]
    target_diag = next((d for d in diags if str(d.get("code")) == "GDS2012"), None)
    assert target_diag is not None, "Did not receive VoidCannotReturnValue diagnostic."

    msg_id = lsp.send_message("textDocument/codeAction", {
        "textDocument": {"uri": uri},
        "range": target_diag["range"],
        "context": {
            "diagnostics": [target_diag]
        }
    })

    res = lsp.read_message()["result"]
    action = next((a for a in res if "empty return" in a["title"].lower()), None)
    assert action is not None
    
    changes = action["edit"]["changes"][uri]
    assert len(changes) == 1
    assert changes[0]["newText"] == "return;"

def test_refactor_extract_magic_number(lsp):
    uri = "file:///action_extract.gdshader"
    shader_code = (
        "shader_type spatial;\n"           # Line 0
        "void fragment() {\n"              # Line 1
        "    ALBEDO = vec3(3.14159);\n"    # Line 2 (Cursor on 3.14159)
        "}\n"
    )
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": uri, "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)

    # Consume diagnostics (should be empty or irrelevant)
    lsp.read_message()

    # Request code action. Cursor is placed exactly on the '3' of '3.14159'
    cursor_position = {"line": 2, "character": 18}
    
    msg_id = lsp.send_message("textDocument/codeAction", {
        "textDocument": {"uri": uri},
        "range": {
            "start": cursor_position,
            "end": cursor_position
        },
        "context": {
            "diagnostics": [] # Refactoring actions are user-initiated, no diagnostics required
        }
    })

    res = lsp.read_message()["result"]
    assert res is not None and res != []

    # Check if our refactoring option was served
    action = next((a for a in res if a["title"] == "Extract to Local Variable"), None)
    assert action is not None
    assert action["kind"] == "refactor.extract"
    
    # Verify the TextEdits
    changes = action["edit"]["changes"][uri]
    assert len(changes) == 2 # 1 replacement + 1 insertion

    # Sort changes by line so we know which is which (top insertion vs inline replacement)
    changes.sort(key=lambda c: c["range"]["start"]["line"])

    insert_edit = changes[0]
    replace_edit = changes[1]

    # Verify the Inline Replacement
    assert replace_edit["newText"] == "extracted_float"
    assert replace_edit["range"]["start"]["line"] == 2
    assert replace_edit["range"]["start"]["character"] == 18 # Starts at '3'
    assert replace_edit["range"]["end"]["character"] == 25   # Ends after '9'

    # Verify the Variable Declaration Insertion
    assert "float extracted_float = 3.14159;" in insert_edit["newText"]
    assert insert_edit["range"]["start"]["line"] == 1 # Inserted just after "void fragment() {"

def test_quick_fix_typo_correction(lsp):
    uri = "file:///action_typo.gdshader"
    shader_code = (
        "shader_type spatial;\n"
        "void fragment() {\n"
        "    ALBDO = vec3(1.0);\n" # Typo: Should be ALBEDO
        "}\n"
    )
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": uri, "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)

    # 1. Catch the diagnostic
    diags = lsp.read_message()["params"]["diagnostics"]
    
    # Target GDS2000 (UndefinedIdentifier)
    target_diag = next((d for d in diags if str(d.get("code")) == "GDS2000"), None)
    assert target_diag is not None, "Did not receive UndefinedIdentifier diagnostic."

    # 2. Request Code Actions
    msg_id = lsp.send_message("textDocument/codeAction", {
        "textDocument": {"uri": uri},
        "range": target_diag["range"],
        "context": {
            "diagnostics": [target_diag]
        }
    })

    # 3. Verify the Code Action Response
    res = lsp.read_message()["result"]
    assert res is not None
    
    # Check if our specific Levenshtein fix was suggested
    action = next((a for a in res if "Change to 'ALBEDO'" in a["title"]), None)
    assert action is not None, "Typo correction failed to suggest 'ALBEDO'"
    assert action["kind"] == "quickfix"
    
    # 4. Verify the Text Edit
    changes = action["edit"]["changes"][uri]
    assert len(changes) == 1
    assert changes[0]["newText"] == "ALBEDO"

def test_generate_function_stub(lsp):
    uri = "file:///action_stub.gdshader"
    shader_code = (
        "shader_type spatial;\n"
        "\n"
        "void fragment() {\n"
        "    calculate_fresnel(vec3(1.0), 0.5);\n" # Unknown function call
        "}\n"
    )
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {"uri": uri, "languageId": "gdshader", "version": 1, "text": shader_code}
    }, is_notification=True)

    # 1. Catch the GDS2017 (UnknownFunction) diagnostic
    diags = lsp.read_message()["params"]["diagnostics"]
    target_diag = next((d for d in diags if str(d.get("code")) == "GDS2017"), None)
    assert target_diag is not None, "Did not receive UnknownFunction diagnostic."

    # 2. Request Code Actions
    msg_id = lsp.send_message("textDocument/codeAction", {
        "textDocument": {"uri": uri},
        "range": target_diag["range"],
        "context": {
            "diagnostics": [target_diag]
        }
    })

    # 3. Verify the Code Action Response
    res = lsp.read_message()["result"]
    assert res is not None
    
    # Check if the Generate Stub fix was suggested
    action = next((a for a in res if "Generate function 'calculate_fresnel'" in a["title"]), None)
    assert action is not None, "Generate function stub action missing"
    assert action["kind"] == "quickfix"
    
    # 4. Verify the Text Edit Output
    changes = action["edit"]["changes"][uri]
    assert len(changes) == 1
    
    generated_code = changes[0]["newText"]
    
    # Ensure it correctly parsed the argument types (vec3 and float)
    assert "void calculate_fresnel(vec3 arg1, float arg2)" in generated_code
    
    # Verify insertion point (Line 2, just above void fragment())
    assert changes[0]["range"]["start"]["line"] == 2