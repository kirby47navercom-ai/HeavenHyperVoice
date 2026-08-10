"""Browser test client bridge for the login, chat and field servers.

A browser cannot open a raw TLS socket, so this process sits in between:
it serves index.html and translates JSON into the FlatBuffers protocols.

    python bridge.py                # then open http://127.0.0.1:8080
    python bridge.py --port 8081    # a second one, to play two accounts at once

Login is request/response over POST. Field movement runs at 20 Hz, which is
too chatty for one HTTP request per update, so the field and chat traffic go
over a WebSocket instead. The handshake and framing are implemented here with
the standard library only.

Development only. It holds a single session in module state and does not verify
the server certificate, which is self-signed.
"""

import argparse
import base64
import hashlib
import json
import os
import socket
import ssl
import struct
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import flatbuffers
from flatbuffers.number_types import (BoolFlags, Float32Flags, Uint8Flags,
                                      Uint16Flags, Uint32Flags, Uint64Flags)
from flatbuffers.table import Table

HERE = os.path.dirname(os.path.abspath(__file__))
MAX_BODY = 64 * 1024

# Payload union tags, in schema declaration order.
LOGIN_REQUEST = 1
LOGIN_RESPONSE = 2
REGISTER_REQUEST = 3
REGISTER_RESPONSE = 4
CREATE_CHARACTER_REQUEST = 5
CHARACTER_LIST_RESPONSE = 6
SELECT_CHARACTER_REQUEST = 7
SELECT_CHARACTER_RESPONSE = 8
DELETE_CHARACTER_REQUEST = 9
RELEASE_PARTNER_REQUEST = 10

CHAT_HELLO, CHAT_SAY, CHAT_NOTICE, CHAT_CHAT = 1, 2, 3, 4
FIELD_ENTER, FIELD_ENTER_ACK, FIELD_MOVE, FIELD_SNAPSHOT, FIELD_NOTICE = 1, 2, 3, 4, 5


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


def ticket_payload(ticket):
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


def character_id_payload(character_id):
    """SelectCharacterRequest 와 ReleasePartnerRequest 가 같은 모양이다."""
    def build(b):
        b.StartObject(1)
        b.PrependUint64Slot(0, character_id, 0)
        return b.EndObject()

    return build


def delete_character_payload(character_id, confirm_nickname):
    def build(b):
        nick = b.CreateString(confirm_nickname)
        b.StartObject(2)
        b.PrependUint64Slot(0, character_id, 0)
        b.PrependUOffsetTRelativeSlot(1, nick, 0)
        return b.EndObject()

    return build


def move_payload(x, y, facing):
    def build(b):
        b.StartObject(3)
        b.PrependFloat32Slot(0, x, 0.0)
        b.PrependFloat32Slot(1, y, 0.0)
        b.PrependFloat32Slot(2, facing, 0.0)
        return b.EndObject()

    return build


def root(buf):
    return Table(buf, struct.unpack_from("<I", buf, 0)[0])


def field_at(table, slot):
    return table.Offset(4 + 2 * slot)


def read_string(table, slot):
    o = field_at(table, slot)
    return table.String(o + table.Pos).decode("utf-8") if o else ""


def read_scalar(table, slot, flags, default=0):
    o = field_at(table, slot)
    return table.Get(flags, o + table.Pos) if o else default


def read_bytes(table, slot):
    o = field_at(table, slot)
    if not o:
        return b""
    start = table.Vector(o)
    return bytes(table.Bytes[start:start + table.VectorLen(o)])


def read_table(table, slot):
    o = field_at(table, slot)
    return Table(table.Bytes, table.Indirect(o + table.Pos)) if o else None


def read_table_vector(table, slot):
    o = field_at(table, slot)
    if not o:
        return []
    start = table.Vector(o)
    return [Table(table.Bytes, table.Indirect(start + i * 4))
            for i in range(table.VectorLen(o))]


