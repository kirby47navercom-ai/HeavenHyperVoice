#include "Room.h"

namespace heaven::chat {

std::shared_ptr<TlsSession> Room::join(std::uint64_t accountId,
                                       const std::shared_ptr<TlsSession>& session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_.insert(session);

    std::shared_ptr<TlsSession> previous;
    const auto it = byAccount_.find(accountId);
    if (it != byAccount_.end() && it->second != session) {
        previous = it->second;
    }
    byAccount_[accountId] = session;
    return previous;
}

void Room::leave(std::uint64_t accountId, const std::shared_ptr<TlsSession>& session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_.erase(session);

    const auto it = byAccount_.find(accountId);
    if (it != byAccount_.end() && it->second == session) {
        byAccount_.erase(it);
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

std::size_t Room::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return sessions_.size();
}

}  // namespace heaven::chat
