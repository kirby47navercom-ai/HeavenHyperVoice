"""채팅 서버의 발화 상한이 실제로 걸리는지 확인한다.

  1. 서버를 띄운다 (개발 저장소로 충분하다)
       .\build\windows-x64\bin\Debug\LoginServer.exe --account-store dev
       .\build\windows-x64\bin\Debug\ChatServer.exe
  2. python .\tools\test-chat-limits.py

프레이밍은 webclient\bridge.py 의 헬퍼를 그대로 빌려 쓴다. 스키마가 바뀌면 브리지가
먼저 깨지므로 여기서 따로 관리할 것이 없다.

채팅 서버는 Say 를 보낸 본인에게도 브로드캐스트하므로, 돌아온 Chat 프레임을 세면
무엇이 통과했는지 알 수 있다.
"""
import os
import sys
import time

# bridge.py 는 임포트 시점에 인자를 파싱한다. 이 스크립트의 argv 를 먹지 않게 비운다.
sys.argv = ["bridge.py"]
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "webclient"))
import bridge  # noqa: E402

LOGIN_REQUEST, LOGIN_RESPONSE = 1, 2
CREATE_CHARACTER_REQUEST, CHARACTER_LIST_RESPONSE = 5, 6
SELECT_CHARACTER_REQUEST, SELECT_CHARACTER_RESPONSE = 7, 8

USERNAME = "limittest"
NICKNAME = "리밋"


def count_broadcasts(sock, seconds):
    """seconds 동안 돌아온 Chat 프레임 수."""
    chats = 0
    deadline = time.time() + seconds
    sock.settimeout(0.2)
    while time.time() < deadline:
        try:
            body = bridge.recv_frame(sock)
        except OSError:
            continue  # 조용한 구간이다. 타임아웃은 실패가 아니다.
        if body is None:
            break
        tag, payload = bridge.read_payload(bridge.root(body))
        if tag == bridge.CHAT_CHAT:
            chats += 1
    return chats


def say(sock, text):
    bridge.send_frame(sock, bridge.envelope(bridge.strings_table(text), bridge.CHAT_SAY))


def open_chat():
    """로그인 -> 캐릭터 확보 -> 선택 -> 채팅 접속. 채팅 소켓을 돌려준다."""
    login = bridge.connect("127.0.0.1", 9100)
    payload = bridge.exchange(
        login, bridge.envelope(bridge.strings_table(USERNAME, "x"), LOGIN_REQUEST), LOGIN_RESPONSE)
    if not bridge.read_scalar(payload, 0, bridge.BoolFlags, False):
        raise SystemExit(f"login failed: {bridge.read_string(payload, 1)}")

    characters = bridge.decode_characters(payload, 2)
    if not characters:
        payload = bridge.exchange(
            login, bridge.envelope(bridge.create_character_payload(NICKNAME, 1),
                                   CREATE_CHARACTER_REQUEST), CHARACTER_LIST_RESPONSE)
        characters = bridge.decode_characters(payload, 2)
        if not characters:
            raise SystemExit(f"character creation failed: {bridge.read_string(payload, 1)}")

    payload = bridge.exchange(
        login, bridge.envelope(bridge.character_id_payload(characters[0]["id"]),
                               SELECT_CHARACTER_REQUEST), SELECT_CHARACTER_RESPONSE)
    if not bridge.read_scalar(payload, 0, bridge.BoolFlags, False):
        raise SystemExit(f"select failed: {bridge.read_string(payload, 1)}")

    endpoints = {}
    for entry in bridge.read_table_vector(payload, 2):
        endpoints[bridge.read_string(entry, 0)] = (bridge.read_string(entry, 1),
                                                   bridge.read_scalar(entry, 2, bridge.Uint16Flags),
                                                   bridge.read_bytes(entry, 3))
    login.close()

    host, port, ticket = endpoints["chat"]
    chat = bridge.connect(host, port)
    bridge.send_frame(chat, bridge.envelope(bridge.ticket_payload(ticket), bridge.CHAT_HELLO))
    count_broadcasts(chat, 0.5)  # 입장 공지를 흘려보낸다
    return chat


def main():
    chat = open_chat()
    try:
        # 도배: 연달아 다섯 개. kMinSayInterval 이 200ms 라 하나만 통과해야 한다.
        for i in range(5):
            say(chat, f"spam{i}")
        burst = count_broadcasts(chat, 1.0)

        # 길이: kMaxChatTextBytes 가 1 KiB 라 2000 바이트는 버려야 한다.
        time.sleep(0.3)
        say(chat, "x" * 2000)
        oversized = count_broadcasts(chat, 1.0)

        # 위 둘이 걸린 뒤에도 정상 발화는 통과해야 한다. 막히면 채팅이 죽은 것이다.
        time.sleep(0.3)
        say(chat, "정상")
        normal = count_broadcasts(chat, 1.0)
    finally:
        chat.close()

    print(f"burst of 5      -> {burst} broadcast (expect 1)")
    print(f"2000 byte say   -> {oversized} broadcast (expect 0)")
    print(f"normal say      -> {normal} broadcast (expect 1)")

    assert burst == 1, f"rate limit let {burst} messages through"
    assert oversized == 0, "an oversized message was broadcast"
    assert normal == 1, "a normal message was blocked"
    print("OK")


if __name__ == "__main__":
    try:
        main()
    except ConnectionRefusedError:
        raise SystemExit("cannot reach the servers. Start LoginServer (9100) and ChatServer (9000).")
