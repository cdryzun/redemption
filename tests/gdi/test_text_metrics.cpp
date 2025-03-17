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
   Copyright (C) Wallix 2020
   Author(s): Christophe Grosjean, Jonathan Poelen

   Unit test to conversion of RDP drawing orders to PNG images
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/compare_collection.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "gdi/text_metrics.hpp"
#include "utils/sugar/to_sv.hpp"
#include "utils/utf.hpp"
#include "core/font.hpp"
#include "test_only/core/font.hpp"
#include "test_only/gdi/test_graphic.hpp"

#if !REDEMPTION_UNIT_TEST_FAST_CHECK
#include <iomanip>
#endif

#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/gdi/text_metrics/"


RED_AUTO_TEST_CASE(TextMetrics)
{
    {
        gdi::TextMetrics text(global_font_deja_vu_14(), "abc"_av);
        RED_CHECK_EQUAL(18, text.height);
        RED_CHECK_EQUAL(25, text.width);
    }
    {
        gdi::TextMetrics text(global_font_deja_vu_14(), "abcde"_av);
        RED_CHECK_EQUAL(18, text.height);
        RED_CHECK_EQUAL(43, text.width);
    }
    {
        gdi::TextMetrics text(global_font_deja_vu_14(), "Ay"_av);
        RED_CHECK_EQUAL(18, text.height);
        RED_CHECK_EQUAL(19, text.width);
    }
}


struct LinesForTest
{
    array_view<gdi::MultiLineTextMetrics::Line> fc_lines;
    array_view<std::string_view> str_lines;
    Font const& font;

    bool operator == (LinesForTest const& other) const
    {
        auto fc_lines = other.fc_lines.empty() ? this->fc_lines : other.fc_lines;
        auto str_lines = other.str_lines.empty() ? this->str_lines : other.str_lines;
        if (fc_lines.size() != str_lines.size()) {
            return false;
        }

        auto str_line_it = str_lines.begin();
        for (auto fc_line : fc_lines) {
            UTF8TextReader utf8_reader{*str_line_it};
            for (auto fc : fc_line) {
                if (!utf8_reader.has_value()) {
                    return false;
                }
                if (*fc != font.item(utf8_reader.next()).view) {
                    return false;
                }
            }
            if (utf8_reader.has_value()) {
                return false;
            }
            ++str_line_it;
        }

        return true;
    }
};

#if !REDEMPTION_UNIT_TEST_FAST_CHECK
static ut::assertion_result test_comp_lines(LinesForTest const& a, LinesForTest const& b)
{
    if (a == b) {
        return ut::assertion_result{true};
    }

    auto fc_lines = a.fc_lines.empty() ? b.fc_lines : a.fc_lines;
    std::size_t n = 0;
    for (auto fc_line : fc_lines) {
        n += fc_line.size();
    }
    std::unique_ptr<char[]> ascii_buf(new char[n]);
    std::vector<std::string_view> lines;
    lines.resize(fc_lines.size());
    auto charp = ascii_buf.get();

    auto fc_to_uc = [&a](FontCharView const& fc){
        for (uint32_t uc = ' '; uc < '~'; ++uc) {
            if (a.font.item(uc).view.data == fc.data) {
                return uc;
            }
        }
        return uint32_t{};
    };

    auto line_it = lines.begin();
    for (auto fc_line : fc_lines) {
        auto start = charp;
        for (auto fc : fc_line) {
            auto uc = fc_to_uc(*fc);
            *charp++ = checked_int(uc ? uc : '_');
        }
        *line_it++ = {start, charp};
    }

    auto a_lines = array_view{lines};
    auto b_lines = a.str_lines.empty() ? b.str_lines : a.str_lines;

    return ut::ops::compare_collection_EQ(a_lines, b_lines, [&](std::ostream& out, size_t pos, char const* op)
    {
        auto put_view = [&](std::ostream& oss, array_view<std::string_view> lines){
            if (lines.empty()) {
                oss << "--";
            }
            else {
                size_t i = 0;
                auto put = [&](std::string_view line) {
                    oss << (i == pos ? "*{" : "{") << std::quoted(line) << "}";
                    ++i;
                };
                put(lines.front());
                for (auto const& l : lines.from_offset(1)) {
                    oss << " ";
                    put(l);
                }
            }
        };

        ut::put_data_with_diff(out, a_lines, op, b_lines, put_view);
    });
}

RED_TEST_DISPATCH_COMPARISON_EQ((), (LinesForTest), (LinesForTest), ::test_comp_lines)
#endif

