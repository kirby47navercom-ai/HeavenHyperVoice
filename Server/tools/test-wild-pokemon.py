"""필드 서버 야생 포켓몬 실동작 검증.

  1. 서버를 야생과 함께 띄운다 (bin 에서 실행하거나 --wild-script 절대경로)
       .\build\windows-x64\bin\Debug\FieldServer.exe --dev-no-auth --wild-count 600
  2. python .\tools\test-wild-pokemon.py

--dev-no-auth 로 뜬 필드 서버에 dev Enter 로 붙어, 몇 초간 Snapshot 을 받아
  1) species != 0 인 엔티티(야생)가 spawn 되는가
  2) 그 엔티티의 좌표가 시간에 따라 바뀌는가 (AI 가 실제로 움직이는가)
를 확인한다.

field.fbs 의 파이썬 바인딩은 flatc 로 즉석 생성한다 (pip install flatbuffers 필요).
"""
import os, socket, ssl, struct, subprocess, sys, tempfile, time


def generate_bindings():
    here = os.path.dirname(os.path.abspath(__file__))
    schema = os.path.join(here, "..", "Protocol", "field.fbs")
    root = os.path.join(here, "..")
    flatc = None
    for base, _dirs, files in os.walk(os.path.join(root, "build")):
        if "flatc.exe" in files:
            flatc = os.path.join(base, "flatc.exe")
            break
    if flatc is None:
        sys.exit("flatc.exe not found under build/. Configure the project first.")
    out = tempfile.mkdtemp(prefix="hhv_fbgen_")
    subprocess.run([flatc, "--python", "-o", out, schema], check=True)
    sys.path.insert(0, out)


generate_bindings()

import flatbuffers
from HeavenField import Envelope, Enter, Snapshot, EntityState, Payload

HOST, PORT = "127.0.0.1", 9200


def build_enter():
    b = flatbuffers.Builder(128)
    name = b.CreateString("wildtester")
    Enter.Start(b)
    Enter.AddDevName(b, name)
    Enter.AddDevCharacterId(b, 123456789)  # dev 모드라 아무 값
    Enter.AddDevPartnerSpecies(b, 0)
    enter = Enter.End(b)
    Envelope.Start(b)
    Envelope.AddPayloadType(b, Payload.Payload.Enter)
    Envelope.AddPayload(b, enter)
    b.Finish(Envelope.End(b))
    body = bytes(b.Output())
    return struct.pack("<I", len(body)) + body


def frames(sock):
    buf = b""
    sock.settimeout(0.3)
    while True:
        try:
            chunk = sock.recv(8192)
        except (ssl.SSLWantReadError, socket.timeout):
            yield None
            continue
        if not chunk:
            return
        buf += chunk
        while len(buf) >= 4:
            n = struct.unpack("<I", buf[:4])[0]
            if len(buf) - 4 < n:
                break
            yield buf[4:4 + n]
            buf = buf[4 + n:]


def read_entities(vec_fn, count_fn):
    out = []
    for i in range(count_fn()):
        e = vec_fn(i)
        out.append((e.EntityId(), e.X(), e.Y(), e.Species()))
    return out


ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE
raw = socket.create_connection((HOST, PORT), timeout=5)
sock = ctx.wrap_socket(raw, server_hostname=HOST)
sock.sendall(build_enter())

wild_first = {}   # entity_id -> (x, y) 처음 본 위치
wild_last = {}    # entity_id -> (x, y) 마지막 위치
saw_species = False
entered = False

deadline = time.time() + 6.0
for body in frames(sock):
    if time.time() > deadline:
        break
    if body is None:
        continue
    env = Envelope.Envelope.GetRootAs(body, 0)
    pt = env.PayloadType()
    if pt == Payload.Payload.EnterAck:
        entered = True
        continue
    if pt != Payload.Payload.Snapshot:
        continue
    snap = Snapshot.Snapshot()
    snap.Init(env.Payload().Bytes, env.Payload().Pos)

    for eid, x, y, sp in read_entities(snap.Spawned, snap.SpawnedLength):
        if sp != 0:
            saw_species = True
            wild_first.setdefault(eid, (x, y))
            wild_last[eid] = (x, y)
    for eid, x, y, sp in read_entities(snap.Moved, snap.MovedLength):
        if eid in wild_first:
            wild_last[eid] = (x, y)

sock.close()

moved = 0
for eid, (fx, fy) in wild_first.items():
    lx, ly = wild_last[eid]
    if abs(lx - fx) > 1.0 or abs(ly - fy) > 1.0:
        moved += 1

print(f"entered field         : {entered}")
print(f"wild seen (species!=0): {len(wild_first)}")
print(f"wild that moved        : {moved}")
assert entered, "did not receive EnterAck"
assert saw_species and wild_first, "no wild pokemon entered view (raise --wild-count?)"
assert moved > 0, "wild pokemon were visible but never moved"
print("OK")
