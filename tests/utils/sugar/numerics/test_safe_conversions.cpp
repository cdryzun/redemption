/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/sugar/numerics/safe_conversions.hpp"


template<class T, T value>
class TInt
{};

template<auto value>
using tint_from = TInt<decltype(value), value>;


#define CHECK(expr, expected) tint_from<expected>{} = tint_from<expr>{}

int main()
{
    using s8 = signed char;
    using u8 = unsigned char;

    CHECK(int(saturated_cast<s8>(1233412)), 127);
    CHECK(int(saturated_cast<s8>(-1233412)), -128);
    CHECK(unsigned(saturated_cast<u8>(1233412)), 255u);
    CHECK(unsigned(saturated_cast<u8>(-1233412)), 0u);
    CHECK(saturated_cast<int>(-1233412), -1233412);

    CHECK(checked_cast<char>(12), char{12});
    CHECK(checked_cast<int>(12), 12);

    CHECK(safe_cast<char>(char(12)), char{12});
    CHECK(safe_cast<int>(12), 12);

    CHECK(int(saturated_int<s8>(122312)), 127);
    CHECK(int(saturated_int<s8>(122312) = -3213), -128);

    CHECK(int(checked_int<s8>(12)), 12);
    CHECK(int(checked_int<s8>(12) = 13), 13);

    CHECK(int(safe_int<s8>(s8(12))), 12);
    CHECK(int(safe_int<s8>(s8(12)) = s8(13)), 13);

    enum uE : unsigned char { uMin = 0, uMax = 255 };
    enum sE : signed char { sMin = -128, sMax = 127 };

    CHECK(int(saturated_cast<sE>(1233412)), 127);
    CHECK(int(saturated_cast<sE>(-1233412)), -128);
    CHECK(unsigned(saturated_cast<uE>(1233412)), 255u);
    CHECK(unsigned(saturated_cast<uE>(-1233412)), 0u);

    CHECK(checked_cast<char>(12), char{12});

    CHECK(int(saturated_int<sE>(122312)), 127);
    CHECK(int(saturated_int<sE>(122312) = -3213), -128);

    CHECK(int(checked_int<sE>(12)), 12);
    CHECK(int(checked_int<sE>(12) = 13), 13);

    is_safe_convertible<int, long long>{} = std::true_type{}; CHECK(safe_cast<long long>(1), 1ll);
    is_safe_convertible<long long, int>{} = std::bool_constant<(sizeof(int) == sizeof(long long))>{};
    is_safe_convertible<int, long>{} = std::true_type{}; CHECK(safe_cast<long>(1), 1l);
    is_safe_convertible<long, int>{} = std::bool_constant<(sizeof(int) == sizeof(long))>{};
    is_safe_convertible<signed char, unsigned>{} = std::false_type{};
    is_safe_convertible<unsigned, signed char>{} = std::false_type{};
    is_safe_convertible<sE, signed char>{} = std::true_type{}; CHECK(safe_cast<sE>(s8(1)), sE{1});
    is_safe_convertible<uE, long>{} = std::true_type{}; CHECK(safe_cast<long>(uE{}), long{});
    is_safe_convertible<int, int>{} = std::true_type{}; CHECK(safe_cast<int>(int{}), 0);
    is_safe_convertible<uE, signed char>{} = std::false_type{};
    is_safe_convertible<signed char, uE>{} = std::false_type{};

    enum Bool : bool {};
    is_safe_convertible<bool, Bool>{} = std::true_type{};
    is_safe_convertible<char, Bool>{} = std::false_type{};
    is_safe_convertible<s8, Bool>{} = std::false_type{};
    is_safe_convertible<u8, Bool>{} = std::false_type{};

}

#undef CHECK
