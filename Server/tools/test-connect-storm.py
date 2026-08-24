"""접속을 반복해서 Net 계층의 첫 프레임 유실을 재현한다.

  1. 서버를 띄운다
       .\build\windows-x64\bin\Release\FieldServer.exe --dev-no-auth --wild-count 0 --verbose
  2. python .\tools\test-connect-storm.py

증상: 접속의 2~15% 가 EnterAck 을 영영 받지 못한다.

  - 클라이언트: TLS 핸드셰이크가 정상 완료되고(wrap_socket 이 즉시 반환)
    Enter 프레임 sendall 도 성공한다. 그런데 응답이 한 바이트도 오지 않는다.
  - 서버 로그: `accepted: 127.0.0.1:PORT` 만 남고 그 세션의 `entered` 가 없다.
    10초 뒤 `handshake timed out` 으로 정리된다. TLS 오류 로그는 없다.

즉 서버가 소켓을 accept 하고 TLS 는 끝냈는데 첫 프레임이 올라오지 않는다.
FieldServer 가 아니라 Net 계층(TlsServer/TlsSession) 문제다 — 세션 핸들러가
불리기 전에 멈춘다. LoginServer/ChatServer 도 같은 코드를 쓴다.

혼자 순차로 붙였다 끊으면 잘 안 나온다. 관찰자 하나를 붙여 두고(= 서버가 20Hz
로 스냅샷을 내보내는 상태) 접속을 반복해야 잘 재현된다.
"""
import importlib.util
import os
import sys
import time

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "field_session", os.path.join(_here, "test-field-session.py"))
_session = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_session)

Client = _session.Client
wait_for = _session.wait_for

ROUNDS = int(sys.argv[1]) if len(sys.argv) > 1 else 40


def main():
    # 관찰자를 붙여 둔다. 서버가 조용하면 재현율이 크게 떨어진다.
    observer = Client("storm-obs", 9500)
    if not wait_for(lambda: observer.spawn is not None, 8.0):
        raise SystemExit("관찰자부터 못 붙었다. 서버가 --dev-no-auth 로 떠 있는가?")

    stalled = []
    for i in range(ROUNDS):
        character_id = 9600 + i
        started = time.time()
        client = Client(f"s{i}", character_id)
        if not wait_for(lambda: client.spawn is not None, 5.0):
            stalled.append((character_id, round(time.time() - started, 1)))
            print(f"STALL: id {character_id} - EnterAck 없음 "
                  f"(alive={client.alive}, {time.time() - started:.1f}s)")
        client.close()
        time.sleep(0.05)

    observer.close()
    rate = 100.0 * len(stalled) / ROUNDS
    print(f"\n멈춘 접속: {len(stalled)}/{ROUNDS} ({rate:.1f}%)")
    if stalled:
        print("서버 로그에서 위 시각의 accepted / handshake timed out 을 확인할 것.")
        raise SystemExit(1)
    print("이번 판은 전부 통과했다. 버그가 확률적이니 몇 번 더 돌려볼 것.")


if __name__ == "__main__":
    main()
