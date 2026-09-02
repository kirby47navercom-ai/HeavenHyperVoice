"""인스턴스 서버의 방 분할을 확인한다.

  1. 서버를 띄운다 (로그인 서버도 DB 도 필요 없다)
       .\\build\\windows-x64\\bin\\Debug\\InstanceServer.exe --dev-no-auth ^
           --room-capacity 2 --instance-types 1,2 --wild-per-room 4 --room-idle 3
  2. python .\\tools\\test-instance-rooms.py

여섯 가지를 본다.

  방 분할     정원 2인 방에 3명을 넣으면 앞의 둘은 같은 방, 셋째는 새 방이다.
  격리        다른 방 사람은 서로 보이지 않는다. 방마다 World 가 따로라
              시야 계산에 아예 후보로도 안 올라와야 한다.
  같은 방     같은 방 사람끼리는 서로 보인다. 격리가 "아무도 안 보인다" 로
              통과해 버리는 것을 막는 대조군이다.
  야생        방마다 야생이 스폰되고 움직인다. 필드에서 빠진 것이 여기 있다.
  종류 분리   종류가 다르면 정원이 남아 있어도 다른 방이다.
  거절        목록에 없는 종류는 거절한다. 이게 없으면 클라가 아무 번호나
              보내는 것만으로 방을 무한히 만든다.
  회수        전원 나가고 --room-idle 이 지나면 방이 닫힌다. 다시 붙었을 때
              방 번호가 예전 것이 아니면 닫힌 것이다.

프레임 조립은 test-field-session.py 의 것을 그대로 쓴다. 슬롯 번호는 field.fbs
의 선언 순서다. 새 필드는 항상 뒤에 붙일 것.
"""
import importlib.util
import os
import socket
import ssl
import struct
import sys
import threading
import time

import flatbuffers
from flatbuffers.number_types import Float32Flags, Uint8Flags, Uint32Flags, Uint64Flags

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "field_session", os.path.join(_here, "test-field-session.py"))
_session = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_session)

envelope = _session.envelope
root = _session.root
scalar = _session.scalar
child = _session.child
table_vector = _session.table_vector
u64_vector = _session.u64_vector
wait_for = _session.wait_for

ENTER, MOVE = 1, 3
ENTER_ACK, SNAPSHOT, NOTICE = 2, 4, 5

PORT = 9300

# World.h 의 kWildIdBase. 야생 엔티티 번호는 여기서부터 시작한다.
WILD_ID_BASE = 1 << 52

# 서버를 --room-capacity 2 --room-idle 3 으로 띄웠다고 본다.
CAPACITY = 2
ROOM_IDLE = 3.0


def dev_enter(name, character_id, instance_type):
    def build(builder):
        offset = builder.CreateString(name)
        builder.StartObject(5)
        builder.PrependUOffsetTRelativeSlot(1, offset, 0)       # dev_name
        builder.PrependUint64Slot(2, character_id, 0)           # dev_character_id
        builder.PrependUint16Slot(3, 0, 0)                      # dev_partner_species
        builder.PrependUint32Slot(4, instance_type, 0)          # instance_type
        return builder.EndObject()
    return build


