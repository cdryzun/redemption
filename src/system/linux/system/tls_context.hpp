/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2013
   Author(s): Christophe Grosjean, Raphael Zhou, Meng Tan

   Transport layer abstraction, socket implementation with TLS support
*/


#pragma once

#include "core/app_path.hpp"
#include "utils/log.hpp"

#include "transport/transport.hpp" // Transport::TlsResult, Transport::CertificateChecker

#include "cxx/diagnostic.hpp"

#include <memory>
#include <utility>
#include <cstring>

#include <fcntl.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>


REDEMPTION_DIAGNOSTIC_PUSH()
REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wold-style-cast")
REDEMPTION_DIAGNOSTIC_GCC_ONLY_IGNORE("-Wzero-as-null-pointer-constant")
#if REDEMPTION_WORKAROUND(REDEMPTION_COMP_CLANG, >= 500)
    REDEMPTION_DIAGNOSTIC_CLANG_IGNORE("-Wzero-as-null-pointer-constant")
#endif

// inline void ssl_debug_log(SSL* ssl)
// {
//     SSL_set_msg_callback(ssl, SSL_trace);
//     SSL_set_msg_callback_arg(ssl, BIO_new_fp(stderr, 0));
// }

REDEMPTION_NOINLINE
inline bool tls_ctx_print_error(char const* funcname, char const* error_msg)
{
    LOG(LOG_ERR, "TLSContext::%s: %s", funcname, error_msg);
    unsigned long error;
    char buf[1024];
    while ((error = ERR_get_error()) != 0) {
        ERR_error_string_n(error, buf, sizeof(buf));
        LOG(LOG_ERR, "print_error %s", buf);
    }
    return false;
}

/// \return nullptr when no error. Otherwise function name with an error
inline char const* apply_tls_config(
    SSL_CTX* ctx, TlsConfig const& tls_config, bool verbose, char const* funcname)
{
    LOG_IF(verbose, LOG_INFO,
        "TLSContext::%s: TLS: min_level=%u%s, max_level=%u%s, cipher_list='%s', TLSv1.3 ciphersuites='%s', signature_algorithms='%s'",
        funcname,
        tls_config.min_level, tls_config.min_level == 0 ? " (system-wide)" : "",
        tls_config.max_level, tls_config.max_level == 0 ? " (system-wide)" : "",
        tls_config.cipher_list.empty() ? "(system-wide)"
            : tls_config.cipher_list.c_str(),
        tls_config.tls_1_3_ciphersuites.empty() ? "(system-wide)"
            : tls_config.tls_1_3_ciphersuites.c_str(),
        tls_config.signature_algorithms.empty() ? "(system-wide)"
            : tls_config.signature_algorithms.c_str()
    );

    auto to_version = [](uint32_t level) {
        return level == 0 ? 0
             : level == 1 ? TLS1_1_VERSION
             : level == 2 ? TLS1_2_VERSION
             : TLS1_3_VERSION;
    };

#define CHECK_CALL(func, ...) do { if (!func(__VA_ARGS__)) { \
        return #func;                                        \
    } } while (0)

    /*
     * This is necessary, because the Microsoft TLS implementation is not perfect.
     * SSL_OP_ALL enables a couple of workarounds for buggy TLS implementations,
     * but the most important workaround being SSL_OP_TLS_BLOCK_PADDING_BUG.
     * As the size of the encrypted payload may give hints about its contents,
     * block padding is normally used, but the Microsoft TLS implementation
     * won't recognize it and will disconnect you after sending a TLS alert.
     */
    SSL_CTX_set_options(ctx, SSL_OP_ALL);
    if (tls_config.enable_legacy_server_connect) {
        SSL_CTX_set_options(ctx, SSL_OP_LEGACY_SERVER_CONNECT);
    }

    CHECK_CALL(SSL_CTX_set_min_proto_version, ctx, to_version(tls_config.min_level));
    if (tls_config.max_level) {
        CHECK_CALL(SSL_CTX_set_max_proto_version, ctx, to_version(tls_config.max_level));
    }

    // https://www.openssl.org/docs/man1.1.1/man3/SSL_CTX_set_ciphersuites.html

    // when not defined, use system default
    // "DEFAULT@SECLEVEL=1" (<= openssl-1.1)
    // "DEFAULT@SECLEVEL=2" (>= openssl-2)
    if (not tls_config.cipher_list.empty()) {
        // Not compatible with MSTSC 6.1 on XP and W2K3
        // SSL_CTX_set_cipher_list(ctx, "HIGH:!ADH:!3DES");
        CHECK_CALL(SSL_CTX_set_cipher_list, ctx, tls_config.cipher_list.c_str());
    }

    if (not tls_config.key_exchange_groups.empty()) {
        CHECK_CALL(SSL_CTX_set1_groups_list, ctx,
            const_cast<char*>(tls_config.key_exchange_groups.c_str()));
    }

    // when not defined, use system default
    if (not tls_config.tls_1_3_ciphersuites.empty()) {
        CHECK_CALL(SSL_CTX_set_ciphersuites, ctx, tls_config.tls_1_3_ciphersuites.c_str());
    }

    // when not defined, use system default
    if (not tls_config.signature_algorithms.empty()) {
        REDEMPTION_DIAGNOSTIC_PUSH()
        // SSL_CTX_set1_sigalgs_list is a macro casting to char*
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wcast-qual")
        CHECK_CALL(SSL_CTX_set1_sigalgs_list, ctx, tls_config.signature_algorithms.c_str());
        REDEMPTION_DIAGNOSTIC_POP()
    }

