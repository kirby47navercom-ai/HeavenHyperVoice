"""파티 시스템 왕복 시험.

계정 둘을 만들고 캐릭터를 세운 뒤 채팅 서버에 붙어서
초대 -> 수락 -> 파티챗 -> 파티장 입장 순서를 확인한다.

인스턴스 서버가 떠 있으면 개별 입장 거절까지 본다.

    python Server/tools/test-party.py
"""

import importlib.util
import os
import random
import socket
import ssl
import struct
import sys
import time

import flatbuffers
from flatbuffers.number_types import BoolFlags, Uint8Flags, Uint16Flags, Uint64Flags

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "fs", os.path.join(HERE, "test-field-session.py"))
fs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fs)

LOGIN_PORT = 9100
CHAT_PORT = 9000

# login.fbs union 순서 (1부터)
L_LOGIN, L_REGISTER, L_CREATE, L_SELECT = 1, 3, 5, 7

# chat.fbs union 순서 (1부터)
C_HELLO, C_SAY, C_NOTICE, C_CHAT = 1, 2, 3, 4
C_INVITE, C_ACCEPT, C_DECLINE, C_LEAVE = 5, 6, 7, 8
C_KICK, C_ENTER_INSTANCE, C_SAY_PARTY = 9, 10, 11
C_PARTY_STATE, C_PARTY_INVITED, C_PARTY_READY = 12, 13, 14

failures = []


def check(condition, label):
    mark = "OK  " if condition else "FAIL"
    print(f"  [{mark}] {label}")
    if not condition:
        failures.append(label)


def connect(port):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    raw = socket.create_connection(("127.0.0.1", port), timeout=10)
    return context.wrap_socket(raw, server_hostname="127.0.0.1")


def send(sock, body):
    sock.sendall(struct.pack("<I", len(body)) + body)


def recv(sock, timeout=5.0):
    sock.settimeout(timeout)
    try:
        header = sock.recv(4)
    except (socket.timeout, OSError):
        return None
    if len(header) < 4:
        return None
    size = struct.unpack("<I", header)[0]
    buffer = b""
    while len(buffer) < size:
        chunk = sock.recv(size - len(buffer))
        if not chunk:
            return None
        buffer += chunk
    return fs.root(buffer)


def drain(sock, wanted_tag, timeout=5.0, limit=12):
    """원하는 태그가 나올 때까지 프레임을 읽어 버린다."""
    deadline = time.time() + timeout
    for _ in range(limit):
        remaining = deadline - time.time()
        if remaining <= 0:
            return None
        frame = recv(sock, remaining)
        if frame is None:
            return None
        if fs.scalar(frame, 0, Uint8Flags) == wanted_tag:
            return frame
    return None


def string_at(table, slot, default=""):
    offset = table.Offset(4 + slot * 2)
    return table.String(table.Pos + offset).decode("utf-8") if offset else default


def two_strings(first, second):
    def build(builder):
        a = builder.CreateString(first)
        b = builder.CreateString(second)
        builder.StartObject(2)
        builder.PrependUOffsetTRelativeSlot(0, a, 0)
        builder.PrependUOffsetTRelativeSlot(1, b, 0)
        return builder.EndObject()
    return build


def one_string(value):
    def build(builder):
        a = builder.CreateString(value)
        builder.StartObject(1)
        builder.PrependUOffsetTRelativeSlot(0, a, 0)
        return builder.EndObject()
    return build


def one_u64(value):
    def build(builder):
        builder.StartObject(1)
        builder.PrependUint64Slot(0, value, 0)
        return builder.EndObject()
    return build


def one_u32(value):
    def build(builder):
        builder.StartObject(1)
        builder.PrependUint32Slot(0, value, 0)
        return builder.EndObject()
    return build


def empty_table():
    def build(builder):
        builder.StartObject(0)
        return builder.EndObject()
    return build