def read_u64_vector(table, slot):
    o = field_at(table, slot)
    if not o:
        return []
    start = table.Vector(o)
    return [table.Get(Uint64Flags, start + i * 8) for i in range(table.VectorLen(o))]


def read_payload(table):
    """Returns (tag, inner table) for an Envelope."""
    return read_scalar(table, 0, Uint8Flags), read_table(table, 1)


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
    return [{
        "id": read_scalar(entry, 0, Uint64Flags),
        "nickname": read_string(entry, 1),
        "level": read_scalar(entry, 2, Uint32Flags),
        "partner": decode_partner(read_table(entry, 3)),
    } for entry in read_table_vector(response, slot)]


def decode_entities(snapshot, slot):
    return [{
        "id": str(read_scalar(entry, 0, Uint64Flags)),
        "x": read_scalar(entry, 1, Float32Flags, 0.0),
        "y": read_scalar(entry, 2, Float32Flags, 0.0),
        "facing": read_scalar(entry, 3, Float32Flags, 0.0),
        "nickname": read_string(entry, 4),
        "partnerSpecies": read_scalar(entry, 5, Uint16Flags),
    } for entry in read_table_vector(snapshot, slot)]


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


# ------------------------------------------------------------------ WebSocket
#
# 20Hz movement over one HTTP request per update would be silly. The protocol
# is small enough that the handshake and framing fit in a page.

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def ws_accept_key(key):
    digest = hashlib.sha1((key + WS_GUID).encode("ascii")).digest()
    return base64.b64encode(digest).decode("ascii")


def ws_encode(payload, opcode=0x1):
    header = bytearray([0x80 | opcode])
    length = len(payload)
    if length < 126:
        header.append(length)
    elif length < 65536:
        header.append(126)
        header += struct.pack(">H", length)
    else:
        header.append(127)
        header += struct.pack(">Q", length)
    return bytes(header) + payload


def ws_read(rfile):
    """Returns (opcode, payload) or None when the peer is gone."""
    first = rfile.read(1)
    if not first:
        return None
    second = rfile.read(1)
    if not second:
        return None

    opcode = first[0] & 0x0F
    masked = second[0] & 0x80
    length = second[0] & 0x7F
    if length == 126:
        length = struct.unpack(">H", rfile.read(2))[0]
    elif length == 127:
        length = struct.unpack(">Q", rfile.read(8))[0]

    mask = rfile.read(4) if masked else b"\0\0\0\0"
    data = bytearray(rfile.read(length))
    for i in range(length):
        data[i] ^= mask[i % 4]
    return opcode, bytes(data)


# -------------------------------------------------------------------- session

