/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "translation/gettext.hpp"
#include "utils/stream.hpp"
#include "utils/strutils.hpp"
#include "utils/ascii.hpp"
#include "utils/sugar/chars_to_int.hpp"
#include "utils/sugar/split.hpp"

#include <cstring>


namespace
{

// f(token, type, prior, is_left_to_right)
#define REDEMPTION_GETTEXT_DEFS(f)            \
    f(Num, Expr, 0, false) /* [0-9]+ */       \
    f(Id, Expr, 0, false) /* 'n' */           \
    f(NEq, BinaryOp, 30, true) /* != */       \
    f(Eq, BinaryOp, 30, true) /* == */        \
    f(LT, BinaryOp, 40, true) /* < */         \
    f(GT, BinaryOp, 40, true) /* > */         \
    f(LE, BinaryOp, 40, true) /* <= */        \
    f(GE, BinaryOp, 40, true) /* >= */        \
    f(Not, UnaryOp, 90, false) /* ! */        \
    f(Mul, BinaryOp, 80, true) /* * */        \
    f(Div, BinaryOp, 80, true) /* / */        \
    f(Mod, BinaryOp, 80, true) /* % */        \
    f(Add, BinaryOp, 60, true) /* + binary */ \
    f(Sub, BinaryOp, 60, true) /* - binary */ \
    f(Plus, UnaryOp, 90, false) /* + unary */ \
    f(Neg, UnaryOp, 90, false) /* - unary */  \
    f(And, BinaryOp, 20, true) /* && */       \
    f(Or, BinaryOp, 10, true) /* || */        \
    f(TernIf, BinaryOp, 0, false) /* ? */     \
    f(TernElse, BinaryOp, 0, false) /* : */   \
    f(Open, UnaryOp, 0, false) /* ( */        \
    f(Close, Expr, 100, false) /* ) */

enum class Token : uint8_t
{
#define MK_TOKEN(token, type, prior, is_left_to_right) token,
    REDEMPTION_GETTEXT_DEFS(MK_TOKEN)
#undef MK_TOKEN
};

enum class ExprType : uint8_t
{
    UnaryOp,
    BinaryOp,
    Expr,
};

struct TokenDesc
{
    ExprType type;
    Token token;
    uint8_t prior;
    bool is_left_to_right;
};

namespace tokendesc
{
#define MK_DESC(token, type, prior, is_left_to_right) \
    constexpr auto token = TokenDesc{ExprType::type, Token::token, prior, is_left_to_right};

    REDEMPTION_GETTEXT_DEFS(MK_DESC)
#undef MK_TOKEN
}

constexpr bool IS_VALID_TRANSITIONS_TABLE[3][3]{
    // left / right = UnaryOp  BinaryOp  Expr
    /* UnaryOp */    {true,     false,   true},
    /* BinaryOp */   {true,     false,   true},
    /* Expr */       {false,    true,    false},
};

} // namespace

struct GettextPlural::ItemImpl : GettextPlural::Item
{
    static constexpr unsigned bit_shift = 6;
    static constexpr unsigned data_mask = ~uint_type() >> (sizeof(uint_type) * 8 - bit_shift);
    static constexpr uint_type max_value = ~uint_type() << bit_shift >> bit_shift;

    static ItemImpl make(Token token, uint_type num = 0)
    {
        return ItemImpl{static_cast<uint_type>(token) | (num << bit_shift)};
    }

    Token token() const
    {
        return Token(data & data_mask);
    }

    uint_type number() const
    {
        return (data >> bit_shift) & max_value;
    }
};


MoMsgStrIterator::MoMsgStrIterator(chars_view messages) noexcept
: p(messages.data())
, len(messages.size())
{
}

bool MoMsgStrIterator::has_value() const noexcept
{
    return len;
}

chars_view MoMsgStrIterator::next() noexcept
{
    auto n = strnlen(p, len);
    chars_view s{p, n};
    if (n != len) {
        ++n;
    }
    p += n;
    len -= n;
    return s;
}


