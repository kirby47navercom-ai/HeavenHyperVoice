"""필드에 누가 있고 움직이고 있는지 옆에서 지켜본다.

  1. 서버를 띄운다
       cd Server
       .\build\windows-x64\bin\Release\FieldServer.exe --dev-no-auth
  2. python .\tools\watch-field.py

관찰자로 한 자리 차지하고 들어가서, 시야에 들어온 엔티티와 초당 이동 갱신 횟수를
1초마다 찍는다. 언리얼 클라이언트가 실제로 Move 를 보내고 있는지 확인하는 용도다.
움직이지 않고 서 있어도 클라이언트는 20Hz 로 Move 를 보내므로, 붙어 있으면 rate 가
0 이 아니다. 0 이면 붙어는 있는데 보내지 않는 것이고, 목록에 없으면 입장 자체를
못 한 것이다.

관찰자는 월드 중앙에 선다. 시야는 2800uu 라 그보다 멀리 있는 사람은 안 보인다.
--at 으로 다른 곳에서 볼 수 있다.
"""
import argparse
import socket
import ssl
import struct
import threading
import time

import flatbuffers
from flatbuffers.number_types import Float32Flags, Uint8Flags, Uint16Flags, Uint64Flags
from flatbuffers.table import Table

ENTER, MOVE = 1, 3
SNAPSHOT = 4

# 관찰자 자신의 번호. 실제 캐릭터와 겹치지 않게 높은 값을 쓴다.
OBSERVER_ID = 999001


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
        builder.PrependUOffsetTRelativeSlot(1, offset, 0)
        builder.PrependUint64Slot(2, character_id, 0)
        builder.PrependUint16Slot(3, 0, 0)
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


def scalar_vector(table, slot, flags, width):
    offset = table.Offset(4 + 2 * slot)
    if not offset:
        return []
    start = table.Vector(offset)
    return [table.Get(flags, start + i * width) for i in range(table.VectorLen(offset))]


def read_string(table, slot):
    offset = table.Offset(4 + 2 * slot)
    return table.String(offset + table.Pos).decode("utf-8", "replace") if offset else ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9200)
    parser.add_argument("--at", nargs=2, type=float, metavar=("X", "Y"),
                        default=[25600.0, 25600.0], help="관찰자가 설 서버 좌표")
    args = parser.parse_args()

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE  # 개발 인증서는 자체 서명이다
    sock = context.wrap_socket(
        socket.create_connection((args.host, args.port), timeout=10),
        server_hostname=args.host)
    # 연결에만 걸린 타임아웃이다. 그대로 두면 조용한 구간에서 recv 가 끊긴다.
    sock.settimeout(None)

    known = {}       # entity_id -> 표시 이름
    positions = {}   # entity_id -> (x, y)
    counts = {}      # entity_id -> 이번 창에서 받은 이동 갱신 수
    lock = threading.Lock()

    def reader():
        try:
            while True:
                header = sock.recv(4)
                if len(header) < 4:
                    return
                size = struct.unpack("<I", header)[0]
                buf = b""
                while len(buf) < size:
                    chunk = sock.recv(size - len(buf))
                    if not chunk:
                        return
                    buf += chunk

                frame = root(buf)
                if scalar(frame, 0, Uint8Flags) != SNAPSHOT:
                    continue
                payload = child(frame, 1)
                if payload is None:
                    continue

                with lock:
                    for state in table_vector(payload, 0):  # spawned
                        entity_id = scalar(state, 0, Uint64Flags)
                        nickname = read_string(state, 4) or f"#{entity_id}"
                        species = scalar(state, 5, Uint16Flags)
                        known[entity_id] = nickname + (f" (포켓몬 {species})" if species else "")
                        positions[entity_id] = (scalar(state, 1, Float32Flags, 0.0),
                                                scalar(state, 2, Float32Flags, 0.0))
                    for state in table_vector(payload, 1):  # moved
                        entity_id = scalar(state, 0, Uint64Flags)
                        positions[entity_id] = (scalar(state, 1, Float32Flags, 0.0),
                                                scalar(state, 2, Float32Flags, 0.0))
                        counts[entity_id] = counts.get(entity_id, 0) + 1
                    for entity_id in scalar_vector(payload, 2, Uint64Flags, 8):  # despawned
                        known.pop(entity_id, None)
                        positions.pop(entity_id, None)
                        counts.pop(entity_id, None)
        except OSError:
            pass

    threading.Thread(target=reader, daemon=True).start()

    def send(frame):
        sock.sendall(struct.pack("<I", len(frame)) + frame)

    send(envelope(dev_enter("관찰자", OBSERVER_ID), ENTER))
    time.sleep(0.5)
    send(envelope(move(args.at[0], args.at[1], 0.0, 1), MOVE))

    # 살아있는 모니터라 파이프로 넘길 때도 바로 보여야 한다.
    print(f"({args.at[0]:.0f}, {args.at[1]:.0f}) 에서 관찰 중. 시야 2800uu. Ctrl+C 로 종료.",
          flush=True)
    try:
        while True:
            time.sleep(1.0)
            with lock:
                others = sorted(k for k in known if k != OBSERVER_ID)
                snapshot = [(k, known[k], positions.get(k, (0.0, 0.0)), counts.get(k, 0))
                            for k in others]
                counts.clear()

            if not snapshot:
                print("시야에 아무도 없음", flush=True)
                continue
            for entity_id, name, (x, y), rate in snapshot:
                print(f"  {entity_id:>8}  {name:<20} ({x:8.0f}, {y:8.0f})  {rate:>3} 갱신/초",
                      flush=True)
    except KeyboardInterrupt:
        print()


if __name__ == "__main__":
    try:
        main()
    except ConnectionRefusedError:
        raise SystemExit("필드 서버(9200)에 못 붙었다. --dev-no-auth 로 띄웠는지 확인.")
