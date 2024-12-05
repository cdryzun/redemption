/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2010
   Author(s): Christophe Grosjean, Javier Caverni, Meng Tan,
              Jennifer Inthavong
   Based on xrdp Copyright (C) Jay Sorg 2004-2010

   Unit test for Lightweight UTF library

*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/file.hpp"

#include "translation/gettext.hpp"
#include "translation/gettext.cpp"
#include "utils/sugar/int_to_chars.hpp"

#include <string_view>
#include <string>
#include <vector>

namespace
{
    char const *token_to_string(Token t)
    {
    #define MK_TOKEN_CASE(token, type, prior, is_left_to_right) case Token::token: return #token;
        switch (t) {
            REDEMPTION_GETTEXT_DEFS(MK_TOKEN_CASE)
        }
    #undef MK_TOKEN_CASE
        return "Token::???";
    }

    struct Msg
    {
        std::string_view msgid;
        std::string_view msgid_plurals;
        std::string_view msgs;

        friend bool operator == (Msg const& a, Msg const& b) noexcept
        {
            return a.msgid == b.msgid && a.msgid_plurals == b.msgid_plurals && a.msgs == b.msgs;
        }
    };

    using MsgsAV = array_view<Msg>;
} // anonymous namespace

#if !REDEMPTION_UNIT_TEST_FAST_CHECK
# include "test_only/test_framework/compare_collection.hpp"
# include <algorithm>

static ut::assertion_result test_comp_patt_results(MsgsAV a, MsgsAV b)
{
    auto putseq = [&](std::ostream& out, MsgsAV results){
        if (results.empty()) {
            out << "{}";
            return;
        }
        for (auto result : results) {
            std::string msgs;
            msgs.reserve(result.msgs.size() * 4);
            for (char c : result.msgs) {
                if (c) {
                    msgs += c;
                }
                else {
                    msgs += "\\x00";
                }
            }
            out << "{"
                ".msgid=\"" << result.msgid << "\", "
                ".msgid_plurals=\"" << result.msgid_plurals << "\", "
                ".msgs=\""<< msgs << "\""
                "}, "
            ;
        }
    };

    return ut::ops::compare_collection_EQ(a, b, [&](
        std::ostream& out, size_t /*pos*/, char const* op
    ){
        ut::put_data_with_diff(out, a, op, b, putseq);
    });
}

RED_TEST_DISPATCH_COMPARISON_EQ((), (::MsgsAV), (::MsgsAV), ::test_comp_patt_results)
#endif


