import sys
from http.server import SimpleHTTPRequestHandler, HTTPServer

directory = sys.argv[1] if len(sys.argv) > 1 else "./"
host = sys.argv[2] if len(sys.argv) > 2 else "localhost"

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=directory, **kwargs)
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

httpd = HTTPServer((host, 8000), CORSRequestHandler)
httpd.serve_forever()