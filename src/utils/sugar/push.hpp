/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <vector>
#include <type_traits>
#include "utils/sugar/bytes_view.hpp"

inline void push(std::vector<uint8_t> & v, bytes_view av)
{
    v.insert(v.end(), av.begin(), av.end());
}

template<class T>
void push(std::vector<T> & v, array_view<std::type_identity_t<T>> av)
{
    v.insert(v.end(), av.begin(), av.end());
}

template<class T>
void push(std::vector<T> & v, std::type_identity_t<T> const & value)
{
    v.push_back(value);
}

template<class T>
void push(std::vector<T> & v, std::type_identity_t<T> && value)
{
    v.push_back(static_cast<T&&>(value));
}
