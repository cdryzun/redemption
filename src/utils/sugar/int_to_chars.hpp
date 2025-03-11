/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Product name: redemption, a FLOSS RDP proxy
Copyright (C) Wallix 2021
Author(s): Proxy Team
*/

#pragma once

#include "utils/sugar/zstring_view.hpp"
#include "utils/sugar/bounded_sequence.hpp"
#include "utils/traits/is_null_terminated.hpp"

#include <type_traits>
#include <cstring>

#include <string_view>
#include <bit>


constexpr std::size_t buffer_size_of_uint64_to_chars = 20;

namespace detail
{
    struct int_to_chars_result_access;
}

struct int_to_chars_result
{
    int_to_chars_result() = default;

    char const* data() const noexcept { return buffer + ibeg; }
    std::size_t size() const noexcept { return buffer_size_of_uint64_to_chars - ibeg; }

    std::string_view sv() const noexcept { return {data(), size()}; }
    operator std::string_view() const noexcept { return sv(); }

    static constexpr std::size_t max_capacity() noexcept { return buffer_size_of_uint64_to_chars; }

private:
    friend detail::int_to_chars_result_access;

    char buffer[buffer_size_of_uint64_to_chars];
    unsigned ibeg = buffer_size_of_uint64_to_chars;
};

struct int_to_zchars_result
{
    int_to_zchars_result() noexcept
    {
        buffer[buffer_size_of_uint64_to_chars] = '\0';
    }

    char const* data() const noexcept { return buffer + ibeg; }
    std::size_t size() const noexcept { return buffer_size_of_uint64_to_chars - ibeg; }

    char const* c_str() const noexcept { return data(); }

    std::string_view sv() const noexcept { return {data(), size()}; }
    zstring_view zv() const noexcept
    {
        return zstring_view::from_null_terminated(data(), size());
    }

    operator std::string_view() const noexcept { return sv(); }
    operator zstring_view() const noexcept { return zv(); }

    static std::size_t constexpr max_capacity() noexcept { return buffer_size_of_uint64_to_chars; }

private:
    friend detail::int_to_chars_result_access;

    char buffer[buffer_size_of_uint64_to_chars+1];
    unsigned ibeg = buffer_size_of_uint64_to_chars;
};


/// To decimal chars.
//@{
template<class T>
int_to_chars_result int_to_decimal_chars(T n) noexcept;

template<class T>
int_to_zchars_result int_to_decimal_zchars(T n) noexcept;

template<class T>
void int_to_decimal_chars(int_to_chars_result& out, T n) noexcept;

template<class T>
void int_to_decimal_zchars(int_to_zchars_result& out, T n) noexcept;
//@}


/// To hexadecimal upper case chars.
//@{
template<class T>
int_to_chars_result int_to_hexadecimal_upper_chars(T n) noexcept;

template<class T>
int_to_zchars_result int_to_hexadecimal_upper_zchars(T n) noexcept;

template<class T>
void int_to_hexadecimal_upper_chars(int_to_chars_result& out, T n) noexcept;

template<class T>
void int_to_hexadecimal_upper_zchars(int_to_zchars_result& out, T n) noexcept;
//@}


/// To hexadecimal lower case chars.
//@{
template<class T>
int_to_chars_result int_to_hexadecimal_lower_chars(T n) noexcept;

template<class T>
int_to_zchars_result int_to_hexadecimal_lower_zchars(T n) noexcept;

template<class T>
void int_to_hexadecimal_lower_chars(int_to_chars_result& out, T n) noexcept;

template<class T>
void int_to_hexadecimal_lower_zchars(int_to_zchars_result& out, T n) noexcept;
//@}


/// To fixed hexadecimal upper case chars.
/// \tparam NbBytes number of no-significative bytes display. -1 is equivalent to sizeof(T)
//@{
template<int NbBytes = -1, class T>
int_to_chars_result int_to_fixed_hexadecimal_upper_chars(T n) noexcept;

template<int NbBytes = -1, class T>
int_to_zchars_result int_to_fixed_hexadecimal_upper_zchars(T n) noexcept;

template<int NbBytes = -1, class T>
void int_to_fixed_hexadecimal_upper_chars(int_to_chars_result& out, T n) noexcept;

template<int NbBytes = -1, class T>
void int_to_fixed_hexadecimal_upper_zchars(int_to_zchars_result& out, T n) noexcept;

