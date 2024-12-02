/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "translation/trkey.hpp"

#include <cstdio>

namespace detail
{
    constexpr std::size_t trkey_start_counter = __COUNTER__;
}

namespace trkeys
{

#define TR_KV_PLURAL_FMT(name, msg, plural_msg)             \
    struct TrKeyPluralFmt##_##name                          \
    {                                                       \
        template<class... Ts>                               \
        static auto check_printf_result(                    \
            char* s, std::size_t n, Ts... xs                \
        ) {                                                 \
            (void)std::snprintf(s, n, msg, xs...);          \
            (void)std::snprintf(s, n, plural_msg, xs...);   \
            return unsigned();                              \
        }                                                   \
    };                                                      \
    constexpr TrKeyPluralFmt<TrKeyPluralFmt##_##name> name{ \
        __COUNTER__ - detail::trkey_start_counter - 1};

#define TR_KV_FMT(name, msg)                       \
    struct TrKeyFmt##_##name                       \
    {                                              \
        template<class... Ts>                      \
        static auto check_printf_result(           \
            char* s, std::size_t n, Ts... xs       \
        ) {                                        \
            (void)std::snprintf(s, n, msg, xs...); \
            return unsigned();                     \
        }                                          \
    };                                             \
    constexpr TrKeyFmt<TrKeyFmt##_##name> name{__COUNTER__ - detail::trkey_start_counter - 1};

#define TR_KV(name, msg) constexpr TrKey name{__COUNTER__ - detail::trkey_start_counter - 1};

#include "translation/trkeys_def.hpp"

#undef TR_KV
#undef TR_KV_FMT
#undef TR_KV_PLURAL_FMT

} // namespace trkeys
