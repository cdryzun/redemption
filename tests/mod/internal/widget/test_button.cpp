/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2012
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"

#include "mod/internal/widget/button.hpp"
#include "mod/internal/widget/composite.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"


namespace
{
struct ButtonContextTest
{
    NotifyTrace notifier;
    TestGraphic drawable;
    WidgetButton wbutton;

    ButtonContextTest(
        chars_view text,
        int16_t x, int16_t y,
        int16_t dx = 0, int16_t dy = 0,
        uint16_t cx = 0, uint16_t cy = 0,
        bool force_focus = false
    )
    : ButtonContextTest(100, 50, text, x, y, dx, dy, cx, cy, force_focus)
    {}

    ButtonContextTest(
        uint16_t w, uint16_t h,
        chars_view text,
        int16_t x, int16_t y,
        int16_t dx = 0, int16_t dy = 0,
        uint16_t cx = 0, uint16_t cy = 0,
        bool force_focus = false
    )
    : drawable(w, h)
    , wbutton{
        drawable, global_font_deja_vu_14(), text,
        WidgetButton::Colors{
            .focus = {
                .fg = NamedBGRColor::YELLOW,
                .bg = NamedBGRColor::RED,
                .border = NamedBGRColor::WINBLUE,
            },
            .blur = {
                .fg = NamedBGRColor::RED,
                .bg = NamedBGRColor::YELLOW,
                .border = NamedBGRColor::GREEN,
            },
        },
        notifier
    }
    {
        wbutton.set_xy(x, y);

        if (force_focus) {
            wbutton.has_focus = true;
        }

        // ask to widget to redraw at it's current position
        wbutton.rdp_input_invalidate(Rect(
            dx + wbutton.x(),
            dy + wbutton.y(),
            cx ? cx : wbutton.cx(),
            cy ? cy : wbutton.cy()
        ));
    }
};
} // anonymous namespace

#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/button/"

RED_AUTO_TEST_CASE(TraceWidgetButton)
{
    RED_CHECK_IMG(ButtonContextTest("test1"_av, 0, 0).drawable, IMG_TEST_PATH "button_1.png");
    RED_CHECK_IMG(ButtonContextTest("test2"_av, 10, 10).drawable, IMG_TEST_PATH "button_2.png");
    RED_CHECK_IMG(ButtonContextTest("test3"_av, -20, 10).drawable, IMG_TEST_PATH "button_3.png");
    RED_CHECK_IMG(ButtonContextTest("test4"_av, 70, 30).drawable, IMG_TEST_PATH "button_4.png");
    RED_CHECK_IMG(ButtonContextTest("test5"_av, -20, -7).drawable, IMG_TEST_PATH "button_5.png");
    RED_CHECK_IMG(ButtonContextTest("test6"_av, 60, -7).drawable, IMG_TEST_PATH "button_6.png");
    RED_CHECK_IMG(ButtonContextTest("test6"_av, 60, -7, 20).drawable, IMG_TEST_PATH "button_7.png");
    RED_CHECK_IMG(ButtonContextTest("test6"_av, 0, 0, 20, 5, 30, 10).drawable, IMG_TEST_PATH "button_8.png");
}

RED_AUTO_TEST_CASE(TraceWidgetButtonDownAndUp)
{
    ButtonContextTest button("test6"_av, 10, 10, 0, 0, 0, 0, true);
    RED_CHECK_IMG(button.drawable, IMG_TEST_PATH "button_9.png");

    auto& wbutton = button.wbutton;

    wbutton.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, 15, 15);
    RED_CHECK_IMG(button.drawable, IMG_TEST_PATH "button_10.png");

    wbutton.rdp_input_mouse(MOUSE_FLAG_BUTTON1, 15, 15);
    RED_CHECK_IMG(button.drawable, IMG_TEST_PATH "button_9.png");
}

RED_AUTO_TEST_CASE(TraceWidgetButtonEvent)
{
    const int16_t x = 0;
    const int16_t y = 0;

    ButtonContextTest ctx(""_av, x, y);
    auto & wbutton = ctx.wbutton;

    wbutton.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, x, y);
    RED_CHECK(ctx.notifier.get_and_reset() == 0);

    wbutton.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, x, y);
    RED_CHECK(ctx.notifier.get_and_reset() == 0);

    wbutton.rdp_input_mouse(MOUSE_FLAG_BUTTON1, x, y);
    RED_CHECK(ctx.notifier.get_and_reset() == 1);

    wbutton.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, x, y);
    RED_CHECK(ctx.notifier.get_and_reset() == 0);

    Keymap keymap(*find_layout_by_id(KeyLayout::KbdId(0x040C)));
    using KFlags = Keymap::KbdFlags;
    using Scancode = Keymap::Scancode;

    keymap.event(KFlags(), Scancode::A);
    wbutton.rdp_input_scancode(KFlags(), Scancode::A, 0, keymap);
    RED_CHECK(ctx.notifier.get_and_reset() == 0);

    keymap.event(KFlags(), Scancode::Space);
    wbutton.rdp_input_scancode(KFlags(), Scancode::Space, 0, keymap);
    RED_CHECK(ctx.notifier.get_and_reset() == 1);

    keymap.event(KFlags(), Scancode::Enter);
    wbutton.rdp_input_scancode(KFlags(), Scancode::Enter, 0, keymap);
    RED_CHECK(ctx.notifier.get_and_reset() == 1);
}

