"""인스턴스 서버의 지형 판정을 확인한다. 바닥(하이트맵)과 경계(구)다.

  1. 서버를 띄운다 (로그인 서버도 DB 도 필요 없다)
       .\\build\\windows-x64\\bin\\Debug\\InstanceServer.exe --dev-no-auth ^
           --instance-map 1=maps\\test-terrain.txt --wild-per-room 0
  2. python .\\tools\\test-instance-terrain.py

maps\\test-terrain.txt 이 셋을 따로 확인할 수 있게 짜여 있다.

  절벽   스폰 동쪽 400 ~ 600 에서 바닥이 0 -> 1000 으로 선다. 표본(34uu)당 170uu
         상승이라 maxStepUp(100) 에 걸린다. 스폰에서 동쪽 450 이면 여기 닿고,
         구 반지름 500 안이라 경계 때문이 아니라 **절벽 때문에** 막힌다.
  경계   스폰 중심 반지름 500 의 구. 북쪽 700 은 평지이고 속도 상한(800)도
         넘지 않으므로 **경계 때문에만** 막힌다.
  평지   동쪽 300 은 절벽 앞이고 구 안이다. 통과해야 한다 — 앞의 둘이
         "무조건 막는다" 로 통과해 버리는 것을 막는 대조군이다.

막힌 이동은 Correction 이 직전 위치로 돌아온다. 통과한 이동은 Correction 이
없고, 관찰자의 스냅샷에 실제로 옮겨간 좌표가 보인다.

프레임 조립은 test-field-session.py 의 것을 그대로 쓴다.
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
wait_for = _session.wait_for

ENTER, MOVE = 1, 3
ENTER_ACK, SNAPSHOT, NOTICE, CORRECTION = 2, 4, 5, 6

PORT = 9300
INSTANCE_TYPE = 1

# 속도 예산이 다시 차기를 기다린다. kMaxSpeed 600 + slack 200 = 800uu/s 라
# 1초를 쉬면 아래 이동 거리는 전부 속도 상한에 안 걸린다.
SETTLE = 1.1


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


def move(x, y, facing, sequence):
    def build(builder):
        builder.StartObject(4)
        builder.PrependFloat32Slot(0, x, 0.0)
        builder.PrependFloat32Slot(1, y, 0.0)
        builder.PrependFloat32Slot(2, facing, 0.0)
        builder.PrependUint32Slot(3, sequence, 0)
        return builder.EndObject()
    return build


class Client:
    def __init__(self, name, character_id):
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE  # 개발 인증서는 자체 서명이다
        raw = socket.create_connection(("127.0.0.1", PORT), timeout=10)
        self.sock = context.wrap_socket(raw, server_hostname="127.0.0.1")

        self.character_id = character_id
        self.spawn = None
        self.room_id = None
        self.origin = None
        self.sequence = 0
        self.lock = threading.Lock()
        self.corrections = {}   # sequence -> (x, y)
        self.positions = {}     # entity_id -> (x, y)

        threading.Thread(target=self._read, daemon=True).start()
        self.send(envelope(dev_enter(name, character_id, INSTANCE_TYPE), ENTER))

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

    def _dispatch(self, frame):
        tag = scalar(frame, 0, Uint8Flags)
        payload = child(frame, 1)
        if payload is None:
            return
        if tag == ENTER_ACK:
            self.spawn = (scalar(payload, 1, Float32Flags, 0.0),
                          scalar(payload, 2, Float32Flags, 0.0))
            self.room_id = scalar(payload, 5, Uint32Flags, 0)
            self.origin = scalar(payload, 6, Float32Flags, 0.0)
        elif tag == CORRECTION:
            with self.lock:
                self.corrections[scalar(payload, 0, Uint32Flags, 0)] = (
                    scalar(payload, 1, Float32Flags, 0.0),
                    scalar(payload, 2, Float32Flags, 0.0))
        elif tag == SNAPSHOT:
            with self.lock:
                for slot in (0, 1):  # spawned, moved
                    for entity in table_vector(payload, slot):
                        self.positions[scalar(entity, 0, Uint64Flags, 0)] = (
                            scalar(entity, 1, Float32Flags, 0.0),
                            scalar(entity, 2, Float32Flags, 0.0))

    def send(self, body):
        self.sock.sendall(struct.pack("<I", len(body)) + body)

    def walk(self, x, y):
        """이동 하나를 보내고 Correction 이 오는지 본다. 오면 (x, y), 아니면 None."""
        self.sequence += 1
        sequence = self.sequence
        self.send(envelope(move(x, y, 0.0, sequence), MOVE))
        # 보정은 즉시 돌아온다. 안 오는 것을 확인하려면 잠깐 기다려야 한다.
        wait_for(lambda: sequence in self.corrections, 0.5)
        with self.lock:
            return self.corrections.get(sequence)

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def main():
    subject = Client("terrain", 8101)
    observer = Client("watcher", 8102)
    if not wait_for(lambda: subject.spawn and observer.spawn, 5.0):
        print("FAIL: EnterAck 이 오지 않았다. 서버가 떠 있고 --instance-map 1=maps/test-terrain.txt 인지 확인할 것")
        return 1

    sx, sy = subject.spawn
    print(f"스폰 ({sx:.0f}, {sy:.0f}) room {subject.room_id} "
          f"origin_offset {subject.origin:.0f}")

    # EnterAck 이 좌표 변환 기준을 실어 준다. 이게 0 이면 클라가 ini 값으로
    # 되돌아가고, 필드와 인스턴스의 월드 크기가 달라 전원이 엉뚱한 자리에 선다.
    origin_ok = subject.origin is not None and abs(subject.origin - sx) < 1.0

    # 1) 절벽. 구 안(450 < 500)이라 막히는 이유는 바닥뿐이다.
    time.sleep(SETTLE)
    cliff = subject.walk(sx + 450.0, sy)
    cliff_blocked = cliff is not None and abs(cliff[0] - sx) < 1.0

    # 2) 경계. 평지이고 속도 상한(800)도 안 넘으므로 막히는 이유는 구뿐이다.
    time.sleep(SETTLE)
    outside = subject.walk(sx, sy + 700.0)
    bounds_blocked = outside is not None and abs(outside[1] - sy) < 1.0

    # 3) 대조군. 절벽 앞이고 구 안이라 통과해야 한다.
    time.sleep(SETTLE)
    flat = subject.walk(sx + 300.0, sy)
    moved = wait_for(
        lambda: abs(observer.positions.get(subject.character_id, (sx, sy))[0] - sx) > 250.0,
        2.0)

    print(f"절벽 450  : {'막힘' if cliff_blocked else '통과'}  (막혀야 정상)")
    print(f"경계 700  : {'막힘' if bounds_blocked else '통과'}  (막혀야 정상)")
    print(f"평지 300  : {'통과' if flat is None else '막힘'}  (통과해야 정상)")
    print(f"관찰자가 본 이동 : {moved}")
    print(f"좌표 기준 전달  : {origin_ok}  (스폰이 월드 중앙이라 offset 과 같아야 한다)")

    ok = cliff_blocked and bounds_blocked and flat is None and moved and origin_ok
    print("\nPASS" if ok else "\nFAIL")

    subject.close()
    observer.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