template<int NbBytes = -1, class T>
char* int_to_fixed_hexadecimal_upper_chars(char* out, T n) noexcept;
//@}


/// To fixed hexadecimal lower case chars.
/// \tparam NbBytes number of no-significative bytes display. -1 is equivalent to sizeof(T)
//@{
template<int NbBytes = -1, class T>
int_to_chars_result int_to_fixed_hexadecimal_lower_chars(T n) noexcept;

template<int NbBytes = -1, class T>
int_to_zchars_result int_to_fixed_hexadecimal_lower_zchars(T n) noexcept;

template<int NbBytes = -1, class T>
void int_to_fixed_hexadecimal_lower_chars(int_to_chars_result& out, T n) noexcept;

template<int NbBytes = -1, class T>
void int_to_fixed_hexadecimal_lower_zchars(int_to_zchars_result& out, T n) noexcept;

template<int NbBytes = -1, class T>
char* int_to_fixed_hexadecimal_lower_chars(char* out, T n) noexcept;
//@}


//
// IMPLEMENTATION
//

namespace detail
{
    inline constexpr uint32_t pow10_32[] = {
        0u,
        10u,
        100u,
        1000u,
        10000u,
        100000u,
        1000000u,
        10000000u,
        100000000u,
        1000000000u,
    };

    constexpr int number_length(uint32_t dec) noexcept
    {
        int t = (32 - std::countl_zero(dec)) * 1233 >> 12;
        return (t - (dec < pow10_32[t]) + 1);
    }

    inline constexpr char const * digit_pairs
      = "00010203040506070809"
        "10111213141516171819"
        "20212223242526272829"
        "30313233343536373839"
        "40414243444546474849"
        "50515253545556575859"
        "60616263646566676869"
        "70717273747576777879"
        "80818283848586878889"
        "90919293949596979899";

    inline char* push_2digits(char* p, uint64_t n) noexcept
    {
        assert(n < 100);
        memcpy(p, detail::digit_pairs + n * 2, 2);
        return p + 2;
    }

    inline char* push_2digits(char* p, int n) noexcept
    {
        assert(n >= 0 && n < 100);
        memcpy(p, detail::digit_pairs + n * 2, 2);
        return p + 2;
    }

    // https://github.com/jk-jeon/dragonbox/blob/master/source/dragonbox_to_chars.cpp
    struct Split4DigitsBy2
    {
        inline Split4DigitsBy2(uint64_t n) noexcept
            : d1{n * uint64_t{42949673}}
            , d2{(d1 & uint32_t{0xffffffff}) * 100}
        {
            assert(/*0 <= n &&*/ n <= 9999);
        }

        inline uint64_t low() const noexcept
        {
            return d2 >> 32;
        }

        inline uint64_t high() const noexcept
        {
            return d1 >> 32;
        }

        inline char* push_low_in(char* p) const noexcept
        {
            return push_2digits(p, d2 >> 32);
        }

        inline char* push_high_in(char* p) const noexcept
        {
            return push_2digits(p, d1 >> 32);
        }

        inline char* push_in(char* p) const noexcept
        {
            p = push_high_in(p);
            p = push_low_in(p);
            return p;
        }

    private:
        uint64_t d1;
        uint64_t d2;
    };

    inline char* push_4digits(char* p, uint64_t n) noexcept
    {
        return Split4DigitsBy2{n}.push_in(p);
    }

    inline char* to_decimal_chars_impl(char *end, uint64_t value) noexcept
    {
        char* out = end;

        while (value >= 100) {
            out -= 2;
            push_2digits(out, value % 100);
            value /= 100;
        }

        if (value < 10) {
            *--out = static_cast<char>('0' + value);
            return out;
        }

        out -= 2;
        push_2digits(out, value);
        return out;
    }

    template<class Int>
    inline char* to_decimal_chars(char *end, Int value) noexcept
    {
        static_assert(std::is_integral_v<Int>);
        static_assert(sizeof(Int) <= 64);

        auto abs_value = static_cast<uint64_t>(value); /* NOLINT(bugprone-signed-char-misuse, cert-str34-c) */
        if constexpr (std::is_signed_v<Int>) {
            bool negative = (value < 0);
            if (negative) abs_value = 0 - abs_value;
            char* begin = to_decimal_chars_impl(end, abs_value);
            if (negative) *--begin = '-';
            return begin;
        }
        else {
            return to_decimal_chars_impl(end, abs_value);
        }
    }

