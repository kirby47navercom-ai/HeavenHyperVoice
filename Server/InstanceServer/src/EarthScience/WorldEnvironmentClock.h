#pragma once
#include <chrono>

namespace heaven::instance {
// 방의 생성/퇴장과 무관하게 서버 세션 전체가 공유하는 단조 시계예요.
// 시스템 날짜 변경은 경과 시간에 영향을 주지 않아요. 종료 중에는 정지하고 저장 시점부터 이어져요.
class WorldEnvironmentClock {
public:
    double elapsedRealSeconds() const {
        return restoredSeconds_+std::chrono::duration<double>(std::chrono::steady_clock::now()-started_).count();
    }
    void restore(double seconds) { restoredSeconds_=seconds;started_=std::chrono::steady_clock::now(); }
private:
    double restoredSeconds_=0;
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
};
}
