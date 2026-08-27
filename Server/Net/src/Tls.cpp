#include "Tls.h"

#include <stdexcept>

#include "OpenSslError.h"

namespace heaven::net {

// ------------------------------------------------------------ TlsContext

TlsContext::TlsContext(const std::string& certFile, const std::string& keyFile) {
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (ctx_ == nullptr) {
        throw std::runtime_error("SSL_CTX_new failed: " + proto::opensslError());
    }

    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
    SSL_CTX_set_options(ctx_, SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE);

    if (SSL_CTX_use_certificate_chain_file(ctx_, certFile.c_str()) != 1) {
        const std::string err = proto::opensslError();
        SSL_CTX_free(ctx_);
        throw std::runtime_error("failed to load certificate " + certFile + ": " + err);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx_, keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
        const std::string err = proto::opensslError();
        SSL_CTX_free(ctx_);
        throw std::runtime_error("failed to load private key " + keyFile + ": " + err);
    }
    if (SSL_CTX_check_private_key(ctx_) != 1) {
        const std::string err = proto::opensslError();
        SSL_CTX_free(ctx_);
        throw std::runtime_error("certificate and private key do not match: " + err);
    }
}

TlsContext::~TlsContext() {
    if (ctx_ != nullptr) {
        SSL_CTX_free(ctx_);
    }
}

// ------------------------------------------------------------ TlsChannel

TlsChannel::TlsChannel(SSL_CTX* ctx) {
    ssl_ = SSL_new(ctx);
    if (ssl_ == nullptr) {
        throw std::runtime_error("SSL_new failed: " + proto::opensslError());
    }

    rbio_ = BIO_new(BIO_s_mem());
    wbio_ = BIO_new(BIO_s_mem());
    if (rbio_ == nullptr || wbio_ == nullptr) {
        if (rbio_ != nullptr) BIO_free(rbio_);
        if (wbio_ != nullptr) BIO_free(wbio_);
        SSL_free(ssl_);
        throw std::runtime_error("BIO_new failed: " + proto::opensslError());
    }

    // 메모리 BIO 는 비면 재시도를 요청해야 한다. 이게 없으면 EOF 로 오인한다.
    BIO_set_mem_eof_return(rbio_, -1);
    BIO_set_mem_eof_return(wbio_, -1);

    SSL_set_bio(ssl_, rbio_, wbio_);  // 소유권이 SSL 로 넘어간다
    SSL_set_accept_state(ssl_);
}

TlsChannel::~TlsChannel() {
    if (ssl_ != nullptr) {
        SSL_free(ssl_);  // BIO 도 함께 해제된다
    }
}

void TlsChannel::captureError(const char* what, int sslError) {
    const std::string detail = proto::opensslError();
    lastError_ =
        std::string(what) + " failed (SSL error " + std::to_string(sslError) + "): " + detail;
}

bool TlsChannel::advanceHandshake() {
    const int result = SSL_do_handshake(ssl_);
    if (result > 0) {
        return true;
    }
    const int err = SSL_get_error(ssl_, result);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return true;  // 더 주고받아야 한다
    }
    captureError("SSL_accept", err);
    return false;
}

void TlsChannel::feedEncrypted(const char* data, std::size_t len) {
    // 메모리 BIO 는 필요한 만큼 자라므로 부분 쓰기를 걱정하지 않아도 된다.
    BIO_write(rbio_, data, static_cast<int>(len));
}

bool TlsChannel::pump(std::vector<unsigned char>& plainOut) {
    if (!SSL_is_init_finished(ssl_)) {
        if (!advanceHandshake()) {
            return false;
        }
        if (!SSL_is_init_finished(ssl_)) {
            return true;  // 아직 주고받을 게 남았다
        }
    }

    unsigned char buffer[8192];
    for (;;) {
        const int read = SSL_read(ssl_, buffer, static_cast<int>(sizeof(buffer)));
        if (read > 0) {
            plainOut.insert(plainOut.end(), buffer, buffer + read);
            continue;
        }

        const int err = SSL_get_error(ssl_, read);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            return true;  // 데이터가 더 필요하다. 정상.
        }
        if (err == SSL_ERROR_ZERO_RETURN) {
            lastError_ = "peer closed the TLS session";
            return false;
        }
        captureError("SSL_read", err);
        return false;
    }
}

bool TlsChannel::writePlain(const unsigned char* data, std::size_t len) {
    std::size_t offset = 0;
    while (offset < len) {
        const int written = SSL_write(ssl_, data + offset, static_cast<int>(len - offset));
        if (written > 0) {
            offset += static_cast<std::size_t>(written);
            continue;
        }

        const int err = SSL_get_error(ssl_, written);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            lastError_ = "SSL_write would block";
            return false;
        }
        captureError("SSL_write", err);
        return false;
    }
    return true;
}

std::size_t TlsChannel::drainEncrypted(std::vector<char>& out) {
    std::size_t total = 0;
    for (;;) {
        const int pending = BIO_pending(wbio_);
        if (pending <= 0) {
            break;
        }

        const std::size_t offset = out.size();
        out.resize(offset + static_cast<std::size_t>(pending));
        const int read = BIO_read(wbio_, out.data() + offset, pending);
        if (read <= 0) {
            out.resize(offset);
            break;
        }
        out.resize(offset + static_cast<std::size_t>(read));
        total += static_cast<std::size_t>(read);
    }
    return total;
}

}  // namespace heaven::net
