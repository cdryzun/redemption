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
   Copyright (C) Wallix 2010-2014
   Author(s): Christophe Grosjean, Javier Caverni, Meng Tan

   openssl headers

   Based on xrdp and rdesktop
   Copyright (C) Jay Sorg 2004-2010
   Copyright (C) Matthew Chapman 1999-2007
*/

#pragma once

#include "utils/sugar/bounded_bytes_view.hpp"

#include <cstdint>
#include <cstring>
#include <cassert>

#include <openssl/rc4.h>

# if 0
//# if OPENSSL_VERSION_NUMBER >= 0x30000000L

#include "core/error.hpp"
#include "utils/sugar/movable_ptr.hpp"

#include <openssl/evp.h>

class SslRC4
{
    movable_ptr<EVP_CIPHER_CTX> rc4;

public:
    SslRC4()
    {
        rc4 = EVP_CIPHER_CTX_new();
        if (!rc4) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
    }

    ~SslRC4()
    {
        if (rc4) {
            EVP_CIPHER_CTX_free(rc4);
        }
    }

    void set_key(sized_bytes_view<8> key)
    {
        if (EVP_CipherInit_ex(rc4, EVP_rc4(), nullptr, key.data(), nullptr, -1) != 1) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
    }

    void set_key(sized_bytes_view<16> key)
    {
        if (EVP_CipherInit_ex(rc4, EVP_rc4(), nullptr, key.data(), nullptr, -1) != 1) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
    }

    void crypt(size_t data_size, const uint8_t * const indata, uint8_t * const outdata)
    {
        int len;
        if (EVP_CipherUpdate(rc4, outdata, &len, indata, data_size) != 1) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
        int final_len;
        if (EVP_CipherFinal_ex(rc4, outdata + len, &final_len) != 1) {
            throw Error(ERR_SSL_CALL_FAILED);
        }
        assert(data_size == final_len + len);
    }
};

# else

class SslRC4
{
    RC4_KEY rc4;

public:
    SslRC4() = default;

    void set_key(sized_bytes_view<8> key)
    {
        RC4_set_key(&this->rc4, key.size(), key.data());
    }

    void set_key(sized_bytes_view<16> key)
    {
        RC4_set_key(&this->rc4, key.size(), key.data());
    }

    void crypt(size_t data_size, const uint8_t * const indata, uint8_t * const outdata)
    {
        RC4(&this->rc4, data_size, indata, outdata);
    }
};

# endif
