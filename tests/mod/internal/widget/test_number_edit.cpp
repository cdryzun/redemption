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
#include "mod/internal/widget/number_edit.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/number_edit/"


RED_AUTO_TEST_CASE(WidgetNumberEditEventPushChar)
{
    TestGraphic drawable(100, 30);
    CopyPaste copy_paste(false);

    NotifyTrace onsubmit;

    WidgetEdit::Colors colors{
        .fg = NamedBGRColor::GREEN,
        .bg = NamedBGRColor::RED,
        .border = NamedBGRColor::BLUE,
        .focus_border = NamedBGRColor::BLUE,
        .cursor = NamedBGRColor::GREY,
    };

    WidgetNumberEdit wnumber_edit(
        drawable, global_font_deja_vu_14(), copy_paste, colors, onsubmit
    );
    wnumber_edit.update_width(100);
    wnumber_edit.set_text("123456"_av, {});
    wnumber_edit.init_focus();

    wnumber_edit.rdp_input_invalidate(wnumber_edit.get_rect());
    RED_CHECK_IMG(drawable, IMG_TEST_PATH "number_edit_1.png");

    Keymap keymap(*find_layout_by_id(KeyLayout::KbdId(0x409))); // en-US
    using KFlags = Keymap::KbdFlags;
    using Scancode = Keymap::Scancode;

    keymap.event(KFlags(), Scancode::A);
    wnumber_edit.rdp_input_scancode(KFlags(), Scancode::A, 0, keymap);
    RED_CHECK_IMG(drawable, IMG_TEST_PATH "number_edit_1.png");
    RED_CHECK(onsubmit.get_and_reset() == 0);

    keymap.event(KFlags(), Scancode::Digit2);
    wnumber_edit.rdp_input_scancode(KFlags(), Scancode::Digit2, 0, keymap);
    RED_CHECK_IMG(drawable, IMG_TEST_PATH "number_edit_3.png");
    RED_CHECK(onsubmit.get_and_reset() == 0);

    RED_CHECK(wnumber_edit.get_text() == "1234562"_av);
    RED_CHECK(wnumber_edit.get_text_as_uint() == 1234562);
}
