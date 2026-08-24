"""필드 서버의 벽 충돌 판정이 실제로 걸리는지 확인한다.

  1. 서버를 띄운다 (로그인 서버도 DB 도 필요 없다)
       .\build\windows-x64\bin\Release\FieldServer.exe --dev-no-auth --map .\maps\test-wall.txt
  2. python .\tools\test-field-walls.py

maps\test-wall.txt 은 x=26600 에 남북으로 선 반두께 25 의 벽 하나다. 캡슐 반지름이
34 이므로 서 있을 수 있는 가장 동쪽 지점은 26600 - 25 - 34 = 26541 이다.

세 가지를 본다.

  통과   벽면에 정확히 붙는 것. 클라이언트 물리가 캡슐을 여기에 세우므로 서버가
         이 위치를 거부하면 접속하자마자 보정이 무한히 날아간다.
  통과   벽에 붙은 채로 벽을 따라 걷는 것.
  차단   벽 안쪽으로 파고드는 것.

프레임 조립은 flatc 가 만든 코드 없이 손으로 한다. 슬롯 번호는 field.fbs 의 선언
순서와 같으므로, 필드를 중간에 끼워 넣으면 여기가 조용히 어긋난다. 새 필드는 항상
뒤에 붙일 것.
"""
import socket
import ssl
import struct
import threading
import time

import flatbuffers
from flatbuffers.number_types import Float32Flags, Uint8Flags, Uint32Flags
from flatbuffers.table import Table

ENTER, MOVE = 1, 3
ENTER_ACK, CORRECTION = 2, 6

WALL_CENTER_X = 26600.0
WALL_HALF_THICKNESS = 25.0
CAPSULE_RADIUS = 34.0
WALL_FACE_X = WALL_CENTER_X - WALL_HALF_THICKNESS - CAPSULE_RADIUS

CHARACTER_ID = 5002
STEP = 100.0

# 서버의 최소 이동 간격(kMinMoveInterval 10ms)보다 넉넉히 잡는다. 더 빠르면
# 프레임이 그냥 버려진다.
#
# 위쪽 상한도 있다. STEP / SEND_INTERVAL 이 kMaxSpeed(600uu/s) 를 넘으면 벽이
# 아니라 속도 클램프가 보정을 내보내서 이 시험이 벽을 못 봐도 통과한다.
# 100uu / 0.2s = 500uu/s 로 상한 아래에 둔다. 실제 클라이언트는 260 x 1.5 =
# 390uu/s 라 이보다도 느리다.
SEND_INTERVAL = 0.2


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


class Client:
    def __init__(self, host="127.0.0.1", port=9200):
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE  # 개발 인증서는 자체 서명이다
        raw = socket.create_connection((host, port), timeout=10)
        self.sock = context.wrap_socket(raw, server_hostname=host)
        # 연결에만 걸린 타임아웃이다. 그대로 두면 조용한 구간에서 recv 가 끊긴다.
        self.sock.settimeout(None)

        self.spawn = (0.0, 0.0)
        self.corrections = []
        self.sequence = 0
        threading.Thread(target=self._read, daemon=True).start()

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
                frame = root(buf)
                tag = scalar(frame, 0, Uint8Flags)
                payload = child(frame, 1)
                if payload is None:
                    continue
                if tag == ENTER_ACK:
                    self.spawn = (scalar(payload, 1, Float32Flags, 0.0),
                                  scalar(payload, 2, Float32Flags, 0.0))
                elif tag == CORRECTION:
                    self.corrections.append(scalar(payload, 0, Uint32Flags))
        except OSError:
            pass

    def send(self, frame):
        self.sock.sendall(struct.pack("<I", len(frame)) + frame)

    def step(self, x, y):
        self.sequence += 1
        self.send(envelope(move(x, y, 0.0, self.sequence), MOVE))
        time.sleep(SEND_INTERVAL)
        return self.sequence


def main():
    client = Client()
    client.send(envelope(dev_enter("벽시험", CHARACTER_ID), ENTER))
    time.sleep(1.0)

    x, y = client.spawn
    assert x > 0.0, "no EnterAck. Is the server running with --dev-no-auth?"
    assert x < WALL_FACE_X, f"spawn {x} is already past the wall"

    # 벽면 앞까지 걸어간다. 여기까지는 아무것도 보정되면 안 된다.
    while x + STEP <= WALL_FACE_X:
        x += STEP
        client.step(x, y)
    time.sleep(0.4)
    assert not client.corrections, f"open ground was corrected: {client.corrections}"

    # 벽면에 정확히 붙인다.
    touch = client.step(WALL_FACE_X, y)
    time.sleep(0.4)
    assert touch not in client.corrections, "standing against the wall was rejected"

    # 붙은 채로 벽을 따라 북쪽으로.
    slide = [client.step(WALL_FACE_X, y + STEP * i) for i in range(1, 11)]
    time.sleep(0.6)
    hit = [s for s in slide if s in client.corrections]
    assert not hit, f"sliding along the wall was blocked at {hit}"

    # 벽 안쪽. 여기서만 막혀야 한다.
    dig = client.step(WALL_CENTER_X - 10.0, y + STEP * 10)
    time.sleep(0.4)
    assert dig in client.corrections, "walked into the wall"

    # 한 틱에 벽을 통째로 건너뛰는 경우. 도착점만 보면 그냥 지나간다.
    #
    # 거리는 속도 예산(kMaxSpeed * 경과 + 남은 slack) 안에 둬야 한다. 넘기면
    # 속도 클램프가 먼저 보정을 내보내서, 벽을 못 봐도 이 검사가 통과한다.
    tunnel = client.step(WALL_CENTER_X + 100.0, y + STEP * 10)
    time.sleep(0.4)
    assert tunnel in client.corrections, "tunneled through the wall"

    print("OK")


if __name__ == "__main__":
    try:
        main()
    except ConnectionRefusedError:
        raise SystemExit("cannot reach the field server on 9200. "
                         "Start it with --dev-no-auth --map maps\test-wall.txt.")
