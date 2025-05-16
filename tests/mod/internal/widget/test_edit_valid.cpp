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
 *   Author(s): Christophe Grosjean, Raphael Zhou, Jonathan Poelen,
 *              Meng Tan
 */

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "mod/internal/widget/edit_valid.hpp"
#include "mod/internal/copy_paste.hpp"
#include "gdi/screen_functions.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/edit_valid/"

struct TestWidgetEditValid
{
    TestGraphic drawable;
    WidgetEditValid::Options opts;
    WidgetEditValid::Colors colors {
        .fg = NamedBGRColor::RED,
        .bg = NamedBGRColor::YELLOW,
        .placeholder = NamedBGRColor::MEDIUM_GREY,
        .edit_fg = NamedBGRColor::PINK,
        .edit_bg = NamedBGRColor::CYAN,
        .border = NamedBGRColor::BLUE,
        .focus_border = NamedBGRColor::BROWN,
        .cursor = NamedBGRColor::GREY,
        .password_toggle = NamedBGRColor::MEDIUM_GREY,
    };
    CopyPaste copy_paste {false};
    Font const& font = global_font_deja_vu_14();
    NotifyTrace onsubmit {};

    WidgetEditValid edit()
    {
        return WidgetEditValid(drawable, font, copy_paste, opts, colors, onsubmit);
    }

    void click_down(WidgetEditValid& edit, uint16_t x, uint16_t y)
    {
        edit.rdp_input_mouse(MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN, x, y);
    }

    void click_up(WidgetEditValid& edit, uint16_t x, uint16_t y)
    {
        edit.rdp_input_mouse(MOUSE_FLAG_BUTTON1, x, y);
    }

    void click(WidgetEditValid& edit, uint16_t x, uint16_t y)
    {
        click_down(edit, x, y);
        click_up(edit, x, y);
    }

    struct KeyBoard
    {
        WidgetEditValid& edit;
        Keymap keymap{*find_layout_by_id(KeyLayout::KbdId(0x409))}; // US

        void send_scancode(kbdtypes::KeyCode keycode)
        {
            auto scancode = kbdtypes::keycode_to_scancode(keycode);
            auto flags = kbdtypes::keycode_to_kbdflags(keycode);
            keymap.event(flags, scancode);
            edit.rdp_input_scancode(flags, scancode, 0, keymap);
        }
    };

    KeyBoard keyboard(WidgetEditValid& edit)
    {
        return KeyBoard{edit};
    }
};

RED_AUTO_TEST_CASE(TraceWidgetEditWithLabel)
{
    TestWidgetEditValid ctx{
        .drawable {150, 50},
        .opts {
            .type = WidgetEditValid::Type::Edit,
            .label = "text"_av,
        }
    };

    auto edit = ctx.edit();
    edit.init_focus();

    edit.set_xy(10, 10);
    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(false) + 19),
        .label_as_placeholder = false,
    });

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_1.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Key_A);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_2.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Backspace);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_1.png");

    gdi_clear_screen(ctx.drawable, {ctx.drawable.width(), ctx.drawable.height()});
    edit.set_text("Yliajli"_av, {});
    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(false) + 28),
        .label_as_placeholder = false,
    });

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_3.png");

    ctx.click(edit, 71, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_4.png");

    edit.blur();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_5.png");

    edit.focus();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_4.png");

    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    ctx.click_down(edit, 110, 15);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_6.png");

    ctx.click_up(edit, 110, 15);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 1);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_4.png");
}

RED_AUTO_TEST_CASE(TraceWidgetEditWithPlaceholder)
{
    TestWidgetEditValid ctx{
        .drawable {150, 50},
        .opts {
            .type = WidgetEditValid::Type::Edit,
            .label = "text"_av,
        }
    };

    auto edit = ctx.edit();
    edit.init_focus();

    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(true) + 29),
        .label_as_placeholder = true,
    });
    edit.set_xy(10, 10);

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_1.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Key_A);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_2.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Backspace);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_1.png");

    edit.set_text("Ylajali"_av, {});
    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(true) + 29),
        .label_as_placeholder = true,
    });

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_3.png");

    ctx.click(edit, 20, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_4.png");

    edit.blur();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_5.png");

    edit.focus();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_4.png");

    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    ctx.click_down(edit, 110, 15);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_6.png");

    ctx.click_up(edit, 110, 15);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 1);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_placeholder_4.png");
}

RED_AUTO_TEST_CASE(TraceWidgetEditLabelsPassword)
{
    TestWidgetEditValid ctx{
        .drawable {150, 50},
        .opts {
            .type = WidgetEditValid::Type::Password,
            .label = "Password"_av,
        }
    };

    auto edit = ctx.edit();
    edit.init_focus();

    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(true) + 29),
        .label_as_placeholder = true,
    });
    edit.set_xy(10, 10);

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_1.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Key_A);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_2.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Backspace);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_1.png");

    edit.set_text("Ylajaiiiii"_av, {});
    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(true) + 29),
        .label_as_placeholder = true,
    });

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_3.png");

    ctx.click(edit, 50, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_4.png");

    edit.blur();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_5.png");

    edit.focus();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_4.png");

    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    ctx.click_down(edit, 110, 15);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_6.png");

    ctx.click_up(edit, 110, 15);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 1);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_4.png");

    ctx.click_down(edit, 95, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_7.png");

    ctx.click_up(edit, 95, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_8.png");

    ctx.click_down(edit, 95, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_9.png");

    ctx.click_up(edit, 95, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_password_4.png");
}

RED_AUTO_TEST_CASE(TraceWidgetEditTextWithLabel)
{
    TestWidgetEditValid ctx{
        .drawable {150, 50},
        .opts {
            .type = WidgetEditValid::Type::Text,
            .label = "Text"_av,
            .edit = "My text"_av,
        }
    };

    auto edit = ctx.edit();
    edit.init_focus();

    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(false) + 20),
        .label_as_placeholder = false,
    });
    edit.set_xy(10, 10);

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_1.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Key_A);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_1.png");

    ctx.keyboard(edit).send_scancode(kbdtypes::KeyCode::Backspace);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_1.png");

    edit.set_text("Ylajaiiiii"_av, {});
    edit.update_layout(WidgetEditValid::Layout{
        .width = 120,
        .edit_offset = checked_int(edit.label_width(true) + 29),
        .label_as_placeholder = true,
    });
    gdi_clear_screen(ctx.drawable, {ctx.drawable.width(), ctx.drawable.height()});

    edit.rdp_input_invalidate(edit.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_2.png");

    ctx.click(edit, 50, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_2.png");

    edit.blur();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_2.png");

    edit.focus();
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_2.png");

    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    ctx.click_up(edit, 110, 15);
    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_2.png");

    ctx.click_down(edit, 95, 15);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_valid_text_2.png");
}
