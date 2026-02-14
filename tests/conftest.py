import pytest
import os
import sys
from lsp_wrapper import LSPClient

# 1. Try to get path from Env Var (CI), otherwise default to local path
DEFAULT_PATH = "./bin/linux/debug/gdshader_lsp_debug"
BINARY_PATH = os.environ.get("LSP_BINARY_PATH", os.path.abspath(DEFAULT_PATH))

@pytest.fixture
def lsp():
    if not os.path.exists(BINARY_PATH):
        pytest.fail(f"LSP Binary not found at: {BINARY_PATH}")

    client = LSPClient(BINARY_PATH)
    
    # Initialize handshake
    client.send_message("initialize", {
        "processId": os.getpid(),
        "rootUri": "file:///tmp/gdshader_test",
        "capabilities": {}
    })
    client.read_message() 
    client.send_message("initialized", {}, is_notification=True)
    
    yield client
    
    client.stop()