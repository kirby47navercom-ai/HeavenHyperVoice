#pragma once

// 소켓에서 분리된 TLS. IOCP 가 네트워크 I/O 를 담당하므로 OpenSSL 은 소켓을
// 직접 만지면 안 된다. 메모리 BIO 한 쌍을 물려 암호문/평문 변환만 시킨다.
//
//   네트워크 --(암호문)--> feedEncrypted -> rbio -> SSL -> pump --(평문)--> 앱
//   앱 --(평문)--> writePlain -> SSL -> wbio -> drainEncrypted --(암호문)--> 네트워크

#include <openssl/ssl.h>

#include <cstddef>
#include <string>
#include <vector>

namespace heaven::net {

// SSL_CTX 소유. 프로세스당 하나면 충분하며 스레드 안전하다.
class TlsContext {
public:
    TlsContext(const std::string& certFile, const std::string& keyFile);
    ~TlsContext();

    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;

    SSL_CTX* native() const { return ctx_; }

private:
    SSL_CTX* ctx_ = nullptr;
};

// 연결 하나의 TLS 상태. 스레드 안전하지 않으므로 호출자가 직렬화해야 한다.
class TlsChannel {
public:
    explicit TlsChannel(SSL_CTX* ctx);
    ~TlsChannel();

    TlsChannel(const TlsChannel&) = delete;
    TlsChannel& operator=(const TlsChannel&) = delete;

    // 네트워크에서 받은 암호문을 밀어 넣는다.
    void feedEncrypted(const char* data, std::size_t len);

    // 핸드셰이크를 진행시키고, 완료됐다면 평문을 plainOut 뒤에 덧붙인다.
    // 치명적 오류면 false. 이때 lastError() 에 사유가 들어간다.
    bool pump(std::vector<unsigned char>& plainOut);

    // 평문을 TLS 레코드로 만든다. 결과 암호문은 drainEncrypted 로 꺼내야 한다.
    bool writePlain(const unsigned char* data, std::size_t len);

    // 보낼 암호문을 out 뒤에 덧붙인다. 덧붙인 바이트 수를 반환.
    std::size_t drainEncrypted(std::vector<char>& out);

    const std::string& lastError() const { return lastError_; }

private:
    void captureError(const char* what, int sslError);
    // 핸드셰이크 한 걸음. 계속 진행 가능하면 true.
    bool advanceHandshake();

    SSL* ssl_ = nullptr;
    BIO* rbio_ = nullptr;  // 네트워크 -> SSL
    BIO* wbio_ = nullptr;  // SSL -> 네트워크
    std::string lastError_;
};

}  // namespace heaven::net
