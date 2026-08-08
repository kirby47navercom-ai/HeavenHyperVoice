# HeavenHyperVoice Server

로그인 서버와 채팅 서버. 둘 다 IOCP + TLS 위에서 돌고, 클라이언트는 로그인해서 받은
**서명 티켓**으로만 채팅에 들어갈 수 있다.

## 구성

| 디렉터리 | 내용 |
|---|---|
| `Net/` | 공용 IOCP + TLS 기반. 서버(`TlsServer`/`TlsSession`), 클라이언트(`TlsClient`), Redis 래퍼, 자격증명 관리자 접근 |
| `Protocol/` | `.fbs` 스키마 + 인코딩 헬퍼 + 티켓 서명/검증. 빌드 시점에 헤더 생성 |
| `LoginServer/` | 자격증명 확인 → 티켓 발급. 기본 포트 **9100** |
| `ChatServer/` | 티켓 검증 → 채팅방. 기본 포트 **9000** |
| `Launcher/` | 서버들을 한꺼번에 띄운다 |
| `tools/webclient/` | 브라우저 테스트 클라이언트 + 프로토콜 브리지 (Python) |
| `DataBase/` | 계정 스키마 마이그레이션 (`tools\apply-migrations.ps1` 로 적용) |

의존성은 vcpkg 매니페스트(`vcpkg.json`)로 baseline 고정: `openssl`, `flatbuffers`, `spdlog`,
`argon2`, `hiredis`. ODBC 드라이버 매니저와 자격증명 관리자는 Windows SDK 구성요소라
vcpkg 를 타지 않는다. **Windows 전용**이다 (IOCP).

## 인증 — Ed25519 서명 티켓

```
LoginServer  개인키 보유 → 티켓 발급 가능
ChatServer   공개키만 보유 → 검증만 가능, 위조 불가
```

공유 비밀키(HMAC)였다면 검증하는 서버가 전부 위조도 할 수 있다. 서버 종류가 늘수록
위험이 커지므로 비대칭 서명을 쓴다. **발급을 나중에 별도 서비스로 빼내도 검증자 코드는 그대로다.**

**티켓 형식**: `[AuthTicket FlatBuffer][64바이트 Ed25519 서명]`

| 클레임 | 역할 |
|---|---|
| `issuer` | 발급자 식별. 추후 발급자 다중화 대비 |
| `audience` | 이 티켓이 통용되는 서비스(`chat`/`voice`/…). **없으면 채팅 티켓을 음성 서버에 재사용할 수 있다** |
| `key_id` | 서명 키 식별. 무중단 키 교체용 |
| `account_id`, `nickname` | 계정 정보 |
| `issued_unix`, `expires_unix` | 기본 유효기간 60초 |

검증은 `verifyTicket(ticket, expectedAudience, keys, now, out)` 하나로 하며,
**audience 를 인자로 강제**해 호출자가 확인을 빼먹을 수 없게 했다.
닉네임은 티켓에서 나오므로 클라이언트가 스스로 주장할 수 없다.

### 흐름

```
클라이언트             LoginServer                      ChatServer
    |--TLS------------->|                                   |
    |--LoginRequest---->| AccountStore 조회                   |
    |                   | 티켓 발급 (개인키 서명)               |
    |<--LoginResponse---| {ticket, chat_host, chat_port}     |
    |   (서버가 연결 종료) |                                   |
    |--TLS---------------------------------------------------->|
    |--Hello{ticket}------------------------------------------>|
    |                            공개키 검증 + audience + 만료 확인 |
    |<--Notice / Chat------------------------------------------|
```

### 확장 시 (아직 구현 안 함)

필드/인스턴스 서버는 클라이언트가 직접 붙지 않는다. **게이트웨이**가 티켓을 한 번
검증하고 세션을 유지하며, 그 뒤 내부 서버들은 내부망 신뢰로 동작한다. 음성은 예외로
클라이언트가 직접 붙으므로 `audience:"voice"` 티켓을 따로 발급받는다.
즉 다음 아키텍처 단계는 토큰 서버가 아니라 게이트웨이다.

## 준비

개발용 자체 서명 인증서와 티켓 서명 키를 만든다. `certs/`는 gitignore 처리돼 있다.

```powershell
.\tools\gen-dev-cert.ps1
.\tools\gen-auth-key.ps1
```

`gen-auth-key.ps1`은 `certs/auth.key`(개인, LoginServer 전용)와
`certs/auth.pub`(공개, 검증하는 서버에 배포)를 만든다.

