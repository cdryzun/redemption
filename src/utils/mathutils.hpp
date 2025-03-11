/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <type_traits>
#include "utils/sugar/array.hpp"


/// Returns the smaller of \c a and \c b in the smaller type.
/// Unlike `std::min()`, this function allows comparison between values
/// of different types.
///
/// Problematic usage with `std::min()`:
///
/// \code
/// uint16_t value1;
/// uint32_t value2;
///
/// result = std::min<uint16_t>(value1, value2);
/// // or with a cast:
/// result = std::min(static_cast<uint16_t>(value1), value2);
/// \endcode
///
/// These approaches are dangerous: the cast may truncate values and
/// produce incorrect results.
///
/// Example:
/// \code
/// uint16_t value1 = 10000;
/// uint32_t value2 = 65536;
/// std::min(value1, static_cast<uint16_t>(value2)) == 0
/// \endcode
///
/// Additionally, such patterns are fragile — if the types of `value1` or `value2`
/// change, the casts must be updated accordingly. These casts also suppress
/// useful compiler warnings, making the code harder to maintain and debug.
//@{
template<class IntT, class IntU>
    requires std::is_integral_v<IntT> && std::is_integral_v<IntU>
constexpr auto mmin(IntT a, IntU b) noexcept
{
    constexpr bool a_is_uint = std::is_unsigned_v<IntT>;
    constexpr bool b_is_uint = std::is_unsigned_v<IntU>;

    if constexpr (a_is_uint ^ b_is_uint)
    {
        static_assert(a_is_uint ^ b_is_uint, "unimplemented");
    }
    else // both signed or both unsigned
    {
        using Common = std::conditional_t<sizeof(IntT) < sizeof(IntU), IntU, IntT>;
        using MinT = std::conditional_t<sizeof(IntT) < sizeof(IntU), IntT, IntU>;
        return (Common{b} < Common{a}) ? static_cast<MinT>(b) : static_cast<MinT>(a);
    }
}

template<class T, class U>
    requires std::is_same_v<T, U>
          || std::is_base_of_v<T, U>
          || std::is_base_of_v<U, T>
constexpr auto * mmin(T * a, U * b) noexcept
{
    return (b < a) ? b : a;
}

template<class E>
    requires std::is_enum_v<E>
constexpr E mmin(E a, E b) noexcept
{
    return (b < a) ? b : a;
}

template<class IntOrPtrT, std::size_t N>
    requires std::is_integral_v<IntOrPtrT> || std::is_pointer_v<IntOrPtrT>
constexpr auto mmin(IntOrPtrT const (&arr)[N]) noexcept
{
    static_assert(N > 0);

    IntOrPtrT r = arr[0];
    for (std::size_t i = 1; i < N; ++i)
    {
        r = mmin(r, arr[i]);
    }
    return r;
}
//@}


/// Returns the greater of \c a and \c b in the greater type.
/// Unlike `std::mmax()`, this function allows comparison between values
/// of different types.
/// See \c mmin for problematic usage of `std::min` / `std::max`.
//@{
template<class IntT, class IntU>
    requires std::is_integral_v<IntT> && std::is_integral_v<IntU>
constexpr auto mmax(IntT a, IntU b) noexcept
{
    constexpr bool a_is_uint = std::is_unsigned_v<IntT>;
    constexpr bool b_is_uint = std::is_unsigned_v<IntU>;

    if constexpr (a_is_uint ^ b_is_uint)
    {
        static_assert(a_is_uint ^ b_is_uint, "unimplemented");
    }
    else // both signed or both unsigned
    {
        using Common = std::conditional_t<sizeof(IntT) < sizeof(IntU), IntU, IntT>;
        using MaxT = std::conditional_t<sizeof(IntT) < sizeof(IntU), IntU, IntT>;
        return (Common{a} < Common{b}) ? static_cast<MaxT>(b) : static_cast<MaxT>(a);
    }
}

template<class T, class U>
    requires std::is_same_v<T, U>
          || std::is_base_of_v<T, U>
          || std::is_base_of_v<U, T>
constexpr auto * mmax(T * a, U * b) noexcept
{
    return (a < b) ? b : a;
}

template<class E>
    requires std::is_enum_v<E>
constexpr E mmax(E a, E b) noexcept
{
    return (a < b) ? b : a;
}

