#pragma once

// 벽 충돌 판정.
//
// 클라이언트의 HHV::Map 을 옮겨온 것이 아니라, 같은 파일 형식을 읽고 같은
// 판정을 하도록 다시 구현한 것이다. 서버가 자기 코드를 소유해야 클라이언트
// 프로젝트 구조에 묶이지 않는다.
//
// 지금은 wall_obb 만 본다. 바닥(하이트맵)은 맵 파일 실물이 생기면 붙인다 —
// 절벽 보간 규칙이 까다로워서 시험할 데이터 없이 짜면 틀린 것을 못 알아챈다.

#include <cstdint>
#include <string>
#include <vector>

namespace heaven::field {

struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// 축이 정규직교라고 가정한다. 맵 파일이 그렇게 내보낸다.
struct Obb {
    Vec3 center;
    Vec3 halfExtent;
    Vec3 axisX{1.f, 0.f, 0.f};
    Vec3 axisY{0.f, 1.f, 0.f};
    Vec3 axisZ{0.f, 0.f, 1.f};
};

// 캐릭터를 감싸는 캡슐. 클라이언트 기본값과 같아야 예측이 어긋나지 않는다.
struct AgentSettings {
    float capsuleRadius = 34.f;
    float capsuleHalfHeight = 88.f;
};

class MapCollision {
public:
    // 팀원 exporter 가 내보내는 텍스트 형식. wall_obb 줄만 읽고 나머지는 넘긴다.
    //
    //   wall_obb <profile> cx cy cz  hx hy hz  axX axY axZ  ayX ayY ayZ  azX azY azZ
    //
    // 벽이 한 줄도 없으면 로드 실패로 본다. 빈 맵을 실수로 물린 채 "충돌 검사
    // 켜짐" 이라고 착각하는 것이 제일 나쁘다.
    bool loadFromFile(const std::string& path, std::string& error);

    bool loaded() const { return !walls_.empty(); }
    std::size_t wallCount() const { return walls_.size(); }

    // from 에서 to 까지 캡슐 반지름 간격으로 훑는다. 한 틱에 캡슐 지름보다 멀리
    // 움직이면 도착점만 봐서는 얇은 벽을 그냥 통과한다.
    bool blockedAlong(const Vec3& from, const Vec3& to, const AgentSettings& agent) const;

private:
    // 캡슐 중심이 벽 안에 있는가. 점 하나만 보는 것은 얇은 벽을 놓치므로
    // 밖에서는 blockedAlong 만 쓴다.
    bool blocked(const Vec3& capsuleCenter, const AgentSettings& agent) const;

    std::vector<Obb> walls_;
};

}  // namespace heaven::field