    constexpr inline auto& hex_upper_table = "0123456789ABCDEF";
    constexpr inline auto& hex_lower_table = "0123456789abcdef";

    template<class UInt>
    inline char* to_hexadecimal_chars(char *end, UInt value, char const(&hex_table)[17]) noexcept
    {
        static_assert(std::is_unsigned_v<UInt>);
        static_assert(sizeof(UInt) <= 64);

        char* out = end;

        while (value > 0xf) {
            *--out = hex_table[value & 0xf];
            value >>= 4;
        }

        if (value <= 0xf) {
            *--out = hex_table[value & 0xf];
        }

        return out;
    }

    template<int NbBytes, class T>
    inline char* to_fixed_hexadecimal_chars(char* out, T n, char const(&hex_table)[17]) noexcept
    {
        static_assert(std::is_unsigned_v<T>);
        static_assert(sizeof(T) <= 64);
        static_assert(NbBytes == -1 || NbBytes <= int(sizeof(T)),
            "number of NbBytes must not exceed sizeof(T)");

        constexpr std::size_t start = (NbBytes == -1)
            ? 0
            : (sizeof(T) - std::size_t(NbBytes));

        for (std::size_t i = start; i < sizeof(T); ++i) {
            std::size_t d = (sizeof(T) - 1 - i) * 8;
            *out++ = hex_table[(n >> (d+4)) & 0xfu];
            *out++ = hex_table[(n >>  d   ) & 0xfu];
        }

        return out;
    }

    struct int_to_chars_result_access
    {
        template<class T>
        inline static char* buffer(T& r) noexcept
        {
            return r.buffer;
        }

        template<class T>
        inline static void set_ibeg(T& r, std::ptrdiff_t n) noexcept
        {
            r.ibeg = unsigned(n);
        }
    };
} // namespace detail

template<class T>
inline int_to_chars_result int_to_decimal_chars(T n) noexcept
{
    int_to_chars_result r;
    int_to_decimal_chars(r, n);
    return r;
}

template<class T>
inline int_to_zchars_result int_to_decimal_zchars(T n) noexcept
{
    int_to_zchars_result r;
    int_to_decimal_zchars(r, n);
    return r;
}

template<class T>
inline int_to_chars_result int_to_hexadecimal_upper_chars(T n) noexcept
{
    int_to_chars_result r;
    int_to_hexadecimal_upper_chars(r, n);
    return r;
}

template<class T>
inline int_to_zchars_result int_to_hexadecimal_upper_zchars(T n) noexcept
{
    int_to_zchars_result r;
    int_to_hexadecimal_upper_zchars(r, n);
    return r;
}

template<class T>
inline int_to_chars_result int_to_hexadecimal_lower_chars(T n) noexcept
{
    int_to_chars_result r;
    int_to_hexadecimal_lower_chars(r, n);
    return r;
}

template<class T>
inline int_to_zchars_result int_to_hexadecimal_lower_zchars(T n) noexcept
{
    int_to_zchars_result r;
    int_to_hexadecimal_lower_zchars(r, n);
    return r;
}

template<int NbBytes, class T>
inline int_to_chars_result int_to_fixed_hexadecimal_upper_chars(T n) noexcept
{
    int_to_chars_result r;
    int_to_fixed_hexadecimal_upper_chars<NbBytes>(r, n);
    return r;
}

template<int NbBytes, class T>
inline int_to_zchars_result int_to_fixed_hexadecimal_upper_zchars(T n) noexcept
{
    int_to_zchars_result r;
    int_to_fixed_hexadecimal_upper_zchars<NbBytes>(r, n);
    return r;
}

template<int NbBytes, class T>
inline int_to_chars_result int_to_fixed_hexadecimal_lower_chars(T n) noexcept
{
    int_to_chars_result r;
    int_to_fixed_hexadecimal_lower_chars<NbBytes>(r, n);
    return r;
}

template<int NbBytes, class T>
inline int_to_zchars_result int_to_fixed_hexadecimal_lower_zchars(T n) noexcept
{
    int_to_zchars_result r;
    int_to_fixed_hexadecimal_lower_zchars<NbBytes>(r, n);
    return r;
}


template<class T>
inline void int_to_decimal_chars(int_to_chars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    char* end = buffer + buffer_size_of_uint64_to_chars;
    access::set_ibeg(out, detail::to_decimal_chars(end, n) - buffer);
}

