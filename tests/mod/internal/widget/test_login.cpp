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

#include "mod/internal/copy_paste.hpp"
#include "mod/internal/widget/login.hpp"
#include "mod/internal/widget/screen.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "utils/theme.hpp"

#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/login/"

constexpr auto LOGON_MESSAGE = "Warning! Unauthorized access to this system is forbidden and will be prosecuted by law."_av;

struct TestWidgetLoginCtx
{
    TestGraphic drawable{800, 600};
    CopyPaste copy_paste{false};
    WidgetScreen parent;
    NotifyTrace onsubmit;
    NotifyTrace oncancel;
    WidgetLogin flat_login;

    TestWidgetLoginCtx(
        chars_view caption,
        chars_view login,
        chars_view password,
        chars_view target,
        chars_view login_message = LOGON_MESSAGE,
        Theme theme = Theme(),
        bool enable_target_field = false,
        uint16_t w = 800,
        uint16_t h = 600)
    : drawable{w, h}
    , parent{drawable, w, h, global_font_deja_vu_14(), Theme{}}
    , flat_login(
        drawable, copy_paste, parent, 0, 0, parent.cx(), parent.cy(),
        {onsubmit, oncancel, WidgetEventNotifier()},
        caption, login, password, target,
        "Login"_av, "Password"_av, "Target"_av, ""_av,
        login_message, nullptr, enable_target_field, global_font_deja_vu_14(),
        MsgTranslationCatalog::default_catalog(), theme)
    {}
};

RED_AUTO_TEST_CASE(TraceWidgetLogin)
{
    TestWidgetLoginCtx ctx("test1"_av, "rec"_av, "rec"_av, ""_av);

    ctx.flat_login.rdp_input_invalidate(ctx.flat_login.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_1.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLogin2)
{
    TestWidgetLoginCtx ctx("test2"_av, nullptr, nullptr, nullptr);

    ctx.flat_login.rdp_input_invalidate(ctx.flat_login.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_2.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLogin3)
{
    TestWidgetLoginCtx ctx("test3"_av, nullptr, nullptr, nullptr, LOGON_MESSAGE);

    (void)ctx.flat_login.next_focus(Widget::FocusDirection::Forward, Widget::FocusStrategy::Next);
    (void)ctx.flat_login.next_focus(Widget::FocusDirection::Forward, Widget::FocusStrategy::Next);
    // password
    (void)ctx.flat_login.next_focus(Widget::FocusDirection::Forward, Widget::FocusStrategy::Next);

    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    RED_CHECK(ctx.oncancel.get_and_reset() == 0);

    Keymap keymap(*find_layout_by_id(KeyLayout::KbdId(0x040C)));
    keymap.event(Keymap::KbdFlags(), Keymap::Scancode(0x1c));
    ctx.flat_login.rdp_input_scancode(Keymap::KbdFlags(), Keymap::Scancode(0x1c), 0, keymap);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 1);
    RED_CHECK(ctx.oncancel.get_and_reset() == 0);

    // ask to widget to redraw at it's current position
    ctx.flat_login.rdp_input_invalidate(ctx.flat_login.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_3.png");

    keymap.event(Keymap::KbdFlags(), Keymap::Scancode(0x01));
    ctx.flat_login.rdp_input_scancode(Keymap::KbdFlags(), Keymap::Scancode(0x01), 0, keymap);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    RED_CHECK(ctx.oncancel.get_and_reset() == 1);
}

RED_AUTO_TEST_CASE(TraceWidgetLoginHelp)
{
    TestWidgetLoginCtx ctx("test4"_av, nullptr, nullptr, nullptr);

    ctx.flat_login.rdp_input_invalidate(ctx.flat_login.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_help_1.png");

    ctx.flat_login.rdp_input_mouse(MOUSE_FLAG_MOVE, 728, 557);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_help_2.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLoginClip)
{
    TestWidgetLoginCtx ctx("test6"_av, nullptr, nullptr, nullptr);

    ctx.flat_login.rdp_input_invalidate(Rect(
        20 + ctx.flat_login.x(),
        ctx.flat_login.y(),
        ctx.flat_login.cx(),
        ctx.flat_login.cy()
    ));

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_4.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLoginClip2)
{
    TestWidgetLoginCtx ctx("test6"_av, nullptr, nullptr, nullptr);

    ctx.flat_login.rdp_input_invalidate(Rect(
        20 + ctx.flat_login.x(),
        5 + ctx.flat_login.y(),
        30,
        10
    ));

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_5.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLogin4)
{
    TestWidgetLoginCtx ctx(
        "test1"_av,
        "rec"_av, "rec"_av, "rec"_av,
        "WARNING: Unauthorized access to this system is forbidden and will be prosecuted by law.\n\n"
        "By accessing this system, you agree that your actions may be monitored if unauthorized usage is suspected."_av);

    ctx.flat_login.rdp_input_invalidate(ctx.flat_login.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_6.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLogin_target_field)
{
    TestWidgetLoginCtx ctx("test1"_av, "rec"_av, "rec"_av, ""_av, LOGON_MESSAGE, Theme{}, true);

    ctx.flat_login.rdp_input_invalidate(ctx.flat_login.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_7.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLogin_little_width)
{
    TestWidgetLoginCtx ctx(
        "caption"_av, "login"_av, "password"_av, "target"_av,
        LOGON_MESSAGE, Theme{}, true, 300, 300
    );

    ctx.flat_login.rdp_input_invalidate(ctx.flat_login.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "login_8.png");
}