template<class IntOrPtrT, std::size_t N>
    requires std::is_integral_v<IntOrPtrT> || std::is_pointer_v<IntOrPtrT>
constexpr auto mmax(IntOrPtrT const (&arr)[N]) noexcept
{
    static_assert(N > 0);

    IntOrPtrT r = arr[0];
    for (std::size_t i = 1; i < N; ++i)
    {
        r = mmax(r, arr[i]);
    }
    return r;
}
//@}


/// Returns value between min and max (inclusive).
/// Unlike `std::clamp()`, this function allows comparison between values
/// of different types.
/// See \c mmin for problematic usage of `std::min` / `std::max`.
template<class IntOrPtrT, class IntOrPtrMin, class IntOrPtrMax>
constexpr auto mclamp(IntOrPtrT value, IntOrPtrMin min, IntOrPtrMax max) noexcept
    -> decltype(mmax(mmin(value, max), min))
{
    return mmax(mmin(value, max), min);
}


struct min_auto_t
{
    template<class IntT>
        requires std::is_integral_v<IntT>
    constexpr operator IntT () const noexcept
    {
        if constexpr (std::is_unsigned_v<IntT>)
        {
            return IntT{};
        }
        else
        {
            using UInt = std::make_unsigned_t<IntT>;
            return -static_cast<IntT>(static_cast<UInt>(~UInt{}) / 2) - 1;
        }
    }
};

inline constexpr min_auto_t min_auto {};


struct max_auto_t
{
    template<class IntT>
        requires std::is_integral_v<IntT>
    constexpr operator IntT () const noexcept
    {
        if constexpr (std::is_unsigned_v<IntT>)
        {
            // just to inhibit -Wbool-operation
            if constexpr (std::is_same_v<bool, IntT>)
            {
                return true;
            }
            else
            {
                return static_cast<IntT>(~IntT{});
            }
        }
        else
        {
            using UInt = std::make_unsigned_t<IntT>;
            return static_cast<IntT>(static_cast<UInt>(~UInt{}) >> 1);
        }
    }
};

inline constexpr max_auto_t max_auto {};



template<class F, class T, class... Ts>
    requires std::is_member_function_pointer_v<F>
auto apply(F f, T && obj, Ts && ... args)
    REDEMPTION_DECLTYPE_AUTO_RETURN_NOEXCEPT((static_cast<T&&>(obj).*f)(static_cast<Ts&&>(args)...))

template<class F, class T>
    requires std::is_member_object_pointer_v<F>
auto apply(F mem, T && obj) noexcept
    REDEMPTION_DECLTYPE_AUTO_RETURN(static_cast<T&&>(obj).*mem)

template<class F, class... Ts>
    requires (!std::is_member_pointer_v<F>)
auto apply(F && f, Ts && ... args)
    REDEMPTION_DECLTYPE_AUTO_RETURN_NOEXCEPT(static_cast<F&&>(f)(static_cast<Ts&&>(args)...))


template<auto f>
struct CFunc
{
    template<class... Ts>
    auto operator()(Ts && ... args) const
        REDEMPTION_DECLTYPE_AUTO_RETURN_NOEXCEPT(apply(f, static_cast<Ts&&>(args)...))
};

template<auto f>
inline constexpr auto c_fn = CFunc<f>{};


struct FnIdentity
{
    template<class T>
    T && operator()(T && value) const noexcept
    {
        return static_cast<T&&>(value);
    }
};

template<class Cont, class IntT, class Proj = FnIdentity>
    requires std::is_integral_v<IntT>
IntT sum(Cont const& cont, IntT start_value, Proj && proj = Proj{})
    noexcept(noexcept(apply(proj, *utils::detail_::begin_impl(cont))))
{
    for (auto && x : cont)
    {
        start_value += apply(proj, x);
    }
    return start_value;
}

template<class Cont, class Proj>
    requires (!std::is_integral_v<Proj>)
std::size_t sum(Cont const& cont, Proj && proj)
    REDEMPTION_AUTO_RETURN_NOEXCEPT(sum(cont, std::size_t{}, proj))

template<class IntT, class Cont, class Proj>
    requires std::is_integral_v<IntT>
IntT sum(Cont const& cont, Proj && proj)
    REDEMPTION_AUTO_RETURN_NOEXCEPT(sum(cont, IntT{}, proj))
