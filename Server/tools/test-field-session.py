"""필드 서버의 세션 수명과 속도 상한을 확인한다.

  1. 서버를 띄운다 (로그인 서버도 DB 도 필요 없다)
       .\build\windows-x64\bin\Release\FieldServer.exe --dev-no-auth
  2. python .\tools\test-field-session.py

세 가지를 본다.

  재접속   같은 캐릭터로 다시 붙으면 먼저 있던 세션이 끊긴다. 그 세션의 정리가
           **새로 들어온 쪽**을 월드에서 지워 버리면 안 된다. 끊긴 뒤에도 새
           세션의 이동이 관찰자에게 계속 보여야 한다.
  퇴장     그냥 끊은 캐릭터는 관찰자 시야에서 despawn 으로 사라져야 한다.
  속도     Move 를 최소 간격으로 계속 밀어 넣어도 실제 이동은 상한 근처여야
           한다. 지터 여유를 메시지마다 새로 주면 여기서 몇 배가 튄다.

프레임 조립은 test-field-walls.py 와 같은 방식이다 (flatc 없이 손으로).
슬롯 번호는 field.fbs 의 선언 순서다. 새 필드는 항상 뒤에 붙일 것.
"""
import socket
import ssl
import struct
import threading
import time

import flatbuffers
from flatbuffers.number_types import Float32Flags, Uint8Flags, Uint64Flags
from flatbuffers.table import Table

ENTER, MOVE = 1, 3
ENTER_ACK, SNAPSHOT, NOTICE, CORRECTION = 2, 4, 5, 6

OBSERVER_ID = 7001
SUBJECT_ID = 7002
LEAVER_ID = 7003

# 서버의 kMinMoveInterval 은 10ms 다. 속도 시험은 일부러 그보다 살짝 위에서
# 계속 밀어 넣는다 — 더 빠르면 프레임이 그냥 버려져 시험이 무의미해진다.
SPAM_INTERVAL = 0.012

# FieldGeometry.h: kMaxSpeed 600 + kSlackRefill 200 = 지속 상한 800uu/s.
# 측정 노이즈와 첫 예산(kSpeedSlack 200uu)을 감안해 넉넉히 잡는다.
# 메시지마다 여유를 새로 주던 시절에는 여기가 4000uu/s 를 넘었다.
SPEED_CEILING = 1600.0


def envelope(build, tag):
    builder = flatbuffers.Builder(256)
    payload = build(builder)
    builder.StartObject(2)
    builder.PrependUint8Slot(0, tag, 0)
    builder.PrependUOffsetTRelativeSlot(1, payload, 0)
    builder.Finish(builder.EndObject())
    return bytes(builder.Output())


def dev_enter(name, character_id):
    def build(builder):
        offset = builder.CreateString(name)
        builder.StartObject(4)
        builder.PrependUOffsetTRelativeSlot(1, offset, 0)  # dev_name
        builder.PrependUint64Slot(2, character_id, 0)      # dev_character_id
        builder.PrependUint16Slot(3, 0, 0)                 # dev_partner_species
        return builder.EndObject()
    return build


def move(x, y, facing, sequence):
    def build(builder):
        builder.StartObject(4)
        builder.PrependFloat32Slot(0, x, 0.0)
        builder.PrependFloat32Slot(1, y, 0.0)
        builder.PrependFloat32Slot(2, facing, 0.0)
        builder.PrependUint32Slot(3, sequence, 0)
        return builder.EndObject()
    return build


def root(buf):
    return Table(buf, struct.unpack_from("<I", buf, 0)[0])


def scalar(table, slot, flags, default=0):
    offset = table.Offset(4 + 2 * slot)
    return table.Get(flags, offset + table.Pos) if offset else default


def child(table, slot):
    offset = table.Offset(4 + 2 * slot)
    return Table(table.Bytes, table.Indirect(offset + table.Pos)) if offset else None


def table_vector(table, slot):
    offset = table.Offset(4 + 2 * slot)
    if not offset:
        return []
    start = table.Vector(offset)
    return [Table(table.Bytes, table.Indirect(start + i * 4))
            for i in range(table.VectorLen(offset))]


def u64_vector(table, slot):
    offset = table.Offset(4 + 2 * slot)
    if not offset:
        return []
    start = table.Vector(offset)
    return [table.Get(Uint64Flags, start + i * 8)
            for i in range(table.VectorLen(offset))]


