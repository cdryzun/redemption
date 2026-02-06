#pragma once

#include <memory>

#include <openssl/x509.h>

struct BIOFree {
    void operator()(BIO* bio) noexcept {
        ::BIO_free(bio);
    }
};
using unique_bio_ptr = std::unique_ptr<BIO, BIOFree>;

struct X509Free {
    void operator()(X509* x509) noexcept {
        ::X509_free(x509);
    }
};
using unique_x509_ptr = std::unique_ptr<X509, X509Free>;

struct X509StackFree {
    void operator()(STACK_OF(X509)* chain) noexcept {
        ::sk_X509_pop_free(chain, X509_free);
    }
};
using unique_x509_chain_ptr=std::unique_ptr<STACK_OF(X509), X509StackFree>;

struct X509StoreFree {
    void operator()(X509_STORE* store) noexcept {
        ::X509_STORE_free(store);
    }
};
using unique_x509_store_ptr = std::unique_ptr<X509_STORE, X509StoreFree>;

struct X509StoreCtxFree {
    void operator()(X509_STORE_CTX* ctx) noexcept {
        ::X509_STORE_CTX_free(ctx);
    }
};
using unique_x509_store_ctx_ptr = std::unique_ptr<X509_STORE_CTX, X509StoreCtxFree>;

struct X509VerifyParamFree {
    void operator()(X509_VERIFY_PARAM* param) noexcept {
        ::X509_VERIFY_PARAM_free(param);
    }
};
using unique_x509_verify_param_ptr = std::unique_ptr<X509_VERIFY_PARAM, X509VerifyParamFree>;