### 계정 데이터베이스

LoginServer 는 기본적으로 MySQL 을 ODBC 로 조회한다. **MySQL Connector/ODBC(64-bit,
Unicode)** 가 필요하며, 드라이버 이름은 버전이 섞이므로 설치된 것 중에서 자동으로 찾는다
(`--db-driver` 로 지정 가능).

```powershell
.\tools\apply-migrations.ps1              # DataBase\*.sql 을 순서대로 적용
.\build\windows-x64\bin\Debug\LoginServer.exe --save-db-password   # 최초 1회
```

DB 비밀번호는 **명령줄로 받지 않는다** — 프로세스 목록에 그대로 노출되기 때문이다.
`--save-db-password` 로 한 번 저장해두면 Windows 자격증명 관리자(DPAPI, 사용자 계정에
묶인 암호화)에서 읽어오므로, 런처를 더블클릭해도 환경변수 설정 없이 붙는다.
CI 나 컨테이너에서는 `HHV_DB_PASSWORD` 환경변수를 먼저 본다.
저장된 값은 `--forget-db-password` 로 지운다.

비밀번호는 **argon2id** 로 해시한다(OWASP 권장 최소치, `m=19456 t=2 p=1`). 계정 행을
손으로 넣을 때 쓸 해시는 이렇게 뽑는다:

```powershell
.\build\windows-x64\bin\Debug\LoginServer.exe --hash-password '비밀번호' 2>$null
```

DB 없이 채팅 흐름만 보고 싶으면 `--account-store dev` 로 띄운다 (아래 *한계* 참고).

### 세션 레지스트리 (Redis / Memurai)

ChatServer 는 접속 중인 계정을 Redis 에 `session:{account_id} → {server_id}|{session_id}`
로 등록한다. Windows 에서는 Redis 호환 서버인 **Memurai** 를 쓴다.

```powershell
.\tools\setup-memurai.ps1                                            # 관리자 권한, 최초 1회
.\build\windows-x64\bin\Debug\ChatServer.exe --save-redis-password   # 최초 1회
```

`setup-memurai.ps1` 은 벤더 설정 파일 끝에 표시된 블록만 덧붙인다(`-Remove` 로 되돌림).
기본 설치 상태의 두 가지를 고친다 — `requirepass` 가 없어 로컬의 아무 프로세스나 캐시를
읽고 쓸 수 있는 것, 그리고 `maxmemory` 가 수 GiB 에 정책이 `noeviction` 인 것(캐시인데
메모리가 차면 오래된 키를 버리는 대신 쓰기가 실패한다 → `512mb` / `volatile-lru`).
비밀번호는 DB 와 같은 이유로 명령줄로 받지 않고 자격증명 관리자에 둔다
(`--forget-redis-password` 로 삭제).

**레지스트리는 없어도 되는 자원이다.** 못 붙으면 경고만 남기고 채팅은 그대로 받는다
(`--no-redis` 로 아예 끌 수도 있다). Redis 가 중간에 죽어도 마찬가지이며,
`RedisClient` 는 200 ms 타임아웃을 걸어 멈춘 Redis 가 IOCP 워커를 붙잡지 못하게 한다.

```powershell
--redis-host <h> --redis-port <n>   # 기본 127.0.0.1:6379
--server-id <id>                    # 레지스트리에 기록될 이 프로세스 이름 (기본 chat-1)
--no-redis                          # 레지스트리 없이 기동
```

RedisInsight 같은 GUI 로 붙을 때도 `requirepass` 를 설정했으므로 연결 설정에 같은
비밀번호를 넣어야 한다. 이미 저장해둔 연결이 있다면 `NOAUTH` 로 실패한다.

## 중복 로그인 — 후접속 우선

같은 계정으로 다시 접속하면 **나중 접속이 이기고 먼저 붙어 있던 세션이 끊긴다.**
끊기는 쪽은 `다른 곳에서 로그인하여 연결을 종료합니다` 공지를 받고 전송이 끝난 뒤 닫힌다.

강제 종료를 실제로 수행하는 것은 `Room` 의 계정 인덱스다. 세션 객체를 쥐고 있는 것은
그 프로세스뿐이라, 끊는 일은 항상 그 세션을 소유한 서버가 한다.

