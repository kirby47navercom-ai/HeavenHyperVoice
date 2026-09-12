#include "PartyStore.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>

#include "RedisClient.h"

namespace heaven::party {

namespace {

using net::arg;

// 스크립트가 실패했을 때 경고에 붙는 이름.
constexpr const char* kWhat = "party script";

// 숫자를 읽고 커서를 옮긴다. 실패하면 false.
bool readNumber(const std::string& raw, std::size_t& at, char terminator, std::uint64_t& out) {
    const std::size_t end = raw.find(terminator, at);
    if (end == std::string::npos) {
        return false;
    }
    if (std::from_chars(raw.data() + at, raw.data() + end, out).ec != std::errc{}) {
        return false;
    }
    at = end + 1;
    return true;
}

// find 스크립트의 응답을 푼다.
//
//   <partyId>;<accountId>,<닉네임 바이트수>,<닉네임><accountId>,...
//
// 닉네임을 길이로 잘라내는 이유는 사용자 입력이라서다. 구분자를 쓰면 닉네임에
// 그 문자가 들어간 순간 파싱이 어긋난다.
PartyView parseParty(const std::string& raw) {
    PartyView view;
    std::size_t at = 0;
    std::uint64_t id = 0;
    if (!readNumber(raw, at, ';', id) || id == 0) {
        return view;
    }
    view.id = id;

    while (at < raw.size()) {
        std::uint64_t accountId = 0;
        std::uint64_t length = 0;
        if (!readNumber(raw, at, ',', accountId) || !readNumber(raw, at, ',', length)) {
            break;
        }
        if (at + length > raw.size()) {
            break;
        }
        PartyMember member;
        member.accountId = accountId;
        member.nickname = raw.substr(at, static_cast<std::size_t>(length));
        view.members.push_back(std::move(member));
        at += static_cast<std::size_t>(length);
    }

    if (view.members.empty()) {
        return {};
    }
    return view;
}

// 파티를 읽으면서 생존 신호가 끊긴 멤버를 목록에서 뺀다.
// ARGV: partyId
constexpr const char* kFindScript = R"lua(
local pid = ARGV[1]
local key = 'party:' .. pid
local names = 'party:names:' .. pid
local ids = redis.call('LRANGE', key, 0, -1)
local out = {}
local alive = 0
for i = 1, #ids do
  if redis.call('EXISTS', 'party:member:' .. ids[i]) == 1 then
    alive = alive + 1
    local nick = redis.call('HGET', names, ids[i])
    if not nick then nick = '' end
    out[#out + 1] = ids[i] .. ',' .. string.len(nick) .. ',' .. nick
  else
    redis.call('LREM', key, 0, ids[i])
    redis.call('HDEL', names, ids[i])
  end
end
if alive == 0 then
  redis.call('DEL', key)
  redis.call('DEL', names)
  return ''
end
return tostring(pid) .. ';' .. table.concat(out, '')
)lua";

// ARGV: inviter, inviterNickname, target, memberTtl, inviteTtl, maxMembers, partyTtl
constexpr const char* kInviteScript = R"lua(
local pid = redis.call('GET', 'party:member:' .. ARGV[1])
if not pid then
  pid = tostring(redis.call('INCR', 'party:seq'))
  redis.call('RPUSH', 'party:' .. pid, ARGV[1])
  redis.call('SET', 'party:member:' .. ARGV[1], pid, 'EX', ARGV[4])
end
local key = 'party:' .. pid
redis.call('HSET', 'party:names:' .. pid, ARGV[1], ARGV[2])
redis.call('EXPIRE', key, ARGV[7])
redis.call('EXPIRE', 'party:names:' .. pid, ARGV[7])
if redis.call('LLEN', key) >= tonumber(ARGV[6]) then return 'FULL' end
if redis.call('EXISTS', 'party:member:' .. ARGV[3]) == 1 then return 'BUSY' end
redis.call('HSET', 'party:invite:' .. ARGV[3], pid, ARGV[1])
redis.call('EXPIRE', 'party:invite:' .. ARGV[3], ARGV[5])
return 'OK:' .. tostring(pid)
)lua";

// ARGV: accountId, nickname, partyId, memberTtl, maxMembers, partyTtl
constexpr const char* kAcceptScript = R"lua(
local invites = 'party:invite:' .. ARGV[1]
if redis.call('HGET', invites, ARGV[3]) == false then return 'NOINVITE' end
redis.call('DEL', invites)
if redis.call('EXISTS', 'party:member:' .. ARGV[1]) == 1 then return 'BUSY' end
local key = 'party:' .. ARGV[3]
if redis.call('EXISTS', key) == 0 then return 'GONE' end
if redis.call('LLEN', key) >= tonumber(ARGV[5]) then return 'FULL' end
redis.call('RPUSH', key, ARGV[1])
redis.call('HSET', 'party:names:' .. ARGV[3], ARGV[1], ARGV[2])
redis.call('SET', 'party:member:' .. ARGV[1], ARGV[3], 'EX', ARGV[4])
redis.call('EXPIRE', key, ARGV[6])
redis.call('EXPIRE', 'party:names:' .. ARGV[3], ARGV[6])
return 'OK'
)lua";