#undef CHECK_CALL

    return nullptr;
}

inline void log_cipher_list(SSL* ssl, char const* origin)
{
    int priority = 0;

    while(const char * cipher_name = SSL_get_cipher_list(ssl, priority)) {
        priority++;
        LOG(LOG_INFO, "TLSContext::%s cipher %d: %s", origin, priority, cipher_name);
    }

    if (priority == 0) {
        LOG(LOG_INFO, "TLSContext::%s cipher list empty", origin);
    }
}

/**
 * @brief all the context needed to manipulate TLS context for a TLS object
 */
class TLSContext
{
    SSL_CTX * allocated_ctx = nullptr;
    SSL     * allocated_ssl = nullptr;
    SSL     * io = nullptr;
    std::unique_ptr<uint8_t[]> public_key;
    size_t public_key_length = 0;

    struct X509_deleter
    {
        void operator()(X509* px509) noexcept
        {
            X509_free(px509);
        }
    };
    using X509UniquePtr = std::unique_ptr<X509, X509_deleter>;
    X509UniquePtr cert_external_validation_wait_ctx;

    bool verbose;

public:
    TLSContext(bool verbose = false)
    : verbose(verbose)
    {}

    ~TLSContext()
    {
        if (this->allocated_ssl) {
            //SSL_shutdown(this->allocated_ssl);
            SSL_free(this->allocated_ssl);
        }

        if (this->allocated_ctx) {
            SSL_CTX_free(this->allocated_ctx);
        }
    }

    [[nodiscard]] int pending_data() const
    {
        return SSL_pending(this->allocated_ssl);
    }

    [[nodiscard]] u8_array_view get_public_key() const noexcept
    {
        return {this->public_key.get(), this->public_key_length};
    }

    bool enable_client_tls_start(int sck, TlsConfig const& tls_config)
    {
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());

        if (ctx == nullptr) {
            return tls_ctx_print_error("enable_client_tls", "SSL_CTX_new returned NULL");
        }

        this->allocated_ctx = ctx;
        SSL_CTX_set_mode(ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER/* | SSL_MODE_ENABLE_PARTIAL_WRITE*/);

        if (auto* funcname = apply_tls_config(ctx, tls_config, verbose, "enable_client_tls")) {
            return tls_ctx_print_error("enable_client_tls", funcname);
        }

