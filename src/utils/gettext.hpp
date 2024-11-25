/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bytes_view.hpp"
#include "utils/function_ref.hpp"


struct GettextPlural
{
    static constexpr unsigned stack_capacity = 64;

    GettextPlural() noexcept
    {}

    class constexpr_t {};
    constexpr GettextPlural(constexpr_t) noexcept
      : m_output_len(0)
      , m_output{}
    {}

    /// \return error position
    char const* parse(chars_view s);

    unsigned long eval(unsigned long n);

    struct ItemImpl;
    array_view<ItemImpl> items();

private:
    struct Item
    {
        using uint_type = uint32_t;
        uint_type data;
    };

    uint32_t m_output_len = 0;
    Item m_output[stack_capacity];
};


enum class [[nodiscard]] MoParserErrorCode : uint8_t
{
    NoError,
    BadMagicNumber,
    BadVersionNumber,
    InvalidFormat,
    InvalidNPlurals,
    InitError,
    InvalidMessage,
};

struct [[nodiscard]] MoParserError
{
    MoParserErrorCode ec;
    uint32_t number;
};

struct MoMsgStrIterator
{
    MoMsgStrIterator(chars_view messages) noexcept;

    bool has_value() const noexcept;
    chars_view next() noexcept;

    chars_view raw_messages() const noexcept
    {
        return {p, len};
    }

private:
    char const* p;
    std::size_t len;
};

struct MoParserCallables
{
    FunctionRef<bool(uint32_t msgcount, uint32_t nplurals, chars_view plural_expr)> init;
    FunctionRef<bool(chars_view msgid, chars_view msgid_plurals, MoMsgStrIterator msgs)> push_msg;
};

MoParserError parse_mo(bytes_view data, MoParserCallables callables);
