import pytest # type: ignore

# List of test cases: (Name, Shader Code, Expected Error Substring)
ERROR_CASES = [

    # Basic syntax
    (
    "missing_semicolon",
        """
        shader_type spatial;
        void fragment() {
            float x = 1.0
        }
        """,
        "expected ';'"
    ),
    (
        "unbalanced_braces",
        """
        shader_type spatial;
        void fragment() {
            if (true) {
                float x = 1.0;
            // Missing closing brace
        }
        """,
        "}"
    ),

    # Type safety

    (
        "assign_int_to_vec",
        """
        shader_type spatial;
        void fragment() {
            vec3 v = 1;
        }
        """,
        "Type mismatch"
    ),
    (
        "too_few_args",
        """
        shader_type spatial;
        void fragment() {
            vec3 v = vec3(1.0, 2.0);
        }
        """,
        "components"
    ),
    (
        "assign_int_to_float",
        """
        shader_type spatial;
        void fragment() {
            float f = 1;
        }
        """,
        "Type mismatch"
    ),
    (
        "assign_vec3_to_float",
        """
        shader_type spatial;
        void fragment() {
            float f = vec3(1.0);
        }
        """,
        "Type mismatch"
    ),
    (
        "bool_arithmetic",
        """
        shader_type spatial;
        void fragment() {
            bool a = true;
            bool b = false;
            int c = a + b;
        }
        """,
        "Invalid binary operation"
    ),

    # Vector and swizzle logic

    (
        "swizzle_invalid_component",
        """
        shader_type spatial;
        void fragment() {
            vec3 v = vec3(1.0);
            float f = v.q;
        }
        """,
        "swizzle" # or "member"
    ),
    (
        "swizzle_out_of_bounds",
        """
        shader_type spatial;
        void fragment() {
            vec2 v = vec2(1.0);
            float f = v.z;
        }
        """,
        "swizzle"
    ),
    (
        "swizzle_assignment_mismatch",
        """
        shader_type spatial;
        void fragment() {
            vec3 v = vec3(1.0);
            v.xy = vec3(1.0);
        }
        """,
        "assign" # "Cannot assign 'vec3' to 'vec2'"
    ),

    # Shader stage validation

    (
        "discard_in_vertex",
        """
        shader_type spatial;
        void vertex() {
            discard;
        }
        """,
        "discard" # "discard only allowed in fragment shader"
    ),
    (
        "writing_albedo_in_vertex",
        """
        shader_type spatial;
        void vertex() {
            ALBEDO = vec3(1.0);
        }
        """,
        "Undefined" # or "ALBEDO"
    ),

    # Qualifiers and constants

    (
        "write_to_uniform",
        """
        shader_type spatial;
        uniform float u_time;
        void fragment() {
            u_time = 10.0;
        }
        """,
        "Cannot assign"
    ),
    (
        "write_to_const",
        """
        shader_type spatial;
        void fragment() {
            const float PI = 3.14;
            PI = 3.14159;
        }
        """,
        "read-only"
    ),
    (
        "opaque_type_construction",
        """
        shader_type spatial;
        void fragment() {
            sampler2D s = sampler2D(1.0);
        }
        """,
        "construct"
    ),

    # Functions and control flow

    (
        "void_function_return_value",
        """
        shader_type spatial;
        
        void do_nothing() {
        
        }

        void fragment() {
            float x = do_nothing();
        }
        """,
        "void"
    ),
    (
        "missing_return",
        """
        shader_type spatial;
        float get_val() {
            
        }
        void fragment() {
        
        }
        """,
        "return"
    ),
    (
        "recursion_check",
        """
        shader_type spatial;
        void recursive() {
            recursive();
        }
        void fragment() { recursive(); }
        """,
        "recursion"
    ),
    (
        "missing_shader_type",
        """
        void fragment() {
            ALBEDO = vec3(1.0);
        }
        """,
        "shader_type missing"
    )

]

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