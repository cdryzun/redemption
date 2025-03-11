/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/stream.hpp"
#include "utils/utf.hpp"

/// utf16_to_utf8(buflen) log format convert a utf16 data to utf8 string
/// \example
/// \code
/// PROTOCOL_PARSER_DECL_STRUCT(
///     File, (),
///     (null_terminated_utf16<520>, UnicodefileName, utf16_to_utf8(384))
/// );
/// \endcode
#define PROTOCOL_PARSER_IMPL_LOG_VAR_utf16_to_utf8(buflen) PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_LOG_utf16_to_utf8(name) "%s"
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_utf16_to_utf8(buflen) \
    PROTOCOL_PARSER_IMPL_LOG_utf16_to_utf8
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_utf16_to_utf8(buflen) \
    , protop_fmt::init_utf16_to_utf8(                          \
        protop_fmt::Buffer<buflen+1>{}.buf, buflen+1,            \
        PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_utf16_to_utf8(fn) PROTOCOL_PARSER_IMPL_LOG_NONAME

namespace protop
{

template<std::size_t TotalBytes>
struct null_terminated_utf16
{
    static_assert(TotalBytes > 2 && !(TotalBytes & 1));

    static constexpr unsigned pdu_max_len = TotalBytes;
    static constexpr unsigned pdu_min_len = TotalBytes;

    using value_type = bounded_bytes_view<0, TotalBytes - 2>;

    static value_type read(InStream & in_stream) noexcept
    {
        auto data = in_stream.in_skip_bytes(TotalBytes);
        // null character is dropped and not checked
        auto str = data.first(UTF16ByteLen(data.drop_back(2)));
        return value_type::assumed(str);
    }

    static void write(OutStream & out_stream, bytes_view value) noexcept
    {
        out_stream.out_copy_bytes(value);
        out_stream.out_clear_bytes(TotalBytes - value.size()); // null terminated
    }
};

}

namespace protop_fmt
{
    REDEMPTION_NOINLINE
    char const * init_utf16_to_utf8(
        char * utf8_buf, std::size_t utf8_buf_len,
        bytes_view utf16_str
    ) noexcept;
}
