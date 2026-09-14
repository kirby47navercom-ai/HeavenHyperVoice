#pragma once

#include "TriangleWorld.h"
#include <memory>
#include <string>
#include <vector>

namespace heaven {

// 서버 전용 길찾기 지도. 좌표는 공통 코어와 같은 cm / Z-up / 캡슐 중심 기준이다.
// 빌드 후에는 불변이며, 각 요청이 독립된 Detour 검색 상태를 사용한다.
class NavigationMesh {
  public:
    NavigationMesh();
    ~NavigationMesh();

    NavigationMesh(const NavigationMesh &) = delete;
    NavigationMesh &operator=(const NavigationMesh &) = delete;

    bool build(const hhv::movement::TriangleWorld &collision, const hhv::movement::Config &config,
               std::string &error);
    bool findPath(hhv::movement::Vec3 start, hhv::movement::Vec3 goal, int maxNodes,
                  std::vector<hhv::movement::Vec3> &points) const;
    bool clearSegment(hhv::movement::Vec3 start, hhv::movement::Vec3 goal) const;
    std::size_t polygonCount() const;

  private:
    struct Data;
    std::unique_ptr<Data> data_;
};

} // namespace heaven
