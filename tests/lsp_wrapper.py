import socket
import json
import time
import sys

class LSPClient:
    def __init__(self, port=6005):
        self.port = port
        self.sock = None
        self.request_id = 0
        self.buffer = b""  # Internal buffer for handling TCP fragmentation

    def connect(self):
        """Attempts to connect to the server with retries."""
        attempts = 0
        while attempts < 10:
            try:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.connect(('127.0.0.1', self.port))
                print(f"[PY] Connected to port {self.port}", file=sys.stderr)
                return
            except ConnectionRefusedError:
                time.sleep(0.5)
                attempts += 1
                print(f"[PY] Waiting for server on port {self.port}...", file=sys.stderr)
        
        raise TimeoutError(f"Could not connect to LSP server on port {self.port}")

    def send_message(self, method, params, is_notification=False):
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params
        }
        
        if not is_notification:
            self.request_id += 1
            payload["id"] = self.request_id

        json_str = json.dumps(payload)
        body_bytes = json_str.encode('utf-8')
        header = f"Content-Length: {len(body_bytes)}\r\n\r\n"
        
        # Send everything at once
        full_msg = header.encode('utf-8') + body_bytes
        self.sock.sendall(full_msg)
        
        return payload.get("id")

    def _recv_exact(self, num_bytes):
        """Helper to read exactly n bytes from the socket/buffer."""
        result = b""
        
        # First, drain our internal python buffer
        if len(self.buffer) >= num_bytes:
            result = self.buffer[:num_bytes]
            self.buffer = self.buffer[num_bytes:]
            return result
        else:
            result = self.buffer
            self.buffer = b""
            num_bytes -= len(result)

        # Then read from the socket
        while num_bytes > 0:
            chunk = self.sock.recv(min(4096, num_bytes))
            if not chunk:
                raise EOFError("Socket closed unexpectedly")
            result += chunk
            num_bytes -= len(chunk)
            
        return result

    def _read_line(self):
        """Reads until \r\n, handling internal buffering."""
        while b"\r\n" not in self.buffer:
            chunk = self.sock.recv(4096)
            if not chunk:
                if not self.buffer: raise EOFError("Socket closed")
                break
            self.buffer += chunk
            
        line, sep, rest = self.buffer.partition(b"\r\n")
        self.buffer = rest + sep[2:] # Keep 'rest' minus the \r\n? No, partition keeps sep.
        # Logic fix: partition returns (head, sep, tail)
        # We want to return head, and keep tail in buffer
        self.buffer = rest
        return line.decode('utf-8')

    def read_message(self, timeout=5.0):
        self.sock.settimeout(timeout)
        try:
            # 1. Read Headers until \r\n\r\n (Empty line)
            content_length = 0
            while True:
                line = self._read_line()
                if not line: # Empty line means end of headers
                    break
                if line.lower().startswith("content-length:"):
                    content_length = int(line.split(':')[1].strip())

            if content_length == 0:
                raise ValueError("Missing or invalid Content-Length")

            # 2. Read Body
            body_bytes = self._recv_exact(content_length)
            return json.loads(body_bytes.decode('utf-8'))
            
        except socket.timeout:
            raise TimeoutError("Socket timed out waiting for response")

    def close(self):
        if self.sock:
            self.sock.close()