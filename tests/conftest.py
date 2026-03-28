import pytest # type: ignore
import os
import subprocess
import sys
from lsp_wrapper import LSPClient

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(TESTS_DIR)
DEFAULT_BIN = os.path.join(PROJECT_ROOT, "bin/linux/debug/gdshader_lsp_debug_linux")
BINARY_PATH = os.environ.get("LSP_BINARY_PATH", DEFAULT_BIN)
LSP_PORT = 6007

# The fixture is now parameterized. It will run every test suite twice
@pytest.fixture(params=["tcp", "stdio"])
def lsp(request):
    mode = request.param
    print(f"\n[PY] Spawning server ({mode.upper()} mode): {BINARY_PATH}", file=sys.stderr)
    
    if mode == "tcp":
        server_process = subprocess.Popen(
            [BINARY_PATH, f"--port={LSP_PORT}"], 
            cwd=PROJECT_ROOT,
            stdout=sys.stdout, 
            stderr=sys.stderr  
        )
        client = LSPClient(port=LSP_PORT, mode="tcp")
        try:
            client.connect()
        except Exception as e:
            server_process.terminate()
            pytest.fail(f"Failed to connect to LSP (TCP): {e}")
            
    elif mode == "stdio":
        server_process = subprocess.Popen(
            [BINARY_PATH, "--stdio"],
            cwd=PROJECT_ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=sys.stderr # Still let logs go straight to console
        )
        client = LSPClient(process=server_process, mode="stdio")

    # 3. Handshake (Identical for both modes)
    client.send_message("initialize", {
        "processId": os.getpid(),
        "rootUri": f"file://{PROJECT_ROOT}",
        "capabilities": {}
    })
    
    client.read_message()
    client.send_message("initialized", {}, is_notification=True)
    
    yield client
    
    # 4. Cleanup
    client.close()
    server_process.terminate()
    server_process.wait()