#include "Room.h"

namespace heaven::chat {

std::shared_ptr<TlsSession> Room::join(std::uint64_t accountId, const std::string& nickname,
                                       const std::shared_ptr<TlsSession>& session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_.insert(session);

    std::shared_ptr<TlsSession> previous;
    const auto it = byAccount_.find(accountId);
    if (it != byAccount_.end() && it->second != session) {
        previous = it->second;
    }
    byAccount_[accountId] = session;

    // 밀려난 세션이 같은 계정이면 닉네임도 같다. 덮어써도 문제가 없다.
    const auto old = nicknames_.find(accountId);
    if (old != nicknames_.end() && old->second != nickname) {
        byNickname_.erase(old->second);
    }
    nicknames_[accountId] = nickname;
    byNickname_[nickname] = accountId;
    return previous;
}

void Room::leave(std::uint64_t accountId, const std::shared_ptr<TlsSession>& session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_.erase(session);

    const auto it = byAccount_.find(accountId);
    if (it != byAccount_.end() && it->second == session) {
        byAccount_.erase(it);

        // 계정 인덱스를 실제로 넘겨받은 세션만 닉네임도 지운다. 밀려난 옛 세션이
        // 나가면서 새 세션의 색인을 지우면 안 된다 (byAccount_ 와 같은 이유).
        const auto named = nicknames_.find(accountId);
        if (named != nicknames_.end()) {
            const auto owner = byNickname_.find(named->second);
            if (owner != byNickname_.end() && owner->second == accountId) {
                byNickname_.erase(owner);
            }
            nicknames_.erase(named);
        }
    }
}

void Room::broadcast(const proto::Bytes& frame, const TlsSession* except) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& session : sessions_) {
        if (session.get() == except) {
            continue;
        }
        session->send(frame);
    }
}

void Room::sendToAccounts(const std::vector<std::uint64_t>& accountIds,
                          const proto::Bytes& frame) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const std::uint64_t accountId : accountIds) {
        const auto it = byAccount_.find(accountId);
        if (it != byAccount_.end()) {
            it->second->send(frame);
        }
    }
}

std::shared_ptr<TlsSession> Room::findAccount(std::uint64_t accountId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = byAccount_.find(accountId);
    return it == byAccount_.end() ? nullptr : it->second;
}

std::uint64_t Room::accountByNickname(const std::string& nickname) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = byNickname_.find(nickname);
    return it == byNickname_.end() ? 0 : it->second;
}

std::vector<std::uint64_t> Room::onlineAccounts() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::uint64_t> ids;
    ids.reserve(byAccount_.size());
    for (const auto& [accountId, session] : byAccount_) {
        ids.push_back(accountId);
    }
    return ids;
}

std::size_t Room::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return sessions_.size();
}

}  // namespace heaven::chat
