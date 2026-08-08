"""Browser test client bridge for the login and chat servers.

A browser cannot open a raw TLS socket, so this process sits in between:
it serves index.html and translates JSON calls into the FlatBuffers protocol.

    python bridge.py            # then open http://127.0.0.1:8080

Development only. It holds a single chat session in module state and does not
verify the server certificate, exactly like the C++ TestClient.
"""

import argparse
import json
import os
import queue
import socket
import ssl
import struct
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import flatbuffers
from flatbuffers.number_types import BoolFlags, Uint8Flags, Uint16Flags
from flatbuffers.table import Table

HERE = os.path.dirname(os.path.abspath(__file__))
MAX_BODY = 64 * 1024

# Payload union tags, in schema declaration order.
LOGIN_REQUEST, LOGIN_RESPONSE, REGISTER_REQUEST, REGISTER_RESPONSE = 1, 2, 3, 4
HELLO, SAY, NOTICE, CHAT = 1, 2, 3, 4


# ----------------------------------------------------------------- FlatBuffers
#
# The schemas are small enough to encode by hand, which avoids committing a
# tree of flatc-generated modules. Field slots follow declaration order, so a
# vtable offset is 4 + 2 * slot.


def envelope(build_payload, tag):
    b = flatbuffers.Builder(256)
    payload = build_payload(b)
    b.StartObject(2)
    b.PrependUint8Slot(0, tag, 0)
    b.PrependUOffsetTRelativeSlot(1, payload, 0)
    b.Finish(b.EndObject())
    return bytes(b.Output())


def strings_table(*values):
    def build(b):
        offsets = [b.CreateString(v) for v in reversed(values)]
        offsets.reverse()
        b.StartObject(len(values))
        for slot, offset in enumerate(offsets):
            b.PrependUOffsetTRelativeSlot(slot, offset, 0)
        return b.EndObject()

    return build


def hello_payload(ticket):
    def build(b):
        vector = b.CreateByteVector(ticket)
        b.StartObject(1)
        b.PrependUOffsetTRelativeSlot(0, vector, 0)
        return b.EndObject()

    return build


def root(buf):
    return Table(buf, struct.unpack_from("<I", buf, 0)[0])


def field(table, slot):
    return table.Offset(4 + 2 * slot)


def read_string(table, slot):
    o = field(table, slot)
    return table.String(o + table.Pos).decode("utf-8") if o else ""


def read_scalar(table, slot, flags, default=0):
    o = field(table, slot)
    return table.Get(flags, o + table.Pos) if o else default


def read_bytes(table, slot):
    o = field(table, slot)
    if not o:
        return b""
    start = table.Vector(o)
    return bytes(table.Bytes[start:start + table.VectorLen(o)])


def read_payload(table):
    """Returns (tag, inner table) for an Envelope."""
    tag = read_scalar(table, 0, Uint8Flags)
    o = field(table, 1)
    if not o:
        return tag, None
    return tag, Table(table.Bytes, table.Indirect(o + table.Pos))


# ------------------------------------------------------------------ transport


def connect(host, port):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    raw = socket.create_connection((host, port), timeout=10)
    return context.wrap_socket(raw, server_hostname=host)


def send_frame(sock, body):
    sock.sendall(struct.pack("<I", len(body)) + body)


def recv_exact(sock, count):
    chunks = []
    while count:
        chunk = sock.recv(count)
        if not chunk:
            return None
        chunks.append(chunk)
        count -= len(chunk)
    return b"".join(chunks)


def recv_frame(sock):
    header = recv_exact(sock, 4)
    if header is None:
        return None
    length = struct.unpack("<I", header)[0]
    if not 0 < length <= MAX_BODY:
        raise ValueError(f"frame length out of range: {length}")
    return recv_exact(sock, length)


def request_response(host, port, body):
    """One round trip against the login server, which closes after replying."""
    sock = connect(host, port)
    try:
        send_frame(sock, body)
        return recv_frame(sock)
    finally:
        sock.close()


# -------------------------------------------------------------------- session

