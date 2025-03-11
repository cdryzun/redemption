/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/static_string.hpp"
#include "utils/structural_string.hpp"
#include "utils/sugar/array_view.hpp"

#include <type_traits>

template<class TName, class T>
struct NameAndValue
{
    TName name;
    T value;
};

template<std::size_t N>
struct NameOf : structural_string<N>
{
    using structural_string<N>::structural_string;

    constexpr NameOf(structural_string<N> const & other) noexcept
      : structural_string<N>{other}
    {}

    template<class T>
    constexpr NameAndValue<structural_string<N>, T> operator()(T value) const
    {
        return {*this, value};
    }
};

template<structural_string str>
constexpr auto operator ""_name_of() noexcept
{
    return NameOf<str.size()+1>{str};
}


class StaticStringFromEnumFlags
{
    template<class E, bool = std::is_enum_v<E>>
    struct int_type_w
    {
        static_assert(std::is_unsigned_v<E>);
        using type = E;
    };

    template<class E>
    struct int_type_w<E, true>
      : int_type_w<std::underlying_type_t<E>>
    {};

public:
    static constexpr chars_view unknown_value = "???"_av;

    template<class E>
    struct Impl
    {
        struct Item
        {
            E flag;
            chars_view str;
        };

        using Int = typename int_type_w<E>::type;

        static delayed_build_string_buffer::ResultLen
        build(delayed_build_string_buffer buffer, E flags, array_view<Item> items) noexcept
        {
            Int known {};
            auto builder = buffer.builder();

            for (Item d : items)
            {
                known |= Int(d.flag);
                if (Int(flags) & Int(d.flag))
                {
                    builder.push_if_not_empty('|');
                    builder.push(d.str);
                }
            }

            if (Int(flags) & ~known)
            {
                builder.push_if_not_empty('|');
                builder.push(unknown_value);
            }

            return builder.eos();
        }

        static constexpr std::size_t compute_max_capacity(array_view<Item> items) noexcept
        {
            std::size_t len = items.size(); // '|' sep

            Int known {};

            for (Item d : items)
            {
                known |= Int(d.flag);
                len += d.str.size();
            }

            if (Int(~Int{}) != known)
            {
                len += unknown_value.size();
            }
            // remove last '|'
            else if (len)
            {
                len -= 1;
            }

            return len;
        }
    };

    template<class E, std::size_t N>
    static auto string_builder(E flags, typename Impl<E>::Item const (&items)[N]) noexcept
    {
        return [flags, &items](delayed_build_string_buffer buffer) noexcept
        {
            assert(Impl<E>::compute_max_capacity(items)
                    < buffer.chars_with_null_terminated().size());
            return Impl<E>::build(buffer, flags, items);
        };
    }

    template<NameAndValue... item>
    struct Maker
    {
        template<class E>
        /* C++23: static constexpr */auto operator()(E flags) const noexcept
        {
            static_assert((... && std::is_same_v<E, decltype(item.value)>));
            static constexpr typename Impl<E>::Item items[] { {item.value, item.name}... };
            return static_string<Impl<E>::compute_max_capacity(items)> {
                delayed_build_t{},
                string_builder(flags, items),
            };
        }
    };

    template<NameAndValue... item>
    static constexpr Maker<item...> make {};
};
