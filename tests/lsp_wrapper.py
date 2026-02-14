import subprocess
import json
import time

class LSPClient:
    def __init__(self, binary_path):
        self.process = subprocess.Popen(
            [binary_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, # Capture stderr to debug crashes
            text=False # We need binary mode for exact byte counting
        )
        self.request_id = 0

    def send_message(self, method, params, is_notification=False):
        """Encodes and sends a JSON-RPC message."""
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params
        }
        
        if not is_notification:
            self.request_id += 1
            payload["id"] = self.request_id

        json_str = json.dumps(payload)
        content = f"Content-Length: {len(json_str)}\r\n\r\n{json_str}"
        
        self.process.stdin.write(content.encode('utf-8'))
        self.process.stdin.flush()
        return payload.get("id")

    def read_message(self, timeout=2.0):
        """Reads a single JSON-RPC message from stdout."""
        # LSPs send "Content-Length: <num>\r\n\r\n" first
        header = b""
        start_time = time.time()
        
        while b"\r\n\r\n" not in header:
            if time.time() - start_time > timeout:
                raise TimeoutError("Timed out waiting for LSP header")
            char = self.process.stdout.read(1)
            if not char:
                raise EOFError("LSP process closed stdout unexpectedly")
            header += char

        # Parse Content-Length
        header_str = header.decode('utf-8')
        content_length = 0
        for line in header_str.split('\r\n'):
            if line.lower().startswith("content-length:"):
                content_length = int(line.split(':')[1].strip())
                break

        # Read the exact body size
        body = self.process.stdout.read(content_length)
        return json.loads(body)

    def stop(self):
        self.process.terminate()
        self.process.wait()