def create_character(nickname, dex):
    def build(builder):
        name = builder.CreateString(nickname)
        builder.StartObject(4)
        builder.PrependUOffsetTRelativeSlot(0, name, 0)
        builder.PrependUint16Slot(3, dex, 0)
        return builder.EndObject()
    return build


def hello(ticket):
    def build(builder):
        blob = builder.CreateByteVector(ticket)
        builder.StartObject(1)
        builder.PrependUOffsetTRelativeSlot(0, blob, 0)
        return builder.EndObject()
    return build


def make_account(nickname):
    """가입 -> 로그인 -> 캐릭터 생성 -> 선택. 채팅 티켓을 돌려준다."""
    user = "qa_%08d" % random.randint(0, 99999999)
    password = "verylongpassword1"

    sock = connect(LOGIN_PORT)
    send(sock, fs.envelope(two_strings(user, password), L_REGISTER))
    recv(sock)
    sock.close()

    sock = connect(LOGIN_PORT)
    send(sock, fs.envelope(two_strings(user, password), L_LOGIN))
    recv(sock)

    send(sock, fs.envelope(create_character(nickname, 4), L_CREATE))
    listing = recv(sock)
    payload = fs.child(listing, 1)
    characters = payload.Offset(8)
    vector = payload.Vector(characters)
    entry = flatbuffers.table.Table(payload.Bytes, payload.Indirect(vector))
    character_id = fs.scalar(entry, 0, Uint64Flags, 0)

    send(sock, fs.envelope(one_u64(character_id), L_SELECT))
    response = recv(sock)
    sock.close()

    payload = fs.child(response, 1)
    if not fs.scalar(payload, 0, BoolFlags, False):
        raise RuntimeError("캐릭터 선택 실패: " + string_at(payload, 1))

    endpoints = payload.Offset(8)
    vector = payload.Vector(endpoints)
    count = payload.VectorLen(endpoints)
    for index in range(count):
        row = flatbuffers.table.Table(payload.Bytes, payload.Indirect(vector + index * 4))
        if string_at(row, 0) != "chat":
            continue
        blob = row.Offset(10)
        start = row.Vector(blob)
        length = row.VectorLen(blob)
        return bytes(row.Bytes[start:start + length])
    raise RuntimeError("채팅 엔드포인트가 없다")


def join_chat(ticket):
    sock = connect(CHAT_PORT)
    send(sock, fs.envelope(hello(ticket), C_HELLO))
    return sock


def party_state(frame):
    payload = fs.child(frame, 1)
    party_id = fs.scalar(payload, 0, Uint64Flags, 0)
    members = []
    offset = payload.Offset(6)
    if offset:
        vector = payload.Vector(offset)
        for index in range(payload.VectorLen(offset)):
            row = flatbuffers.table.Table(payload.Bytes,
                                          payload.Indirect(vector + index * 4))
            members.append(string_at(row, 1))
    return party_id, members, string_at(payload, 2)


