#!/usr/bin/env python3
"""MCP server: serves the web build directory over HTTP."""
import json, sys, threading, http.server, socketserver, os

PORT = int(os.environ.get("FRACTORY_PORT", "8080"))
BUILD_DIR = os.environ.get("FRACTORY_BUILD_DIR",
    os.path.join(os.path.dirname(__file__), "..", "build", "web"))
BUILD_DIR = os.path.abspath(BUILD_DIR)

httpd = None
server_thread = None

def send(msg):
    sys.stdout.write(json.dumps(msg) + "\n")
    sys.stdout.flush()

def recv():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        yield json.loads(line)

class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write(f"[serve] {args[0]} {args[1]} {args[2]}\n")
        sys.stderr.flush()

def run_server(port):
    os.chdir(BUILD_DIR)
    s = socketserver.TCPServer(("127.0.0.1", port), Handler)
    s.serve_forever()

def main():
    global httpd, server_thread
    initialized = False
    req_id = 0

    for msg in recv():
        method = msg.get("method", "")
        params = msg.get("params", {})
        rid = msg.get("id")

        if method == "initialize":
            initialized = True
            send({"jsonrpc": "2.0", "id": rid, "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {}}
            }})

        elif method == "notifications/initialized":
            pass

        elif method == "tools/list":
            send({"jsonrpc": "2.0", "id": rid, "result": {
                "tools": [
                    {
                        "name": "serve",
                        "description": f"Start HTTP server on port {PORT} serving {BUILD_DIR}",
                        "inputSchema": {
                            "type": "object",
                            "properties": {
                                "port": {"type": "integer", "default": PORT}
                            }
                        }
                    },
                    {
                        "name": "stop",
                        "description": "Stop the HTTP server",
                        "inputSchema": {"type": "object", "properties": {}}
                    }
                ]
            }})

        elif method == "tools/call":
            name = params.get("name", "")
            args = params.get("arguments", {})
            if name == "serve":
                port = args.get("port", PORT)
                url = "http://127.0.0.1:{}/".format(port)
                if server_thread and server_thread.is_alive():
                    send({"jsonrpc": "2.0", "id": rid, "result": {
                        "content": [{"type": "text", "text": "Already serving at " + url}]}})
                else:
                    server_thread = threading.Thread(target=run_server, args=(port,), daemon=True)
                    server_thread.start()
                    msg = "Serving {} at {}".format(BUILD_DIR, url)
                    send({"jsonrpc": "2.0", "id": rid, "result": {
                        "content": [{"type": "text", "text": msg}]}})
            elif name == "stop":
                send({"jsonrpc": "2.0", "id": rid, "result": {
                    "content": [{"type": "text", "text": "Server stopped"}]}})
            else:
                send({"jsonrpc": "2.0", "id": rid, "error": {"code": -32601, "message": f"Unknown tool: {name}"}})

        elif method == "resources/list":
            send({"jsonrpc": "2.0", "id": rid, "result": {"resources": []}})

        elif rid:
            send({"jsonrpc": "2.0", "id": rid, "error": {"code": -32601, "message": f"Unknown method: {method}"}})

if __name__ == "__main__":
    main()
