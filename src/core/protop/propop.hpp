/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/stream.hpp"
#include "utils/log.hpp"

#include <cinttypes>


/// \param name struct name
/// \param values... list of field, mem or padding
///
/// - field(type, name, log_format)
/// - mem(cpp_code)
/// - pad(len)
///
/// \example
/// \code
/// PROTOCOL_PARSER_DECL_STRUCT(
///     File,
///     mem(
///         static constexpr int max_fd = 3;
///     ),
///     pad(8),
///     field(u32_le, fd, i)
/// );
/// \endcode
///
/// Available log format:
///
/// - no for no log
/// - i for integer in decimal format (equivalent to "%d")
/// - x for integer in hexadecimal format (equivalent to "%x")
/// - c(replacement_char) for ascii char with unprintable char replaced by replacement_char
/// - s for "%.*s" with field.data() for %s and field.size() for the size
/// - named(to_string_fn) for "%s(%x)" with
///     protop_fmt::as_c_str(to_string_fn(value)) for %s and value for %x
/// - flags(to_string_fn) for "<%s>(%x)" with
///     protop_fmt::as_c_str(to_string_fn(value)) for %s and value for %x
/// - fn(to_string_fn) for "%s" with
///     protop_fmt::as_c_str(to_string_fn(value)) for %s
/// - str(fn) for "%.*s" with
///     fn(value).data() for %s and fn(value).size() for the size
#define PROTOCOL_PARSER_DECL_STRUCT(name, ...) \
    PROTOCOL_PARSER_DECL_STRUCT_I(name, PROTOCOL_PARSER_N_PARAM(__VA_ARGS__), __VA_ARGS__)
#define PROTOCOL_PARSER_DECL_STRUCT_I(name, n, ...) \
    PROTOCOL_PARSER_DECL_STRUCT_II(name, n, __VA_ARGS__)
#define PROTOCOL_PARSER_DECL_STRUCT_II(name, n, ...)                                         \
    struct name {                                                                            \
        PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_DECL_FIELD_, __VA_ARGS__)         \
                                                                                             \
        static constexpr unsigned pdu_min_len = 0                                            \
            PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_MIN_LEN_, __VA_ARGS__);       \
        static constexpr unsigned pdu_max_len = 0                                            \
            PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_MAX_LEN_, __VA_ARGS__);       \
                                                                                             \
        template<class = void> /* Non-templated function cannot have a requires clause */    \
        static constexpr unsigned pdu_len() noexcept                                         \
            requires(pdu_min_len == pdu_max_len)                                             \
        {                                                                                    \
            return pdu_max_len;                                                              \
        }                                                                                    \
                                                                                             \
        PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_MEMBER_, __VA_ARGS__)             \
                                                                                             \
        [[nodiscard]] bool read(InStream & in_stream) noexcept {                             \
            if (in_stream.in_remain() < pdu_min_len) {                                       \
                return false;                                                                \
            }                                                                                \
            read_unchecked(in_stream);                                                       \
            return true;                                                                     \
        }                                                                                    \
                                                                                             \
        [[nodiscard]] bool read(bytes_view data) noexcept {                                  \
            InStream in_stream{data};                                                        \
            return read(in_stream);                                                          \
        }                                                                                    \
                                                                                             \
        void read_unchecked(InStream & in_stream) noexcept {                                 \
            assert(in_stream.in_remain() >= pdu_min_len);                                    \
            PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_INPLACE_READ_, __VA_ARGS__)   \
            (void)in_stream; /* not used when n = 0 (no filed) */                            \
        }                                                                                    \
                                                                                             \
        void read_unchecked(bytes_view data) noexcept {                                      \
            InStream in_stream{data};                                                        \
            read_unchecked(in_stream);                                                       \
        }                                                                                    \
                                                                                             \
        static name from_unchecked_read(InStream & in_stream) noexcept {                     \
            PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_READ_, __VA_ARGS__)           \
            return name { PROTOCOL_PARSER_IMPL_MK_PARAMS(                                    \
                PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_USE_, __VA_ARGS__)        \
            ) };                                                                             \
            (void)in_stream; /* not used when n = 0 (no filed) */                            \
        }                                                                                    \
                                                                                             \
        static name from_unchecked_read(bytes_view data) noexcept {                          \
            InStream in_stream{data};                                                        \
            return from_unchecked_read(in_stream);                                           \
        }                                                                                    \
                                                                                             \
        void write_unchecked(OutStream & out_stream) const noexcept {                        \
            assert(out_stream.tailroom() >= pdu_max_len);                                    \
            PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_WRITE_, __VA_ARGS__)          \
            (void)out_stream; /* not used when n = 0 (no filed) */                           \
        }                                                                                    \
                                                                                             \
        [[nodiscard]] bool write(OutStream & out_stream) const noexcept {                    \
            if (out_stream.tailroom() < pdu_max_len) {                                       \
                return false;                                                                \
            }                                                                                \
            write_unchecked(out_stream);                                                     \
            return true;                                                                     \
        }                                                                                    \
                                                                                             \
        void log(char const* prefix, int priority = LOG_INFO) const noexcept {               \
            static constexpr auto fmt_obj = protop_fmt::str_concat(                          \
                "%s (%d/%d) -- %s" #name ":"                                                 \
                PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_LOG_TEXT_, __VA_ARGS__)   \
                ""                                                                           \
            );                                                                               \
            ::detail::LOG_REDEMPTION_INTERNAL(                                               \
                priority, fmt_obj.data, prefix                                               \
                PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_LOG_PARAM_, __VA_ARGS__)  \
            );                                                                               \
        }                                                                                    \
                                                                                             \
        void log_if(bool cond, char const* prefix, int priority = LOG_INFO) const noexcept { \
            if (cond) [[unlikely]] log(prefix, priority);                                    \
        }                                                                                    \
                                                                                             \
        static name make(PROTOCOL_PARSER_IMPL_MK_PARAMS(                                     \
            PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_MK_PARAM_, __VA_ARGS__)       \
        )) noexcept {                                                                        \
            return name {                                                                    \
                PROTOCOL_PARSER_FOR_EACH_##n (PROTOCOL_PARSER_IMPL_MK_VAR_, __VA_ARGS__)     \
            };                                                                               \
        }                                                                                    \
    }