        SSL* ssl = SSL_new(ctx);

        if (ssl == nullptr) {
            return tls_ctx_print_error("enable_client_tls", "SSL_new returned NULL");
        }

        this->allocated_ssl = ssl;

        if (0 == SSL_set_fd(ssl, sck)) {
            return tls_ctx_print_error("enable_client_tls", "SSL_set_fd failed");
        }

        if (tls_config.show_common_cipher_list){
            log_cipher_list(this->allocated_ssl, "Client");
        }

        return true;
    }

    Transport::TlsResult enable_client_tls_loop()
    {
        int const connection_status = SSL_connect(this->allocated_ssl);
        if (connection_status <= 0) {
            char const* error_msg;
            int errnum = 0;
            switch (SSL_get_error(this->allocated_ssl, connection_status))
            {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    return Transport::TlsResult::Want;
                case SSL_ERROR_ZERO_RETURN:
                    error_msg = "Server closed TLS connection";
                    break;
                case SSL_ERROR_SYSCALL:
                    error_msg = "I/O error";
                    errnum = errno;
                    break;
                case SSL_ERROR_SSL:
                    error_msg = "Failure in SSL library (protocol error?)";
                    break;
                default:
                    error_msg = "Unknown error";
                    break;
            }
            tls_ctx_print_error("enable_client_tls", error_msg);
            if (errnum) {
                LOG(LOG_ERR, "TLSContext::enable_client_tls errno=%d %s",
                    errnum, strerror(errnum));
            }
            return Transport::TlsResult::Fail;
        }

        return Transport::TlsResult::Ok;
    }

private:
    bool extract_public_key(X509* px509)
    {
        LOG(LOG_INFO, "TLSContext::X509_get_pubkey()");

        // extract the public key
        EVP_PKEY* pkey = X509_get_pubkey(px509);
        if (!pkey) {
            return false;
        }

        LOG(LOG_INFO, "TLSContext::i2d_PublicKey()");

        // i2d_X509() encodes the structure pointed to by x into DER format.
        // If out is not nullptr is writes the DER encoded data to the buffer at *out,
        // and increments it to point after the data just written.
        // If the return value is negative an error occurred, otherwise it returns
        // the length of the encoded data.

        // export the public key to DER format
        this->public_key_length = checked_int(i2d_PublicKey(pkey, nullptr));
        this->public_key = std::make_unique<uint8_t[]>(this->public_key_length);
        // hexdump_c(this->public_key, this->public_key_length);

        uint8_t * tmp = this->public_key.get();
        i2d_PublicKey(pkey, &tmp);

        EVP_PKEY_free(pkey);

        return true;
    }

    Transport::TlsResult final_check_certificate(X509& x509)
    {
        if (!extract_public_key(&x509)) {
            LOG(LOG_WARNING, "TLSContext::crypto_cert_get_public_key: X509_get_pubkey() failed");
            return Transport::TlsResult::Fail;
        }

        this->io = this->allocated_ssl;

        // LOG(LOG_INFO, "TLSContext::enable_client_tls() done");
        LOG(LOG_INFO, "Connected to target using TLS version %s", SSL_get_version(this->allocated_ssl));

        return Transport::TlsResult::Ok;
    }

