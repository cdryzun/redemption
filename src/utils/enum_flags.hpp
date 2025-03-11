/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <type_traits>


#define REDEMPTION_DECLARE_ENUM_FLAG_OPS(Prefix, enum_name)            \
    Prefix enum_name operator | (enum_name x, enum_name y) noexcept    \
    {                                                                  \
        using int_type = std::underlying_type_t<enum_name>;            \
        return enum_name(int_type(x) | int_type(y));                   \
    }                                                                  \
    Prefix enum_name operator & (enum_name x, enum_name y) noexcept    \
    {                                                                  \
        using int_type = std::underlying_type_t<enum_name>;            \
        return enum_name(int_type(x) & int_type(y));                   \
    }                                                                  \
                                                                       \
    Prefix enum_name& operator |= (enum_name& x, enum_name y) noexcept \
    { return x = x | y; }                                              \
    Prefix enum_name& operator &= (enum_name& x, enum_name y) noexcept \
    { return x = x & y; }                                              \
                                                                       \
    Prefix enum_name operator ~ (enum_name x) noexcept                 \
    { return enum_name(~std::underlying_type_t<enum_name>(x)); }


#define REDEMPTION_DECLARE_IS_ENUM_FLAGS(ename)            \
    namespace detail                                       \
    {                                                      \
        template<>                                         \
        inline constexpr bool is_enum_flags<ename> = true; \
    }

#define REDEMPTION_DECLARE_ENUM_FLAGS_IN_CLASS(ename) \
    REDEMPTION_DECLARE_ENUM_FLAG_OPS(friend constexpr, ename)

#define REDEMPTION_DECLARE_ENUM_FLAGS(ename) \
    REDEMPTION_DECLARE_ENUM_FLAG_OPS(constexpr, ename)

#define REDEMPTION_DECLARE_ENUM_FLAGS_NS(ns, ename)    \
    REDEMPTION_DECLARE_ENUM_FLAG_OPS(constexpr, ename) \
    } /* ns end */                                     \
    namespace ns {


namespace detail
{
    template<class T>
    inline constexpr bool is_flags_v = false;


    template<class E, class FlagsContraints>
    struct ConstevalFlagsContraints : FlagsContraints
    {
        consteval ConstevalFlagsContraints(FlagsContraints constraints)
            : FlagsContraints{constraints}
        {
            if (E{} != (constraints.match & constraints.reject))
            {
                throw "mask overlap";
            }
        }
    };
}

namespace meta
{
    template<class T>
    concept Flags = std::is_unsigned_v<T>
                 || std::is_unsigned_v<std::underlying_type_t<T>>
                 || ::detail::is_flags_v<T>;
}

template<meta::Flags E>
struct FlagsContraints
{
    E match;
    E reject;

    using Consteval = detail::ConstevalFlagsContraints<E, FlagsContraints>;
};

template<meta::Flags E>
constexpr bool flags_test(E flags, typename FlagsContraints<E>::Consteval contraints) noexcept
{
    return (flags & (contraints.match | contraints.reject)) == contraints.match;
}

/// \return true when all flags of \p with are in \p flags.
template<meta::Flags E>
constexpr bool flags_test(E flags, E with) noexcept
{
    return (flags & with) == with;
}

/// \return true when any flags of \p with is in \p flags.
template<meta::Flags E>
constexpr bool flags_any(E flags, E with) noexcept
{
    return (flags & with) != E{};
}

/// \return true when none flags of \p with is in \p flags.
template<meta::Flags E>
constexpr bool flags_none(E flags, E with) noexcept
{
    return (flags & with) == E{};
}

template<meta::Flags E>
[[nodiscard]]
constexpr bool flags_is_empty(E flags) noexcept
{
    return flags == E{};
}

template<meta::Flags E>
constexpr void flags_remove(E & flags, E removed) noexcept
{
    flags = flags & ~removed;
}

template<meta::Flags E>
constexpr void flags_add(E & flags, E added) noexcept
{
    flags = flags | added;
}

template<meta::Flags E>
struct FlagsUpdate
{
    E add {};
    E remove {};
};

template<meta::Flags E>
constexpr void flags_update(E & flags, FlagsUpdate<E> update_data) noexcept
{
    flags = flags | update_data.add;
    flags = flags & ~update_data.remove;
}

/// \return true when the masking values of \p flags1 and \p flags2 are equals.
template<meta::Flags E>
constexpr bool flags_equal_with_mask(E flags1, E flags2, E mask) noexcept
{
    return (flags1 & mask) == (flags2 & mask);
}