// ARGV: accountId
constexpr const char* kLeaveScript = R"lua(
local pid = redis.call('GET', 'party:member:' .. ARGV[1])
if not pid then return '' end
redis.call('DEL', 'party:member:' .. ARGV[1])
local key = 'party:' .. pid
redis.call('LREM', key, 0, ARGV[1])
redis.call('HDEL', 'party:names:' .. pid, ARGV[1])
if redis.call('LLEN', key) == 0 then
  redis.call('DEL', key)
  redis.call('DEL', 'party:names:' .. pid)
end
return tostring(pid)
)lua";

// ARGV: accountId, memberTtl, partyTtl
constexpr const char* kTouchScript = R"lua(
local pid = redis.call('GET', 'party:member:' .. ARGV[1])
if not pid then return '' end
redis.call('EXPIRE', 'party:member:' .. ARGV[1], ARGV[2])
redis.call('EXPIRE', 'party:' .. pid, ARGV[3])
redis.call('EXPIRE', 'party:names:' .. pid, ARGV[3])
return tostring(pid)
)lua";

// ARGV: partyId, instanceType, fallbackRoomId, roomTtl
constexpr const char* kClaimRoomScript = R"lua(
local key = 'party:' .. ARGV[1] .. ':room:' .. ARGV[2]
local current = redis.call('GET', key)
if current then
  redis.call('EXPIRE', key, ARGV[4])
  return current
end
redis.call('SET', key, ARGV[3], 'EX', ARGV[4])
return ARGV[3]
)lua";

}  // namespace

bool PartyView::contains(std::uint64_t accountId) const {
    return std::any_of(members.begin(), members.end(),
                       [accountId](const PartyMember& m) { return m.accountId == accountId; });
}

std::vector<std::uint64_t> PartyView::accountIds() const {
    std::vector<std::uint64_t> ids;
    ids.reserve(members.size());
    for (const PartyMember& member : members) {
        ids.push_back(member.accountId);
    }
    return ids;
}

std::uint64_t PartyStore::partyIdOf(std::uint64_t accountId) {
    if (accountId == 0) {
        return 0;
    }
    const auto reply = redis_.commandForString({"GET", "party:member:" + arg(accountId)});
    if (!reply.has_value()) {
        return 0;
    }
    std::uint64_t id = 0;
    if (std::from_chars(reply->data(), reply->data() + reply->size(), id).ec != std::errc{}) {
        return 0;
    }
    return id;
}

PartyView PartyStore::findById(std::uint64_t partyId) {
    if (partyId == 0) {
        return {};
    }
    return parseParty(redis_.eval(kWhat, kFindScript, {arg(partyId)}));
}

PartyView PartyStore::find(std::uint64_t accountId) {
    return findById(partyIdOf(accountId));
}

