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

    class plural_1_neq_n {};
    constexpr GettextPlural(plural_1_neq_n) noexcept
      : m_stack_len(3)
      , m_stack{}
    {
        // 1 + n (see .cpp). Checked in test_gettext.cpp
        m_stack[0].data = 1 << 6; // num = 1
        m_stack[1].data = 1; // id (n)
        m_stack[2].data = 2; // binary +
    }

    /// \return error position
    char const* parse(chars_view s) noexcept;

    unsigned long eval(unsigned long n) const noexcept;

    struct ItemImpl;
    array_view<ItemImpl> items() const noexcept;

private:
    struct Item
    {
        using uint_type = uint32_t;
        uint_type data;
    };

    uint32_t m_stack_len = 0;
    Item m_stack[stack_capacity];
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

MoParserError parse_mo(bytes_view data,
    FunctionRef<bool(uint32_t msgcount, uint32_t nplurals, chars_view plural_expr)> init,
    FunctionRef<bool(chars_view msgid, chars_view msgid_plurals, MoMsgStrIterator msgs)> push_msg);
