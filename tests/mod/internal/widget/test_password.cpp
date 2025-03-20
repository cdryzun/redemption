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
#include "mod/internal/widget/password.hpp"
#include "mod/internal/widget/composite.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/password/"


RED_AUTO_TEST_CASE(TraceWidgetPassword)
{
    TestGraphic drawable{70, 30};
    CopyPaste copy_paste{false};
    Font const& font = global_font_deja_vu_14();
    NotifyTrace onsubmit {};
    WidgetEdit::Colors colors {
        .fg = NamedBGRColor::RED,
        .bg = NamedBGRColor::YELLOW,
        .border = NamedBGRColor::BLUE,
        .focus_border = NamedBGRColor::GREEN,
        .cursor = NamedBGRColor::GREY,
    };

    WidgetPassword pass(drawable, font, copy_paste, colors, onsubmit);

    pass.update_width(60);
    pass.init_focus();
    pass.rdp_input_invalidate(pass.get_rect());

    RED_CHECK_IMG(drawable, IMG_TEST_PATH "password_empty.png");
    RED_CHECK(pass.get_text() == ""_av);

    pass.set_text("abc def"_av, {WidgetEdit::Redraw::Yes});
    RED_CHECK(pass.get_text() == "abc def"_av);
    RED_CHECK_IMG(drawable, IMG_TEST_PATH "password_1.png");

    Keymap keymap{*find_layout_by_id(KeyLayout::KbdId(0x409))}; // US
    using KC = kbdtypes::KeyCode;

    auto scancode = [&](KC keycode, bool ctrl)
    {
        if (ctrl) {
            keymap.event(kbdtypes::KbdFlags::NoFlags, kbdtypes::Scancode::LCtrl);
        }

        auto scancode = kbdtypes::keycode_to_scancode(keycode);
        auto flags = kbdtypes::keycode_to_kbdflags(keycode);
        keymap.event(flags, scancode);
        pass.rdp_input_scancode(flags, scancode, 0, keymap);

        if (ctrl) {
            keymap.event(kbdtypes::KbdFlags::Release, kbdtypes::Scancode::LCtrl);
        }
    };

    scancode(KC::LeftArrow, false);
    RED_CHECK_IMG(drawable, IMG_TEST_PATH "password_left.png");

    // space in "abc def" should be ignored
    scancode(KC::LeftArrow, true);
    RED_CHECK_IMG(drawable, IMG_TEST_PATH "password_ctrl_left.png");
}
