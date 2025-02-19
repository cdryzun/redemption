/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cxx/cxx.hpp"
#include "utils/sugar/zstring_view.hpp"

#include <cstdio>
#include <cassert>
#include <cstdarg>


REDEMPTION_CXX_ANNOTATION_ATTRIBUTE_GCC_CLANG(__format__ (__printf__, 2, 3))
inline writable_chars_view snprintf_av(writable_chars_view buf, char const* format, ...) noexcept
{
    int res;
    va_list ap;

    va_start(ap, format);
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wformat-nonliteral")
    res = std::vsnprintf(buf.data(), buf.size(), format, ap);
    REDEMPTION_DIAGNOSTIC_POP()
    va_end(ap);

    assert(res >= 0);

    std::size_t n = static_cast<unsigned>(res);
    // when buf is too short
    if (n >= buf.size()) {
        n = buf.size();
        // buf is not empty, skip null terminated char
        if (n) {
            --n;
        }
    }

    return buf.first(n);
}

// N is the buffer size with null character
template<std::size_t N>
struct SNPrintf
{
    static_assert(N > 0);

    REDEMPTION_CXX_ANNOTATION_ATTRIBUTE_GCC_CLANG(__format__ (__printf__, 2, 3))
    SNPrintf(char const* format, ...) noexcept
    {
        int res;
        va_list ap;

        va_start(ap, format);
        REDEMPTION_DIAGNOSTIC_PUSH()
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wformat-nonliteral")
        res = std::vsnprintf(buf, N, format, ap);
        REDEMPTION_DIAGNOSTIC_POP()
        va_end(ap);

        assert(res >= 0);

        n = static_cast<unsigned>(res);
        // ignore null terminated char when buf is too short
        if (n >= N) {
            n = N - 1;
        }
    }

    SNPrintf(SNPrintf const&) = delete;
    SNPrintf operator=(SNPrintf const&) = delete;

    operator chars_view () const
    {
        return str();
    }

    operator zstring_view () const
    {
        return str();
    }

    zstring_view str() const
    {
        return zstring_view::from_null_terminated(buf, n);
    }

private:
    std::size_t n;
    char buf[N];
};
