#include "Room.h"

namespace heaven::chat {

void Room::join(const std::shared_ptr<TlsSession>& session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_.insert(session);
}

void Room::leave(const std::shared_ptr<TlsSession>& session) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_.erase(session);
}

void Room::broadcast(const Frame& frame, const TlsSession* except) {
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
