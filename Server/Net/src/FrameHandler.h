#pragma once

#include "Framing.h"

namespace heaven::net {

class TlsSession;

// 각 서버가 구현하는 프레임 처리기. 세션 하나당 하나가 만들어진다.
//
// onFrame 은 **세션 락 밖에서** 호출된다. 세션당 WSARecv 가 하나만 떠 있으므로
// 한 세션의 프레임은 항상 한 스레드에서 순서대로 전달된다. 따라서 구현체는
// 자기 상태에 락을 걸 필요가 없고, 방/등록부 같은 공유 자원은 자유롭게 만져도 된다.
class FrameHandler {
public:
    virtual ~FrameHandler() = default;

    // false 를 반환하면 연결을 끊는다.
    virtual bool onFrame(TlsSession& session, const proto::Bytes& body) = 0;

    // 세션이 정리될 때 한 번 호출된다. onFrame 과 같은 규칙이 적용된다.
    virtual void onClosed(TlsSession&) {}
};

}  // namespace heaven::net
