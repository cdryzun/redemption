/*
    This program is free software; you can redistribute it and/or modify it
     under the terms of the GNU General Public License as published by the
     Free Software Foundation; either version 2 of the License, or (at your
     option) any later version.

    This program is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
     Public License for more details.

    You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     675 Mass Ave, Cambridge, MA 02139, USA.

    Product name: redemption, a FLOSS RDP proxy
    Copyright (C) Wallix 2014
    Author(s): Christophe Grosjean, Raphael Zhou, Jonathan Poelen
*/

#include "utils/strutils.hpp"

#include <cstring>
#include <algorithm>
#include <functional>

namespace utils
{

std::size_t strlcpy(char* dest, chars_view src, std::size_t buflen) noexcept
{
    auto const nsrc = src.size();
    if (buflen) {
        auto const n = std::min(buflen-1u, nsrc);
        std::memcpy(dest, src.data(), n);
        dest[n] = 0;
    }
    return nsrc;
}

std::size_t strlcpy(char* dest, char const* src, std::size_t buflen) noexcept
{
    return strlcpy(dest, chars_view{src, strlen(src)}, buflen);
}


void str_replace_inplace(std::string& str, chars_view pattern, chars_view replacement)
{
    assert(!pattern.empty());

    std::boyer_moore_searcher searcher(pattern.begin(), pattern.end());

    std::ptrdiff_t i = 0;

    for (;;) {
        auto rng = searcher(str.begin() + i, str.end());
        if (rng.first != rng.second) {
            i = rng.first - str.begin();
            i += std::ptrdiff_t(replacement.size());
            str.replace(rng.first, rng.second, replacement.begin(), replacement.end());
        }
        else {
            break;
        }
    }
}

StrReplaceBetweenPattern::StrReplaceBetweenPattern(
    chars_view str, char pattern, chars_view replacement)
{
    char const* p1 = strchr(str, pattern);
    if (!p1) {
        storage_view = str.data();
        storage_len = str.size();
        return ;
    }

    char const* p2 = strchr(str.after(p1), pattern);
    if (!p2) {
        storage_view = str.data();
        storage_len = str.size();
        return ;
    }

    // compute final string len
    storage_len = str.size() - static_cast<std::size_t>(p2 - p1) - 1;
    char const* last_p = p2;
    for (;;) {
        storage_len += replacement.size();
        if (char const* a = strchr(str.after(last_p), pattern)) {
            if (char const* b = strchr(str.after(a), pattern)) {
                last_p = b;
                storage_len -= static_cast<std::size_t>(b - a + 1);
                continue;
            }
        }
        break;
    }

    // ini buffer
    storage_owned = static_cast<char*>(operator new(storage_len));
    storage_view = storage_owned;
    char* dest = storage_owned;
    auto push = [&dest](chars_view s){
        memcpy(dest, s.data(), s.size());
        dest += s.size();
    };
    for (;;) {
        push(str.before(p1));
        push(replacement);
        str = str.after(p2);
        if (last_p == p2) {
            break;
        }

        p1 = strchr(str, pattern);
        p2 = strchr(str.after(p1), pattern);
    }
    push(str);
}

} // namespace utils
