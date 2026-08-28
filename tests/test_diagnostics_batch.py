import pytest # type: ignore
import json
import os

# --- Load Error Cases from JSON ---
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
JSON_PATH = os.path.join(TEST_DIR, "diagnostics_batch.json")

def load_error_cases():
    with open(JSON_PATH, "r") as f:
        raw_cases = json.load(f)
    
    formatted_cases = []
    for case in raw_cases:
        # Reconstruct the array of strings back into a multiline string
        code_str = "\n".join(case["code"]) if isinstance(case["code"], list) else case["code"]
        formatted_cases.append((case["name"], code_str, case["expected_error"]))
        
    return formatted_cases

ERROR_CASES = load_error_cases()

@pytest.mark.parametrize("name, code, expected_error", ERROR_CASES)
def test_error_messages_specifically(lsp, name, code, expected_error):
    """
    Data-driven test to verify specific error messages.
    Useful for refining error text clarity.
    """
    
    # Create unique URI for each case to avoid caching issues
    uri = f"file:///error_{name}.gdshader"
    
    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": uri,
            "languageId": "gdshader",
            "version": 1,
            "text": code
        }
    }, is_notification=True)
    
    res = lsp.read_message()
    
    # Assert we got diagnostics
    assert res["method"] == "textDocument/publishDiagnostics"
    diags = res["params"]["diagnostics"]
    
    if len(diags) == 0:
        pytest.fail(f"Expected error containing '{expected_error}', but got valid compilation.")
        
    # Check if ANY of the diagnostics contain our expected string
    found = False
    messages = []
    for d in diags:
        msg = d["message"]
        messages.append(msg)
        if expected_error.lower() in msg.lower():
            found = True
            break
            
    if not found:
        pytest.fail(f"Expected error to contain '{expected_error}'. Got: {messages}")

# --- Valid Matrix Constructor Cases ---
# Matrix constructors are strictly typed (Godot shading language docs):
# columns of matching vectors, a single diagonal scalar, or a matrix of a
# different dimension. Valid forms must not produce any error diagnostics.
VALID_MATRIX_CASES = [
    ("mat2_columns", "mat2 m = mat2(vec2(1.0, 0.0), vec2(0.0, 1.0));"),
    ("mat3_columns", "mat3 m = mat3(vec3(1.0), vec3(1.0), vec3(1.0));"),
    ("mat4_columns", "mat4 m = mat4(vec4(1.0), vec4(1.0), vec4(1.0), vec4(1.0));"),
    ("diagonal_identity", "mat4 m = mat4(1.0);"),
    ("diagonal_int", "mat3 m = mat3(1);"),
    ("matrix_from_larger", "mat3 m = mat3(mat4(1.0));"),
    ("matrix_from_smaller", "mat2 m = mat2(mat3(1.0));"),
    ("builtin_matrix", "mat3 basis = mat3(MODEL_MATRIX);"),
]

@pytest.mark.parametrize("name, statement", VALID_MATRIX_CASES)
def test_valid_matrix_constructors(lsp, name, statement):
    """Valid matrix construction forms must not produce any error diagnostics."""

    code = "shader_type spatial;\nvoid fragment() {\n    " + statement + "\n}\n"
    uri = f"file:///valid_matrix_{name}.gdshader"

    lsp.send_message("textDocument/didOpen", {
        "textDocument": {
            "uri": uri,
            "languageId": "gdshader",
            "version": 1,
            "text": code
        }
    }, is_notification=True)

    res = lsp.read_message()

    # Assert we got diagnostics
    assert res["method"] == "textDocument/publishDiagnostics"

    # Filter for actual errors (severity 1), ignoring warnings like unused variables
    errors = [d for d in res["params"]["diagnostics"] if d.get("severity", 1) == 1]

    assert errors == [], \
        f"Expected no errors for valid matrix construction, got: {[d['message'] for d in errors]}"