RED_AUTO_TEST_CASE(TraceWidgetButtonAndComposite)
{
    struct WComposite : WidgetComposite
    {
        using WidgetComposite::WidgetComposite;
        using WidgetComposite::set_wh;
    };

    TestGraphic drawable(300, 200);

    WidgetEventNotifier notifier2;

    WComposite wcomposite(drawable, Widget::Focusable::No, NamedBGRColor::BLACK);
    wcomposite.set_wh(300, 200);
    wcomposite.set_xy(0, 0);

    auto colors = [](Widget::Color fg, Widget::Color bg, Widget::Color focus) {
        return WidgetButton::Colors{
            .focus = {
                .fg = fg,
                .bg = focus,
                .border = fg,
            },
            .blur = {
                .fg = fg,
                .bg = bg,
                .border = fg,
            },
        };
    };

    WidgetButton wbutton1(
        drawable, global_font_deja_vu_14(), "abababab"_av,
        colors(NamedBGRColor::YELLOW, NamedBGRColor::BLACK, NamedBGRColor::WINBLUE),
        notifier2
    );
    wbutton1.set_xy(0, 0);

    WidgetButton wbutton2(
        drawable, global_font_deja_vu_14(), "ggghdgh"_av,
        colors(NamedBGRColor::WHITE, NamedBGRColor::RED, NamedBGRColor::WINBLUE),
        notifier2
    );
    wbutton2.set_xy(0, 50);

    WidgetButton wbutton3(
        drawable, global_font_deja_vu_14(), "lldlslql"_av,
        colors(NamedBGRColor::BLUE, NamedBGRColor::RED, NamedBGRColor::WINBLUE),
        notifier2
    );
    wbutton3.set_xy(100, 50);

    WidgetButton wbutton4(
        drawable, global_font_deja_vu_14(), "LLLLMLLM"_av,
        colors(NamedBGRColor::PINK, NamedBGRColor::DARK_GREEN, NamedBGRColor::WINBLUE),
        notifier2
    );
    wbutton4.set_xy(150, 130);

    WidgetButton wbutton5(
        drawable, global_font_deja_vu_14(), "dsdsdjdjs"_av,
        colors(NamedBGRColor::LIGHT_GREEN, NamedBGRColor::DARK_BLUE, NamedBGRColor::WINBLUE),
        notifier2
    );
    wbutton5.set_xy(250, -10);

    WidgetButton wbutton6(
        drawable, global_font_deja_vu_14(), "xxwwp"_av,
        colors(NamedBGRColor::ANTHRACITE, NamedBGRColor::PALE_GREEN, NamedBGRColor::WINBLUE),
        notifier2
    );
    wbutton6.set_xy(-10, 175);

    wcomposite.add_widget(wbutton1);
    wcomposite.add_widget(wbutton2);
    wcomposite.add_widget(wbutton3);
    wcomposite.add_widget(wbutton4);
    wcomposite.add_widget(wbutton5);
    wcomposite.add_widget(wbutton6);

    wcomposite.rdp_input_invalidate(Rect(50, 25, 100, 100));

    RED_CHECK_IMG(drawable, IMG_TEST_PATH "button_12.png");

    wcomposite.rdp_input_invalidate(wcomposite.get_rect());

    RED_CHECK_IMG(drawable, IMG_TEST_PATH "button_13.png");
}

RED_AUTO_TEST_CASE(TraceWidgetButtonFocus)
{
    ButtonContextTest button(72, 40, "test7"_av, 10, 10);
    RED_CHECK_IMG(button.drawable, IMG_TEST_PATH "button_14.png");

    auto& wbutton = button.wbutton;

    wbutton.focus();
    RED_CHECK_IMG(button.drawable, IMG_TEST_PATH "button_15.png");

    wbutton.blur();
    RED_CHECK_IMG(button.drawable, IMG_TEST_PATH "button_14.png");

    wbutton.focus();
    RED_CHECK_IMG(button.drawable, IMG_TEST_PATH "button_15.png");
}
