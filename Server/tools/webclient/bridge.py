"""Browser test client bridge for the login and chat servers.

A browser cannot open a raw TLS socket, so this process sits in between:
it serves index.html and translates JSON calls into the FlatBuffers protocol.

    python bridge.py            # then open http://127.0.0.1:8080
    python bridge.py --port 8081    # a second one, to play two accounts at once

Login is two round trips on one connection (log in, list characters, pick one),
so the login socket stays open until a character is selected.

Development only. It holds a single session in module state and does not verify
the server certificate, which is self-signed.
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
from flatbuffers.number_types import (BoolFlags, Uint8Flags, Uint16Flags,
                                      Uint32Flags, Uint64Flags)
from flatbuffers.table import Table

HERE = os.path.dirname(os.path.abspath(__file__))
MAX_BODY = 64 * 1024

# Payload union tags, in schema declaration order.
LOGIN_REQUEST = 1
LOGIN_RESPONSE = 2
REGISTER_REQUEST = 3
REGISTER_RESPONSE = 4
CREATE_CHARACTER_REQUEST = 5
CREATE_CHARACTER_RESPONSE = 6
SELECT_CHARACTER_REQUEST = 7
SELECT_CHARACTER_RESPONSE = 8

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


def create_character_payload(nickname, species_id):
    def build(b):
        nick = b.CreateString(nickname)
        b.StartObject(2)
        b.PrependUOffsetTRelativeSlot(0, nick, 0)
        b.PrependUint16Slot(1, species_id, 0)
        return b.EndObject()

    return build


def select_character_payload(character_id):
    def build(b):
        b.StartObject(1)
        b.PrependUint64Slot(0, character_id, 0)
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


def read_table(table, slot):
    """A nested table field, or None when absent."""
    o = field(table, slot)
    if not o:
        return None
    return Table(table.Bytes, table.Indirect(o + table.Pos))


def read_table_vector(table, slot):
    o = field(table, slot)
    if not o:
        return []
    start = table.Vector(o)
    return [Table(table.Bytes, table.Indirect(start + i * 4))
            for i in range(table.VectorLen(o))]


def read_payload(table):
    """Returns (tag, inner table) for an Envelope."""
    tag = read_scalar(table, 0, Uint8Flags)
    return tag, read_table(table, 1)


def decode_partner(partner):
    if partner is None:
        return None
    return {
        "speciesId": read_scalar(partner, 0, Uint16Flags),
        "nickname": read_string(partner, 1),
        "level": read_scalar(partner, 2, Uint32Flags),
        "maxHp": read_scalar(partner, 3, Uint16Flags),
        "atk": read_scalar(partner, 4, Uint16Flags),
        "def": read_scalar(partner, 5, Uint16Flags),
        "spAtk": read_scalar(partner, 6, Uint16Flags),
        "spDef": read_scalar(partner, 7, Uint16Flags),
        "speed": read_scalar(partner, 8, Uint16Flags),
    }


def decode_characters(response, slot):
    characters = []
    for entry in read_table_vector(response, slot):
        characters.append({
            "id": read_scalar(entry, 0, Uint64Flags),
            "nickname": read_string(entry, 1),
            "level": read_scalar(entry, 2, Uint32Flags),
            "partner": decode_partner(read_table(entry, 3)),
        })
    return characters


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


def exchange(sock, body, expected_tag):
    """One request and its reply on an already open connection."""
    send_frame(sock, body)
    reply = recv_frame(sock)
    if reply is None:
        raise RuntimeError("서버가 응답 없이 연결을 끊었습니다")
    tag, payload = read_payload(root(reply))
    if tag != expected_tag or payload is None:
        raise RuntimeError(f"예상치 못한 응답 (tag {tag})")
    return payload


# -------------------------------------------------------------------- session

# ponytail: one session in module state, enough for a single browser tab.
# Run a second bridge on another port to drive two accounts at once.
class Session:
    def __init__(self):
        # 캐릭터를 고를 때까지 열어두는 로그인 연결. 서버가 120초 상한을 건다.
        self.login = None
        self.chat = None
        self.nickname = ""
        self.lock = threading.Lock()
        self.subscribers = []

    def subscribe(self):
        """Each event stream needs its own queue: Queue.get() consumes, so a
        shared queue would split events between a reloaded page and the stale
        thread the reload left behind."""
        events = queue.Queue()
        with self.lock:
            self.subscribers.append(events)
        return events

    def unsubscribe(self, events):
        with self.lock:
            if events in self.subscribers:
                self.subscribers.remove(events)

    def publish(self, kind, **fields):
        event = {"kind": kind, **fields}
        with self.lock:
            targets = list(self.subscribers)
        for events in targets:
            events.put(event)

    @staticmethod
    def _shutdown(sock):
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def close_login(self):
        sock, self.login = self.login, None
        self._shutdown(sock)

    def close_chat(self):
        sock, self.chat = self.chat, None
        self._shutdown(sock)

    def close(self):
        self.close_login()
        self.close_chat()

    def require_login(self):
        if self.login is None:
            raise RuntimeError("로그인 세션이 없습니다. 다시 로그인하세요")
        return self.login

    def start_chat(self, host, port, ticket, nickname):
        self.close_chat()
        sock = connect(host, port)
        # The connect timeout would otherwise apply to every recv, and a quiet
        # chat room would look like a dead socket after ten seconds.
        sock.settimeout(None)
        send_frame(sock, envelope(hello_payload(ticket), HELLO))
        self.chat = sock
        self.nickname = nickname
        threading.Thread(target=self._read_loop, args=(sock,), daemon=True).start()

    def say(self, text):
        if self.chat is None:
            raise RuntimeError("not connected")
        send_frame(self.chat, envelope(strings_table(text), SAY))

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
            if self.chat is sock:
                self.chat = None
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
        events = session.subscribe()
        try:
            while True:
                try:
                    line = json.dumps(events.get(timeout=15), ensure_ascii=False)
                except queue.Empty:
                    line = None  # a ping proves the browser is still there
                chunk = f"data: {line}\n\n" if line else ": ping\n\n"
                self.wfile.write(chunk.encode("utf-8"))
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            session.unsubscribe(events)

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
                "/api/characters/create": self.api_create_character,
                "/api/characters/select": self.api_select_character,
                "/api/say": self.api_say,
                "/api/logout": self.api_logout,
            }[self.path]
        except KeyError:
            return self.send_error(404)

        try:
            self.reply(handler(body))
        except (OSError, ValueError, RuntimeError) as e:
            # 로그인 연결이 끊어졌으면 다시 로그인해야 한다.
            if self.path.startswith("/api/characters"):
                session.close_login()
            self.reply({"ok": False, "message": f"{type(e).__name__}: {e}"}, 502)

    def api_register(self, body):
        sock = connect(args.login_host, args.login_port)
        try:
            payload = exchange(
                sock,
                envelope(strings_table(body.get("username", ""), body.get("password", "")),
                         REGISTER_REQUEST),
                REGISTER_RESPONSE,
            )
            return {"ok": bool(read_scalar(payload, 0, BoolFlags, False)),
                    "message": read_string(payload, 1)}
        finally:
            sock.close()

    def api_login(self, body):
        session.close()

        sock = connect(args.login_host, args.login_port)
        payload = exchange(
            sock,
            envelope(strings_table(body.get("username", ""), body.get("password", "")),
                     LOGIN_REQUEST),
            LOGIN_RESPONSE,
        )

        if not read_scalar(payload, 0, BoolFlags, False):
            sock.close()
            return {"ok": False, "message": read_string(payload, 1)}

        # 캐릭터를 고를 때까지 이 연결을 열어둔다.
        session.login = sock
        return {"ok": True,
                "characters": decode_characters(payload, 2),
                "maxSlots": read_scalar(payload, 3, Uint8Flags)}

    def api_create_character(self, body):
        payload = exchange(
            session.require_login(),
            envelope(create_character_payload(body.get("nickname", ""),
                                              int(body.get("speciesId", 0))),
                     CREATE_CHARACTER_REQUEST),
            CREATE_CHARACTER_RESPONSE,
        )
        return {"ok": bool(read_scalar(payload, 0, BoolFlags, False)),
                "message": read_string(payload, 1),
                "characters": decode_characters(payload, 2)}

    def api_select_character(self, body):
        payload = exchange(
            session.require_login(),
            envelope(select_character_payload(int(body.get("characterId", 0))),
                     SELECT_CHARACTER_REQUEST),
            SELECT_CHARACTER_RESPONSE,
        )
        # 서버가 이 응답 뒤에 로그인 연결을 끊는다.
        session.close_login()

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
