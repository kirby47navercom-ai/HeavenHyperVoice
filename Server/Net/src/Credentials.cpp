#include "Credentials.h"

#include <windows.h>
// windows.h 를 먼저 넣어야 wincred.h 가 필요한 타입을 찾는다.
#include <wincred.h>

#include <stdexcept>
#include <vector>

namespace heaven::net {

namespace {

std::wstring widen(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                             static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(),
                          needed);
    return wide;
}

std::string lastErrorText(const char* what) {
    return std::string(what) + " failed (error " + std::to_string(::GetLastError()) + ")";
}

}  // namespace

std::optional<std::string> readStoredPassword(const std::string& target) {
    const std::wstring wideTarget = widen(target);

    PCREDENTIALW credential = nullptr;
    if (!::CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        return std::nullopt;
    }

    // CredBlob 은 바이트열이다. 우리는 UTF-8 로 저장하므로 그대로 읽는다.
    std::string password(reinterpret_cast<const char*>(credential->CredentialBlob),
                         credential->CredentialBlobSize);
    ::CredFree(credential);

    if (password.empty()) {
        return std::nullopt;
    }
    return password;
}

void storePassword(const std::string& target, const std::string& password) {
    std::wstring wideTarget = widen(target);
    std::vector<char> blob(password.begin(), password.end());

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = wideTarget.data();
    credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    // 로그인 세션이 아니라 사용자 프로필에 저장해 재부팅 후에도 남는다.
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!::CredWriteW(&credential, 0)) {
        throw std::runtime_error(lastErrorText("CredWrite"));
    }
}

bool erasePassword(const std::string& target) {
    const std::wstring wideTarget = widen(target);
    return ::CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0) != FALSE;
}

}  // namespace heaven::net
