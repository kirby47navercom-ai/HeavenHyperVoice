#include "RoomManager.h"
#include "EarthScience/EnvironmentMovement.h"
#include <iomanip>
#include <sstream>
#include <set>
#include <stdexcept>
namespace heaven::instance {
std::string RoomManager::saveEnvironment() const {
    std::ostringstream out;
    // 모든 방을 한 시점에 캡처한 뒤 파일 I/O는 호출자 저장 스레드가 해요.
    std::unique_lock alive(lifetimeMutex_);std::lock_guard lock(mutex_);
    out<<std::setprecision(17)<<"ROOMS1 "<<worldClock_.elapsedRealSeconds()<<' '<<nextRoomId_<<' '<<rooms_.size()<<'\n';
    for(const auto& room:rooms_) {out<<room->type<<' '<<room->id<<'\n';room->weather.save(out);}
    return out.str();
}
void RoomManager::restoreEnvironment(const std::string& saved) {
    std::istringstream in(saved);std::string magic;double clock=0;std::uint32_t next=0;std::size_t count=0;
    if(!(in>>magic>>clock>>next>>count) || magic!="ROOMS1" || !std::isfinite(clock) || clock<0 || clock>1e12 ||
        next==0 || count>10000) throw std::runtime_error("Invalid environment checkpoint header");
    struct Saved {std::uint32_t type,id;InstanceWeather weather;};std::vector<Saved> records;
    std::set<std::uint32_t> ids;std::map<std::uint32_t,int> perType;
    for(std::size_t i=0;i<count;++i) {
        Saved record{};
        if(!(in>>record.type>>record.id) || !isKnownType(record.type) || record.id==0 || record.id>=next || !ids.insert(record.id).second)
            throw std::runtime_error("Invalid saved room");
        if(settings_.maxRoomsPerType>0 && ++perType[record.type]>settings_.maxRoomsPerType) throw std::runtime_error("Saved room limit exceeded");
        record.weather.initialize(record.type,record.id,types_.at(record.type).weather,clock);
        if(!record.weather.restore(in)) throw std::runtime_error("Invalid saved climate");
        records.push_back(std::move(record));
    }
    std::string extra;if(in>>extra) throw std::runtime_error("Unexpected saved environment data");
    std::unique_lock alive(lifetimeMutex_);std::lock_guard lock(mutex_);
    if(!rooms_.empty()) throw std::runtime_error("Restore must precede room admission");
    worldClock_.restore(clock);nextRoomId_=next;
    for(const auto& record:records) {
        auto* room=createRoomLocked(record.type,record.id,&record.weather);
        room->waitingForRestoredEntry=true;room->emptySince=std::chrono::steady_clock::now();
    }
}
}
