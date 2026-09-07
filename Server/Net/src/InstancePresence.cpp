#include "InstancePresence.h"

#include <spdlog/spdlog.h>

#include <charconv>

#include "RedisClient.h"

namespace heaven::instancechat {

namespace {

template <typename T>
std::string text(T value) {
    return std::to_string(value);
}

// ARGV: accountId, "<type>:<roomId>", ttl
constexpr const char* kEnterScript = R"lua(
local previous = redis.call('GET', 'player:instance:' .. ARGV[1])
if previous then
  redis.call('SREM', 'instance:' .. previous, ARGV[1])
end
redis.call('SET', 'player:instance:' .. ARGV[1], ARGV[2], 'EX', ARGV[3])
redis.call('SADD', 'instance:' .. ARGV[2], ARGV[1])
redis.call('EXPIRE', 'instance:' .. ARGV[2], ARGV[3])
return 'OK'
)lua";

// ARGV: accountId
constexpr const char* kLeaveScript = R"lua(
local room = redis.call('GET', 'player:instance:' .. ARGV[1])
if not room then return '' end
redis.call('DEL', 'player:instance:' .. ARGV[1])
redis.call('SREM', 'instance:' .. room, ARGV[1])
if redis.call('SCARD', 'instance:' .. room) == 0 then
  redis.call('DEL', 'instance:' .. room)
end
return room
)lua";

// ARGV: accountId
//
// 계정 번호만 돌려주므로 쉼표로 이어도 안전하다 (사용자 입력이 아니다).
constexpr const char* kRoommatesScript = R"lua(
local room = redis.call('GET', 'player:instance:' .. ARGV[1])
if not room then return '' end
local ids = redis.call('SMEMBERS', 'instance:' .. room)
if #ids == 0 then return '' end
return table.concat(ids, ',')
)lua";

}  // namespace

std::string InstancePresence::eval(const char* script, const std::vector<std::string>& args) {
    std::vector<std::string> command;
    command.reserve(args.size() + 3);
    command.emplace_back("EVAL");
    command.emplace_back(script);
    command.emplace_back("0");  // KEYS 없음. 단일 노드 전제 (헤더 주석 참고)
    for (const std::string& arg : args) {
        command.push_back(arg);
    }

    const auto reply = redis_.commandForString(command);
    if (!reply.has_value() && !redis_.lastError().empty()) {
        spdlog::warn("instance presence script failed: {}", redis_.lastError());
    }
    return reply.value_or(std::string{});
}

void InstancePresence::enter(std::uint64_t accountId, std::uint32_t instanceType,
                             std::uint32_t roomId) {
    if (accountId == 0) {
        return;
    }
    const std::string room = text(instanceType) + ":" + text(roomId);
    eval(kEnterScript, {text(accountId), room, text(kPresenceTtlSeconds)});
}

void InstancePresence::leave(std::uint64_t accountId) {
    if (accountId == 0) {
        return;
    }
    eval(kLeaveScript, {text(accountId)});
}

std::vector<std::uint64_t> InstancePresence::roommates(std::uint64_t accountId) {
    std::vector<std::uint64_t> ids;
    if (accountId == 0) {
        return ids;
    }

    const std::string raw = eval(kRoommatesScript, {text(accountId)});
    std::size_t at = 0;
    while (at < raw.size()) {
        const std::size_t comma = raw.find(',', at);
        const std::size_t end = comma == std::string::npos ? raw.size() : comma;
        std::uint64_t id = 0;
        if (std::from_chars(raw.data() + at, raw.data() + end, id).ec == std::errc{} && id != 0) {
            ids.push_back(id);
        }
        if (comma == std::string::npos) {
            break;
        }
        at = comma + 1;
    }
    return ids;
}

}  // namespace heaven::instancechat