array_view<GettextPlural::ItemImpl> GettextPlural::items() const noexcept
{
    return {
        static_cast<ItemImpl const*>(m_stack + 0),
        static_cast<ItemImpl const*>(m_stack + m_stack_len),
    };
}

/*
expr = n ? 1 : 2
=> offset = 0    1                 2       3                   4      5
           {Id} {TernIf,offset=4} {Num=1} {TernElse,offset=5} {Num=2} end
                               |___________________________|___|      |
                                       offset from 0       |__________|
                                                           offset from 0
*/
char const* GettextPlural::parse(chars_view s) noexcept
{
    struct OpStack
    {
        Token token;
        uint8_t prior;
    };
    uint8_t ternary_offset_stack[stack_capacity / 2];
    OpStack op_stack[stack_capacity];
    ExprType previous_type = ExprType::UnaryOp;
    op_stack[0] = {Token{}, 0};

    auto* output_it = m_stack;
    auto* op_it = op_stack;
    auto* ternary_offset_it = ternary_offset_stack;

    auto output_size = [&]{
        return output_it - std::begin(m_stack);
    };

    auto op_stack_size = [&]{
        return op_it - std::begin(op_stack);
    };

    #define CHECK_REMAINING_SPACE(n) do {                                           \
        if (output_size() + op_stack_size() + (n) >= stack_capacity) [[unlikely]] { \
            return p;                                                               \
        }                                                                           \
    } while (0)

    auto is_valid_transition = [&](ExprType t){
        return IS_VALID_TRANSITIONS_TABLE[underlying_cast(previous_type)][underlying_cast(t)];
    };

    auto close_ternary = [&]{
        for (;;) {
            while (op_it->prior) {
                assert(op_it != std::begin(op_stack));
                if (op_it->token != Token::Plus) {
                    *output_it++ = ItemImpl::make(op_it->token);
                }
                --op_it;
            }

            if (op_it->token != Token::TernElse) {
                break;
            }

            // update TernElse position from output
            --op_it;
            --ternary_offset_it;
            const auto offset_expr = checked_int(output_size());
            m_stack[*ternary_offset_it] = ItemImpl::make(Token::TernElse, offset_expr);
        }
    };

    auto const* p = s.begin();
    auto const* end = s.end();
    for (; p != end; ++p) {
        TokenDesc desc;

        #define CASE_OP(c, op) case c: desc = tokendesc::op; break

        #define CASE_OP2(c1, op1, c2, op2)       \
            case c1:                             \
                if (p + 1 < end && p[1] == c2) { \
                    ++p;                         \
                    desc = tokendesc::op2;       \
                }                                \
                else {                           \
                    desc = tokendesc::op1;       \
                }                                \
                break

        #define CASE_OP_2C(c1, c2, op2)          \
            case c1:                             \
                if (p + 1 < end && p[1] == c2) { \
                    ++p;                         \
                    desc = tokendesc::op2;       \
                }                                \
                else {                           \
                    return p;                    \
                }                                \
                break

        switch (*p) {
            case ' ':
            case '\t':
                continue;

            case ';':
                goto after_loop;

            case 'n':
                CHECK_REMAINING_SPACE(1);
                desc = tokendesc::Id;
                *output_it++ = ItemImpl::make(Token::Id);
                previous_type = desc.type;
                continue;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                CHECK_REMAINING_SPACE(1);

                desc = tokendesc::Num;
                if (!is_valid_transition(desc.type)) [[unlikely]] {
                    return p;
                }

                if (auto r = decimal_chars_to_int<uint32_t>(s.from_offset(p))) {
                    if (r.val <= ItemImpl::max_value) {
                        *output_it++ = ItemImpl::make(Token::Num, r.val);
                        previous_type = desc.type;
                        p = r.ptr - 1;
                        continue;
                    }
                }
                return p;

            CASE_OP_2C('=', '=', Eq);
            CASE_OP_2C('&', '&', And);
            CASE_OP_2C('|', '|', Or);
            CASE_OP2('!', Not, '=', NEq);
            CASE_OP2('<', LT, '=', LE);
            CASE_OP2('>', GT, '=', GE);
            CASE_OP('*', Mul);
            CASE_OP('/', Div);
            CASE_OP('%', Mod);
            CASE_OP('?', TernIf);
            CASE_OP(':', TernElse);

            case '+':
                desc = (previous_type == ExprType::Expr) ? tokendesc::Add : tokendesc::Plus;
                break;

            case '-':
                desc = (previous_type == ExprType::Expr) ? tokendesc::Sub : tokendesc::Neg;
                break;

            case '(':
                CHECK_REMAINING_SPACE(1);

                desc = tokendesc::Open;
                if (!is_valid_transition(desc.type)) [[unlikely]] {
                    return p;
                }
                *++op_it = {desc.token, desc.prior};
                continue;

            case ')':
                desc = tokendesc::Close;

                close_ternary();

                if (op_it->token != Token::Open) {
                    return p;
                }
                --op_it;
                previous_type = desc.type;
                continue;

            default:
                return p;
        }

        #undef CASE_OP
        #undef CASE_OP2
        #undef CASE_OP_2C

        CHECK_REMAINING_SPACE(1);

        if (!is_valid_transition(desc.type)) [[unlikely]] {
            return p;
        }

        previous_type = desc.type;
        while (op_it->prior > desc.prior || (op_it->prior == desc.prior && desc.is_left_to_right)) {
            assert(op_it != std::begin(op_stack));
            *output_it++ = ItemImpl::make(op_it->token);
            --op_it;
        }
        *++op_it = {desc.token, desc.prior};

        if (*p == '?') {
            CHECK_REMAINING_SPACE(1);
            *ternary_offset_it++ = checked_int(output_size());
            *output_it++ = ItemImpl::make(desc.token);
        }
        else if (*p == ':') {
            if (ternary_offset_it == ternary_offset_stack) {
                return p;
            }

            --op_it;
            close_ternary();
            if (op_it->token != Token::TernIf) {
                return p;
            }

            // replace TernIf with TernElse and set a new output position

            *op_it = {desc.token, desc.prior};

            const auto offset_expr = output_size();
            *output_it++ = ItemImpl::make(desc.token);

            m_stack[*(ternary_offset_it - 1)] = ItemImpl::make(Token::TernIf, checked_int(offset_expr + 1));
            *(ternary_offset_it - 1) = checked_int(offset_expr);
            continue;
        }
    }

    after_loop:
    if (previous_type != ExprType::Expr) {
        // empty expression, force to 0
        if (output_size() + op_stack_size() == 0) {
            *output_it++ = ItemImpl::make(Token::Num, 0);
            return nullptr;
        }
        return p;
    }

    close_ternary();

    if (op_it != op_stack) {
        return p;
    }

    m_stack_len = checked_int(output_size());
    return nullptr;
}

