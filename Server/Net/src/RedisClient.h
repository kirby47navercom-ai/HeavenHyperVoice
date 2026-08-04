#pragma once

// 최소한의 Redis 클라이언트. 세션 레지스트리에 필요한 만큼만 감싼다.
//
// 동기 호출이지만 타임아웃이 걸려 있다. 로컬/LAN 기준 한 번에 1ms 미만이라
// IOCP 워커에서 불러도 되지만, Redis 가 멈춰도 워커가 무한정 붙잡히지 않도록
// 타임아웃이 상한 역할을 한다. 이 상한을 넘기는 작업을 여기에 추가하지 말 것.
//
// 캐시는 없어도 되는 자원이다. 접속 실패나 타임아웃은 예외가 아니라
// "결과 없음"으로 돌려주고, 호출자는 해당 기능만 포기하고 계속 진행한다.

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct redisContext;

namespace heaven::net {

struct RedisSettings {
    std::string host = "127.0.0.1";
    std::uint16_t port = 6379;
    std::string password;
    std::chrono::milliseconds timeout{200};
};

class RedisClient {
public:
    explicit RedisClient(RedisSettings settings);
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    // 접속을 시도한다. 실패해도 예외를 던지지 않는다.
    bool connect();

    bool connected();

    // 인자를 그대로 넘겨 명령을 실행한다. 이진 안전하다.
    // 문자열 응답이면 그 값을, 그 외(nil/오류/정수)면 nullopt.
    // 끊겨 있으면 한 번 재접속을 시도한다.
    std::optional<std::string> commandForString(const std::vector<std::string>& arguments);

    // 정수 응답을 기대하는 명령. 실패하면 nullopt.
    std::optional<long long> commandForInteger(const std::vector<std::string>& arguments);

    // 응답 내용을 보지 않는 명령. 성공 여부만 돌려준다.
    bool command(const std::vector<std::string>& arguments);

    const std::string& target() const { return target_; }
    const std::string& lastError() const { return lastError_; }

private:
    // mutex_ 를 잡은 채로 호출한다.
    bool ensureConnectedLocked();
    void dropLocked();

    RedisSettings settings_;
    std::string target_;

    std::mutex mutex_;
    redisContext* context_ = nullptr;
    std::string lastError_;
};

}  // namespace heaven::net
