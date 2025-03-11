/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bytes_view.hpp"

#include <cassert>
#include <cstring>


/// \return src.size()
inline size_t bytes_copy(writable_bytes_view dest, bytes_view src) noexcept
{
    assert(dest.size() >= src.size());
    std::memcpy(dest.data(), src.data(), src.size());
    return src.size();
}

/// \return dest + src.size()
inline char * bytes_copy(char * dest, bytes_view src) noexcept
{
    std::memcpy(dest, src.data(), src.size());
    return dest + src.size();
}

/// \return dest + src.size()
inline uint8_t * bytes_copy(uint8_t * dest, bytes_view src) noexcept
{
    std::memcpy(dest, src.data(), src.size());
    return dest + src.size();
}

/// \return dest + src.size()
inline char * bytes_copy_and_advance(char * dest, bytes_view src) noexcept
{
    std::memcpy(dest, src.data(), src.size());
    return dest + src.size();
}

/// \return dest + src.size()
inline uint8_t * bytes_copy_and_advance(uint8_t * dest, bytes_view src) noexcept
{
    std::memcpy(dest, src.data(), src.size());
    return dest + src.size();
}

/// \return dest.drop_front(src.size())
inline writable_bytes_view
bytes_copy_and_advance(writable_bytes_view dest, bytes_view src) noexcept
{
    assert(dest.size() >= src.size());
    std::memcpy(dest.data(), src.data(), src.size());
    return dest.drop_front(src.size());
}


/// \return src.size()
inline size_t bytes_move(writable_bytes_view dest, bytes_view src) noexcept
{
    assert(dest.size() >= src.size());
    std::memmove(dest.data(), src.data(), src.size());
    return src.size();
}

/// \return dest + src.size()
inline char * bytes_move_and_advance(char * dest, bytes_view src) noexcept
{
    std::memmove(dest, src.data(), src.size());
    return dest + src.size();
}

/// \return dest + src.size()
inline uint8_t * bytes_move_and_advance(uint8_t * dest, bytes_view src) noexcept
{
    std::memmove(dest, src.data(), src.size());
    return dest + src.size();
}

/// \return dest.drop_front(src.size())
inline writable_bytes_view
bytes_move_and_advance(writable_bytes_view dest, bytes_view src) noexcept
{
    assert(dest.size() >= src.size());
    std::memmove(dest.data(), src.data(), src.size());
    return dest.drop_front(src.size());
}

/// \return dest + src.size()
inline char * unchecked_bytes_copy_and_advance(char * dest, bytes_view src) noexcept
{
    std::memcpy(dest, src.data(), src.size());
    return dest + src.size();
}
