/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/mathutils.hpp"
#include <limits>

int main()
{
    constexpr unsigned char a = 10;
    constexpr unsigned long b = 10000;
    enum class E { A, B };

    static_assert(mmin(a, b) == 10);
    static_assert(mmin(b, a) == 10);
    static_assert(mmin(E::A, E::B) == E::A);
    static_assert(mmin({12, -2, 7}) == -2);

    static_assert(mmax(a, b) == 10000);
    static_assert(mmax(b, a) == 10000);
    static_assert(mmax(E::A, E::B) == E::B);
    static_assert(mmax({12, -2, 7}) == 12);

    static_assert(mclamp(12, -2, 7) == 7);
    static_assert(mclamp(-12, -2, 7) == -2);
    static_assert(mclamp(-12, -2, 7l) == -2);
    static_assert(mclamp(-12, -2l, 7) == -2);
    static_assert(mclamp(-12l, -2, 7) == -2);

    std::type_identity<decltype(mmin(a, b))>{} = std::type_identity<unsigned char>{};
    std::type_identity<decltype(mmax(a, b))>{} = std::type_identity<unsigned long>{};
    std::type_identity<decltype(mmin(b, a))>{} = std::type_identity<unsigned char>{};
    std::type_identity<decltype(mmax(b, a))>{} = std::type_identity<unsigned long>{};
    std::type_identity<decltype(mmin({12, -2, 7}))>{} = std::type_identity<int>{};
    std::type_identity<decltype(mmax({12, -2, 7}))>{} = std::type_identity<int>{};
    std::type_identity<decltype(mclamp(12, -2, 7))>{} = std::type_identity<int>{};
    std::type_identity<decltype(mclamp(12, -2, 7l))>{} = std::type_identity<int>{};
    std::type_identity<decltype(mclamp(12, -2l, 7))>{} = std::type_identity<long>{};
    std::type_identity<decltype(mclamp(12l, -2, 7))>{} = std::type_identity<int>{};

    constexpr long long min_ll = min_auto;
    constexpr long long max_ll = max_auto;
    constexpr unsigned char min_u8 = min_auto;
    constexpr unsigned char max_u8 = max_auto;

    static_assert(min_ll == std::numeric_limits<long long>::min());
    static_assert(max_ll == std::numeric_limits<long long>::max());
    static_assert(min_u8 == std::numeric_limits<unsigned char>::min());
    static_assert(max_u8 == std::numeric_limits<unsigned char>::max());

    static_assert(static_cast<char>(min_auto) == std::numeric_limits<char>::min());
    static_assert(static_cast<char>(max_auto) == std::numeric_limits<char>::max());
    static_assert(static_cast<signed char>(min_auto) == std::numeric_limits<signed char>::min());
    static_assert(static_cast<signed char>(max_auto) == std::numeric_limits<signed char>::max());
    static_assert(static_cast<short>(min_auto) == std::numeric_limits<short>::min());
    static_assert(static_cast<short>(max_auto) == std::numeric_limits<short>::max());
    static_assert(static_cast<int>(min_auto) == std::numeric_limits<int>::min());
    static_assert(static_cast<int>(max_auto) == std::numeric_limits<int>::max());
    static_assert(static_cast<long>(min_auto) == std::numeric_limits<long>::min());
    static_assert(static_cast<long>(max_auto) == std::numeric_limits<long>::max());
    static_assert(static_cast<long long>(min_auto) == std::numeric_limits<long long>::min());
    static_assert(static_cast<long long>(max_auto) == std::numeric_limits<long long>::max());

    static_assert(static_cast<unsigned char>(min_auto) == std::numeric_limits<unsigned char>::min());
    static_assert(static_cast<unsigned char>(max_auto) == std::numeric_limits<unsigned char>::max());
    static_assert(static_cast<unsigned short>(min_auto) == std::numeric_limits<unsigned short>::min());
    static_assert(static_cast<unsigned short>(max_auto) == std::numeric_limits<unsigned short>::max());
    static_assert(static_cast<unsigned>(min_auto) == std::numeric_limits<unsigned>::min());
    static_assert(static_cast<unsigned>(max_auto) == std::numeric_limits<unsigned>::max());
    static_assert(static_cast<unsigned long>(min_auto) == std::numeric_limits<unsigned long>::min());
    static_assert(static_cast<unsigned long>(max_auto) == std::numeric_limits<unsigned long>::max());
    static_assert(static_cast<unsigned long long>(min_auto) == std::numeric_limits<unsigned long long>::min());
    static_assert(static_cast<unsigned long long>(max_auto) == std::numeric_limits<unsigned long long>::max());

    static_assert(static_cast<bool>(min_auto) == std::numeric_limits<bool>::min());
    static_assert(static_cast<bool>(max_auto) == std::numeric_limits<bool>::max());
}
