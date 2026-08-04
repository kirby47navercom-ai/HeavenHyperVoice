#include "SessionRegistry.h"

#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

namespace heaven::chat {

namespace {

// 아직 내 것일 때만 지운다. GET 과 DEL 을 따로 하면 그 사이에 다른 서버가
// 등록할 수 있고, 그걸 지워버리면 새 세션이 유령이 된다.
constexpr const char* kReleaseScript =
    "if redis.call('GET', KEYS[1]) == ARGV[1] then "
    "  return redis.call('DEL', KEYS[1]) "
    "else "
    "  return 0 "
    "end";

}  // namespace

SessionRegistry::SessionRegistry(net::RedisClient& redis, std::string serverId,
                                 std::chrono::seconds entryTtl)
    : redis_(redis), serverId_(std::move(serverId)), entryTtl_(entryTtl) {}

std::string SessionRegistry::keyFor(std::uint64_t accountId) const {
    return "session:" + std::to_string(accountId);
}

std::string SessionRegistry::valueFor(const std::string& sessionId) const {
    return serverId_ + "|" + sessionId;
}

std::optional<SessionOwner> SessionRegistry::parse(const std::string& value) const {
    const std::size_t separator = value.find('|');
    if (separator == std::string::npos) {
        return std::nullopt;
    }
    return SessionOwner{value.substr(0, separator), value.substr(separator + 1)};
}

std::optional<SessionOwner> SessionRegistry::claim(std::uint64_t accountId,
                                                   const std::string& sessionId) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        held_[accountId] = sessionId;
    }

    // SET ... GET 은 Redis 6.2 부터다. 덮어쓰기와 이전 값 조회가 한 번에 일어난다.
    const auto previous = redis_.commandForString({"SET", keyFor(accountId),
                                                   valueFor(sessionId), "EX",
                                                   std::to_string(entryTtl_.count()), "GET"});
    if (!previous.has_value()) {
        // 이전 주인이 없었거나 Redis 를 못 썼다. 둘 다 "끊을 대상 없음" 으로 본다.
        return std::nullopt;
    }
    return parse(*previous);
}

void SessionRegistry::release(std::uint64_t accountId, const std::string& sessionId) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = held_.find(accountId);
        if (it != held_.end() && it->second == sessionId) {
            held_.erase(it);
        }
    }

    const auto removed = redis_.commandForInteger(
        {"EVAL", kReleaseScript, "1", keyFor(accountId), valueFor(sessionId)});
    if (!removed.has_value()) {
        spdlog::debug("session registry: release failed for account {}: {}", accountId,
                      redis_.lastError());
    }
}

void SessionRegistry::refreshAll() {
    std::vector<std::pair<std::uint64_t, std::string>> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.assign(held_.begin(), held_.end());
    }

    for (const auto& [accountId, sessionId] : snapshot) {
        if (!redis_.command({"SET", keyFor(accountId), valueFor(sessionId), "EX",
                             std::to_string(entryTtl_.count())})) {
            spdlog::debug("session registry: refresh failed for account {}: {}", accountId,
                          redis_.lastError());
            return;  // Redis 가 안 되는 상태다. 나머지도 실패할 테니 그만둔다.
        }
    }
}

std::size_t SessionRegistry::heldCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return held_.size();
}

}  // namespace heaven::chat