// Proto Parser
namespace protop
{
    template<class E, class ProtoT>
    struct Enum
    {
        static constexpr unsigned pdu_max_len = ProtoT::pdu_max_len;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = E;

        static_assert(sizeof(E) == pdu_max_len);

        static E read(InStream & in_stream) noexcept
        {
            return static_cast<E>(ProtoT::read(in_stream));
        }

        static void write(OutStream & out_stream, E value) noexcept
        {
            auto v = static_cast<typename ProtoT::value_type>(value);
            ProtoT::write(out_stream, v);
        }
    };

    struct u8
    {
        template<class E> using as = Enum<E, u8>;

        static constexpr unsigned pdu_max_len = 1;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = uint8_t;

        static value_type read(InStream & in_stream) noexcept
        {
            return in_stream.in_uint8();
        }

        static void write(OutStream & out_stream, value_type value) noexcept
        {
            out_stream.out_uint8(value);
        }
    };

    struct u16_le
    {
        template<class E> using as = Enum<E, u16_le>;

        static constexpr unsigned pdu_max_len = 2;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = uint16_t;

        static value_type read(InStream & in_stream) noexcept
        {
            return in_stream.in_uint16_le();
        }

        static void write(OutStream & out_stream, value_type value) noexcept
        {
            out_stream.out_uint16_le(value);
        }
    };

    struct u32_le
    {
        template<class E> using as = Enum<E, u32_le>;

        static constexpr unsigned pdu_max_len = 4;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = uint32_t;

        static value_type read(InStream & in_stream) noexcept
        {
            return in_stream.in_uint32_le();
        }

        static void write(OutStream & out_stream, value_type value) noexcept
        {
            out_stream.out_uint32_le(value);
        }
    };

    struct u64_le
    {
        template<class E> using as = Enum<E, u64_le>;

        static constexpr unsigned pdu_max_len = 8;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = uint64_t;

        static value_type read(InStream & in_stream) noexcept
        {
            return in_stream.in_uint64_le();
        }

        static void write(OutStream & out_stream, value_type value) noexcept
        {
            out_stream.out_uint64_le(value);
        }
    };

    struct u16_be
    {
        template<class E> using as = Enum<E, u16_be>;

        static constexpr unsigned pdu_max_len = 2;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = uint16_t;

        static value_type read(InStream & in_stream) noexcept
        {
            return in_stream.in_uint16_be();
        }

        static void write(OutStream & out_stream, value_type value) noexcept
        {
            out_stream.out_uint16_be(value);
        }
    };

