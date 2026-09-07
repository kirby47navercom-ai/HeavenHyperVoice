#include "RedisClient.h"

// hiredis 는 timeval 을 쓰지만 직접 선언하지 않는다. Windows 에서는 winsock2 가 준다.
#include <winsock2.h>

#include <hiredis/hiredis.h>
#include <spdlog/spdlog.h>

#include <utility>

namespace heaven::net {

namespace {

timeval toTimeval(std::chrono::milliseconds value) {
    timeval tv{};
    tv.tv_sec = static_cast<long>(value.count() / 1000);
    tv.tv_usec = static_cast<long>((value.count() % 1000) * 1000);
    return tv;
}

// redisReply 를 놓치지 않게 감싼다.
struct ReplyGuard {
    void* reply = nullptr;
    ~ReplyGuard() {
        if (reply != nullptr) {
            freeReplyObject(reply);
        }
    }
};

}  // namespace

RedisClient::RedisClient(RedisSettings settings) : settings_(std::move(settings)) {
    target_ = settings_.host + ":" + std::to_string(settings_.port);
}

RedisClient::~RedisClient() {
    std::lock_guard<std::mutex> lock(mutex_);
    dropLocked();
}

void RedisClient::dropLocked() {
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
}

bool RedisClient::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    return ensureConnectedLocked();
}

bool RedisClient::ensureConnectedLocked() {
    if (context_ != nullptr && context_->err == 0) {
        return true;
    }
    dropLocked();

    const timeval timeout = toTimeval(settings_.timeout);
    context_ = redisConnectWithTimeout(settings_.host.c_str(), settings_.port, timeout);
    if (context_ == nullptr) {
        lastError_ = "out of memory while connecting";
        return false;
    }
    if (context_->err != 0) {
        lastError_ = context_->errstr;
        dropLocked();
        return false;
    }

    // 명령이 걸려도 워커가 무한정 붙잡히지 않게 읽기/쓰기에도 상한을 둔다.
    redisSetTimeout(context_, timeout);

    if (!settings_.password.empty()) {
        ReplyGuard guard;
        guard.reply = redisCommand(context_, "AUTH %s", settings_.password.c_str());
        auto* reply = static_cast<redisReply*>(guard.reply);
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            lastError_ = reply != nullptr ? reply->str : "AUTH failed";
            dropLocked();
            return false;
        }
    }

    // 실제로 명령이 통하는지 확인한다. 비밀번호가 저장돼 있지 않으면 위 AUTH 를
    // 건너뛰는데, 그 상태로 "접속됨"을 돌려주면 이후 모든 명령이 NOAUTH 로 죽는다.
    // 호출자는 캐시 실패를 조용히 넘기도록 돼 있어서 아무도 눈치채지 못한다.
    {
        ReplyGuard guard;
        guard.reply = redisCommand(context_, "PING");
        auto* reply = static_cast<redisReply*>(guard.reply);
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            lastError_ = reply != nullptr && reply->str != nullptr ? reply->str : "PING failed";
            dropLocked();
            return false;
        }
    }

    lastError_.clear();
    return true;
}

bool RedisClient::executeLocked(const std::vector<std::string>& arguments,
                                const std::function<void(void*)>& onReply) {
    if (!ensureConnectedLocked()) {
        return false;
    }

    std::vector<const char*> argv;
    std::vector<std::size_t> lengths;
    argv.reserve(arguments.size());
    lengths.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        argv.push_back(argument.data());
        lengths.push_back(argument.size());
    }

    ReplyGuard guard;
    guard.reply = redisCommandArgv(context_, static_cast<int>(argv.size()), argv.data(),
                                   lengths.data());
    auto* reply = static_cast<redisReply*>(guard.reply);

    if (reply == nullptr) {
        lastError_ = context_->errstr;
        dropLocked();  // 다음 호출에서 재접속한다
        return false;
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        lastError_ = reply->str != nullptr ? reply->str : "redis error";
        return false;
    }
    onReply(reply);
    return true;
}

std::optional<std::string> RedisClient::commandForString(
    const std::vector<std::string>& arguments) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::optional<std::string> value;
    executeLocked(arguments, [&value](void* raw) {
        // nil 이나 정수 등은 "문자열 없음" 으로 본다.
        auto* reply = static_cast<redisReply*>(raw);
        if (reply->type == REDIS_REPLY_STRING) {
            value = std::string(reply->str, reply->len);
        }
    });
    return value;
}

bool RedisClient::command(const std::vector<std::string>& arguments) {
    std::lock_guard<std::mutex> lock(mutex_);
    return executeLocked(arguments, [](void*) {});
}

}  // namespace heaven::net
