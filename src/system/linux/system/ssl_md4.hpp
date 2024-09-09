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

#include <openssl/md4.h>

# if 0 && OPENSSL_VERSION_NUMBER >= 0x30000000L

#include "core/error.hpp"
#include "utils/sugar/movable_ptr.hpp"

#include <openssl/evp.h>

class SslMd4
{
    movable_ptr<EVP_MD_CTX> md4;

public:
    enum : unsigned { DIGEST_LENGTH = MD4_DIGEST_LENGTH };

    SslMd4()
    {
        reset();
    }

    ~SslMd4()
    {
        if (md4) {
            EVP_MD_CTX_free(md4);
        }
    }

    void update(bytes_view data)
    {
        if (!md4 || EVP_DigestUpdate(md4, data.as_u8p(), data.size()) != 1) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
    }

    void final(sized_writable_u8_array_view<DIGEST_LENGTH> out_data)
    {
        if (!md4) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
        unsigned int out_len = DIGEST_LENGTH;
        if (EVP_DigestFinal_ex(md4, out_data.data(), &out_len) != 1) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
    }

    void reset()
    {
        if (md4) {
            EVP_MD_CTX_free(md4);
        }
        md4 = EVP_MD_CTX_new();
        if (!md4) {
            throw Error(ERR_SSL_CALL_FAILED);
        }

        const EVP_MD* md4_md = EVP_md4();
        if (EVP_DigestInit_ex(md4, md4_md, nullptr) != 1) {
            EVP_MD_CTX_free(md4);
            throw Error(ERR_SSL_CALL_FAILED);
        }
    }
};

# else


class SslMd4
{
    MD4_CTX md4;

public:
    enum : unsigned { DIGEST_LENGTH = MD4_DIGEST_LENGTH};

    SslMd4()
    {
        MD4_Init(&this->md4);
    }

    void update(bytes_view data)
    {
        MD4_Update(&this->md4, data.as_u8p(), data.size());
    }

    void final(sized_writable_u8_array_view<DIGEST_LENGTH> out_data)
    {
        MD4_Final(out_data.data(), &this->md4);
    }
};

# endif