    struct u32_be
    {
        template<class E> using as = Enum<E, u32_be>;

        static constexpr unsigned pdu_max_len = 4;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = uint32_t;

        static value_type read(InStream & in_stream) noexcept
        {
            return in_stream.in_uint32_be();
        }

        static void write(OutStream & out_stream, value_type value) noexcept
        {
            out_stream.out_uint32_be(value);
        }
    };

    struct u64_be
    {
        template<class E> using as = Enum<E, u64_be>;

        static constexpr unsigned pdu_max_len = 8;
        static constexpr unsigned pdu_min_len = pdu_max_len;

        using value_type = uint64_t;

        static value_type read(InStream & in_stream) noexcept
        {
            return in_stream.in_uint64_be();
        }

        static void write(OutStream & out_stream, value_type value) noexcept
        {
            out_stream.out_uint64_be(value);
        }
    };

    template<class ProtoT>
    struct optional
    {
        static constexpr unsigned pdu_max_len = ProtoT::pdu_max_len;
        static constexpr unsigned pdu_min_len = 0;

        using value_type = typename ProtoT::value_type;

        static value_type read(InStream & in_stream) noexcept
        {
            if (in_stream.in_remain() >= pdu_max_len)
            {
                return ProtoT::read(in_stream);
            }
            else
            {
                return value_type{};
            }
        }

        static void write(OutStream & out_stream, value_type const& value) noexcept
        {
            ProtoT::write(out_stream, value);
        }
    };

} // namespace protop

namespace protop_fmt
{
    template<class T, char fmt>
    constexpr inline auto& to_print_fmt = to_print_fmt<std::underlying_type_t<T>, fmt>;

    template<> constexpr inline char to_print_fmt<uint8_t, 'x'>[] = "0x%02" PRIx16;
    template<> constexpr inline char to_print_fmt<uint8_t, 'i'>[] = "%" PRIu16;

    template<> constexpr inline char to_print_fmt<uint16_t, 'x'>[] = "0x%04" PRIx16;
    template<> constexpr inline char to_print_fmt<uint16_t, 'i'>[] = "%" PRIu16;

    template<> constexpr inline char to_print_fmt<uint32_t, 'x'>[] = "0x%08" PRIx32;
    template<> constexpr inline char to_print_fmt<uint32_t, 'i'>[] = "%" PRIu32;

    template<> constexpr inline char to_print_fmt<uint64_t, 'x'>[] = "0x%08" PRIx64;
    template<> constexpr inline char to_print_fmt<uint64_t, 'i'>[] = "%" PRIu64;


    struct StrRef
    {
        std::size_t len;
        char const* str;
    };

    template<std::size_t N>
    struct Str
    {
        constexpr Str() = default;

        constexpr Str(char const * s) noexcept
        {
            for (std::size_t i = 0; i < N; ++i)
            {
                data[i] = s[i];
            }
            data[N] = '\0';
        }

        static const std::size_t len = N;

        char data[N + 1];
    };

    template<class... S>
    constexpr auto str_concat(S const&... s) noexcept
    {
        Str<(... + sizeof(s)) - sizeof...(s)> str {};
        char* p = str.data;
        for (auto ref : {StrRef{sizeof(s) - 1, s}...})
        {
            for (std::size_t i = 0; i < ref.len; ++i)
            {
                *p++ = ref.str[i];
            }
        }
        return str;
    }

    inline char const * as_c_str(char const * s) noexcept
    {
        return s;
    }

    template<class T>
    char const * as_c_str(T const& obj) noexcept
    {
        return obj.c_str();
    }

    inline char const * to_bytes(char const * s) noexcept
    {
        return s;
    }

    inline char const * to_bytes(uint8_t const * s) noexcept
    {
        return reinterpret_cast<char const*>(s); // NOLINT
    }

    template<class T>
    char const * as_c_bytes(T const& obj) noexcept
    {
        return protop_fmt::to_bytes(obj.data());
    }

    template<class T>
    auto as_c_size(T const& obj) noexcept
    {
        return obj.size();
    }

