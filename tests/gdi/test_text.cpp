/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/compare_collection.hpp"
#include "test_only/test_framework/check_img.hpp"
#include "test_only/core/font.hpp"
#include "test_only/gdi/test_graphic.hpp"

#include "gdi/text.hpp"
#include "utils/sugar/to_sv.hpp"
#include "utils/utf.hpp"
#include "core/font.hpp"
#include "mod/internal/widget/label.hpp"

#if !REDEMPTION_UNIT_TEST_FAST_CHECK
#include <iomanip>
#endif

#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/gdi/text/"


struct LinesForTest
{
    array_view<gdi::MultiLineText::Line> fc_lines;
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

#define TEST_LINES(font, s, preferred_max_width, real_max_width, ...) do { \
    gdi::MultiLineText metrics(font, preferred_max_width, s ""_av); \
    std::string_view expected_[] __VA_ARGS__;                              \
    LinesForTest expected{{}, make_array_view(expected_), font};           \
    LinesForTest lines = {metrics.lines(), {}, font};                      \
    RED_CHECK(lines == expected);                                          \
    RED_CHECK(metrics.max_width() == real_max_width);                      \
    RED_TEST_CONTEXT("rewrap") {                                           \
        metrics.update_dimension(metrics.max_width());                     \
        RED_CHECK(lines == expected);                                      \
        RED_CHECK(metrics.max_width() == real_max_width);                  \
    }                                                                      \
        RED_TEST_CONTEXT("rewrap") {                                       \
        metrics.clear_text();                                              \
        RED_CHECK((LinesForTest{metrics.lines(), {}, font})                \
            == (LinesForTest{{}, {}, font}));                              \
        RED_CHECK(metrics.max_width() == 0);                               \
    }                                                                      \
} while (0)


RED_AUTO_TEST_CASE(MultiLineText)
{
    auto& font14 = global_font_deja_vu_14();

    RED_TEST(gdi::MultiLineText(font14, 0, ""_av).lines().size() == 0);

    // empty size
    {
        gdi::MultiLineText metrics(font14, 0, "ab"_av);
        RED_CHECK(metrics.max_width() == 0);
        RED_CHECK(metrics.lines().size() == 0);
    }

    TEST_LINES(font14, "ab", 1, 10, {
        "a",
        "b",
    });

    TEST_LINES(font14, "abc", 100, 26, {
        "abc",
    });

    TEST_LINES(font14, "abc", 18, 18, {
        "ab",
        "c",
    });

    TEST_LINES(font14, "abc", 1, 10, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "a\nb\nc", 100, 10, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "a\nb\nc", 17, 10, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "a\nb\nc", 1, 10, {
        "a",
        "b",
        "c",
    });

    TEST_LINES(font14, "ab cd", 18, 18, {
        "ab",
        "cd",
    });

    TEST_LINES(font14, "ab   cd", 18, 18, {
        "ab",
        "cd",
    });

    TEST_LINES(font14, "ab cd", 1, 10, {
        "a",
        "b",
        "c",
        "d",
    });

    TEST_LINES(font14, "annvhg jgsy kfhdis hnvlkj gks hxk.hf", 50, 46, {
        "annvh",
        "g jgsy",
        "kfhdis",
        "hnvlkj",
        "gks",
        "hxk.hf",
    });

    TEST_LINES(font14, "annvhg jgsy kfhdis hnvlkj gks hxk.hf", 150, 137, {
        "annvhg jgsy kfhdis",
        "hnvlkj gks hxk.hf",
    });

    TEST_LINES(font14, "veryverylonglonglong string", 100, 98, {
        "veryverylongl",
        "onglong",
        "string",
    });

    TEST_LINES(font14, "  veryverylonglonglong string", 100, 98, {
        "",
        "veryverylongl",
        "onglong",
        "string",
    });

    TEST_LINES(font14, "  veryverylonglonglong string", 130, 128, {
        "",
        "veryverylonglongl",
        "ong string",
    });

    TEST_LINES(font14, "  veryverylonglonglong\n string", 100, 98, {
        "",
        "veryverylongl",
        "onglong",
        " string",
    });

    TEST_LINES(font14, "  veryverylonglonglong \nstring", 100, 98, {
        "",
        "veryverylongl",
        "onglong",
        "string",
    });

    TEST_LINES(font14, "  bla bla \nstring", 100, 56, {
        "  bla bla",
        "string",
    });

    TEST_LINES(font14, "bla bla\n\n - abc\n - def", 100, 46, {
        "bla bla",
        "",
        " - abc",
        " - def",
    });

    TEST_LINES(font14, "Le pastafarisme (mot-valise faisant référence aux pâtes et au mouvement rastafari) est originellement une parodie de religion1,2,3,4 dont la divinité est le Monstre en spaghetti volant (Flying Spaghetti Monster)5,6 créée en 2005 par Bobby Henderson, alors étudiant de l'université d'État de l'Oregon. Depuis, le pastafarisme a été reconnu administrativement comme religion par certains pays7,8,9,10,11, et rejeté en tant que telle par d'autres12,13,14.", 274, 274, {
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

    constexpr auto text = ""
        "Unauthorized access to this system is forbidden and will be prosecuted"
        " by law. By accessing this system, you agree that your actions may be"
        " monitored if unauthorized usage is suspected."
        ""_av
    ;

    using color_encoder = encode_color24;
    gdi::draw_text(
        gd, 0, 0, font.max_height(), gdi::DrawTextPadding{},
        WidgetText<text.size()>(font, text).fcs(),
        color_encoder()(NamedBGRColor::CYAN),
        color_encoder()(NamedBGRColor::BLUE),
        Rect(0, 0, w, h)
    );

    RED_CHECK_IMG(gd, IMG_TEST_PATH "server_draw_text1.png");
}