Redis 레지스트리는 **클라이언트가 직접 붙는 프로세스가 둘 이상일 때**를 위한 것이다
(음성 서버가 생기는 시점). 지금은 ChatServer 하나뿐이라 기능적으로 꼭 필요하지는 않다.

| 동작 | 명령 | 이유 |
|---|---|---|
| 등록 | `SET session:{id} {server}|{session} EX 60 GET` | 덮어쓰기와 이전 주인 조회가 한 번에 일어난다 (Redis 6.2+) |
| 해제 | `EVAL` 로 값 비교 후 `DEL` | GET/DEL 을 나눠 하면 그 사이 새 주인의 등록을 지워 새 세션이 유령이 된다 |
| 갱신 | 20초마다 `SET ... EX 60` | `EXPIRE` 가 아니라 `SET` 이라, Redis 가 재시작돼 키가 사라져도 다음 주기에 스스로 복구된다 |

TTL 을 두는 이유는 서버가 비정상 종료돼도 등록이 영원히 남지 않게 하기 위해서다.

## 빌드

### Rider

`Server/` 폴더를 열면 `CMakePresets.json`의 `windows-x64` 프로파일을 자동으로 잡는다.
vcpkg 경로는 CMake가 스스로 찾으므로 환경변수 설정이 필요 없다.

첫 configure는 vcpkg가 OpenSSL을 소스 빌드하느라 오래 걸린다. 이후엔 캐시가 재사용된다.

### 커맨드라인

```powershell
.\build.ps1 -Config Debug
```

`build.ps1`이 `VCPKG_ROOT`와 MSVC 개발자 환경을 자동으로 잡는다.
**Rider와 동시에 돌리지 말 것** — 빌드 디렉터리와 vcpkg 락을 두고 충돌한다.

## 실행

### 런처로 한 번에

```powershell
.\build\windows-x64\bin\Debug\Launcher.exe
```

자식 프로세스를 **Job Object**에 넣으므로 런처가 어떻게 죽든(Ctrl+C, 강제 종료)
커널이 서버들을 함께 정리한다. 포트가 물려 있는 사고가 안 난다.

### 개별 실행

```powershell
.\build\windows-x64\bin\Debug\LoginServer.exe --port 9100 --chat-port 9000
.\build\windows-x64\bin\Debug\ChatServer.exe  --port 9000
```

인증서 경로는 이 순서로 해석한다: ① 준 경로(작업 디렉터리 기준) ② **Debug 한정**
빌드 시점에 박아둔 소스 루트. ②가 쓰이면 경고 로그가 남는다. IDE의 작업 디렉터리가
빌드 출력 폴더라도 그냥 실행되며, 릴리스 바이너리에는 개발 경로가 들어가지 않는다.

### 테스트 클라이언트

```powershell
python .\tools\webclient\bridge.py     # http://127.0.0.1:8080
```

브라우저는 raw TCP/TLS 소켓을 열 수 없으므로 `bridge.py` 가 중간에 선다. HTML 을
서빙하면서 JSON 호출을 FlatBuffers 프로토콜로 옮긴다. Python 표준 라이브러리와
`flatbuffers` 패키지만 쓰고, 스키마가 작아서 `flatc` 생성 코드 없이 직접 인코딩한다.

로그인/가입은 요청-응답이고, 채팅 수신은 SSE(`/api/events`)로 밀어준다.
세션을 모듈 전역에 하나만 두므로 **브라우저 탭 하나** 기준이다. 개발용이라
서버 인증서를 검증하지 않는다.

두 계정으로 동시에 대화하려면 브리지를 포트를 달리해 하나 더 띄운다.

```powershell
python .\tools\webclient\bridge.py --port 8081
```

## 자원 한계

악의적이거나 고장난 클라이언트 하나가 서버를 죽이지 못하게 하는 제한들이다.

| 상수 | 값 | 위치 | 막는 것 |
|---|---|---|---|
| `kHandshakeTimeout` | 10초 | `Net/src/TlsServer.h` | 붙어놓고 아무것도 보내지 않는 연결 (slowloris) |
| `kMaxSendQueueBytes` | 1 MiB | `Net/src/TlsSession.h` | 소켓을 읽지 않는 클라이언트로 인한 큐 폭증 |
| `kMaxBodyBytes` | 64 KiB | `Protocol/Framing.h` | 거대한 길이 접두사로 메모리를 할당시키는 것 |
| `kMaxUsernameBytes` | 32 | `Protocol/LoginCodec.h` | 과도한 길이의 아이디 |
| `kMaxPasswordBytes` | 128 | `Protocol/LoginCodec.h` | argon2 에 거대한 입력을 밀어 넣는 것 |