public:
    Transport::TlsResult certificate_external_validation(
        Transport::CertificateChecker certificate_chercker,
        const char* ip_address,
        int port)
    {
        // local scope for exception and destruction
        std::unique_ptr px509 = std::exchange(this->cert_external_validation_wait_ctx, nullptr);
        switch (certificate_chercker(px509.get(), ip_address, port))
        {
            case CertificateResult::Wait:
                this->cert_external_validation_wait_ctx = std::move(px509);
                return Transport::TlsResult::WaitExternalEvent;
            case CertificateResult::Valid:
                return this->final_check_certificate(*px509);
            case CertificateResult::Invalid:
                LOG(LOG_WARNING, "server_cert_callback() failed");
                return Transport::TlsResult::Fail;
        }

        REDEMPTION_UNREACHABLE();
    }

    Transport::TlsResult check_certificate(
        Transport::CertificateChecker certificate_chercker,
        const char* ip_address,
        int port,
        bool anon_tls)
    {
        if (anon_tls) {
            /* anonymous TLS doesn't have any certificate, so let's just return OK */
            this->io = this->allocated_ssl;
            return Transport::TlsResult::Ok;
        }

        LOG(LOG_INFO, "SSL_get_peer_certificate()");

        // SSL_get_peer_certificate - get the X509 certificate of the peer
        // ---------------------------------------------------------------

        // SSL_get_peer_certificate() returns a pointer to the X509 certificate
        // the peer presented. If the peer did not present a certificate, nullptr
        // is returned.

        // Due to the protocol definition, a TLS/SSL server will always send a
        // certificate, if present. A client will only send a certificate when
        // explicitly requested to do so by the server (see SSL_CTX_set_verify(3)).
        // If an anonymous cipher is used, no certificates are sent.

        // That a certificate is returned does not indicate information about the
        // verification state, use SSL_get_verify_result(3) to check the verification
        // state.

        // The reference count of the X509 object is incremented by one, so that
        // it will not be destroyed when the session containing the peer certificate
        // is freed. The X509 object must be explicitly freed using X509_free().

        // RETURN VALUES The following return values can occur:

        // nullptr : no certificate was presented by the peer or no connection was established.
        // Pointer to an X509 certificate : the return value points to the certificate
        // presented by the peer.

        X509 * px509 = SSL_get_peer_certificate(this->allocated_ssl);
        LOG_IF(!px509, LOG_WARNING, "SSL_get_peer_certificate() failed");

        // TODO use SSL_get0_peer_certificate and remove X509UniquePtr call
        X509UniquePtr x509_uptr{px509};

        switch (certificate_chercker(px509, ip_address, port))
        {
            case CertificateResult::Wait:
                this->cert_external_validation_wait_ctx = std::move(x509_uptr);
                return Transport::TlsResult::WaitExternalEvent;
            case CertificateResult::Valid:
                return this->final_check_certificate(*px509);
            case CertificateResult::Invalid:
                LOG(LOG_WARNING, "server_cert_callback() failed");
                return Transport::TlsResult::Fail;
        }

        return Transport::TlsResult::Fail;
    }

    bool enable_server_tls_start(int sck, const char * certificate_password, TlsConfig const& tls_config)
    {
        SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());

        if (!ctx) {
            return tls_ctx_print_error("enable_server_tls", "SSL_CTX_new returned NULL");
        }

        this->allocated_ctx = ctx;
        SSL_CTX_set_mode(ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER/* | SSL_MODE_ENABLE_PARTIAL_WRITE*/);

        if (auto* funcname = apply_tls_config(ctx, tls_config, verbose, "enable_server_tls")) {
            return tls_ctx_print_error("enable_server_tls", funcname);
        }

        // -------- End of system wide SSL_Ctx option ----------------------------------

        // --------Start of session specific init code ---------------------------------

        /* Load our keys and certificates*/
        if (!SSL_CTX_use_certificate_chain_file(ctx, app_path(AppPath::CfgCrt))) {
            return tls_ctx_print_error("enable_server_tls", "Can't read certificate file");
        }

        SSL_CTX_set_default_passwd_cb(
            ctx, [](char *buf, int num, int rwflag, void *userdata) {
                (void)rwflag;
                const char * pass = static_cast<const char*>(userdata);
                size_t pass_len = strlen(pass);

                // buffer too small, password is ignored
                if(num < static_cast<int>(pass_len+1u)) {
                    pass_len = 0;
                }

                memcpy(buf, pass, pass_len);
                buf[pass_len] = 0;
                return int(pass_len);
            }
        );
        SSL_CTX_set_default_passwd_cb_userdata(ctx, const_cast<char*>(certificate_password)); /*NOLINT*/

        if (!SSL_CTX_use_PrivateKey_file(ctx, app_path(AppPath::CfgKey), SSL_FILETYPE_PEM)) {
            return tls_ctx_print_error("enable_server_tls", "Can't read key file");
        }

        BIO* bio = BIO_new_file(app_path(AppPath::CfgDhPem), "r");
        if (!bio){
            return tls_ctx_print_error("enable_server_tls", "Couldn't open DH file");
        }

