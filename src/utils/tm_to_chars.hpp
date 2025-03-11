/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bounded_array_view.hpp"
#include "utils/sugar/bytes_copy.hpp"
#include "utils/sugar/int_to_chars.hpp"

#include <ctime>


namespace detail
{
    /// \param tm_year Year minus 1900
    inline char* push_4digits_tm_year(char* p, int tm_year) noexcept
    {
        // deal with overflow
        auto year = static_cast<unsigned>(tm_year + 92);
        year = year & 0x1fffu /* 8191 */;
        year += (1900 - 92);
        // year is in range 1808-9999
        return Split4DigitsBy2{year}.push_in(p);
    }
} // namespace detail

namespace dateformats
{
    // "YYYY-mm-dd"
    struct YYYY_mm_dd
    {
        static constexpr std::size_t output_length = 10;

        struct Seps
        {
            char date_sep = '-';
        };

        static char* to_chars(char* p, struct tm const& tm, Seps seps = {'-'}) noexcept
        {
            p = detail::push_4digits_tm_year(p, tm.tm_year);
            *p++ = seps.date_sep;
            p = detail::push_2digits(p, tm.tm_mon + 1);
            *p++ = seps.date_sep;
            p = detail::push_2digits(p, tm.tm_mday);

            return p;
        }
    };

    // "HH:MM:SS"
    struct HH_MM_SS
    {
        static constexpr std::size_t output_length = 8;

        struct Seps
        {
            char time_sep = ':';
        };

        static char* to_chars(char* p, struct tm const& tm, Seps seps = {':'}) noexcept
        {
            p = detail::push_2digits(p, tm.tm_hour);
            *p++ = seps.time_sep;
            p = detail::push_2digits(p, tm.tm_min);
            *p++ = seps.time_sep;
            p = detail::push_2digits(p, tm.tm_sec);

            return p;
        }
    };

    // "YYYY-mm-dd HH:MM:SS"
    //      ^  ^             date_sep
    //            ^          datetime_sep
    //               ^  ^    time_sep
    struct YYYY_mm_dd_HH_MM_SS
    {
        static constexpr std::size_t output_length = 19;

        struct Seps
        {
            char date_sep = '-';
            char datetime_sep = ' ';
            char time_sep = ':';
        };

        static char* to_chars(char* p, struct tm const& tm, Seps seps = {'-', ' ', ':'}) noexcept
        {
            p = YYYY_mm_dd::to_chars(p, tm, {seps.date_sep});
            *p++ = seps.datetime_sep;
            p = HH_MM_SS::to_chars(p, tm, {seps.time_sep});

            return p;
        }
    };

    // "mm/dd/YYYY HH:MM"
    //      ^  ^          date_sep
    //            ^       datetime_sep
    //               ^    time_sep
    struct mm_dd_YYYY_HH_MM
    {
        static constexpr std::size_t output_length = 16;

        struct Seps
        {
            char date_sep = '/';
            char datetime_sep = ' ';
            char time_sep = ':';
        };

        static char* to_chars(char* p, struct tm const& tm, Seps seps = {'/', ' ', ':'}) noexcept
        {
            p = detail::push_2digits(p, tm.tm_mon + 1);
            *p++ = seps.date_sep;
            p = detail::push_2digits(p, tm.tm_mday);
            *p++ = seps.date_sep;
            p = detail::push_4digits_tm_year(p, tm.tm_year);
            *p++ = seps.datetime_sep;
            p = detail::push_2digits(p, tm.tm_hour);
            *p++ = seps.time_sep;
            p = detail::push_2digits(p, tm.tm_min);

            return p;
        }
    };

    // "MMM dd YYYY HH:MM:SS"
    struct MMM_dd_YYYY_HH_MM_SS
    {
        static constexpr std::size_t output_length = 20;

        struct Seps {};

        static char* to_chars(char* p, struct tm const& tm, Seps = {}) noexcept
        {
            char const* months =
                "Jan "
                "Feb "
                "Mar "
                "Apr "
                "May "
                "Jun "
                "Jul "
                "Aug "
                "Sep "
                "Oct "
                "Nov "
                "Dec "
            ;

            p = bytes_copy_and_advance(p, {months + tm.tm_mon * 4, 4});
            p = detail::push_2digits(p, tm.tm_mday);
            *p++ = ' ';
            p = detail::push_4digits_tm_year(p, tm.tm_year);
            *p++ = ' ';
            p = HH_MM_SS::to_chars(p, tm);

            return p;
        }
    };

    template<class DateFormat>
    struct DateBuffer
    {
        using StringView = sized_chars_view<DateFormat::output_length>;
        using Seps = DateFormat::Seps;

        DateBuffer(struct tm const& tm, Seps seps = {}) noexcept
        {
            DateFormat::to_chars(m_buf, tm, seps);
        }

        StringView sv() const noexcept
        {
            return StringView::assumed(m_buf);
        }

    private:
        char m_buf[DateFormat::output_length];
        std::ptrdiff_t m_len;
    };
} // namespace dateformats
