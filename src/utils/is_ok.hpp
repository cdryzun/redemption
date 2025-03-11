/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace detail
{
    template<class T>
    struct unspecialized_is_ok_v
    {};
}

template<class T>
inline constexpr auto is_ok_v = detail::unspecialized_is_ok_v<T>::value;

template<class T> inline constexpr auto is_ok_v<T &> = is_ok_v<T>;
template<class T> inline constexpr auto is_ok_v<T &&> = is_ok_v<T>;
template<class T> inline constexpr auto is_ok_v<T const> = is_ok_v<T>;

template<class T>
bool is_ok(T && value) noexcept
{
    static_assert(noexcept(value == is_ok_v<T>));
    return value == is_ok_v<T>;
}

template<class T>
bool is_err(T && value) noexcept
{
    return !is_ok(value);
}
