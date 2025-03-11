/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bounded_array_view.hpp"
#include "utils/sugar/cast.hpp"
#include <array>

/// Utf16le hostname with 16 code unit including null terminator.
struct RdpHostname
{
    static const uint8_t hostname_len_with_zero_terminal = 16;

    RdpHostname() noexcept = default;
    RdpHostname(RdpHostname const &) noexcept = default;
    RdpHostname & operator=(RdpHostname const &) noexcept = default;

    template<std::size_t AtLeast, std::size_t AtMost>
        requires (AtMost <= hostname_len_with_zero_terminal)
    RdpHostname(bounded_array_view<uint16_t, AtLeast, AtMost> hostname) noexcept
        : RdpHostname(copy_tag_t{}, hostname)
    {}

    template<class T, std::size_t AtLeast, std::size_t AtMost>
        requires (AtMost <= hostname_len_with_zero_terminal)
              && (std::is_same_v<T, char> || std::is_same_v<T, uint8_t>)
    static RdpHostname
    from_ascii(bounded_array_view<T, AtLeast, AtMost> hostname) noexcept
    {
        return RdpHostname(copy_tag_t{}, hostname);
    }

    bounded_array_view<uint16_t, 0, hostname_len_with_zero_terminal - 1>
    utf16le_zstr() const noexcept
    {
        auto hostname = make_bounded_array_view(m_hostname).drop_back<1>();

        uint8_t n = 0;
        for (auto c : hostname)
        {
            if (!c)
            {
                break;
            }
            ++n;
        }

        return hostname.first(n);
    }

    sized_array_view<uint16_t, hostname_len_with_zero_terminal>
    utf16le_fixed_str() const noexcept
    {
        return m_hostname;
    }

    struct Utf8ZStringMaybeInvalid
    {
        using zstring_view = bounded_array_view<char, 0, hostname_len_with_zero_terminal - 1>;

        char const * c_str() const noexcept
        {
            return char_ptr_cast(m_data);
        }

        zstring_view zstr() const noexcept
        {
            auto hostname = make_bounded_array_view(m_data).drop_back<1>();

            uint8_t n = 0;
            for (auto c : hostname)
            {
                if (!c)
                {
                    break;
                }
                ++n;
            }

            return zstring_view::assumed(char_ptr_cast(m_data), n);
        }

        sized_array_view<uint8_t, hostname_len_with_zero_terminal>
        fixed_av() const noexcept
        {
            return make_bounded_array_view(m_data);
        }

    private:
        friend RdpHostname;

        uint8_t m_data[hostname_len_with_zero_terminal];
    };

    Utf8ZStringMaybeInvalid utf8_fixed_maybe_invalid() const noexcept
    {
        Utf8ZStringMaybeInvalid utf8;
        auto it = utf8.m_data;
        for (auto c : m_hostname)
        {
            *it++ = static_cast<uint8_t>(c);
        }
        return utf8;
    }

private:
    class copy_tag_t {};

    template<class AV>
    RdpHostname(copy_tag_t, AV hostname) noexcept
    {
        auto it = m_hostname;

        for (auto c : hostname)
        {
            if constexpr (std::is_same_v<decltype(c), char>)
            {
                *it++ = static_cast<uint8_t>(c);
            }
            else
            {
                *it++ = c;
            }
        }

        for (std::size_t i = hostname.size(); i < hostname_len_with_zero_terminal; ++i)
        {
            *it++ = 0;
        }

        m_hostname[hostname_len_with_zero_terminal - 1] = 0;
    }

    uint16_t m_hostname[16] = {0};
};