# ponytail: one session in module state, enough for a single browser tab.
# Run a second bridge on another port to drive two accounts at once.
class Session:
    def __init__(self):
        # 캐릭터를 고를 때까지 열어두는 로그인 연결. 서버가 120초 상한을 건다.
        self.login = None
        self.chat = None
        self.field = None
        self.nickname = ""
        self.entity_id = 0

        self.ws = None
        self.ws_lock = threading.Lock()

    # --- 브라우저로 밀어내기 ---

    def attach(self, wfile):
        self.ws = wfile

    def detach(self, wfile):
        if self.ws is wfile:
            self.ws = None

    def push(self, kind, **fields):
        target = self.ws
        if target is None:
            return
        payload = json.dumps({"kind": kind, **fields}, ensure_ascii=False).encode("utf-8")
        try:
            with self.ws_lock:
                target.write(ws_encode(payload))
                target.flush()
        except (OSError, ValueError):
            self.ws = None

    # --- 소켓 ---

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

    def close_world(self):
        chat, self.chat = self.chat, None
        field, self.field = self.field, None
        self._shutdown(chat)
        self._shutdown(field)

    def close(self):
        self.close_login()
        self.close_world()

    def require_login(self):
        if self.login is None:
            raise RuntimeError("로그인 세션이 없습니다. 다시 로그인하세요")
        return self.login

    # --- 채팅 ---

    def start_chat(self, host, port, ticket):
        sock = connect(host, port)
        sock.settimeout(None)  # 조용한 방은 죽은 소켓이 아니다
        send_frame(sock, envelope(ticket_payload(ticket), CHAT_HELLO))
        self.chat = sock
        threading.Thread(target=self._chat_loop, args=(sock,), daemon=True).start()

    def say(self, text):
        if self.chat is None:
            raise RuntimeError("채팅에 연결되어 있지 않습니다")
        send_frame(self.chat, envelope(strings_table(text), CHAT_SAY))

    def _chat_loop(self, sock):
        reason = "연결이 종료되었습니다"
        try:
            while True:
                body = recv_frame(sock)
                if body is None:
                    break
                tag, payload = read_payload(root(body))
                if payload is None:
                    continue
                if tag == CHAT_NOTICE:
                    self.push("notice", text=read_string(payload, 0))
                elif tag == CHAT_CHAT:
                    self.push("chat", nickname=read_string(payload, 0),
                              text=read_string(payload, 1))
        except (OSError, ValueError) as e:
            reason = str(e)
        finally:
            if self.chat is sock:
                self.chat = None
                self.push("closed", text=reason)

    # --- 필드 ---

    def start_field(self, host, port, ticket):
        sock = connect(host, port)
        sock.settimeout(None)
        send_frame(sock, envelope(ticket_payload(ticket), FIELD_ENTER))
        self.field = sock
        threading.Thread(target=self._field_loop, args=(sock,), daemon=True).start()

    def move(self, x, y, facing):
        if self.field is None:
            return
        send_frame(self.field, envelope(move_payload(x, y, facing), FIELD_MOVE))

    def _field_loop(self, sock):
        reason = "필드 연결이 종료되었습니다"
        try:
            while True:
                body = recv_frame(sock)
                if body is None:
                    break
                tag, payload = read_payload(root(body))
                if payload is None:
                    continue

                if tag == FIELD_ENTER_ACK:
                    self.entity_id = read_scalar(payload, 0, Uint64Flags)
                    self.push("enter",
                              entityId=str(self.entity_id),
                              x=read_scalar(payload, 1, Float32Flags, 0.0),
                              y=read_scalar(payload, 2, Float32Flags, 0.0),
                              facing=read_scalar(payload, 3, Float32Flags, 0.0),
                              mapId=read_scalar(payload, 4, Uint32Flags))
                elif tag == FIELD_SNAPSHOT:
                    self.push("snapshot",
                              spawned=decode_entities(payload, 0),
                              moved=decode_entities(payload, 1),
                              despawned=[str(i) for i in read_u64_vector(payload, 2)])
                elif tag == FIELD_NOTICE:
                    self.push("fieldNotice", text=read_string(payload, 0))
        except (OSError, ValueError) as e:
            reason = str(e)
        finally:
            if self.field is sock:
                self.field = None
                self.push("closed", text=reason)


session = Session()