InviteResult PartyStore::invite(std::uint64_t inviter, const std::string& inviterNickname,
                                std::uint64_t target, std::uint64_t& outPartyId) {
    outPartyId = 0;
    if (inviter == 0 || target == 0 || inviter == target) {
        return InviteResult::Failed;
    }

    const std::string reply = redis_.eval(
        kWhat, kInviteScript,
        {arg(inviter), inviterNickname, arg(target), arg(kMemberTtlSeconds),
         arg(kInviteTtlSeconds), arg(static_cast<int>(kMaxMembers)), arg(kPartyTtlSeconds)});

    if (reply == "FULL") {
        return InviteResult::Full;
    }
    if (reply == "BUSY") {
        return InviteResult::TargetBusy;
    }
    if (reply.rfind("OK:", 0) != 0) {
        return InviteResult::Failed;
    }

    std::uint64_t id = 0;
    if (std::from_chars(reply.data() + 3, reply.data() + reply.size(), id).ec != std::errc{} ||
        id == 0) {
        return InviteResult::Failed;
    }
    outPartyId = id;
    return InviteResult::Ok;
}

AcceptResult PartyStore::accept(std::uint64_t accountId, const std::string& nickname,
                                std::uint64_t partyId, PartyView& out) {
    out = {};
    if (accountId == 0 || partyId == 0) {
        return AcceptResult::Failed;
    }

    const std::string reply =
        redis_.eval(kWhat, kAcceptScript,
                    {arg(accountId), nickname, arg(partyId), arg(kMemberTtlSeconds),
                     arg(static_cast<int>(kMaxMembers)), arg(kPartyTtlSeconds)});

    if (reply == "NOINVITE") return AcceptResult::NoInvite;
    if (reply == "BUSY") return AcceptResult::AlreadyInParty;
    if (reply == "GONE") return AcceptResult::Gone;
    if (reply == "FULL") return AcceptResult::Full;
    if (reply != "OK") return AcceptResult::Failed;

    out = find(accountId);
    return AcceptResult::Ok;
}

void PartyStore::decline(std::uint64_t accountId, std::uint64_t partyId) {
    if (accountId == 0 || partyId == 0) {
        return;
    }
    redis_.command({"HDEL", "party:invite:" + arg(accountId), arg(partyId)});
}

std::uint64_t PartyStore::leave(std::uint64_t accountId) {
    if (accountId == 0) {
        return 0;
    }
    const std::string reply = redis_.eval(kWhat, kLeaveScript, {arg(accountId)});
    std::uint64_t id = 0;
    if (std::from_chars(reply.data(), reply.data() + reply.size(), id).ec != std::errc{}) {
        return 0;
    }
    return id;
}

void PartyStore::touch(std::uint64_t accountId) {
    if (accountId == 0) {
        return;
    }
    redis_.eval(kWhat, kTouchScript,
                {arg(accountId), arg(kMemberTtlSeconds), arg(kPartyTtlSeconds)});
}

bool PartyStore::openEntry(std::uint64_t partyId, std::uint32_t instanceType) {
    if (partyId == 0) {
        return false;
    }
    return redis_.command({"SET", "party:" + arg(partyId) + ":enter:" + arg(instanceType), "1",
                           "EX", arg(kEnterTtlSeconds)});
}

bool PartyStore::entryOpen(std::uint64_t partyId, std::uint32_t instanceType) {
    if (partyId == 0) {
        return false;
    }
    // EXISTS 는 정수 응답이라 commandForString 이 nullopt 를 준다. GET 을 쓴다.
    const auto reply =
        redis_.commandForString({"GET", "party:" + arg(partyId) + ":enter:" + arg(instanceType)});
    return reply.has_value();
}

std::uint32_t PartyStore::claimRoom(std::uint64_t partyId, std::uint32_t instanceType,
                                    std::uint32_t fallbackRoomId) {
    if (partyId == 0) {
        return fallbackRoomId;
    }
    const std::string reply = redis_.eval(
        kWhat, kClaimRoomScript,
        {arg(partyId), arg(instanceType), arg(fallbackRoomId), arg(kRoomTtlSeconds)});

    std::uint64_t room = 0;
    if (std::from_chars(reply.data(), reply.data() + reply.size(), room).ec != std::errc{} ||
        room == 0) {
        // Redis 가 답을 못 주면 방을 나누지 못할 뿐이다. 입장 자체는 막지 않는다.
        spdlog::warn("party {}: room claim failed, falling back to room {}", partyId,
                     fallbackRoomId);
        return fallbackRoomId;
    }
    return static_cast<std::uint32_t>(room);
}

}  // namespace heaven::party
