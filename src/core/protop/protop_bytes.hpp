/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/stream.hpp"
#include "utils/sugar/bounded_bytes_view.hpp"

namespace protop
{

template<unsigned n>
struct bytes_array
{
    static constexpr unsigned pdu_max_len = n;
    static constexpr unsigned pdu_min_len = pdu_max_len;

    using value_type = std::array<uint8_t, n>;

    static value_type read(InStream & in_stream) noexcept
    {
        value_type a;
        in_stream.in_copy_bytes(make_writable_array_view(a));
        return a;
    }

    static void write(OutStream & out_stream, bytes_view value) noexcept
    {
        out_stream.out_copy_bytes(value);
    }
};

struct dynamic_bytes_properties
{
    unsigned min = 0;
    unsigned max;
    // consumed and ignored
    unsigned skip_end = 0;
    // not consumed
    unsigned remaining_after = 0;
};

template<dynamic_bytes_properties props>
struct dynamic_bytes
{
    static_assert(props.min <= props.max);
    static_assert(uint32_t{props.max + uint64_t{props.skip_end} + uint64_t{props.remaining_after}} > 0);

    static constexpr unsigned pdu_min_len = props.min + props.skip_end;
    static constexpr unsigned pdu_max_len { props.max + uint64_t{props.skip_end} };

    using value_type = bounded_bytes_view<props.min, props.max>;

    static value_type read(InStream & in_stream) noexcept
    {
        assert(in_stream.in_remain() >= props.min + props.skip_end + props.remaining_after);
        auto n = in_stream.in_remain() - props.skip_end - props.remaining_after;
        auto value = value_type::assumed(in_stream.in_skip_bytes(n));
        if constexpr (props.skip_end)
        {
            in_stream.in_skip_bytes(props.skip_end);
        }
        return value;
    }

    static void write(OutStream & out_stream, bytes_view value) noexcept
    {
        out_stream.out_copy_bytes(value);
        if constexpr (props.skip_end)
        {
            out_stream.out_clear_bytes(props.skip_end);
        }
    }
};

template<unsigned n>
struct bytes : dynamic_bytes<dynamic_bytes_properties{ .min = n, .max = n, }>
{};

}
