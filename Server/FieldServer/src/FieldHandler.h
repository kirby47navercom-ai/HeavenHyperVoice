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
    void handleMove(const HeavenField::Move& request);

    const FieldContext& context_;

    std::mutex mutex_;
    Stage stage_ = Stage::AwaitingEnter;

    // 티켓 검증 뒤에만 채워진다.
    std::uint64_t characterId_ = 0;
    std::uint64_t accountId_ = 0;
    std::string nickname_;
};

// Redis 키. 필드 서버가 죽어도 마지막 위치가 남아 있게 하는 안전망이다.
// Memurai 가 save "" 라 Redis 자신이 재시작하면 같이 사라진다 — 서버만
// 죽는 흔한 경우를 막는 수준이다.
std::string positionKey(std::uint64_t characterId);
std::optional<data::Position> readRedisPosition(net::RedisClient& redis,
                                                std::uint64_t characterId);
void writeRedisPosition(net::RedisClient& redis, std::uint64_t characterId,
                        const data::Position& position);

}  // namespace heaven::field
