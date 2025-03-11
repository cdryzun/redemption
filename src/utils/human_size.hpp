/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bounded_array_view.hpp"
#include "utils/sugar/bytes_copy.hpp"
#include "utils/sugar/int_to_chars.hpp"

struct HumanSizePowerOf2
{
    // [0..=1023]
    // [0..=1023] '.' [0..=99]
    static const unsigned min_size = 1;
    static const unsigned max_size = 7;

    enum class Unit : uint8_t
    {
        Byte,
        Kibi,
        Mebi,
        Gibi,
        Tebi,
        Pebi,
        Exbi,
    };

    struct Numeric
    {
        uint64_t numerator;
        uint64_t denominator;
        Unit unit;

        static Numeric make(uint64_t file_size) noexcept
        {
            if (file_size < 1024)
            {
                return { file_size, 0, Unit::Byte };
            }

            constexpr unsigned base_len = 10; // for 0b11'1111'1111 (1023)
            unsigned bw = static_cast<unsigned>(std::bit_width(file_size));
            // use bw as unsigned, best when compiler assume bw-1 >= 0
            unsigned unit_id = (bw - 1) / base_len;
            unsigned exp = unit_id * base_len;
            uint64_t num = file_size >> exp;
            uint64_t denum = ((file_size - (num << exp)) >> (exp - base_len));
            return { num, denum, checked_int{ unit_id } };
        }

        uint32_t compress() const noexcept
        {
            return (numerator << 14) | (denominator << 4) | static_cast<uint32_t>(unit);
        }
    };

    HumanSizePowerOf2(uint64_t file_size, char fraction_sep) noexcept
    {
        auto * p = m_buf;

        if (file_size < 1024)
        {
            p = detail::push_4digits(p, file_size);

            m_unit = Unit::Byte;
            m_offset = checked_int{4 - detail::number_length(static_cast<uint32_t>(file_size))};
            m_len = checked_int{p - m_buf - m_offset};

            return;
        }

        auto num = Numeric::make(file_size);

        p = detail::push_4digits(p, num.numerator);
        *p++ = fraction_sep;
        p = detail::Split4DigitsBy2{num.denominator * 100 / 1024}.push_low_in(p);

        m_unit = num.unit;
        m_offset = checked_int{4 - detail::number_length(static_cast<uint32_t>(num.numerator))};
        m_len = checked_int{p - m_buf - m_offset};
    }

    bounded_chars_view<min_size, max_size> sv() const noexcept
    {
        return bounded_chars_view<min_size, max_size>::assumed(m_buf + m_offset, m_len);
    }

    Unit unit() const noexcept
    {
        return m_unit;
    }

private:
    Unit m_unit;
    uint8_t m_offset;
    uint8_t m_len;
    char m_buf[max_size];
};
