#include "InstancePresence.h"

#include <charconv>

#include "RedisClient.h"

namespace heaven::instancechat {

namespace {

using net::arg;

// 스크립트가 실패했을 때 경고에 붙는 이름.
constexpr const char* kWhat = "instance presence script";

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

void InstancePresence::enter(std::uint64_t accountId, std::uint32_t instanceType,
                             std::uint32_t roomId) {
    if (accountId == 0) {
        return;
    }
    const std::string room = arg(instanceType) + ":" + arg(roomId);
    redis_.eval(kWhat, kEnterScript,
                {arg(accountId), room, arg(kPresenceTtlSeconds)});
}

void InstancePresence::leave(std::uint64_t accountId) {
    if (accountId == 0) {
        return;
    }
    redis_.eval(kWhat, kLeaveScript, {arg(accountId)});
}

std::vector<std::uint64_t> InstancePresence::roommates(std::uint64_t accountId) {
    std::vector<std::uint64_t> ids;
    if (accountId == 0) {
        return ids;
    }

    const std::string raw = redis_.eval(kWhat, kRoommatesScript, {arg(accountId)});
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
