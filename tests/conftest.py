import pytest # type: ignore
import os
import subprocess
import sys
from lsp_wrapper import LSPClient

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(TESTS_DIR)
DEFAULT_BIN = os.path.join(PROJECT_ROOT, "bin/linux/debug/gdshader_lsp_debug_linux")
BINARY_PATH = os.environ.get("LSP_BINARY_PATH", DEFAULT_BIN)
LSP_PORT = 6005

@pytest.fixture
def lsp():

    print(f"[PY] Spawning server: {BINARY_PATH}", file=sys.stderr)
    
    server_process = subprocess.Popen(
        [BINARY_PATH, str(LSP_PORT)], 
        cwd=PROJECT_ROOT,
        stdout=sys.stdout, # Let logs go straight to console
        stderr=sys.stderr  # Let logs go straight to console
    )

    # 2. Connect via TCP
    client = LSPClient(port=LSP_PORT)
    try:
        client.connect()
    except Exception as e:
        server_process.terminate()
        pytest.fail(f"Failed to connect to LSP: {e}")

    # 3. Handshake
    client.send_message("initialize", {
        "processId": os.getpid(),
        "rootUri": f"file://{PROJECT_ROOT}",
        "capabilities": {}
    })
    
    # Read until we get the response matching our ID
    # (The socket wrapper handles buffering correctly now)
    client.read_message()
    
    client.send_message("initialized", {}, is_notification=True)
    
    yield client
    
    # 4. Cleanup
    client.close()
    server_process.terminate()
    server_process.wait()