/*
    This program is free software; you can redistribute it and/or modify it
     under the terms of the GNU General Public License as published by the
     Free Software Foundation; either version 2 of the License, or (at your
     option) any later version.

    This program is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
     Public License for more details.

    You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     675 Mass Ave, Cambridge, MA 02139, USA.

    Product name: redemption, a FLOSS RDP proxy
    Copyright (C) Wallix 2014
    Author(s): Christophe Grosjean, Meng Tan, Raphael Zhou
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"
#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"

#include "mod/internal/widget/scroll.hpp"
#include "utils/bitfu.hpp"

#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/scroll/"


struct TestScrollCtx
{
    TestGraphic drawable;
    WidgetScrollBar scroll;
    unsigned pos = 0;

    TestScrollCtx(WidgetScrollBar::ScrollDirection scroll_direction)
    : drawable(1, 1)
    , scroll(
        this->drawable, global_font_deja_vu_14(), scroll_direction,
        WidgetScrollBar::Colors{
            .fg = NamedBGRColor::RED,
            .bg = NamedBGRColor::YELLOW,
            .bg_focus = NamedBGRColor::WINBLUE,
        },
        [this]{
            auto new_pos = this->scroll.get_current_value();
            RED_TEST(this->pos != new_pos);
            this->pos = new_pos;
        }
    )
    {
        scroll.set_max_value(50);
        bool is_horizontal = (scroll_direction == WidgetScrollBar::ScrollDirection::Horizontal);
        Dimension dim = is_horizontal
            ? Dimension{200, this->scroll.cy()}
            : Dimension{this->scroll.cx(), 200}
            ;
        this->drawable.resize(align4(dim.w), dim.h);
        this->scroll.set_size(is_horizontal ? this->drawable.width() : this->drawable.height());
        this->scroll.rdp_input_invalidate(this->scroll.get_rect());
    }

    void down(int x, int y)
    {
        this->scroll.rdp_input_mouse(MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN, x, y);
    }

    void up(int x, int y)
    {
        this->scroll.rdp_input_mouse(MOUSE_FLAG_BUTTON1, x, y);
    }
};


RED_AUTO_TEST_CASE(TestWidgetHScrollBar)
{
    TestScrollCtx ctx(WidgetScrollBar::ScrollDirection::Horizontal);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_1.png");

    ctx.down(5, 5);
    RED_TEST(ctx.pos == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_2.png");

    ctx.up(5, 5);
    RED_TEST(ctx.pos == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_1.png");

    ctx.down(ctx.drawable.width() - 5, 5);
    RED_TEST(ctx.pos == 2);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_3.png");

    ctx.up(ctx.drawable.width() - 5, 5);
    RED_TEST(ctx.pos == 2);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_4.png");

    ctx.down(ctx.drawable.width() - 5, 5);
    RED_TEST(ctx.pos == 4);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_5.png");

    ctx.up(ctx.drawable.width() - 5, 5);
    RED_TEST(ctx.pos == 4);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_6.png");

    ctx.down(5, 5);
    RED_TEST(ctx.pos == 2);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_7.png");

    for (unsigned pos = 4; pos < 50; pos += 2) {
        ctx.down(ctx.drawable.width() - 5, 5);
        RED_TEST(ctx.pos == pos);
    }
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_8.png");

    ctx.down(ctx.drawable.width() - 5, 5);
    RED_TEST(ctx.pos == 50);
    ctx.down(ctx.drawable.width() - 5, 5);
    RED_TEST(ctx.pos == 50);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "hscroll_9.png");
}

RED_AUTO_TEST_CASE(TestWidgetVScrollBar)
{
    TestScrollCtx ctx(WidgetScrollBar::ScrollDirection::Vertical);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_1.png");

    ctx.down(5, 5);
    RED_TEST(ctx.pos == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_2.png");

    ctx.up(5, 5);
    RED_TEST(ctx.pos == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_1.png");

    ctx.down(5, ctx.drawable.height() - 5);
    RED_TEST(ctx.pos == 2);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_3.png");

    ctx.up(5, ctx.drawable.height() - 5);
    RED_TEST(ctx.pos == 2);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_4.png");

    ctx.down(5, ctx.drawable.height() - 5);
    RED_TEST(ctx.pos == 4);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_5.png");

    ctx.up(5, ctx.drawable.height() - 5);
    RED_TEST(ctx.pos == 4);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_6.png");

    ctx.down(5, 5);
    RED_TEST(ctx.pos == 2);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_7.png");

    for (unsigned pos = 4; pos < 50; pos += 2) {
        ctx.down(5, ctx.drawable.height() - 5);
        RED_TEST(ctx.pos == pos);
    }
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_8.png");

    ctx.down(5, ctx.drawable.height() - 5);
    RED_TEST(ctx.pos == 50);
    ctx.down(5, ctx.drawable.height() - 5);
    RED_TEST(ctx.pos == 50);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "vscroll_9.png");
}
