#include "EnvironmentCheckpoint.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif
namespace heaven::instance {
namespace {
std::uint64_t checksum(const std::string& text) {
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned char c:text) {hash^=c;hash*=1099511628211ull;}return hash;
}
std::optional<std::string> readOne(const std::filesystem::path& file) {
    std::error_code ec;const auto size=std::filesystem::file_size(file,ec);
    if(ec || size>16*1024*1024) return {};
    std::ifstream in(file,std::ios::binary);std::string header;std::uint64_t expected=0;
    std::getline(in,header);std::istringstream prefix(header);std::string magic,extra;
    if(!(prefix>>magic>>expected) || magic!="HHVENV1" || prefix>>extra) return {};
    std::string state((std::istreambuf_iterator<char>(in)),{});
    if(checksum(state)!=expected) return {};return state;
}
}
std::optional<std::string> readEnvironmentCheckpoint(const std::filesystem::path& file) {
    auto state=readOne(file);return state ? state : readOne(file.string()+".bak");
}
void writeEnvironmentCheckpoint(const std::filesystem::path& file,const std::string& state) {
    if(state.size()>16*1024*1024) throw std::runtime_error("Environment checkpoint exceeds 16 MiB");
    if(!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path());
    const std::filesystem::path temporary=file.string()+".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<"HHVENV1 "<<checksum(state)<<'\n'<<state;
        out.flush();if(!out) throw std::runtime_error("Environment checkpoint write failed");}
    // 정상 파일만 백업해요. 손상된 최신 파일이 좋은 백업까지 덮어쓰지 않아요.
    if(readOne(file)) std::filesystem::copy_file(file,file.string()+".bak",std::filesystem::copy_options::overwrite_existing);
#if defined(_WIN32)
    if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Environment checkpoint replacement failed");
#else
    std::filesystem::rename(temporary,file);
#endif
}
}