unsigned long GettextPlural::eval(unsigned long n) const noexcept
{
    unsigned long stack[GettextPlural::stack_capacity / 2 + 1];
    unsigned long* p = stack;
    *p = 0;

    auto av = items();
    auto it = av.begin();
    auto end = av.end();
    while (it < end) {
        #define CASE_OP2(token, op) \
            case Token::token:      \
                assert(p != stack); \
                --p;                \
                *p = *p op p[1];    \
                break

        switch (it->token()) {
            CASE_OP2(Mul, *);
            CASE_OP2(Div, /);
            CASE_OP2(Mod, %);
            CASE_OP2(Add, +);
            CASE_OP2(Sub, -);

            CASE_OP2(LT, <);
            CASE_OP2(GT, >);
            CASE_OP2(LE, <=);
            CASE_OP2(GE, >=);
            CASE_OP2(Eq, ==);
            CASE_OP2(NEq, !=);

            CASE_OP2(And, &&);
            CASE_OP2(Or, ||);

            case Token::Not: *p = !*p; break;
            case Token::Neg: *p = -*p; break;

            case Token::Id: *++p = n; break;
            case Token::Num: *++p = it->number(); break;

            case Token::TernIf:
                if (*p) {
                    break;
                }
                [[fallthrough]];
            case Token::TernElse: {
                auto new_it = av.begin() + it->number();
                assert(it < new_it);
                it = new_it;
                continue;
            }

            case Token::Open:
            case Token::Plus:
            case Token::Close:
                assert(false);
                break;
        }
        #undef CASE_OP2

        ++it;
    }

    return *p;
}