전송 큐가 상한에 닿으면 해당 연결만 끊는다. 다른 참가자는 영향받지 않는다.
핸드셰이크 타임아웃은 1초 주기 감시 스레드가 검사하므로 실제로는 최대 1초 늦게 걸린다.

## 알아둘 것 — 동시성 규칙

IOCP 워커가 여럿이므로 아래 규칙을 지켜야 한다.

- **`FrameHandler::onFrame` 은 세션 락 밖에서 불린다.** 세션당 `WSARecv`가 하나만
  떠 있으므로 한 세션의 프레임은 항상 한 스레드에서 순서대로 전달된다. 따라서 핸들러는
  자기 상태에 락이 필요 없고, 방 같은 공유 자원도 자유롭게 만질 수 있다.
- **`TlsSession::send` 는 공유 자원을 만지지 않는다.** 그래서 `Room::broadcast` 가
  세션 집합을 순회하는 도중에 호출해도 안전하다. 전송 큐가 넘쳐도 소켓만 닫고,
  방에서 빼는 정리는 나중에 워커가 한다.
- **세션 수명은 대기 중인 중첩 I/O 가 쥔다.** `recvRef_`/`sendRef_` 가 `shared_ptr` 로
  자신을 붙잡고, 완료 처리에서 지역 변수로 옮겨 받는다. `pendingOps_` 가 0 이 되고
  닫힌 뒤에야 정리가 한 번 실행된다.
- **`TlsChannel` 은 스레드 안전하지 않다.** 반드시 세션 락 안에서만 만질 것.
- **느린 일은 IOCP 워커에서 하지 않는다.** 비밀번호 검증(argon2id, Release 기준 약
  18.5ms)과 DB 왕복이 그렇다. `LoginHandler` 는 파싱만 하고 `WorkQueue`(`--auth-threads`,
  기본 4)에 넘긴 뒤 즉시 반환하며, 응답도 인증 스레드가 보낸다 (`TlsSession::send` 는
  스레드 안전하다). 그러지 않으면 로그인 몇 건이 워커를 전부 붙잡아 같은 완료 포트의
  다른 I/O 까지 멈춘다.

## 한계

- **`--account-store dev` 는 실제로 인증하지 않는다.** 비어 있지 않은 아이디면 통과시키고
  비밀번호는 보지 않는다. DB 없이 채팅 흐름만 시험하기 위한 것이며, 기동할 때마다
  경고를 남긴다. 기본값은 `odbc` 다.
- **티켓 재사용 가능**: 만료 전까지 같은 티켓으로 여러 번 접속할 수 있다. 막으려면
  일회용 nonce 등록부(중앙 저장소)가 필요하다. 지금은 유효기간 60초로 대신한다.
- **중앙 강제 차단이 없다.** 서명이 유효하고 만료 전이면 LoginServer 가 죽어도 티켓은
  통한다. 비대칭 서명의 본질적 트레이드오프이며, 필요해지면 그때가 별도 토큰 서비스를
  도입할 시점이다.
- **테스트 클라이언트는 TLS 인증서를 검증하지 않는다.** 개발용 자체 서명 인증서를
  쓰기 때문이며, 실제 클라이언트는 반드시 검증을 켜야 한다.
- **부하/악성 입력 시험 수단이 없다.** 전송 큐 상한(`--stall` + `--flood`)과 위조·만료
  티켓 거절은 C++ TestClient 로 확인했었고, 그걸 지우면서 재현 수단도 같이 없어졌다.
  브라우저 클라이언트는 정상 흐름만 다룬다.
- **세션 레지스트리는 아직 강제력이 없다.** 다른 서버가 갖고 있는 세션을 발견해도
  로그만 남긴다. 실제로 끊으려면 서버 간 통지 경로(pub/sub 등)가 필요하고,
  그건 클라이언트가 직접 붙는 두 번째 프로세스가 생길 때 넣을 일이다.
- **Memurai 설정 파일에 비밀번호가 평문으로 들어간다.** `Program Files` 는 로컬
  사용자가 읽을 수 있으므로 "아무 프로세스나 캐시를 조작하는 것"을 막는 수준이다.
  실제 배포에서는 Redis ACL 과 파일 권한을 함께 써야 한다.
- 자동 테스트가 없다. 검증은 수동 시나리오다.
