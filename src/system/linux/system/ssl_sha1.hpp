/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2013
   Author(s): Christophe Grosjean, Meng Tan
*/

#pragma once

#include "system/basic_hmac.hpp"

#include <cstdint>
#include <cstring>
#include <cassert>

#include <openssl/sha.h>

# if OPENSSL_VERSION_NUMBER >= 0x30000000L

#include "core/error.hpp"
#include "utils/sugar/movable_ptr.hpp"

#include <openssl/evp.h>

class SslSha1
{
    movable_ptr<EVP_MD_CTX> sha1;

public:
    enum : unsigned { DIGEST_LENGTH = SHA_DIGEST_LENGTH };

    SslSha1()
    {
        sha1 = EVP_MD_CTX_new();

        if (!sha1) {
            throw Error(ERR_SSL_CALL_SHA1_INIT_FAILED);
        }

        const EVP_MD* sha1_md = EVP_sha1();

        if (EVP_DigestInit_ex(sha1, sha1_md, nullptr) != 1) {
            EVP_MD_CTX_free(sha1);
            throw Error(ERR_SSL_CALL_SHA1_INIT_FAILED);
        }
    }

    ~SslSha1()
    {
        EVP_MD_CTX_free(sha1);
    }
    void update(bytes_view data)
    {
        if (EVP_DigestUpdate(sha1, data.as_u8p(), data.size()) != 1) {
            throw Error(ERR_SSL_CALL_SHA1_UPDATE_FAILED);
        }
    }

    void final(sized_writable_u8_array_view<DIGEST_LENGTH> out_data)
    {
        unsigned int out_len = DIGEST_LENGTH;
        if (EVP_DigestFinal_ex(sha1, out_data.data(), &out_len) != 1) {
            throw Error(ERR_SSL_CALL_SHA1_FINAL_FAILED);
        }
        assert(out_len == DIGEST_LENGTH);
    }
};

# else

class SslSha1
{
    SHA_CTX sha1;

public:
    enum : unsigned { DIGEST_LENGTH = SHA_DIGEST_LENGTH };

    SslSha1()
    {
        if (0 == SHA1_Init(&this->sha1)){
            throw Error(ERR_SSL_CALL_SHA1_INIT_FAILED);
        }
    }

    void update(bytes_view data)
    {
        if (0 == SHA1_Update(&this->sha1, data.as_u8p(), data.size())){
            throw Error(ERR_SSL_CALL_SHA1_UPDATE_FAILED);
        }
    }

    void final(sized_writable_u8_array_view<DIGEST_LENGTH> out_data)
    {
        if (0 == SHA1_Final(out_data.data(), &this->sha1)){
            throw Error(ERR_SSL_CALL_SHA1_FINAL_FAILED);
        }
    }
};

# endif

using SslHMAC_Sha1 = detail_::basic_HMAC<&EVP_sha1, SslSha1::DIGEST_LENGTH>;
