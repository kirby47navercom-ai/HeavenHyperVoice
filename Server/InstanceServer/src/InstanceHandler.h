#pragma once

// 인스턴스 세션 하나.
//
// 필드와 주고받는 메시지가 같아서(field.fbs 공유) 모양도 FieldHandler 와
// 거의 같다. 다른 점은 셋이다.
//
//   - 티켓 audience 가 "instance" 다. 필드 티켓으로는 못 들어온다.
//   - Enter 에 실린 instance_type 으로 RoomManager 에게 방을 받는다.
//   - 위치를 저장하지 않는다. 인스턴스는 일회성이고, 나가면 필드에 저장된
//     마지막 위치로 돌아간다. 그래서 Redis 도 안 쓴다.

#include <cstdint>
#include <mutex>
#include <string>

#include "AuthTicket.h"
#include "CharacterStore.h"
#include "FieldCodec.h"
#include "FrameHandler.h"
#include "RoomManager.h"
#include "TlsSession.h"
#include "WorkQueue.h"

namespace heaven::instance {

using net::FrameHandler;
using net::TlsSession;

// 세션마다 바뀌지 않는 것들. main 이 소유하고 서버보다 오래 산다.
struct InstanceContext {
    RoomManager* rooms = nullptr;
    const proto::PublicKeyRing* keys = nullptr;

    // 파트너와 파티를 읽는다. 위치는 안 읽는다 — 인스턴스는 스폰 지점 고정이다.
    data::CharacterStore* characters = nullptr;
    net::WorkQueue* dbQueue = nullptr;

    // 개발용. 티켓 검증과 DB 를 모두 건너뛰고 Enter 의 dev_* 를 그대로 믿는다.
    // 이때 characters 는 nullptr 이다.
    bool devNoAuth = false;
};

class InstanceHandler : public FrameHandler {
public:
    explicit InstanceHandler(const InstanceContext& context) : context_(context) {}

    bool onFrame(TlsSession& session, const proto::Bytes& body) override;
    void onClosed(TlsSession& session) override;

private:
    enum class Stage { AwaitingEnter, Entering, InRoom, Done };

    bool handleEnter(TlsSession& session, const HeavenField::Enter& request);
    bool enterWithoutAuth(TlsSession& session, const HeavenField::Enter& request);
    // 방을 인자로 받는다. onFrame 이 stage 와 함께 한 락 안에서 집어온 것이라
    // 널이 아님이 보장된다 — 멤버를 여기서 다시 읽으면 그 사이에 onClosed 가
    // 끼어들어 널이 될 수 있다 (아래 room_ 주석 참고).
    void handleMove(Room& room, const HeavenField::Move& request);
    bool handleSetParty(TlsSession& session, Room& room, const HeavenField::SetParty& request);

    // 방을 배정하고 월드에 넣는다. 락을 쥐지 않은 상태에서 부를 것.
    // 실패하면 사유를 보내고 false — 호출자가 연결을 끊는다.
    bool placeInRoom(const std::shared_ptr<TlsSession>& self, std::uint32_t type,
                     std::uint16_t partnerSpecies);

    static void sendPartyState(const InstanceContext& context, TlsSession& session,
                               std::uint64_t accountId, std::uint64_t characterId,
                               bool ok, std::string_view message);

    const InstanceContext& context_;

    std::mutex mutex_;
    Stage stage_ = Stage::AwaitingEnter;

    // 티켓 검증 뒤에만 채워진다.
    std::uint64_t characterId_ = 0;
    std::uint64_t accountId_ = 0;
    std::string nickname_;

    // 배정받은 방. RoomManager 가 소유하고 이 세션이 나갈 때까지 살아 있다
    // (인원이 0 이 아니면 회수 대상이 아니다).
    //
    // onClosed 는 이 값을 지우지 않는다. onFrame 과 onClosed 는 서로 다른
    // 스레드에서 겹칠 수 있어서 — 프레임 처리 중에 다른 스레드의 전송 완료가
    // 세션 정리를 시작할 수 있다 — 여기서 널로 만들면 그 순간 프레임 처리가
    // 널을 역참조한다. 대신 stage_ 를 Done 으로 올리고, 방 자체는
    // RoomManager 의 회수 유예(최소 1초) 안에서 계속 유효하다.
    Room* room_ = nullptr;
};

}  // namespace heaven::instance