#define TEST_LINES(font, s, max_width, ...) do {                 \
    gdi::MultiLineTextMetrics metrics(font, s ""_av, max_width); \
    std::string_view expected_[] __VA_ARGS__;                    \
    LinesForTest expected{{}, make_array_view(expected_), font}; \
    LinesForTest lines = {metrics.lines(), {}, font};            \
    RED_CHECK(lines == expected);                                \
} while (0)


RED_AUTO_TEST_CASE(MultiLineTextMetrics)
{
    auto& font14 = global_font_deja_vu_14();

    RED_TEST(gdi::MultiLineTextMetrics(font14, ""_av, 0).lines().size() == 0);

    TEST_LINES(font14, "ab", 0, {
        "a",
        "b",
    });

    TEST_LINES(font14, "abc", 100, {
        "abc",
    });

    TEST_LINES(font14, "abc", 17, {
        "ab",
        "c",
    });

    TEST_LINES(font14, "abc", 1, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "a\nb\nc", 100, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "a\nb\nc", 17, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "a\nb\nc", 1, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "ab cd", 17, {
        "ab",
        "cd",
    });

    TEST_LINES(font14, "ab   cd", 17, {
        "ab",
        "cd",
    });

    TEST_LINES(font14, "ab cd", 1, {
        "a",
        "b",
        "c",
        "d",
    });

    TEST_LINES(font14, "annvhg jgsy kfhdis hnvlkj gks hxk.hf", 50, {
        "annvh",
        "g jgsy",
        "kfhdis",
        "hnvlkj",
        "gks",
        "hxk.hf",
    });

    TEST_LINES(font14, "annvhg jgsy kfhdis hnvlkj gks hxk.hf", 150, {
        "annvhg jgsy kfhdis",
        "hnvlkj gks hxk.hf",
    });

    TEST_LINES(font14, "veryverylonglonglong string", 100, {
        "veryverylongl",
        "onglong",
        "string",
    });

    TEST_LINES(font14, "  veryverylonglonglong string", 100, {
        "",
        "veryverylongl",
        "onglong",
        "string",
    });

    TEST_LINES(font14, "  veryverylonglonglong string", 130, {
        "",
        "veryverylonglongl",
        "ong string",
    });

    TEST_LINES(font14, "  veryverylonglonglong\n string", 100, {
        "",
        "veryverylongl",
        "onglong",
        " string",
    });

    TEST_LINES(font14, "  veryverylonglonglong \nstring", 100, {
        "",
        "veryverylongl",
        "onglong",
        "string",
    });

    TEST_LINES(font14, "bla bla\n\n - abc\n - def", 100, {
        "bla bla",
        "",
        " - abc",
        " - def",
    });

    TEST_LINES(font14, "Le pastafarisme (mot-valise faisant référence aux pâtes et au mouvement rastafari) est originellement une parodie de religion1,2,3,4 dont la divinité est le Monstre en spaghetti volant (Flying Spaghetti Monster)5,6 créée en 2005 par Bobby Henderson, alors étudiant de l'université d'État de l'Oregon. Depuis, le pastafarisme a été reconnu administrativement comme religion par certains pays7,8,9,10,11, et rejeté en tant que telle par d'autres12,13,14.", 273, {
        "Le pastafarisme (mot-valise faisant",
        "référence aux pâtes et au",
        "mouvement rastafari) est",
        "originellement une parodie de",
        "religion1,2,3,4 dont la divinité est le",
        "Monstre en spaghetti volant (Flying",
        "Spaghetti Monster)5,6 créée en 2005",
        "par Bobby Henderson, alors étudiant",
        "de l'université d'État de l'Oregon.",
        "Depuis, le pastafarisme a été reconnu",
        "administrativement comme religion",
        "par certains pays7,8,9,10,11, et rejeté",
        "en tant que telle par",
        "d'autres12,13,14.",
    });
}


RED_AUTO_TEST_CASE(TestServerDrawText)
{
    auto& font = global_font_deja_vu_14();

    const uint16_t w = 1800;
    const uint16_t h = 30;
    TestGraphic gd(w, h);

    auto text = ""
        "Unauthorized access to this system is forbidden and will be prosecuted"
        " by law. By accessing this system, you agree that your actions may be"
        " monitored if unauthorized usage is suspected."
        ""_av
    ;

    using color_encoder = encode_color24;
    gdi::server_draw_text(
        gd, font, 0, 0, text,
        color_encoder()(NamedBGRColor::CYAN),
        color_encoder()(NamedBGRColor::BLUE),
        gdi::ColorCtx::from_bpp(color_encoder::bpp, nullptr),
        Rect(0, 0, w, h));

    RED_CHECK_IMG(gd, IMG_TEST_PATH "server_draw_text1.png");
}