RED_AUTO_TEST_CASE(TestParseMO)
{
    auto mo_file =
        "\xDE\x12\x04\x95\x00\x00\x00\x00\x05\x00\x00\x00\x1C\x00\x00\x00"
        "\x44\x00\x00\x00\x07\x00\x00\x00\x6C\x00\x00\x00\x00\x00\x00\x00"
        "\x88\x00\x00\x00\x04\x00\x00\x00\x89\x00\x00\x00\x10\x00\x00\x00"
        "\x8E\x00\x00\x00\x10\x00\x00\x00\x9F\x00\x00\x00\x10\x00\x00\x00"
        "\xB0\x00\x00\x00\x88\x01\x00\x00\xC1\x00\x00\x00\x04\x00\x00\x00"
        "\x4A\x02\x00\x00\x0B\x00\x00\x00\x4F\x02\x00\x00\x23\x00\x00\x00"
        "\x5B\x02\x00\x00\x0B\x00\x00\x00\x7F\x02\x00\x00\x01\x00\x00\x00"
        "\x05\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00"
        "\x02\x00\x00\x00\x03\x00\x00\x00\x00\x6D\x73\x67\x31\x00\x6D\x73"
        "\x67\x32\x00\x6D\x73\x67\x32\x5F\x70\x6C\x75\x72\x61\x6C\x00\x6D"
        "\x73\x67\x33\x00\x6D\x73\x67\x33\x5F\x70\x6C\x75\x72\x61\x6C\x00"
        "\x6D\x73\x67\x34\x00\x6D\x73\x67\x34\x5F\x70\x6C\x75\x72\x61\x6C"
        "\x00\x50\x72\x6F\x6A\x65\x63\x74\x2D\x49\x64\x2D\x56\x65\x72\x73"
        "\x69\x6F\x6E\x3A\x20\x50\x41\x43\x4B\x41\x47\x45\x20\x56\x45\x52"
        "\x53\x49\x4F\x4E\x0A\x52\x65\x70\x6F\x72\x74\x2D\x4D\x73\x67\x69"
        "\x64\x2D\x42\x75\x67\x73\x2D\x54\x6F\x3A\x20\x0A\x50\x4F\x2D\x52"
        "\x65\x76\x69\x73\x69\x6F\x6E\x2D\x44\x61\x74\x65\x3A\x20\x32\x30"
        "\x32\x31\x2D\x30\x33\x2D\x30\x31\x20\x31\x34\x3A\x30\x36\x2B\x30"
        "\x31\x30\x30\x0A\x4C\x61\x73\x74\x2D\x54\x72\x61\x6E\x73\x6C\x61"
        "\x74\x6F\x72\x3A\x20\x41\x75\x74\x6F\x6D\x61\x74\x69\x63\x61\x6C"
        "\x6C\x79\x20\x67\x65\x6E\x65\x72\x61\x74\x65\x64\x0A\x4C\x61\x6E"
        "\x67\x75\x61\x67\x65\x2D\x54\x65\x61\x6D\x3A\x20\x6E\x6F\x6E\x65"
        "\x0A\x4C\x61\x6E\x67\x75\x61\x67\x65\x3A\x20\x65\x6E\x0A\x4D\x49"
        "\x4D\x45\x2D\x56\x65\x72\x73\x69\x6F\x6E\x3A\x20\x31\x2E\x30\x0A"
        "\x43\x6F\x6E\x74\x65\x6E\x74\x2D\x54\x79\x70\x65\x3A\x20\x74\x65"
        "\x78\x74\x2F\x70\x6C\x61\x69\x6E\x3B\x20\x63\x68\x61\x72\x73\x65"
        "\x74\x3D\x55\x54\x46\x2D\x38\x0A\x43\x6F\x6E\x74\x65\x6E\x74\x2D"
        "\x54\x72\x61\x6E\x73\x66\x65\x72\x2D\x45\x6E\x63\x6F\x64\x69\x6E"
        "\x67\x3A\x20\x38\x62\x69\x74\x0A\x50\x6C\x75\x72\x61\x6C\x2D\x46"
        "\x6F\x72\x6D\x73\x3A\x20\x6E\x70\x6C\x75\x72\x61\x6C\x73\x3D\x33"
        "\x3B\x20\x20\x20\x20\x20\x70\x6C\x75\x72\x61\x6C\x3D\x6E\x25\x31"
        "\x30\x3D\x3D\x31\x20\x26\x26\x20\x6E\x25\x31\x30\x30\x21\x3D\x31"
        "\x31\x20\x3F\x20\x30\x20\x3A\x20\x20\x20\x20\x20\x20\x20\x20\x20"
        "\x20\x20\x20\x6E\x25\x31\x30\x3E\x3D\x32\x20\x26\x26\x20\x6E\x25"
        "\x31\x30\x3C\x3D\x34\x20\x26\x26\x20\x28\x6E\x25\x31\x30\x30\x3C"
        "\x31\x30\x20\x7C\x7C\x20\x6E\x25\x31\x30\x30\x3E\x3D\x32\x30\x29"
        "\x20\x3F\x20\x31\x20\x3A\x20\x32\x3B\x00\x6D\x73\x67\x31\x00\x5B"
        "\x30\x5D\x20\x6D\x73\x67\x32\x5F\x74\x72\x00\x5B\x30\x5D\x20\x6D"
        "\x73\x67\x33\x5F\x74\x72\x00\x5B\x31\x5D\x20\x6D\x73\x67\x33\x5F"
        "\x74\x72\x00\x5B\x32\x5D\x20\x6D\x73\x67\x33\x5F\x74\x72\x00\x5B"
        "\x30\x5D\x20\x6D\x73\x67\x34\x5F\x74\x72\x00"_av;

    uint32_t msgcount = 0;
    uint32_t nplurals = 0;
    chars_view plural_expr = {};
    std::vector<Msg> msgs;

    auto init_parser = [&](uint32_t msgcount_, uint32_t nplurals_, chars_view plural_expr_) {
        msgcount = msgcount_;
        nplurals = nplurals_;
        plural_expr = plural_expr_;
        return true;
    };

    auto push_mo_msg = [&](chars_view msgid, chars_view msgid_plurals, MoMsgStrIterator msgs_it) {
        auto str = [](chars_view av) { return av.as<std::string_view>(); };
        msgs.push_back(Msg{str(msgid), str(msgid_plurals), str(msgs_it.raw_messages())});
        return true;
    };

    using namespace std::string_view_literals;

    RED_CHECK(MoParserErrorCode::NoError == parse_mo(mo_file, init_parser, push_mo_msg).ec);
    RED_CHECK(msgcount == 4);
    RED_CHECK(nplurals == 3);
    RED_CHECK(plural_expr ==
        "n%10==1 && n%100!=11 ? 0 : "
        "           n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2;"_av);
    RED_CHECK(MsgsAV(msgs) == make_array_view(std::type_identity_t<Msg[]>{
        {.msgid="msg1", .msgid_plurals="", .msgs="msg1"},
        {.msgid="msg2", .msgid_plurals="msg2_plural", .msgs="[0] msg2_tr"},
        {.msgid="msg3", .msgid_plurals="msg3_plural", .msgs="[0] msg3_tr\x00[1] msg3_tr\x00[2] msg3_tr"sv},
        {.msgid="msg4", .msgid_plurals="msg4_plural", .msgs="[0] msg4_tr"}
    }));
}

