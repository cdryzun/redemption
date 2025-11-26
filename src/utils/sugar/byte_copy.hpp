/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bytes_view.hpp"

#include <cstring>


/// \return p + av.size()
inline char* byte_copy(char* p, bytes_view av)
{
    std::memcpy(p, av.data(), av.size());
    return p + av.size();
}

/// \return p + av.size()
inline uint8_t* byte_copy(uint8_t* p, bytes_view av)
{
    std::memcpy(p, av.data(), av.size());
    return p + av.size();
}


/// \return p + av.size()
inline char* byte_move(char* p, bytes_view av)
{
    std::memmove(p, av.data(), av.size());
    return p + av.size();
}

/// \return p + av.size()
inline uint8_t* byte_move(uint8_t* p, bytes_view av)
{
    std::memmove(p, av.data(), av.size());
    return p + av.size();
}

/// \return dest + src.size()
inline char* unchecked_bytes_copy_and_advance(char* dest, bytes_view src) noexcept
{
    std::memcpy(dest, src.data(), src.size());
    return dest + src.size();
}