template<class T>
inline void int_to_decimal_zchars(int_to_zchars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    char* end = buffer + buffer_size_of_uint64_to_chars;
    access::set_ibeg(out, detail::to_decimal_chars(end, n) - buffer);
}

template<class T>
inline void int_to_hexadecimal_upper_chars(int_to_chars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    char* end = buffer + buffer_size_of_uint64_to_chars;
    char* begin = detail::to_hexadecimal_chars(end, n, detail::hex_upper_table);
    access::set_ibeg(out, begin - buffer);
}

template<class T>
inline void int_to_hexadecimal_upper_zchars(int_to_zchars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    char* end = buffer + buffer_size_of_uint64_to_chars;
    char* begin = detail::to_hexadecimal_chars(end, n, detail::hex_upper_table);
    access::set_ibeg(out, begin - buffer);
}

template<class T>
inline void int_to_hexadecimal_lower_chars(int_to_chars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    char* end = buffer + buffer_size_of_uint64_to_chars;
    char* begin = detail::to_hexadecimal_chars(end, n, detail::hex_lower_table);
    access::set_ibeg(out, begin - buffer);
}

template<class T>
inline void int_to_hexadecimal_lower_zchars(int_to_zchars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    char* end = buffer + buffer_size_of_uint64_to_chars;
    char* begin = detail::to_hexadecimal_chars(end, n, detail::hex_lower_table);
    access::set_ibeg(out, begin - buffer);
}

template<int NbBytes, class T>
inline void int_to_fixed_hexadecimal_upper_chars(int_to_chars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    constexpr int ibeg = buffer_size_of_uint64_to_chars
                       - (NbBytes == -1 ? int(sizeof(T)) : NbBytes) * 2;
    int_to_fixed_hexadecimal_upper_chars<NbBytes>(buffer + ibeg, n);
    access::set_ibeg(out, ibeg);
}

template<int NbBytes, class T>
inline void int_to_fixed_hexadecimal_upper_zchars(int_to_zchars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    constexpr int ibeg = buffer_size_of_uint64_to_chars
                       - (NbBytes == -1 ? int(sizeof(T)) : NbBytes) * 2;
    int_to_fixed_hexadecimal_upper_chars<NbBytes>(buffer + ibeg, n);
    access::set_ibeg(out, ibeg);
}

template<int NbBytes, class T>
inline void int_to_fixed_hexadecimal_lower_chars(int_to_chars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    constexpr int ibeg = buffer_size_of_uint64_to_chars
                       - (NbBytes == -1 ? int(sizeof(T)) : NbBytes) * 2;
    int_to_fixed_hexadecimal_lower_chars<NbBytes>(buffer + ibeg, n);
    access::set_ibeg(out, ibeg);
}

template<int NbBytes, class T>
inline void int_to_fixed_hexadecimal_lower_zchars(int_to_zchars_result& out, T n) noexcept
{
    using access = detail::int_to_chars_result_access;
    auto buffer = access::buffer(out);
    constexpr int ibeg = buffer_size_of_uint64_to_chars
                       - (NbBytes == -1 ? int(sizeof(T)) : NbBytes) * 2;
    int_to_fixed_hexadecimal_lower_chars<NbBytes>(buffer + ibeg, n);
    access::set_ibeg(out, ibeg);
}

template<int NbBytes, class T>
inline char* int_to_fixed_hexadecimal_upper_chars(char* out, T n) noexcept
{
    return detail::to_fixed_hexadecimal_chars<NbBytes>(out, n, detail::hex_upper_table);
}

template<int NbBytes, class T>
inline char* int_to_fixed_hexadecimal_lower_chars(char* out, T n) noexcept
{
    return detail::to_fixed_hexadecimal_chars<NbBytes>(out, n, detail::hex_lower_table);
}


template<>
inline constexpr bool is_null_terminated_v<int_to_zchars_result> = true;

namespace detail
{
    template<>
    struct sequence_to_size_bounds_impl<int_to_chars_result>
    {
        using type = size_bounds<0, buffer_size_of_uint64_to_chars>;
    };

    template<>
    struct sequence_to_size_bounds_impl<int_to_zchars_result>
    {
        using type = size_bounds<0, buffer_size_of_uint64_to_chars>;
    };
} // namespace detail
