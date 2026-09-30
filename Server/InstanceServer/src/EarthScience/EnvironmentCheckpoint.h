#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace heaven::instance {
// 파일 저장은 틱 밖에서 호출해요. 체크섬/이전 파일로 중간 종료에 대비해요.
std::optional<std::string> readEnvironmentCheckpoint(const std::filesystem::path& file);
void writeEnvironmentCheckpoint(const std::filesystem::path& file,const std::string& state);
}
