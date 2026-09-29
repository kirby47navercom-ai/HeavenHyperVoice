#pragma once
#include <string>
#include "../InstanceWeather.h"
namespace heaven::instance {
// 이름=숫자 형식. 알 수 없는 키, 중복 키, 비정상 숫자는 서버 시작 전에 거부한다.
InstanceWeatherProfile loadEnvironmentProfile(const std::string& filename);
}
