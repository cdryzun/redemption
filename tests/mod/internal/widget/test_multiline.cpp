/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "mod/internal/widget/multiline.hpp"

#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/multiline/"

struct TestWidgetMultiLineCtx
{
    TestGraphic drawable;
    chars_view text;

    WidgetMultiLine make_multi_line()
    {
        return WidgetMultiLine(
            drawable,
            {
                .font = global_font_deja_vu_14(),
                .text = text,
                .max_width = 4096,
            },
            {
                .fg = NamedBGRColor::BLUE,
                .bg = NamedBGRColor::CYAN,
            }
        );
    }
};

RED_AUTO_TEST_CASE(TraceWidgetMultiLine)
{
    TestWidgetMultiLineCtx ctx{
        {1500, 150},
        "line 1\n"
        "line 2\n"
        "\n"
        "line 3, blah blah\n"
        "line 4"
        ""_av
    };

    auto multi_line = ctx.make_multi_line();

    int16_t y_step = 20;
    int16_t x_step = 130;
    for (int16_t py = 0; py < 3; ++py) {
        auto compute_x = [=](int i){
            return checked_int{py * x_step * 4 + x_step * i - 10};
        };
        int16_t y = py * y_step - y_step;
        Rect rect;

        multi_line.set_xy(compute_x(0), y);
        multi_line.rdp_input_invalidate(multi_line.get_rect());

        multi_line.set_xy(compute_x(1), y);
        rect = multi_line.get_rect();
        rect.cy /= 2;
        multi_line.rdp_input_invalidate(rect);

        multi_line.set_xy(compute_x(2), y);
        rect = multi_line.get_rect();
        rect.cy /= 2;
        rect.y += rect.cy;
        multi_line.rdp_input_invalidate(rect);

        multi_line.set_xy(compute_x(3), y);
        rect = multi_line.get_rect();
        rect.cy /= 2;
        rect.y += rect.cy / 2;
        multi_line.rdp_input_invalidate(rect);
    }

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "multi_line.png");
}
