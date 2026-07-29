#pragma once

#include <winsock2.h>
// winsock2.h 를 먼저 넣어야 windows.h 가 winsock1 을 끌어오지 않는다.
#include <windows.h>

namespace heaven::net {

enum class OpType { Recv, Send };

// OVERLAPPED 를 상속해 완료 통지에서 어떤 작업인지 되찾는다.
// OVERLAPPED 가 첫 멤버여야 하므로 상속을 쓴다.
struct IoOperation : OVERLAPPED {
    explicit IoOperation(OpType t) : OVERLAPPED{}, type(t) {}
    OpType type;
};

}  // namespace heaven::net
