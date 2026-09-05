"""Real libcurl transport against loopback only; never uses microphone or credentials."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import subprocess
import sys
import threading
import time

errors = []
class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass
    def do_POST(self):
        try:
            body = self.rfile.read(int(self.headers['Content-Length']))
            assert self.headers.get('Authorization') == 'Bearer fixture-token'
            if self.path == '/asr':
                assert self.headers['Content-Type'].startswith('multipart/form-data; boundary=')
                assert b'name="file"' in body and b'RIFF' in body and b'WAVE' in body
                assert b'name="model"' in body and b'fixture-model' in body
            if self.path == '/polish':
                request = json.loads(body)
                assert request['model'] == 'fixture-model'
                assert request['messages'][1]['content'].startswith('<asr_text>\n')
            if self.path == '/slow':
                time.sleep(2)
            status = 401 if self.path == '/denied' else 302 if self.path == '/redirect' else 200
            self.send_response(status)
            if status == 302: self.send_header('Location', '/asr')
            self.end_headers()
            response = {'text': '水杉 voice'}
            if self.path == '/missing': response = {'other': 'not a transcript'}
            if self.path == '/polish': response = {'choices': [{'message': {'content': '水杉'}}]}
            data = json.dumps(response, ensure_ascii=False).encode()
            if self.path == '/invalid': data = b'invalid json'
            if self.path == '/oversize': data = b'x' * (1024 * 1024 + 1)
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception as error:
            errors.append(repr(error))
            self.send_error(500)

server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    result = subprocess.run([sys.argv[1], f'http://127.0.0.1:{server.server_port}'], timeout=20)
    assert not errors, errors
    raise SystemExit(result.returncode)
finally:
    server.shutdown()
    server.server_close()