#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER >= 3
        auto dh_read = [](BIO* bio){ return PEM_read_bio_Parameters(bio, nullptr); };
        auto dh_free = [](EVP_PKEY* pkey){ EVP_PKEY_free(pkey); };
        auto dh_use = [](EVP_PKEY* /*pkey*/){ };
        auto set_dh = [](SSL_CTX* ctx, EVP_PKEY* pkey){ return SSL_CTX_set0_tmp_dh_pkey(ctx, pkey); };
#else
        auto dh_read = [](BIO* bio){ return PEM_read_bio_DHparams(bio, nullptr, nullptr, nullptr); };
        auto dh_free = [](DH* dh){ DH_free(dh); };
        auto dh_use = [](DH* dh){ DH_free(dh); };
        auto set_dh = [](SSL_CTX* ctx, DH* dh){ return SSL_CTX_set_tmp_dh(ctx, dh); /*NOLINT*/ };
#endif
        auto* dh = dh_read(bio);
        BIO_free(bio);
        if (!dh) {
            return tls_ctx_print_error("enable_server_tls", "Can't read DH parameters");
        }

        if (!set_dh(ctx, dh)) {
            dh_free(dh);
            return tls_ctx_print_error("enable_server_tls", "Couldn't set DH parameters");
        }
        dh_use(dh);


        BIO* sbio = BIO_new_socket(sck, BIO_NOCLOSE);
        if (bio == nullptr){
            return tls_ctx_print_error("enable_server_tls", "Couldn't open socket");
        }

        this->allocated_ssl = SSL_new(ctx);

        if (!extract_public_key(SSL_get_certificate(this->allocated_ssl))) {
            tls_ctx_print_error("X509_get_pubkey()", "failed");
            BIO_free(sbio);
            return false;
        }

        SSL_set_bio(this->allocated_ssl, sbio, sbio);

        return true;
    }

    Transport::TlsResult enable_server_tls_loop(bool show_common_cipher_list)
    {
        int r = SSL_accept(this->allocated_ssl);
        if(r <= 0) {
            char const* error_msg;
            int errnum = 0;
            switch (SSL_get_error(this->allocated_ssl, r))
            {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    return Transport::TlsResult::Want;
                case SSL_ERROR_ZERO_RETURN:
                    error_msg = "Client closed TLS connection";
                    break;
                case SSL_ERROR_SYSCALL:
                    error_msg = "I/O error";
                    errnum = errno;
                    break;
                case SSL_ERROR_SSL:
                    error_msg = "Failure in SSL library (protocol error?)";
                    break;
                default:
                    error_msg = "Unknown error";
                    break;
            }
            tls_ctx_print_error("enable_server_tls", error_msg);
            if (errnum) {
                LOG(LOG_ERR, "TLSContext::enable_server_tls errno=%d %s",
                    errnum, strerror(errnum));
            }
            return Transport::TlsResult::Fail;
        }

        this->io = this->allocated_ssl;

        LOG(LOG_INFO, "Incoming connection to Bastion using TLS version %s", SSL_get_version(this->allocated_ssl));

        if (show_common_cipher_list) {
            log_cipher_list(this->allocated_ssl, "Server");
        }

        LOG(LOG_INFO, "TLSContext::Negotiated cipher used %s", SSL_CIPHER_get_name(SSL_get_current_cipher(this->allocated_ssl)));

        return Transport::TlsResult::Ok;
    }

    ssize_t privpartial_recv_tls(uint8_t * data, size_t len)
    {
        for (;;) {
            int rcvd = ::SSL_read(this->io, data, len);
            if (rcvd > 0) {
                return rcvd;
            }
            unsigned long error = SSL_get_error(this->io, rcvd);
            switch (error) {
                case SSL_ERROR_NONE:
                    LOG(LOG_INFO, "recv_tls SSL_ERROR_NONE");
                    return rcvd;

                case SSL_ERROR_WANT_WRITE:
                    LOG(LOG_INFO, "recv_tls WANT WRITE");
                    return 0;

                case SSL_ERROR_WANT_READ:
                    // LOG(LOG_INFO, "recv_tls WANT READ");
                    return 0;

                case SSL_ERROR_WANT_CONNECT:
                    LOG(LOG_INFO, "recv_tls WANT CONNECT");
                    continue;

                case SSL_ERROR_WANT_ACCEPT:
                    LOG(LOG_INFO, "recv_tls WANT ACCEPT");
                    continue;

                case SSL_ERROR_WANT_X509_LOOKUP:
                    LOG(LOG_INFO, "recv_tls WANT X509 LOOKUP");
                    continue;

                case SSL_ERROR_ZERO_RETURN:
                    // Other side closed TLS connection
                    return -1;

                default:
                {
                    do {
                        LOG(LOG_INFO, "partial_recv_tls %s", ERR_error_string(error, nullptr));
                    } while ((error = ERR_get_error()) != 0);

                    // TODO if recv fail with partial read we should return the amount of data received, close socket and store some delayed error value that will be sent back next call
                    // TODO replace this with actual error management, EOF is not even an option for sockets
                    // TODO Manage actual errors, check possible values
                    return -1;
                }
            }
        }
    }

    ssize_t privpartial_send_tls(const uint8_t * data, size_t len)
    {
        const uint8_t * const buffer = data;
        size_t remaining_len = len;
        for (;;){
            int ret = SSL_write(this->io, buffer, remaining_len);
            if (ret > 0) {
                return ret;
            }
            unsigned long error = SSL_get_error(this->io, ret);
            switch (error)
            {
                case SSL_ERROR_NONE:
                    return ret;

                case SSL_ERROR_WANT_READ:
                    LOG_IF(this->verbose, LOG_INFO, "send_tls WANT READ");
                    return 0;

                case SSL_ERROR_WANT_WRITE:
                    LOG_IF(this->verbose, LOG_INFO, "send_tls WANT WRITE");
                    return 0;

                default:
                {
                    LOG(LOG_INFO, "Failure in SSL library, error=%lu, %s [%d]", error, strerror(errno), errno);
                    do {
                        LOG(LOG_INFO, "partial_send_tls %s", ERR_error_string(error, nullptr));
                    } while ((error = ERR_get_error()) != 0);
                    return -1;
                }
            }
        }
    }

    ssize_t privsend_tls(const uint8_t * data, size_t len)
    {
        const uint8_t * const buffer = data;
        size_t remaining_len = len;
        size_t offset = 0;
        while (remaining_len > 0){
            int ret = SSL_write(this->io, buffer + offset, remaining_len);

            unsigned long error = SSL_get_error(this->io, ret);
            switch (error)
            {
                case SSL_ERROR_NONE:
                    remaining_len -= ret;
                    offset += ret;
                    break;

                case SSL_ERROR_WANT_READ:
                    LOG_IF(this->verbose, LOG_INFO, "send_tls WANT READ");
                    continue;

                case SSL_ERROR_WANT_WRITE:
                    LOG_IF(this->verbose, LOG_INFO, "send_tls WANT WRITE");
                    continue;

                default:
                {
                    LOG(LOG_INFO, "Failure in SSL library, error=%lu, %s [%d]", error, strerror(errno), errno);
                    do {
                        LOG(LOG_INFO, "send_tls %s", ERR_error_string(error, nullptr));
                    } while ((error = ERR_get_error()) != 0);
                    return -1;
                }
            }
        }
        return len;
    }

};

REDEMPTION_DIAGNOSTIC_POP()