class Client:
    def __init__(self, name, character_id, host="127.0.0.1", port=9200):
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE  # 개발 인증서는 자체 서명이다
        raw = socket.create_connection((host, port), timeout=10)
        self.sock = context.wrap_socket(raw, server_hostname=host)
        self.sock.settimeout(None)

        self.character_id = character_id
        self.spawn = None
        self.position = None      # 마지막 Correction 이 알려준 서버 기준 좌표
        self.sequence = 0
        self.alive = True
        self.lock = threading.Lock()
        self.seen = []            # (시각, 종류, entity_id), 종류는 spawn/move/despawn

        threading.Thread(target=self._read, daemon=True).start()
        self.send(envelope(dev_enter(name, character_id), ENTER))

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
            self.position = self.spawn
        elif tag == CORRECTION:
            self.position = (scalar(payload, 1, Float32Flags, 0.0),
                             scalar(payload, 2, Float32Flags, 0.0))
        elif tag == SNAPSHOT:
            now = time.time()
            with self.lock:
                for kind, slot in (("spawn", 0), ("move", 1)):
                    for entity in table_vector(payload, slot):
                        self.seen.append((now, kind, scalar(entity, 0, Uint64Flags)))
                for entity_id in u64_vector(payload, 2):
                    self.seen.append((now, "despawn", entity_id))

    def events(self, entity_id, kind=None, since=0.0):
        with self.lock:
            return [e for e in self.seen
                    if e[2] == entity_id and e[0] >= since
                    and (kind is None or e[1] == kind)]

    def send(self, frame):
        self.sock.sendall(struct.pack("<I", len(frame)) + frame)

    def step(self, x, y):
        self.sequence += 1
        self.send(envelope(move(x, y, 0.0, self.sequence), MOVE))

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def wait_for(predicate, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(0.05)
    return False


def connect(name, character_id, attempts=4):
    """EnterAck 을 받을 때까지 다시 붙는다.

    Net 계층에 accept 된 소켓으로 첫 바이트가 한 번도 오지 않는 버그가 있어
    접속의 몇 %가 그대로 멈춘다 (서버 로그에 accepted 뒤 handshake timeout 만
    남는다). 이 시험이 보려는 것과 무관하므로 여기서는 다시 붙는다.
    그 버그가 고쳐지면 이 재시도를 지울 것.
    """
    for _ in range(attempts):
        client = Client(name, character_id)
        if wait_for(lambda: client.spawn is not None, 3.0):
            return client
        client.close()
    raise AssertionError(f"{name}: EnterAck 을 {attempts}번 시도해도 받지 못했다")


def check_reconnect(observer, spawn_x, spawn_y, failures):
    """같은 캐릭터로 다시 붙었을 때 새 세션이 살아남는가."""
    victim = connect("먼저", SUBJECT_ID)
    assert wait_for(lambda: observer.events(SUBJECT_ID, "spawn"), 5.0), \
        "관찰자가 victim 을 보지 못했다"

    intruder = connect("나중", SUBJECT_ID)  # 같은 번호 = 같은 계정 (dev 모드)
    assert wait_for(lambda: not victim.alive, 5.0), "먼저 붙은 세션이 끊기지 않았다"

    settled = time.time()
    time.sleep(0.5)  # victim 의 onClosed 가 끝날 시간을 준다

    # 관찰자 시야를 벗어나지 않게 제자리에서 진동시킨다.
    for i in range(40):
        intruder.step(spawn_x, spawn_y + (200.0 if i % 2 else -200.0))
        time.sleep(0.05)

    if not intruder.alive:
        failures.append("새 세션이 끊겼다")
    late = observer.events(SUBJECT_ID, "move", since=settled + 0.5)
    if not late:
        failures.append("재접속 후 이동이 관찰되지 않는다 "
                        "- 옛 세션의 정리가 새 세션을 지웠다")
    print(f"재접속 후 관찰된 이동   : {len(late)}건")
    return intruder


def check_leave(observer, failures):
    """그냥 끊은 캐릭터가 시야에서 사라지는가."""
    leaver = connect("퇴장", LEAVER_ID)
    assert wait_for(lambda: observer.events(LEAVER_ID, "spawn"), 5.0), \
        "관찰자가 leaver 를 보지 못했다"
    leaver.close()
    gone = wait_for(lambda: observer.events(LEAVER_ID, "despawn"), 5.0)
    if not gone:
        failures.append("끊은 캐릭터가 시야에서 사라지지 않는다 (유령)")
    print(f"퇴장 despawn            : {gone}")


def check_speed(client, spawn_x, spawn_y, failures):
    """최소 간격으로 밀어 넣어도 실제 이동이 상한 근처인가."""
    # 시험용 벽(x=26600)에 닿지 않게 북쪽으로만 민다.
    target_y = spawn_y + 20000.0
    started = time.time()
    while time.time() - started < 3.0:
        client.step(spawn_x, target_y)
        time.sleep(SPAM_INTERVAL)
    elapsed = time.time() - started
    time.sleep(0.3)

    travelled = abs(client.position[1] - spawn_y)
    speed = travelled / elapsed
    print(f"스팸 이동 속도          : {speed:.0f} uu/s "
          f"({travelled:.0f}uu / {elapsed:.1f}s, 상한 {SPEED_CEILING:.0f})")
    if speed > SPEED_CEILING:
        failures.append(f"속도 상한이 새고 있다: {speed:.0f} uu/s")


def main():
    failures = []

    observer = connect("관찰자", OBSERVER_ID)
    spawn_x, spawn_y = observer.spawn

    intruder = check_reconnect(observer, spawn_x, spawn_y, failures)
    check_leave(observer, failures)
    check_speed(intruder, spawn_x, spawn_y, failures)

    for client in (observer, intruder):
        client.close()

    print()
    if failures:
        for line in failures:
            print(f"FAIL: {line}")
        raise SystemExit(1)
    print("PASS")


if __name__ == "__main__":
    main()