# ----------------------------------------------------------------------- HTTP


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass  # the console is for the game, not for access logs

    def reply(self, payload, status=200):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/ws":
            return self.serve_websocket()
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

    def serve_websocket(self):
        key = self.headers.get("Sec-WebSocket-Key")
        if not key:
            return self.send_error(400, "not a websocket handshake")

        self.wfile.write(
            b"HTTP/1.1 101 Switching Protocols\r\n"
            b"Upgrade: websocket\r\n"
            b"Connection: Upgrade\r\n"
            b"Sec-WebSocket-Accept: " + ws_accept_key(key).encode("ascii") + b"\r\n\r\n")
        self.wfile.flush()

        session.attach(self.wfile)
        try:
            while True:
                frame = ws_read(self.rfile)
                if frame is None:
                    break
                opcode, data = frame
                if opcode == 0x8:  # close
                    break
                if opcode == 0x9:  # ping
                    with session.ws_lock:
                        self.wfile.write(ws_encode(data, 0xA))
                        self.wfile.flush()
                    continue
                if opcode != 0x1:
                    continue

                try:
                    message = json.loads(data)
                    if message.get("t") == "move":
                        session.move(float(message["x"]), float(message["y"]),
                                     float(message.get("f", 0.0)))
                    elif message.get("t") == "say":
                        session.say(message.get("text", ""))
                except (ValueError, KeyError, OSError, RuntimeError) as e:
                    session.push("error", text=str(e))
        except (OSError, ValueError):
            pass
        finally:
            session.detach(self.wfile)

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
                "/api/characters/delete": self.api_delete_character,
                "/api/characters/release": self.api_release_partner,
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
                REGISTER_RESPONSE)
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
            LOGIN_RESPONSE)

        if not read_scalar(payload, 0, BoolFlags, False):
            sock.close()
            return {"ok": False, "message": read_string(payload, 1)}

        session.login = sock  # 캐릭터를 고를 때까지 열어둔다
        return {"ok": True,
                "characters": decode_characters(payload, 2),
                "maxSlots": read_scalar(payload, 3, Uint8Flags)}

    def api_create_character(self, body):
        payload = exchange(
            session.require_login(),
            envelope(create_character_payload(body.get("nickname", ""),
                                              int(body.get("speciesId", 0))),
                     CREATE_CHARACTER_REQUEST),
            CHARACTER_LIST_RESPONSE)
        return {"ok": bool(read_scalar(payload, 0, BoolFlags, False)),
                "message": read_string(payload, 1),
                "characters": decode_characters(payload, 2)}

    def api_delete_character(self, body):
        payload = exchange(
            session.require_login(),
            envelope(delete_character_payload(int(body.get("characterId", 0)),
                                              body.get("confirm", "")),
                     DELETE_CHARACTER_REQUEST),
            CHARACTER_LIST_RESPONSE)
        return {"ok": bool(read_scalar(payload, 0, BoolFlags, False)),
                "message": read_string(payload, 1),
                "characters": decode_characters(payload, 2)}

    def api_release_partner(self, body):
        payload = exchange(
            session.require_login(),
            envelope(character_id_payload(int(body.get("characterId", 0))),
                     RELEASE_PARTNER_REQUEST),
            CHARACTER_LIST_RESPONSE)
        return {"ok": bool(read_scalar(payload, 0, BoolFlags, False)),
                "message": read_string(payload, 1),
                "characters": decode_characters(payload, 2)}

    def api_select_character(self, body):
        payload = exchange(
            session.require_login(),
            envelope(character_id_payload(int(body.get("characterId", 0))),
                     SELECT_CHARACTER_REQUEST),
            SELECT_CHARACTER_RESPONSE)
        session.close_login()  # 서버가 이 응답 뒤에 끊는다

        if not read_scalar(payload, 0, BoolFlags, False):
            return {"ok": False, "message": read_string(payload, 1)}

        # 서비스마다 티켓이 따로 서명돼 있다. 필드 티켓을 채팅에 낼 수 없다.
        endpoints = {}
        for entry in read_table_vector(payload, 2):
            endpoints[read_string(entry, 0)] = (read_string(entry, 1),
                                                read_scalar(entry, 2, Uint16Flags),
                                                read_bytes(entry, 3))
        nickname = read_string(payload, 3)
        session.nickname = nickname

        if "field" in endpoints:
            session.start_field(*endpoints["field"])
        if "chat" in endpoints:
            session.start_chat(*endpoints["chat"])

        return {"ok": True, "nickname": nickname,
                "services": {name: f"{host}:{port} ({len(ticket)}B)"
                             for name, (host, port, ticket) in endpoints.items()}}

    def api_logout(self, _body):
        session.close()
        return {"ok": True}


class Server(ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        # A browser closing a tab aborts the socket. That is not an error.
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
