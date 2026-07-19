#!/usr/bin/env python3
"""
mock_llama_server.py — Mock llama.cpp server with verbose logging
Logs ALL requests (method, path, headers, body) to /tmp/mock-llm-requests.log
"""
import json
import sys
import time
from http.server import HTTPServer, BaseHTTPRequestHandler

class MockLLMHandler(BaseHTTPRequestHandler):
    def handle_request(self, method):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8", errors="replace") if content_length > 0 else ""

        # Log everything
        with open("/tmp/mock-llm-requests.log", "a") as f:
            f.write(f"\n=== {time.strftime('%H:%M:%S')} {method} {self.path} ===\n")
            f.write(f"Headers:\n")
            for k, v in self.headers.items():
                f.write(f"  {k}: {v}\n")
            f.write(f"Body ({len(body)} bytes): {body[:500]}\n")
            f.write(f"Client: {self.client_address}\n")

        if method == "POST":
            try:
                req = json.loads(body) if body else {}
                messages = req.get("messages", []) if isinstance(req, dict) else []
                user_msg = ""
                for m in reversed(messages):
                    if isinstance(m, dict) and m.get("role") == "user":
                        user_msg = m.get("content", "")
                        break
            except Exception:
                user_msg = body[:100] if body else "empty"

            response_text = f"[MOCK LLM] Received: '{user_msg[:50]}'. WLTIOS AI pipeline working."
            resp = {
                "id": "chatcmpl-mock",
                "object": "chat.completion",
                "created": int(time.time()),
                "model": "mock-llm",
                "choices": [{"index": 0, "message": {"role": "assistant", "content": response_text}, "finish_reason": "stop"}],
                "usage": {"prompt_tokens": 50, "completion_tokens": 30, "total_tokens": 80}
            }
            resp_json = json.dumps(resp)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(resp_json)))
            self.end_headers()
            self.wfile.write(resp_json.encode())
        elif method == "GET" and self.path == "/health":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"status":"ok"}')
        else:
            self.send_response(404)
            self.end_headers()
            with open("/tmp/mock-llm-requests.log", "a") as f:
                f.write(f" Returned 404\n")

    def do_POST(self):
        self.handle_request("POST")

    def do_GET(self):
        self.handle_request("GET")

    def log_message(self, format, *args):
        pass

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    # Clear log
    open("/tmp/mock-llm-requests.log", "w").close()
    server = HTTPServer(("0.0.0.0", port), MockLLMHandler)
    print(f"[mock-llm] Listening on 0.0.0.0:{port}", flush=True)
    print(f"[mock-llm] Logging to /tmp/mock-llm-requests.log", flush=True)
    server.serve_forever()
