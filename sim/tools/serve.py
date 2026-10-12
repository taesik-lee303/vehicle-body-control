#!/usr/bin/env python3
"""make sim 서버: 저장소 루트를 정적 파일로 내주고, 브라우저 이벤트 로그를 이 터미널에도 찍는다.

판단은 브라우저 안의 C 코어(WebAssembly)가 하므로, 화면 로그(sim/js/termlog.js)가
POST /log 로 보낸 줄을 그대로 출력한다. 예: [  1.000s] 와이퍼: 정지 → 간헐
사용: python3 sim/tools/serve.py [포트]   (저장소 루트에서 실행)
"""
import json
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

MAX_BODY = 256 * 1024
TTY = sys.stdout.isatty()
COLOR = {'cmd': '2', 'ok': '32', 'bad': '31', 'err': '31', 'info': '36'}   # 변화(change)는 기본색


def paint(cls, text):
    code = COLOR.get(cls)
    return f'\033[{code}m{text}\033[0m' if TTY and code else text


class Handler(SimpleHTTPRequestHandler):
    def do_POST(self):
        if self.path != '/log':
            self.send_error(404)
            return
        n = int(self.headers.get('Content-Length') or 0)
        if n <= 0 or n > MAX_BODY:
            self.send_error(413)
            return
        try:
            lines = json.loads(self.rfile.read(n))
            out = [paint(str(x.get('c', '')), str(x.get('t', ''))) for x in lines]
        except (ValueError, AttributeError, TypeError):
            self.send_error(400)
            return
        if out:
            print('\n'.join(out), flush=True)
        self.send_response(204)
        self.end_headers()

    # 성공한 요청(파일 받기, 로그 전송)은 찍지 않는다. 로직 로그가 묻히지 않게
    def log_request(self, code='-', size='-'):
        if isinstance(code, int) and code >= 400:
            super().log_request(code, size)


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    server = ThreadingHTTPServer(('', port), Handler)
    print(f'브라우저에서 http://localhost:{port}/sim/ (종료: Ctrl+C)')
    print('화면의 이벤트 로그(명령, 출력 변화, 시나리오 결과)가 여기에도 찍힙니다.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print()
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
