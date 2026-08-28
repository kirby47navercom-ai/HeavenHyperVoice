#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include "AuthTicket.h"
#include "CharacterStore.h"
#include "FieldCodec.h"
#include "FrameHandler.h"
#include "RedisClient.h"
#include "TlsSession.h"
#include "WorkQueue.h"
#include "World.h"

namespace heaven::field {

using net::FrameHandler;
using net::TlsSession;

// 세션마다 바뀌지 않는 것들. main 이 소유하고 서버보다 오래 산다.
struct FieldContext {
    World* world = nullptr;
    const proto::PublicKeyRing* keys = nullptr;
    data::CharacterStore* characters = nullptr;
    net::WorkQueue* dbQueue = nullptr;

    // 없어도 된다. 못 붙으면 위치를 DB 에서만 읽고 쓴다.
    net::RedisClient* redis = nullptr;

    // 개발용. 티켓 검증과 DB 를 모두 건너뛰고 Enter 의 dev_* 를 그대로 믿는다.
    // 로그인 서버 없이 필드만 클라이언트와 붙여볼 때만 켠다.
    // 이때 characters 와 redis 는 nullptr 이다.
    bool devNoAuth = false;
};

// 세션 하나의 필드 처리.
//
//   AwaitingEnter  티켓을 기다린다
//   Entering       캐릭터와 위치를 읽는 중 (DB 왕복이라 비동기)
//   InField        이동을 받는다
//   Done           끝났다
//
// 입장이 비동기라 그 사이에 연결이 끊기거나 Move 가 들어올 수 있다.
// 로드가 끝났을 때 이미 닫힌 세션이면 World 에 넣지 않는다.
class FieldHandler : public FrameHandler {
public:
    explicit FieldHandler(const FieldContext& context) : context_(context) {}

    bool onFrame(TlsSession& session, const proto::Bytes& body) override;
    void onClosed(TlsSession& session) override;

private:
    enum class Stage { AwaitingEnter, Entering, InField, Done };

    bool handleEnter(TlsSession& session, const HeavenField::Enter& request);
    bool enterWithoutAuth(TlsSession& session, const HeavenField::Enter& request);
    void handleMove(const HeavenField::Move& request);
    bool handleSetParty(TlsSession& session, const HeavenField::SetParty& request);

    // 저장소에서 파티와 해금을 다시 읽어 그대로 보낸다. DB 스레드에서 부를 것.
    static void sendPartyState(const FieldContext& context, TlsSession& session,
                               std::uint64_t accountId, std::uint64_t characterId,
                               bool ok, std::string_view message);

    const FieldContext& context_;

    std::mutex mutex_;
    Stage stage_ = Stage::AwaitingEnter;

    // 티켓 검증 뒤에만 채워진다.
    std::uint64_t characterId_ = 0;
    std::uint64_t accountId_ = 0;
    std::string nickname_;
};

// Redis 에 위치를 남긴다. 필드 서버가 죽어도 마지막 위치가 남아 있게 하는
// 안전망이다. Memurai 가 save "" 라 Redis 자신이 재시작하면 같이 사라진다 —
// 서버만 죽는 흔한 경우를 막는 수준이다.
//
// 주기적 저장 스레드(main)가 부르므로 이것만 밖으로 낸다. 키 구성과 읽기는
// FieldHandler.cpp 안에만 있다.
void writeRedisPosition(net::RedisClient& redis, std::uint64_t characterId,
                        const data::Position& position);

// 캐시에 남은 위치를 지운다. **DB 에 저장한 직후에 반드시 부를 것.**
//
// 입장 경로가 캐시를 DB 보다 먼저 보므로(서버가 죽으면 캐시 쪽이 최신이다),
// 지우지 않으면 방금 저장한 값이 다음 접속에서 최대 한 주기만큼 옛 위치로
// 되돌아간다.
void clearRedisPosition(net::RedisClient& redis, std::uint64_t characterId);

}  // namespace heaven::field