def main():
    leader_name = "리더%05d" % random.randint(0, 99999)
    member_name = "멤버%05d" % random.randint(0, 99999)

    print("계정 준비")
    leader_ticket = make_account(leader_name)
    member_ticket = make_account(member_name)
    print(f"  파티장 {leader_name} / 파티원 {member_name}")

    leader = join_chat(leader_ticket)
    member = join_chat(member_ticket)
    time.sleep(0.5)

    print("\n1. 파티가 없는 상태")
    frame = drain(leader, C_PARTY_STATE)
    check(frame is not None, "접속하면 PartyState 가 온다")
    if frame is not None:
        party_id, members, _ = party_state(frame)
        check(party_id == 0, "처음에는 파티가 없다 (party_id 0)")

    print("\n2. 초대와 수락")
    send(leader, fs.envelope(one_string(member_name), C_INVITE))
    frame = drain(member, C_PARTY_INVITED)
    check(frame is not None, "초대장이 상대에게 도착한다")
    invited_party = 0
    if frame is not None:
        payload = fs.child(frame, 1)
        invited_party = fs.scalar(payload, 0, Uint64Flags, 0)
        check(string_at(payload, 1) == leader_name, "초대한 사람 닉네임이 실려 온다")

    # 초대를 보낸 순간 파티가 생기므로 파티장은 여기서 이미 PartyState(본인 1명)를
    # 받는다. 그걸 먼저 비워야 수락 뒤의 갱신을 보게 된다.
    drain(leader, C_PARTY_STATE, timeout=2.0)

    send(member, fs.envelope(one_u64(invited_party), C_ACCEPT))
    frame = drain(leader, C_PARTY_STATE)
    check(frame is not None, "수락하면 파티장에게 갱신이 온다")
    if frame is not None:
        party_id, members, _ = party_state(frame)
        check(len(members) == 2, f"파티원이 둘이다 (실제 {len(members)})")
        check(members and members[0] == leader_name,
              "목록 첫 번째가 파티장이다")

    print("\n3. 파티 채팅")
    send(member, fs.envelope(one_string("파티원만 보이나요"), C_SAY_PARTY))
    frame = drain(leader, C_CHAT)
    check(frame is not None, "파티 발화가 파티장에게 온다")
    if frame is not None:
        payload = fs.child(frame, 1)
        # channel 은 Chat 의 슬롯 2 (ubyte). 0=General 1=Party 2=Combat
        channel = fs.scalar(payload, 2, Uint8Flags, 0)
        check(channel == 1, f"파티 채널로 표시된다 (channel {channel})")
        check(string_at(payload, 0) == member_name,
              "닉네임에 표시를 섞지 않는다 (화면 표시는 클라 몫)")

    # 도배 차단은 채널 공통이다. 채널을 바꿔 가며 속도를 두 배로 낼 수 없게
    # 한 것이라, 시험도 간격을 지켜야 한다.
    time.sleep(0.3)
    send(member, fs.envelope(one_string("전체에게"), C_SAY))
    frame = drain(leader, C_CHAT)
    check(frame is not None, "일반 발화가 온다")
    if frame is not None:
        channel = fs.scalar(fs.child(frame, 1), 2, Uint8Flags, 0)
        check(channel == 0, f"일반 채널로 표시된다 (channel {channel})")

    print("\n4. 입장 권한")
    send(member, fs.envelope(one_u32(1), C_ENTER_INSTANCE))
    frame = drain(member, C_NOTICE, timeout=3.0)
    check(frame is not None and "파티장만" in string_at(fs.child(frame, 1), 0),
          "파티원은 입장을 시작할 수 없다")

    send(leader, fs.envelope(one_u32(1), C_ENTER_INSTANCE))
    ready_leader = drain(leader, C_PARTY_READY, timeout=3.0)
    ready_member = drain(member, C_PARTY_READY, timeout=3.0)
    check(ready_leader is not None, "파티장에게 입장 신호가 온다")
    check(ready_member is not None, "파티원에게도 입장 신호가 온다")
    if ready_member is not None:
        check(fs.scalar(fs.child(ready_member, 1), 0, flatbuffers.number_types.Uint32Flags, 0) == 1,
              "인스턴스 종류가 실려 온다")

    print("\n5. 탈퇴와 자동 위임")
    send(leader, fs.envelope(empty_table(), C_LEAVE))
    frame = drain(member, C_PARTY_STATE, timeout=3.0)
    check(frame is not None, "남은 사람에게 갱신이 온다")
    if frame is not None:
        party_id, members, _ = party_state(frame)
        check(len(members) == 1, f"한 명만 남는다 (실제 {len(members)})")
        check(members and members[0] == member_name,
              "다음 사람이 파티장이 된다 (자동 위임)")

    leader.close()
    member.close()

    print()
    if failures:
        print(f"실패 {len(failures)}건:")
        for item in failures:
            print("  -", item)
        return 1
    print("전부 통과")
    return 0


if __name__ == "__main__":
    sys.exit(main())