    inline char to_ascii_char(char replacement_char, uint8_t c) noexcept
    {
        return (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : replacement_char;
    }

    inline char to_ascii_char(char replacement_char, char c) noexcept
    {
        return to_ascii_char(replacement_char, static_cast<uint8_t>(c));
    }

    template<std::size_t N>
    struct Buffer
    {
        Buffer() noexcept {}
        char buf[N];
    };
} // namespace protop_fmt


#define PROTOCOL_PARSER_FOR_EACH_0(f, p1)
#define PROTOCOL_PARSER_FOR_EACH_1(f, p1) f##p1
#define PROTOCOL_PARSER_FOR_EACH_2(f, p1, p2) f##p1 f##p2
#define PROTOCOL_PARSER_FOR_EACH_3(f, p1, p2, p3) f##p1 f##p2 f##p3
#define PROTOCOL_PARSER_FOR_EACH_4(f, p1, p2, p3, p4) f##p1 f##p2 f##p3 f##p4
#define PROTOCOL_PARSER_FOR_EACH_5(f, p1, p2, p3, p4, p5) \
    f##p1 f##p2 f##p3 f##p4 f##p5
#define PROTOCOL_PARSER_FOR_EACH_6(f, p1, p2, p3, p4, p5, p6) \
    f##p1 f##p2 f##p3 f##p4 f##p5 f##p6
#define PROTOCOL_PARSER_FOR_EACH_7(f, p1, p2, p3, p4, p5, p6, p7) \
    f##p1 f##p2 f##p3 f##p4 f##p5 f##p6 f##p7
#define PROTOCOL_PARSER_FOR_EACH_8(f, p1, p2, p3, p4, p5, p6, p7, p8) \
    f##p1 f##p2 f##p3 f##p4 f##p5 f##p6 f##p7 f##p8
#define PROTOCOL_PARSER_FOR_EACH_9(f, p1, p2, p3, p4, p5, p6, p7, p8, p9) \
    f##p1 f##p2 f##p3 f##p4 f##p5 f##p6 f##p7 f##p8 f##p9
#define PROTOCOL_PARSER_FOR_EACH_10(f, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) \
    f##p1 f##p2 f##p3 f##p4 f##p5 f##p6 f##p7 f##p8 f##p9 f##p10
#define PROTOCOL_PARSER_FOR_EACH_11(f, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) \
    f##p1 f##p2 f##p3 f##p4 f##p5 f##p6 f##p7 f##p8 f##p9 f##p10 f##p11

#define PROTOCOL_PARSER_N_PARAM(...) \
    PROTOCOL_PARSER_N_PARAM_I(__VA_ARGS__ __VA_OPT__(,) 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define PROTOCOL_PARSER_N_PARAM_I(f, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, n, ...) n

#define PROTOCOL_PARSER_IMPL_ignore(...)

#define PROTOCOL_PARSER_IMPL_DECL_FIELD_field(type, name, log_type) \
    protop::type::value_type name;
#define PROTOCOL_PARSER_IMPL_DECL_FIELD_pad PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_DECL_FIELD_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_MAX_LEN_field(type, name, log_type) + protop::type::pdu_max_len
#define PROTOCOL_PARSER_IMPL_MAX_LEN_pad(n) + n
#define PROTOCOL_PARSER_IMPL_MAX_LEN_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_MIN_LEN_field(type, name, log_type) + protop::type::pdu_min_len
#define PROTOCOL_PARSER_IMPL_MIN_LEN_pad(n) + n
#define PROTOCOL_PARSER_IMPL_MIN_LEN_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_INPLACE_READ_field(type, name, log_type) \
    name = protop::type::read(in_stream);
#define PROTOCOL_PARSER_IMPL_INPLACE_READ_pad(n) in_stream.in_skip_bytes(n);
#define PROTOCOL_PARSER_IMPL_INPLACE_READ_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_READ_field(type, name, log_type) \
    auto name = protop::type::read(in_stream);
#define PROTOCOL_PARSER_IMPL_READ_pad(n) in_stream.in_skip_bytes(n);
#define PROTOCOL_PARSER_IMPL_READ_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_USE_field(type, name, log_type) , name
#define PROTOCOL_PARSER_IMPL_USE_pad PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_USE_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_WRITE_field(type, name, log_type) \
    protop::type::write(out_stream, name);
#define PROTOCOL_PARSER_IMPL_WRITE_pad(n) out_stream.out_clear_bytes(n);
#define PROTOCOL_PARSER_IMPL_WRITE_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_MK_PARAMS(expr) PROTOCOL_PARSER_IMPL_MK_PARAMS_I(expr)
#define PROTOCOL_PARSER_IMPL_MK_PARAMS_I(dummy, ...) __VA_ARGS__
#define PROTOCOL_PARSER_IMPL_MK_PARAM_field(type, name, log_type) , protop::type::value_type name
#define PROTOCOL_PARSER_IMPL_MK_PARAM_pad PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_MK_PARAM_mem PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_MK_VAR_field(type, name, log_type) name,
#define PROTOCOL_PARSER_IMPL_MK_VAR_pad PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_MK_VAR_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_LOG_TEXT_field(type, name, log_type) \
  PROTOCOL_PARSER_IMPL_LOG_VAR_ ## log_type (name) \
  PROTOCOL_PARSER_IMPL_LOG_TEXT_ ## log_type (name)
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_pad PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_LOG_PARAM_field(type, name, log_type) \
  PROTOCOL_PARSER_IMPL_LOG_PARAM_1_ ## log_type (name) \
  PROTOCOL_PARSER_IMPL_LOG_PARAM_2_ ## log_type (name)
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_pad PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_mem PROTOCOL_PARSER_IMPL_ignore

#define PROTOCOL_PARSER_IMPL_LOG_STR_NAME(name) " " #name "="
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_NAME(name) , name
#define PROTOCOL_PARSER_IMPL_LOG_NAME(name) (name)
#define PROTOCOL_PARSER_IMPL_LOG_NAME_TO_VALUE(name) (name).value()
#define PROTOCOL_PARSER_IMPL_LOG_OPEN (
#define PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE(name) (name))
#define PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE2(name) (name)))
#define PROTOCOL_PARSER_IMPL_LOG_NONAME(name)

#define PROTOCOL_PARSER_IMPL_LOG_VAR_no PROTOCOL_PARSER_IMPL_LOG_NONAME
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_no PROTOCOL_PARSER_IMPL_LOG_NONAME
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_no PROTOCOL_PARSER_IMPL_LOG_NONAME
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_no PROTOCOL_PARSER_IMPL_LOG_NONAME

#define PROTOCOL_PARSER_IMPL_LOG_VAR_x PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_FMT_x(name) \
    "", ::protop_fmt::to_print_fmt<decltype(name), 'x'>,
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_x PROTOCOL_PARSER_IMPL_FMT_x
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_x PROTOCOL_PARSER_IMPL_LOG_PARAM_NAME
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_x PROTOCOL_PARSER_IMPL_LOG_NONAME

#define PROTOCOL_PARSER_IMPL_LOG_VAR_i PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_FMT_i(name) \
    "", ::protop_fmt::to_print_fmt<decltype(name), 'i'>,
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_i PROTOCOL_PARSER_IMPL_FMT_i
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_i PROTOCOL_PARSER_IMPL_LOG_PARAM_NAME
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_i PROTOCOL_PARSER_IMPL_LOG_NONAME

#define PROTOCOL_PARSER_IMPL_LOG_VAR_c(replacement_char) PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_LOG_c(name) "%c"
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_c(replacement_char) PROTOCOL_PARSER_IMPL_LOG_c
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_c(replacement_char)    \
    , ::protop_fmt::to_ascii_char PROTOCOL_PARSER_IMPL_LOG_OPEN \
    replacement_char, PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE2
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_c(replacement_char) PROTOCOL_PARSER_IMPL_LOG_NONAME

#define PROTOCOL_PARSER_IMPL_LOG_VAR_s PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_LOG_s(name) "%.*s"
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_s PROTOCOL_PARSER_IMPL_LOG_s
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_s                  \
    , ::protop_fmt::as_c_size PROTOCOL_PARSER_IMPL_LOG_OPEN \
    PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_s                   \
    , ::protop_fmt::as_c_bytes PROTOCOL_PARSER_IMPL_LOG_OPEN \
    PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE

#define PROTOCOL_PARSER_IMPL_LOG_VAR_named(fn) PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_LOG_named(name) \
    "%s(", ::protop_fmt::to_print_fmt<decltype(name), 'x'>, ")"
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_named(fn) PROTOCOL_PARSER_IMPL_LOG_named
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_named(fn)         \
    , ::protop_fmt::as_c_str PROTOCOL_PARSER_IMPL_LOG_OPEN \
    fn PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_named(fn) PROTOCOL_PARSER_IMPL_LOG_PARAM_NAME

#define PROTOCOL_PARSER_IMPL_LOG_VAR_flags(fn) PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_LOG_flags(name) \
    "<%s>(", ::protop_fmt::to_print_fmt<decltype(name), 'x'>, ")"
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_flags(fn) PROTOCOL_PARSER_IMPL_LOG_flags
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_flags PROTOCOL_PARSER_IMPL_LOG_PARAM_1_named
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_flags(fn) PROTOCOL_PARSER_IMPL_LOG_PARAM_NAME

#define PROTOCOL_PARSER_IMPL_LOG_VAR_fn(fn) PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_LOG_fn(name) "%s"
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_fn(fn) PROTOCOL_PARSER_IMPL_LOG_fn
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_fn PROTOCOL_PARSER_IMPL_LOG_PARAM_1_named
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_fn(fn) PROTOCOL_PARSER_IMPL_LOG_NONAME

#define PROTOCOL_PARSER_IMPL_LOG_VAR_str(fn) PROTOCOL_PARSER_IMPL_LOG_STR_NAME
#define PROTOCOL_PARSER_IMPL_LOG_str(name) "%.*s"
#define PROTOCOL_PARSER_IMPL_LOG_TEXT_str(fn) PROTOCOL_PARSER_IMPL_LOG_str
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_1_str(fn)          \
    , static_cast<int> PROTOCOL_PARSER_IMPL_LOG_OPEN      \
    ::protop_fmt::as_c_size PROTOCOL_PARSER_IMPL_LOG_OPEN \
    fn PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE2
#define PROTOCOL_PARSER_IMPL_LOG_PARAM_2_str(fn)             \
    , ::protop_fmt::as_c_bytes PROTOCOL_PARSER_IMPL_LOG_OPEN \
    fn PROTOCOL_PARSER_IMPL_LOG_NAME_AND_CLOSE

#define PROTOCOL_PARSER_IMPL_MEMBER_field PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_MEMBER_pad PROTOCOL_PARSER_IMPL_ignore
#define PROTOCOL_PARSER_IMPL_MEMBER_mem(...) __VA_ARGS__

namespace protop
{

template<class... Pdu>
struct MultiPdus : Pdu...
{
    static constexpr unsigned pdu_min_len = (Pdu::pdu_min_len + ... + 0);
    static constexpr unsigned pdu_max_len = (Pdu::pdu_max_len + ... + 0);

    template<class = void> /* Non-templated function cannot have a requires clause */
    static constexpr unsigned pdu_len() noexcept
        requires(pdu_min_len == pdu_max_len)
    {
        return pdu_max_len;
    }

    [[nodiscard]] bool read(InStream & in_stream) noexcept
    {
        if (in_stream.in_remain() < pdu_min_len)
        {
            return false;
        }
        read_unchecked(in_stream);
        return true;
    }

    [[nodiscard]] bool read(bytes_view data) noexcept
    {
        InStream in_stream{data};
        return read(in_stream);
    }

    void read_unchecked(InStream & in_stream) noexcept
    {
        assert(in_stream.in_remain() >= pdu_min_len);
        (..., static_cast<Pdu&>(*this).read_unchecked());
        (void)in_stream; /* not used when n = 0 (no filed) */
    }

    void read_unchecked(bytes_view data) noexcept
    {
        InStream in_stream{data};
        read_unchecked(in_stream);
    }

    static MultiPdus from_unchecked_read(InStream & in_stream) noexcept
    {
        return MultiPdus { Pdu::from_unchecked_read(in_stream)... };
        (void)in_stream; /* not used when n = 0 (no filed) */
    }

    static MultiPdus from_unchecked_read(bytes_view data) noexcept
    {
        InStream in_stream{data};
        return from_unchecked_read(in_stream);
    }

    void write_unchecked(OutStream & out_stream) const noexcept
    {
        assert(out_stream.tailroom() >= pdu_max_len);
        (..., static_cast<Pdu const&>(*this).write_unchecked(out_stream));
        (void)out_stream; /* not used when n = 0 (no filed) */
    }

    [[nodiscard]] bool write(OutStream & out_stream) const noexcept
    {
        if (out_stream.tailroom() < pdu_max_len) {
            return false;
        }
        write_unchecked(out_stream);
        return true;
    }

    void log(char const* prefix, int priority = LOG_INFO) const noexcept
    {
        (..., static_cast<Pdu const&>(*this).log(prefix, priority));
    }

    void log_if(bool cond, char const* prefix, int priority = LOG_INFO) const noexcept
    {
        if (cond) [[unlikely]] log(prefix, priority);
    }
};

}
