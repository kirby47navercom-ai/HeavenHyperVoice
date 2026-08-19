#pragma once

// 필드의 엔티티와 시야.
//
// 섹터 격자는 **후보를 추리는 broad phase** 일 뿐이다. "누가 보이는가" 는 전적으로
// 엔티티마다 들고 있는 뷰 리스트가 답한다. 덕분에 시야가 사각형이 아니라 원형이고,
// 나중에 은신·차단·진영 필터를 넣을 자리가 생긴다.
//
// 반경이 모두 같으므로 `B ∈ A.visible ⟺ A ∈ B.visible` 이 구조적으로 보장된다.
// 그래서 집합 하나만 든다. "저격수는 더 멀리 본다" 같은 게 생기면 그때
// visible(내가 보는 것)과 watchers(나를 보는 것)로 쪼개야 한다.

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "CharacterStore.h"
#include "MapCollision.h"
#include "FieldCodec.h"
#include "FieldGeometry.h"
#include "TlsSession.h"

namespace heaven::field {

class WildAi;

using net::TlsSession;

// 저장소와 같은 모양을 쓴다. 입장할 때 읽고 퇴장할 때 그대로 넘긴다.
using data::Position;

struct Entity {
    std::uint64_t characterId = 0;
    std::uint64_t accountId = 0;
    std::weak_ptr<TlsSession> session;

    std::string nickname;
    std::uint16_t partnerSpecies = 0;

    // 야생 포켓몬이면 true. 세션이 없고 AI 가 움직인다. species 는 자기 종족.
    bool isWild = false;
    std::uint16_t species = 0;

    Position position;
    int sector = 0;

    // 반경이 균일해 대칭이다. 나를 보는 집합과 같다.
    std::unordered_set<std::uint64_t> visible;

    // 속도 상한 검증용. 이동 사이 경과 시간으로 허용 거리를 낸다.
    std::chrono::steady_clock::time_point lastMoveAt;

    bool movedThisTick = false;
};

class World {
public:
    // 입장. 같은 계정이 이미 있으면 그 세션을 반환한다 (호출자가 내보낸다).
    // 캐릭터 단위가 아니라 계정 단위인 이유는, 한 계정으로 캐릭터 여럿을
    // 동시에 붙이는 것도 막아야 하기 때문이다.
    std::shared_ptr<TlsSession> enter(std::uint64_t characterId, std::uint64_t accountId,
                                      std::string nickname, std::uint16_t partnerSpecies,
                                      const Position& position,
                                      const std::shared_ptr<TlsSession>& session);

    // 퇴장. 마지막 위치를 돌려준다 (저장용). 없던 엔티티면 nullopt.
    std::optional<Position> leave(std::uint64_t characterId);

    // 야생 포켓몬을 월드에 넣는다. 세션이 없어 아무에게도 전송하지 않지만,
    // 근처 플레이어에게는 spawn 으로 나타난다. entityId 는 캐릭터 번호와
    // 겹치지 않는 범위여야 한다 (main 의 kWildIdBase 참고).
    void enterWild(std::uint64_t entityId, std::uint16_t species, const Position& position);

    // 모든 야생 포켓몬을 한 틱 전진시킨다. AI 가 목표를 정하고 서버가 속도와
    // 벽을 강제한다. 틱 스레드에서만 부를 것 (WildAi 는 스레드 안전하지 않다).
    void advanceWild(float dt, WildAi& ai);

    // 클라이언트가 보낸 좌표. 속도 상한을 넘으면 클램프한다.
    void move(std::uint64_t characterId, float x, float y, float facing,
              std::uint32_t sequence);

    // 벽 충돌 판정. nullptr 이면 검사하지 않는다 (맵 없이 띄우는 경우).
    // 서버가 뜬 뒤로는 바뀌지 않으므로 락 없이 읽는다.
    void setCollision(const MapCollision* collision) { collision_ = collision; }

    // 20Hz. 이번 주기에 움직인 것들을 뷰어별로 묶어 보낸다.
    void tick();

    // 주기적 저장용 스냅샷.
    std::vector<std::pair<std::uint64_t, Position>> positions();

    std::size_t size();

private:
    // 아래 셋은 모두 mutex_ 를 쥔 채로 불린다.
    void updateVisibility(Entity& self);
    void removeFromVisibility(Entity& self);
    void sendTo(const Entity& entity, const proto::Bytes& frame) const;

    static proto::EntityView viewOf(const Entity& entity, bool withIdentity);

    // ponytail: 월드 전역 락 하나. 섹터별 락이 맞지만 접속자가 수백 명이 되기
    // 전까지는 경합이 없다. 올릴 때는 sectors_ 를 섹터별 뮤텍스로 감싸고
    // 뷰 리스트 갱신에서 두 섹터를 순서대로 잠그면 된다.
    std::mutex mutex_;

    std::unordered_map<std::uint64_t, Entity> entities_;
    std::unordered_map<std::uint64_t, std::uint64_t> byAccount_;

    const MapCollision* collision_ = nullptr;
    AgentSettings agent_;

    // 후보 추출 전용 공간 인덱스. 시야 판정에는 쓰이지 않는다.
    std::array<std::unordered_set<std::uint64_t>, proto::kSectorCount> sectors_;
};

}  // namespace heaven::field
