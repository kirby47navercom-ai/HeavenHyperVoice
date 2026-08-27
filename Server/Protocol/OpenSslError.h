#pragma once

// OpenSSL 오류 큐를 사람이 읽을 수 있는 한 줄로 비운다.
//
// 큐는 스레드마다 따로이고 읽으면 비워진다. 그래서 실패한 호출 **직후에**
// 불러야 하고, 한 번 부르면 같은 오류를 다시 꺼낼 수 없다.
//
// 티켓 서명(Protocol)과 TLS 채널(Net)이 각자 같은 함수를 들고 있던 것을 모았다.
// ERR_* 는 libcrypto 라 OpenSSL 을 쓰는 쪽이면 어디서든 링크된다.

#include <openssl/err.h>

#include <string>

namespace heaven::proto {

inline std::string opensslError() {
    std::string out;
    while (const unsigned long code = ERR_get_error()) {
        char buf[256];
        ERR_error_string_n(code, buf, sizeof(buf));
        if (!out.empty()) {
            out += "; ";
        }
        out += buf;
    }
    // 큐가 비어 있는데도 실패한 경우가 있다. 빈 문자열보다는 이게 낫다.
    return out.empty() ? "unknown OpenSSL error" : out;
}

}  // namespace heaven::proto