class Client:
    def __init__(self, name, character_id, instance_type):
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE  # 개발 인증서는 자체 서명이다
        raw = socket.create_connection(("127.0.0.1", PORT), timeout=10)
        self.sock = context.wrap_socket(raw, server_hostname="127.0.0.1")
        self.sock.settimeout(None)

        self.character_id = character_id
        self.room_id = None
        self.spawn = None
        self.notices = []
        self.sequence = 0
        self.alive = True
        self.lock = threading.Lock()
        self.seen = []  # (종류, entity_id)

        threading.Thread(target=self._read, daemon=True).start()
        self.send(envelope(dev_enter(name, character_id, instance_type), ENTER))

    def _read(self):
        try:
            while True:
                header = self.sock.recv(4)
                if len(header) < 4:
                    return
                size = struct.unpack("<I", header)[0]
                buf = b""
                while len(buf) < size:
                    chunk = self.sock.recv(size - len(buf))
                    if not chunk:
                        return
                    buf += chunk
                self._dispatch(root(buf))
        except OSError:
            pass
        finally:
            self.alive = False

    def _dispatch(self, frame):
        tag = scalar(frame, 0, Uint8Flags)
        payload = child(frame, 1)
        if payload is None:
            return
        if tag == ENTER_ACK:
            self.spawn = (scalar(payload, 1, Float32Flags, 0.0),
                          scalar(payload, 2, Float32Flags, 0.0))
            self.room_id = scalar(payload, 5, Uint32Flags, 0)
        elif tag == NOTICE:
            self.notices.append("notice")
        elif tag == SNAPSHOT:
            with self.lock:
                for kind, slot in (("spawn", 0), ("move", 1)):
                    for entity in table_vector(payload, slot):
                        self.seen.append((kind, scalar(entity, 0, Uint64Flags)))
                for entity_id in u64_vector(payload, 2):
                    self.seen.append(("despawn", entity_id))

    def ids(self, kind=None):
        with self.lock:
            return {e[1] for e in self.seen if kind is None or e[0] == kind}

    def wild_ids(self, kind=None):
        return {i for i in self.ids(kind) if i >= WILD_ID_BASE}

    def player_ids(self, kind=None):
        return {i for i in self.ids(kind) if i < WILD_ID_BASE}

    def send(self, frame):
        self.sock.sendall(struct.pack("<I", len(frame)) + frame)

    def step(self, x, y):
        self.sequence += 1
        builder = flatbuffers.Builder(128)

        def build(b):
            b.StartObject(4)
            b.PrependFloat32Slot(0, x, 0.0)
            b.PrependFloat32Slot(1, y, 0.0)
            b.PrependFloat32Slot(2, 0.0, 0.0)
            b.PrependUint32Slot(3, self.sequence, 0)
            return b.EndObject()
        del builder
        self.send(envelope(build, MOVE))

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def connect(name, character_id, instance_type, attempts=4):
    """EnterAck 을 받을 때까지 다시 붙는다.

    Net 계층에 accept 된 소켓으로 첫 바이트가 한 번도 오지 않는 버그가 있어
    접속의 몇 %가 그대로 멈춘다. 이 시험이 보려는 것과 무관하므로 재시도한다
    (test-field-session.py 의 connect 와 같은 이유다).
    """
    for _ in range(attempts):
        client = Client(name, character_id, instance_type)
        if wait_for(lambda: client.room_id is not None, 3.0):
            return client
        client.close()
    raise SystemExit(f"{name}: EnterAck 을 못 받았다 (서버가 떠 있는지 확인할 것)")


def main():
    failures = []

    def check(label, ok, detail=""):
        print(f"{label:<22}: {'OK' if ok else 'FAIL'}{'  ' + detail if detail else ''}")
        if not ok:
            failures.append(label)

    # --- 방 분할과 격리 -------------------------------------------------
    a = connect("a", 8001, 1)
    b = connect("b", 8002, 1)
    c = connect("c", 8003, 1)

    check("방 분할", a.room_id == b.room_id and c.room_id != a.room_id,
          f"a={a.room_id} b={b.room_id} c={c.room_id}, 정원 {CAPACITY}")

    # 같은 자리에 세워 시야 안에 반드시 들어오게 한 뒤, 스냅샷이 오길 기다린다.
    for client in (a, b, c):
        client.step(25600.0, 25600.0)
    time.sleep(1.0)

    check("같은 방은 보인다", b.character_id in a.player_ids(),
          f"a 가 본 플레이어 {sorted(a.player_ids())}")
    check("다른 방은 안 보인다",
          c.character_id not in a.player_ids() and a.character_id not in c.player_ids(),
          f"c 가 본 플레이어 {sorted(c.player_ids())}")

    # --- 야생 -----------------------------------------------------------
    check("야생 스폰", len(a.wild_ids("spawn")) > 0 and len(c.wild_ids("spawn")) > 0,
          f"a={len(a.wild_ids('spawn'))} c={len(c.wild_ids('spawn'))}")

    moved_before = a.wild_ids("move")
    time.sleep(2.0)
    check("야생 이동", len(a.wild_ids("move") | moved_before) > 0,
          f"{len(a.wild_ids('move'))} 마리")

    # --- 종류 분리 ------------------------------------------------------
    d = connect("d", 8004, 2)
    check("종류 분리", d.room_id not in (a.room_id, c.room_id),
          f"type2={d.room_id}, type1={a.room_id}/{c.room_id}")

    # --- 모르는 종류는 거절 ---------------------------------------------
    stranger = Client("stranger", 8005, 99)
    refused = wait_for(lambda: not stranger.alive or stranger.notices, 3.0)
    check("모르는 종류 거절", refused and stranger.room_id is None,
          f"room_id={stranger.room_id}")
    stranger.close()

    # --- 빈 방 회수 -----------------------------------------------------
    rooms_before = {a.room_id, c.room_id, d.room_id}
    for client in (a, b, c, d):
        client.close()

    # 회수는 1초마다 돌고 --room-idle 이 지나야 지운다. 넉넉히 기다린다.
    time.sleep(ROOM_IDLE + 2.5)

    e = connect("e", 8006, 1)
    check("빈 방 회수", e.room_id not in rooms_before,
          f"새 방 {e.room_id}, 예전 {sorted(rooms_before)}")
    e.close()

    print()
    if failures:
        print("FAIL:", ", ".join(failures))
        sys.exit(1)
    print("PASS")


if __name__ == "__main__":
    main()
