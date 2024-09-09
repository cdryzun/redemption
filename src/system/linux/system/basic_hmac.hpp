/*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*   Product name: redemption, a FLOSS RDP proxy
*   Copyright (C) Wallix 2010-2016
*   Author(s): Jonathan Poelen
*/

#pragma once

#include "core/error.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/sugar/bounded_array_view.hpp"

#include <cstdint>
#include <cstring>

#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace detail_
{

struct HMACWrap
{
# if OPENSSL_VERSION_NUMBER < 0x10100000L
    using out_size_t = unsigned;
    int init(EVP_MD const * evp, bytes_view key)
    {
        this->hmac = HMAC_CTX_new();
        return HMAC_Init_ex(this->hmac, key.as_u8p(), key.size(), evp, nullptr);
    }
    void deinit() { HMAC_CTX_cleanup(&this->hmac); }
    int update(bytes_view data) { return HMAC_Update(this->hmac, data.as_u8p(), data.size()); }
    int final(unsigned char *out, out_size_t *outl, size_t /*outsize*/) { return HMAC_Final(this->hmac, out, outl); }
    HMAC_CTX * operator->() noexcept { return &this->hmac; }
    operator HMAC_CTX * () noexcept { return &this->hmac; }

private:
    HMAC_CTX hmac;

# elif OPENSSL_VERSION_NUMBER < 0x30000000L
    using out_size_t = unsigned;
    int init(EVP_MD const * evp, bytes_view key)
    {
        this->hmac = HMAC_CTX_new();
        return HMAC_Init_ex(this->hmac, key.as_u8p(), key.size(), evp, nullptr);
    }
    void deinit() { HMAC_CTX_free(this->hmac); }
    int update(bytes_view data) { return HMAC_Update(this->hmac, data.as_u8p(), data.size()); }
    int final(unsigned char *out, out_size_t *outl, size_t /*outsize*/) { return HMAC_Final(this->hmac, out, outl); }
    HMAC_CTX * operator->() noexcept { return this->hmac; }
    operator HMAC_CTX * () noexcept { return this->hmac; }

private:
    HMAC_CTX * hmac = nullptr;

# else
    using out_size_t = size_t;
    int init(EVP_MD const * evp, bytes_view key)
    {
        auto mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
        this->hmac = EVP_MAC_CTX_new(mac);
        EVP_MAC_free(mac);
        OSSL_PARAM params[2];
        params[0] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>(EVP_MD_name(evp)), 0);
        params[1] = OSSL_PARAM_construct_end();
        return EVP_MAC_init(this->hmac, key.as_u8p(), key.size(), params);
    }
    void deinit() { EVP_MAC_CTX_free(this->hmac); }
    int update(bytes_view data) { return EVP_MAC_update(this->hmac, data.as_u8p(), data.size()); }
    int final(unsigned char *out, out_size_t *outl, size_t outsize) { return EVP_MAC_final(this->hmac, out, outl, outsize); }
    EVP_MAC_CTX * operator->() noexcept { return this->hmac; }
    operator EVP_MAC_CTX * () noexcept { return this->hmac; }

private:
    EVP_MAC_CTX * hmac = nullptr;

# endif
};

template<const EVP_MD * (* evp)(), std::size_t DigestLength>
class basic_HMAC
{
    HMACWrap hmac;

public:
    basic_HMAC(bytes_view key)
    {
        if (!this->hmac.init(evp(), key)) {
            throw Error(ERR_SSL_CALL_HMAC_INIT_FAILED);
        }
    }

    ~basic_HMAC()
    {
        this->hmac.deinit();
    }

    void update(bytes_view data)
    {
        if (!this->hmac.update(data)) {
            throw Error(ERR_SSL_CALL_HMAC_UPDATE_FAILED);
        }
    }

    void final(sized_writable_u8_array_view<DigestLength> out_data)
    {
        HMACWrap::out_size_t len = 0;

        if (!this->hmac.final(out_data.data(), &len, DigestLength)) {
            throw Error(ERR_SSL_CALL_HMAC_FINAL_FAILED);
        }
        assert(len == DigestLength);
    }
};

template<const EVP_MD * (* evp)(), std::size_t DigestLength>
class DelayedHMAC
{
    bool initialized = false;
    HMACWrap hmac;

public:
    DelayedHMAC() = default;

    void init(bytes_view key)
    {
        if (this->initialized){
            throw Error(ERR_SSL_CALL_HMAC_INIT_FAILED);
        }

        if (!this->hmac.init(evp(), key)) {
            throw Error(ERR_SSL_CALL_HMAC_INIT_FAILED);
        }
        this->initialized = true;
    }

    ~DelayedHMAC()
    {
        if (this->initialized){
            this->hmac.deinit();
        }
    }

    void update(bytes_view data)
    {
        if (!this->initialized){
            throw Error(ERR_SSL_CALL_HMAC_UPDATE_FAILED);
        }

        if (!this->hmac.update(data)) {
            throw Error(ERR_SSL_CALL_HMAC_UPDATE_FAILED);
        }
    }

    void final(sized_writable_u8_array_view<DigestLength> out_data)
    {
        if (!this->initialized){
            throw Error(ERR_SSL_CALL_HMAC_FINAL_FAILED);
        }
        HMACWrap::out_size_t len = 0;

        if (!this->hmac.final(out_data.data(), &len, DigestLength)) {
            throw Error(ERR_SSL_CALL_HMAC_FINAL_FAILED);
        }
        assert(len == DigestLength);
        this->hmac.deinit();
        this->initialized = false;
    }
};

}  // namespace detail_