/*
https://www.gnu.org/software/gettext/manual/html_node/MO-Files.html

        byte
             +------------------------------------------+
          0  | magic number = 0x950412de (little endian)|
             |                                          |
          4  | file format major = 0                    |
          6  | file format minor = 0                    |
             |                                          |
          8  | number of strings                        |  == N
             |                                          |
         12  | offset of table with original strings    |  == O
             |                                          |
         16  | offset of table with translation strings |  == T
             |                                          |
         20  | size of hashing table                    |  == S
             |                                          |
         24  | offset of hashing table                  |  == H
             |                                          |
             .                                          .
             .    (possibly more entries later)         .
             .                                          .
             |                                          |
          O  | length & offset 0th string  ----------------.
      O + 8  | length & offset 1st string  ------------------.
              ...                                    ...   | |
O + ((N-1)*8)| length & offset (N-1)th string           |  | |
             |                                          |  | |
          T  | length & offset 0th translation  ---------------.
      T + 8  | length & offset 1st translation  -----------------.
              ...                                    ...   | | | |
T + ((N-1)*8)| length & offset (N-1)th translation      |  | | | |
             |                                          |  | | | |
          H  | start hash table                         |  | | | |
              ...                                    ...   | | | |
  H + S * 4  | end hash table                           |  | | | |
             |                                          |  | | | |
             | NUL terminated 0th string  <----------------' | | |
             |                                          |    | | |
             | NUL terminated 1st string  <------------------' | |
             |                                          |      | |
              ...                                    ...       | |
             |                                          |      | |
             | NUL terminated 0th translation  <---------------' |
             |                                          |        |
             | NUL terminated 1st translation  <-----------------'
             |                                          |
              ...                                    ...
             |                                          |
             +------------------------------------------+
*/
MoParserError parse_mo(
    bytes_view buf,
    FunctionRef<bool(uint32_t msgcount, uint32_t nplurals, chars_view plural_expr)> init,
    FunctionRef<bool(chars_view msgid, chars_view msgid_plurals, MoMsgStrIterator msgs)> push_msg
)
{
    constexpr uint32_t LE_MAGIC = 0x950412de;
    constexpr uint32_t BE_MAGIC = 0xde120495;

    if (buf.size() < 5 * 4) {
        return MoParserError{MoParserErrorCode::InvalidFormat, 0};
    }

    Parse in_stream(buf.data());
    auto const magic = in_stream.in_uint32_le();

    bool is_little_endian;
    if (magic == LE_MAGIC) {
        is_little_endian = true;
    }
    else if (magic == BE_MAGIC) {
        is_little_endian = false;
    }
    else {
        return MoParserError{MoParserErrorCode::BadMagicNumber, magic};
    }

    auto read_u32 = [&](Parse& p){
        return is_little_endian ? p.in_uint32_le() : p.in_uint32_be();
    };

    const auto version = read_u32(in_stream);
    auto msgcount = read_u32(in_stream);
    const auto masteridx = read_u32(in_stream);
    const auto transidx = read_u32(in_stream);

    if ((version >> 16) > 1) {
        return MoParserError{MoParserErrorCode::BadVersionNumber, version};
    }

    if (masteridx + msgcount * 4ull >= buf.size() || transidx + msgcount * 4ull >= buf.size()) {
        return MoParserError{MoParserErrorCode::InvalidFormat, 0};
    }

    Parse in_mdata(buf.data() + masteridx);
    Parse in_tdata(buf.data() + transidx);

    unsigned nplurals = 1;
    chars_view plural_expr = ""_av;

    if (msgcount <= 0) [[unlikely]] {
        init(0, 0, ""_av);
        return MoParserError{MoParserErrorCode::NoError, 0};
    }

    struct MsgHeader
    {
        uint32_t mlen;
        uint64_t moff; // u64 for avoid overflow
        uint32_t tlen;
        uint64_t toff; // u64 for avoid overflow

        bool in(bytes_view buf) const
        {
            const auto mend = moff + mlen;
            const auto tend = toff + tlen;
            return mend < buf.size() && tend < buf.size();
        }
    };

    auto read_msg_header = [&]{
        return MsgHeader{
            read_u32(in_mdata),
            read_u32(in_mdata),
            read_u32(in_tdata),
            read_u32(in_tdata),
        };
    };

    // see if we're looking at GNU .mo conventions for metadata
    if (Parse in_mdata_tmp(in_mdata); read_u32(in_mdata_tmp) == 0) {
        --msgcount;

        const auto header = read_msg_header();
        if (!header.in(buf)) [[unlikely]] {
            return MoParserError{MoParserErrorCode::InvalidFormat, 0};
        }
        const auto [mlen, moff, tlen, toff] = header;
        const auto tptr = buf.as_charp() + toff;

        for (auto line : split_with(chars_view{tptr, tlen}, '\n')) {
            line = trim(line);
            if (line.empty()) {
                continue;
            }

            // skip over comment lines:
            if (utils::starts_with(line, "#-#-#-#-#"_av)
                && utils::ends_with(line, "#-#-#-#-#"_av)
            ) {
                continue;
            }

            constexpr auto plural_forms = "plural-forms"_ascii_upper;

            // search "((?i)Plural-Forms\s*:)"
            //@{
            if (!insensitive_starts_with(line, plural_forms)) {
                continue;
            }

            line = line.from_offset(plural_forms.size());
            if (line.empty() || (line[0] != ':' && line[0] != ' ' && line[0] != '\t')) {
                continue;
            }
            //@}

            plural_expr = line;
        }

        if (!plural_expr.empty()) {
            auto consume = [&](auto... cs){
                for (char c : plural_expr) {
                    if (((cs != c) && ...)) {
                        break;
                    }
                    plural_expr = plural_expr.drop_front(1);
                }
            };

            consume(' ', '\t', ':');

            auto read_s = [&](chars_view s){
                if (utils::starts_with(plural_expr, s)) {
                    plural_expr = plural_expr.drop_front(s.size());
                    return true;
                }
                return false;
            };

            if (read_s("nplurals="_av)) {
                if (auto r = decimal_chars_to_int(plural_expr, nplurals)) {
                    plural_expr = plural_expr.from_offset(r.ptr);
                    if (!nplurals) {
                        nplurals = 1;
                    }
                }
                else {
                    return MoParserError{MoParserErrorCode::InvalidNPlurals, 0};
                }
            }

            consume(' ', '\t', ';');
            if (!read_s("plural="_av)) {
                plural_expr = {};
            }
        }
    }

    if (!init(msgcount, nplurals, plural_expr)) {
        return MoParserError{MoParserErrorCode::InitError, 0};
    }

    // read all messages from the .mo file buffer into the catalog.
    for (uint64_t i = 0; i < msgcount; ++i) {
        const auto header = read_msg_header();
        if (!header.in(buf)) [[unlikely]] {
            return MoParserError{MoParserErrorCode::InvalidFormat, 0};
        }
        const auto [mlen, moff, tlen, toff] = header;
        const auto mptr = buf.as_charp() + moff;
        const auto tptr = buf.as_charp() + toff;

        // assume null terminal
        if (mptr[mlen] || tptr[tlen]) {
            return MoParserError{MoParserErrorCode::InvalidMessage, 0};
        }

        const auto n = strnlen(mptr, mlen);
        chars_view msgid{mptr, n};
        chars_view msgid_plurals = (n < mlen)
            ? chars_view(mptr, mlen).from_offset(n + 1)
            : chars_view{};
        auto msgs = zstring_view::from_null_terminated(tptr, tlen);
        if (!push_msg(msgid, msgid_plurals, MoMsgStrIterator(msgs))) {
            return MoParserError{MoParserErrorCode::InvalidMessage, 0};
        }
    }

    return MoParserError{MoParserErrorCode::NoError, 0};
}