RED_AUTO_TEST_CASE(TestParsePlural)
{
    std::string buffer;
    GettextPlural plural;
    bool last_result = false;

    auto plural_to_string = [&](GettextPlural const& plural){
        if (!plural.items().empty()) {
            for (auto item : plural.items()) {
                buffer += '{';
                buffer += token_to_string(item.token());
                buffer += '|';
                buffer += int_to_decimal_chars(item.number());
                buffer += "} ";
            }
            buffer.pop_back();
        }
    };

    auto parse = [&](chars_view s) -> chars_view {
        buffer.clear();
        plural = GettextPlural();
        last_result = true;

        if (auto* p = plural.parse(s)) {
            last_result = false;
            str_assign(buffer, "error at position "_av, int_to_decimal_chars(p - s.data()), " ("_av, *p ? *p : '@', ')');
        }
        else {
            plural_to_string(plural);
        }

        return buffer;
    };

    auto eval = [&](unsigned long n){
        if (!last_result) {
            return ~static_cast<unsigned long>(0);
        }
        return plural.eval(n);
    };

    plural_to_string(GettextPlural(GettextPlural::plural_1_neq_n()));
    RED_CHECK(buffer == "{Num|1} {Id|0} {NEq|0}"_av);

    RED_CHECK(parse(""_av) == ""_av); {
        RED_CHECK(eval(0) == 0);
        RED_CHECK(eval(1) == 0);
        RED_CHECK(eval(10) == 0);
    }

    RED_CHECK(parse("1"_av) == "{Num|1}"_av); {
        RED_CHECK(eval(0) == 1);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 1);
    }

    RED_CHECK(parse("n"_av) == "{Id|0}"_av); {
        RED_CHECK(eval(0) == 0);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 10);
    }

    RED_CHECK(parse("(1)"_av) == "{Num|1}"_av);
    RED_CHECK(parse("(n)"_av) == "{Id|0}"_av);


    RED_CHECK(parse("-1"_av) == "{Num|1} {Neg|0}"_av); {
        RED_CHECK(eval(0) == -1ul);
        RED_CHECK(eval(1) == -1ul);
        RED_CHECK(eval(10) == -1ul);
    }

    RED_CHECK(parse("-n"_av) == "{Id|0} {Neg|0}"_av); {
        RED_CHECK(eval(0) == 0);
        RED_CHECK(eval(1) == -1ul);
        RED_CHECK(eval(10) == -10ul);
    }

    RED_CHECK(parse("--1"_av) == "{Num|1} {Neg|0} {Neg|0}"_av); {
        RED_CHECK(eval(0) == 1);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 1);
    }

    RED_CHECK(parse("--n"_av) == "{Id|0} {Neg|0} {Neg|0}"_av); {
        RED_CHECK(eval(0) == 0);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 10);
    }

    RED_CHECK(parse("(-1)"_av) == "{Num|1} {Neg|0}"_av);
    RED_CHECK(parse("(-n)"_av) == "{Id|0} {Neg|0}"_av);
    RED_CHECK(parse("-(1)"_av) == "{Num|1} {Neg|0}"_av);
    RED_CHECK(parse("-(n)"_av) == "{Id|0} {Neg|0}"_av);


    RED_CHECK(parse("!n"_av) == "{Id|0} {Not|0}"_av); {
        RED_CHECK(eval(0) == 1);
        RED_CHECK(eval(1) == 0);
        RED_CHECK(eval(10) == 0);
    }

    RED_CHECK(parse("1 + !n"_av) == "{Num|1} {Id|0} {Not|0} {Add|0}"_av); {
        RED_CHECK(eval(0) == 2);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 1);
    }


    RED_CHECK(parse("n >= 1"_av) == "{Id|0} {Num|1} {GE|0}"_av); {
        RED_CHECK(eval(0) == 0);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 1);
    }

    RED_CHECK(parse("n != 1"_av) == "{Id|0} {Num|1} {NEq|0}"_av); {
        RED_CHECK(eval(0) == 1);
        RED_CHECK(eval(1) == 0);
        RED_CHECK(eval(10) == 1);
    }

    RED_CHECK(parse("(n == 10)"_av) == "{Id|0} {Num|10} {Eq|0}"_av); {
        RED_CHECK(eval(0) == 0);
        RED_CHECK(eval(1) == 0);
        RED_CHECK(eval(10) == 1);
    }


    RED_CHECK(parse("n?1:2"_av) == "{Id|0} {TernIf|4} {Num|1} {TernElse|5} {Num|2}"_av); {
        RED_CHECK(eval(0) == 2);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 1);
    }

    RED_CHECK(parse("n + 1 ? 1 + n : 2 + n"_av)
        == "{Id|0} {Num|1} {Add|0} {TernIf|8} "
           "{Num|1} {Id|0} {Add|0} {TernElse|11} "
           "{Num|2} {Id|0} {Add|0}"_av
    ); {
        RED_CHECK(eval(0) == 1);
        RED_CHECK(eval(1) == 2);
        RED_CHECK(eval(10) == 11);
    }

    RED_CHECK(parse("(n ? 1 : 2) ? 3 : 4"_av)
        == "{Id|0} {TernIf|4} "
           "{Num|1} {TernElse|5} "
           "{Num|2} {TernIf|8} "
           "{Num|3} {TernElse|9} "
           "{Num|4}"_av
    ); {
        RED_CHECK(eval(0) == 3);
        RED_CHECK(eval(1) == 3);
        RED_CHECK(eval(10) == 3);
    }

    RED_CHECK(parse("n ? (1 ? 2 : 3) : 4"_av)
        == "{Id|0} {TernIf|8} "
           "{Num|1} {TernIf|6} "
           "{Num|2} {TernElse|7} "
           "{Num|3} {TernElse|9} "
           "{Num|4}"_av
    ); {
        RED_CHECK(eval(0) == 4);
        RED_CHECK(eval(1) == 2);
        RED_CHECK(eval(10) == 2);
    }

    RED_CHECK(parse("n ? 1 ? 2 : 3 : 4"_av) // n ? (1 ? 2 : 3) : 4
        == "{Id|0} {TernIf|8} "
           "{Num|1} {TernIf|6} "
           "{Num|2} {TernElse|7} "
           "{Num|3} {TernElse|9} "
           "{Num|4}"_av);

    RED_CHECK(parse("n ? 1 : (2 ? 3 : 4)"_av)
        == "{Id|0} {TernIf|4} "
           "{Num|1} {TernElse|9} "
           "{Num|2} {TernIf|8} "
           "{Num|3} {TernElse|9} "
           "{Num|4}"_av
    ); {
        RED_CHECK(eval(0) == 3);
        RED_CHECK(eval(1) == 1);
        RED_CHECK(eval(10) == 1);
    }

    RED_CHECK(parse("n ? 1 : 2 ? 3 : 4"_av) // n ? 1 : (2 ? 3 : 4)
        == "{Id|0} {TernIf|4} "
           "{Num|1} {TernElse|9} "
           "{Num|2} {TernIf|8} "
           "{Num|3} {TernElse|9} "
           "{Num|4}"_av);


    RED_CHECK(parse("n%10>=2 && n%10<=4"_av)
      ==
        "{Id|0} {Num|10} {Mod|0} {Num|2} {GE|0} {Id|0} {Num|10} {Mod|0} {Num|4} {LE|0} {And|0}"_av); {
        RED_CHECK(eval(0) == 0);
        RED_CHECK(eval(1) == 0);
        RED_CHECK(eval(2) == 1);
        RED_CHECK(eval(3) == 1);
        RED_CHECK(eval(4) == 1);
        RED_CHECK(eval(5) == 0);
        RED_CHECK(eval(10) == 0);
        RED_CHECK(eval(11) == 0);
        RED_CHECK(eval(12) == 1);
        RED_CHECK(eval(13) == 1);
        RED_CHECK(eval(14) == 1);
        RED_CHECK(eval(15) == 0);
    }

    RED_CHECK(parse("n%10==1 && n%100!=11 ? 0 : \
        n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2;"_av)
      ==
        "{Id|0} {Num|10} {Mod|0} {Num|1} {Eq|0} {Id|0} {Num|100} {Mod|0} {Num|11} {NEq|0} {And|0} {TernIf|14} {Num|0} {TernElse|41} {Id|0} {Num|10} {Mod|0} {Num|2} {GE|0} {Id|0} {Num|10} {Mod|0} {Num|4} {LE|0} {And|0} {Id|0} {Num|100} {Mod|0} {Num|10} {LT|0} {Id|0} {Num|100} {Mod|0} {Num|20} {GE|0} {Or|0} {And|0} {TernIf|40} {Num|1} {TernElse|41} {Num|2}"_av
    ); {
        RED_CHECK(eval(0) == 2);
        RED_CHECK(eval(1) == 0);
        RED_CHECK(eval(11) == 2);
        RED_CHECK(eval(21) == 0);
        RED_CHECK(eval(122) == 1);
    }


    RED_CHECK(parse("!"_av) == "error at position 1 (@)"_av);
    RED_CHECK(parse("+"_av) == "error at position 1 (@)"_av);
    RED_CHECK(parse("1+"_av) == "error at position 2 (@)"_av);
    RED_CHECK(parse("n ?"_av) == "error at position 3 (@)"_av);
    RED_CHECK(parse("n ? 1 :"_av) == "error at position 7 (@)"_av);
    RED_CHECK(parse("1 * *"_av) == "error at position 4 (*)"_av);
    RED_CHECK(parse("0 1"_av) == "error at position 2 (1)"_av);
}