# ponytail: one session in module state, enough for a single browser tab.
# Move to a dict keyed by cookie if you ever want two logins at once.
class Session:
    def __init__(self):
        self.sock = None
        self.nickname = ""
        self.events = queue.Queue()

    def publish(self, kind, **fields):
        self.events.put({"kind": kind, **fields})

    def close(self):
        sock, self.sock = self.sock, None
        if sock:
            try:
                sock.close()
            except OSError:
                pass

    def start_chat(self, host, port, ticket, nickname):
        self.close()
        sock = connect(host, port)
        send_frame(sock, envelope(hello_payload(ticket), HELLO))
        self.sock = sock
        self.nickname = nickname
        threading.Thread(target=self._read_loop, args=(sock,), daemon=True).start()

    def say(self, text):
        if not self.sock:
            raise RuntimeError("not connected")
        send_frame(self.sock, envelope(strings_table(text), SAY))

    def _read_loop(self, sock):
        reason = "연결이 종료되었습니다"
        try:
            while True:
                body = recv_frame(sock)
                if body is None:
                    break
                tag, payload = read_payload(root(body))
                if payload is None:
                    continue
                if tag == NOTICE:
                    self.publish("notice", text=read_string(payload, 0))
                elif tag == CHAT:
                    self.publish("chat", nickname=read_string(payload, 0),
                                 text=read_string(payload, 1))
        except (OSError, ValueError) as e:
            reason = str(e)
        finally:
            if self.sock is sock:
                self.sock = None
                self.publish("closed", text=reason)


session = Session()


# ----------------------------------------------------------------------- HTTP


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass  # the console is for chat, not for access logs

    def reply(self, payload, status=200):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.startswith("/api/events"):
            return self.stream_events()
        if self.path in ("/", "/index.html"):
            with open(os.path.join(HERE, "index.html"), "rb") as f:
                body = f.read()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self.send_error(404)

    def stream_events(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        try:
            while True:
                try:
                    event = session.events.get(timeout=15)
                    line = json.dumps(event, ensure_ascii=False)
                except queue.Empty:
                    line = None  # keep-alive comment so proxies do not time out
                chunk = f"data: {line}\n\n" if line else ": ping\n\n"
                self.wfile.write(chunk.encode("utf-8"))
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        try:
            body = json.loads(self.rfile.read(length) or b"{}")
        except json.JSONDecodeError:
            return self.reply({"ok": False, "message": "bad json"}, 400)

        try:
            handler = {
                "/api/register": self.api_register,
                "/api/login": self.api_login,
                "/api/say": self.api_say,
                "/api/logout": self.api_logout,
            }[self.path]
        except KeyError:
            return self.send_error(404)

        try:
            self.reply(handler(body))
        except (OSError, ValueError, RuntimeError) as e:
            self.reply({"ok": False, "message": f"{type(e).__name__}: {e}"}, 502)

    def api_register(self, body):
        frame = envelope(
            strings_table(body.get("username", ""), body.get("password", ""),
                          body.get("nickname", "")),
            REGISTER_REQUEST,
        )
        reply = request_response(args.login_host, args.login_port, frame)
        if reply is None:
            return {"ok": False, "message": "서버가 응답 없이 연결을 끊었습니다"}
        tag, payload = read_payload(root(reply))
        if tag != REGISTER_RESPONSE or payload is None:
            return {"ok": False, "message": "예상치 못한 응답"}
        return {"ok": bool(read_scalar(payload, 0, BoolFlags, False)),
                "message": read_string(payload, 1)}

    def api_login(self, body):
        frame = envelope(
            strings_table(body.get("username", ""), body.get("password", "")),
            LOGIN_REQUEST,
        )
        reply = request_response(args.login_host, args.login_port, frame)
        if reply is None:
            return {"ok": False, "message": "서버가 응답 없이 연결을 끊었습니다"}
        tag, payload = read_payload(root(reply))
        if tag != LOGIN_RESPONSE or payload is None:
            return {"ok": False, "message": "예상치 못한 응답"}

        if not read_scalar(payload, 0, BoolFlags, False):
            return {"ok": False, "message": read_string(payload, 1)}

        ticket = read_bytes(payload, 2)
        host = read_string(payload, 3) or args.login_host
        port = read_scalar(payload, 4, Uint16Flags)
        nickname = read_string(payload, 5)

        session.start_chat(host, port, ticket, nickname)
        return {"ok": True, "nickname": nickname, "ticketBytes": len(ticket),
                "chat": f"{host}:{port}"}

    def api_say(self, body):
        text = body.get("text", "").strip()
        if not text:
            return {"ok": False, "message": "빈 메시지"}
        session.say(text)
        return {"ok": True}

    def api_logout(self, _body):
        session.close()
        return {"ok": True}


class Server(ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        # A browser closing a tab aborts the event stream. That is not an error.
        pass


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--login-host", default="127.0.0.1")
parser.add_argument("--login-port", type=int, default=9100)
parser.add_argument("--port", type=int, default=8080, help="port for this bridge")
args = parser.parse_args()

if __name__ == "__main__":
    print(f"bridge on http://127.0.0.1:{args.port} "
          f"-> login {args.login_host}:{args.login_port}")
    try:
        Server(("127.0.0.1", args.port), Handler).serve_forever()
    except KeyboardInterrupt:
        session.close()
