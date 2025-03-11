/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/protop/protop_utf16.hpp"
#include "utils/mathutils.hpp"

// TODO use utf16 to utf8 (and optimize)
const char * protop_fmt::init_utf16_to_utf8(
    char * utf8_buf, std::size_t utf8_buf_len,
    bytes_view utf16_str) noexcept
{
    auto n = mmin(utf8_buf_len * 2 - 2 /* null terminated */, utf16_str.size()) / 2;
    std::size_t i = 0;

    // convert chars 4 by 4
    while (n - i >= 4)
    {
        uint64_t c;
        memcpy(&c, utf16_str.data() + i * 2, 8);
        static_assert(
            std::endian::native == std::endian::little
         || std::endian::native == std::endian::big);
        constexpr uint64_t mask
            = std::endian::native == std::endian::little
            ? 0xFF80FF80FF80FF80
            : 0x80FF80FF80FF80FF;
        if (!(c & mask))
        {
            utf8_buf[i + 0] = static_cast<char>(utf16_str[i * 2 + 0]);
            utf8_buf[i + 1] = static_cast<char>(utf16_str[i * 2 + 2]);
            utf8_buf[i + 2] = static_cast<char>(utf16_str[i * 2 + 4]);
            utf8_buf[i + 3] = static_cast<char>(utf16_str[i * 2 + 6]);
            i += 4;
        }
        else
        {
            break;
        }
    }

    for (; i < n; ++i)
    {
        utf8_buf[i] = (!utf16_str[i * 2 + 1] && utf16_str[i * 2] < 128)
            ? static_cast<char>(utf16_str[i * 2])
            : '?'
        ;
    }

    utf8_buf[n] = 0;
    return utf8_buf;
}
