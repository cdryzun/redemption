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

#include "mod/internal/widget/edit.hpp"
#include "mod/internal/widget/screen.hpp"
#include "mod/internal/copy_paste.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/edit/"

namespace
{

struct TestWidgetEditCtx
{
    TestGraphic drawable;
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

    WidgetEdit edit()
    {
        return WidgetEdit(drawable, font, copy_paste, colors, onsubmit);
    }

    struct Keyboard
    {
        WidgetEdit& edit;
        Keymap keymap{*find_layout_by_id(KeyLayout::KbdId(0x409))}; // US

        enum class Ctrl { On, Off, };

        void scancode(kbdtypes::KeyCode keycode, Ctrl ctrl)
        {
            if (ctrl == Ctrl::On) {
                keymap.event(kbdtypes::KbdFlags::NoFlags, kbdtypes::Scancode::LCtrl);
            }

            auto scancode = kbdtypes::keycode_to_scancode(keycode);
            auto flags = kbdtypes::keycode_to_kbdflags(keycode);
            keymap.event(flags, scancode);
            edit.rdp_input_scancode(flags, scancode, 0, keymap);

            if (ctrl == Ctrl::On) {
                keymap.event(kbdtypes::KbdFlags::Release, kbdtypes::Scancode::LCtrl);
            }
        }
    };

    Keyboard keyboard(WidgetEdit& edit)
    {
        return {edit};
    }
};

} // anonymous namespace


RED_AUTO_TEST_CASE(WidgetEditGd)
{
    TestWidgetEditCtx ctx{
      .drawable = TestGraphic{200, 100},
    };

    struct D
    {
        WidgetEdit edit;
        chars_view text;
        int16_t x;
        int16_t y;
        Rect clip;
    };

    auto make_data = [&](chars_view text, int16_t x, int16_t y, Rect clip = {}) {
        return D{ctx.edit(), text, x, y, clip};
    };

    D datas[]{
        make_data("test1"_av, -20, -7),
        make_data("test2"_av, 40, 0),
        make_data("test3"_av, 100, -7),
        make_data("test4"_av, 160, -7),
        make_data("test5"_av, -10, 40),
        make_data("test6"_av, 100, 30),
        make_data("test7"_av, 100, 55, Rect(107, 63, 38, 7)),
        make_data("test8"_av, -10, 88),
        make_data("test9"_av, 60, 82),
        make_data("test10"_av, 160, 86),
    };

    for (auto & d : datas) {
        d.edit.set_xy(d.x, d.y);
        d.edit.update_width(50);
        d.edit.set_text(d.text, {WidgetEdit::Redraw::No});
        d.edit.rdp_input_invalidate(d.clip.isempty() ? d.edit.get_rect() : d.clip);
    }

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_1.png");

    for (auto & d : datas) {
        d.edit.focus();
    }

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_2.png");
}

RED_AUTO_TEST_CASE(WidgetEditKbd)
{
    using Ctrl = TestWidgetEditCtx::Keyboard::Ctrl;
    using KC = kbdtypes::KeyCode;

    TestWidgetEditCtx ctx{
      .drawable = TestGraphic{65, 30},
    };

    auto edit = ctx.edit();
    auto keyboard = ctx.keyboard(edit);

    edit.update_width(60);
    edit.init_focus();
    edit.rdp_input_invalidate(edit.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_empty.png");
    RED_CHECK(edit.get_text() == ""_av);

    edit.set_text("trb"_av, {WidgetEdit::Redraw::Yes});
    RED_CHECK(edit.get_text() == "trb"_av);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_trb.png");

    //
    // Scancode
    //

    #define TEST_SCANCODE(keycode, ctrl, text, img_path) \
        keyboard.scancode(keycode, ctrl);                \
        RED_CHECK(edit.get_text() == text);              \
        RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH img_path)

    TEST_SCANCODE(KC::Key_A, Ctrl::Off, "trba"_av, "kbd_trb_insert_a.png");
    TEST_SCANCODE(KC::Key_T, Ctrl::Off, "trbat"_av, "kbd_trba_insert_t.png");
    TEST_SCANCODE(KC::Key_R, Ctrl::Off, "trbatr"_av, "kbd_trbat_insert_r.png");

    TEST_SCANCODE(KC::UpArrow, Ctrl::Off, "trbatr"_av, "kbd_trbatr_left.png");
    TEST_SCANCODE(KC::RightArrow, Ctrl::Off, "trbatr"_av, "kbd_trbat_r_right.png");

    TEST_SCANCODE(KC::Backspace, Ctrl::Off, "trbat"_av, "kbd_trbatr_backspace.png");

    TEST_SCANCODE(KC::LeftArrow, Ctrl::Off, "trbat"_av, "kbd_trbat_left.png");
    TEST_SCANCODE(KC::LeftArrow, Ctrl::Off, "trbat"_av, "kbd_trba_t_left.png");

    TEST_SCANCODE(KC::Delete, Ctrl::Off, "trbt"_av, "kbd_trb_at_delete.png");

    TEST_SCANCODE(KC::LeftArrow, Ctrl::Off, "trbt"_av, "kbd_trb_t_left.png");
    TEST_SCANCODE(KC::LeftArrow, Ctrl::Off, "trbt"_av, "kbd_tr_bt_left.png");
    TEST_SCANCODE(KC::LeftArrow, Ctrl::Off, "trbt"_av, "kbd_t_brt_left.png");
    TEST_SCANCODE(KC::End, Ctrl::Off, "trbt"_av, "kbd_trbt_end.png");
    TEST_SCANCODE(KC::RightArrow, Ctrl::Off, "trbt"_av, "kbd_trbt_end.png");
    TEST_SCANCODE(KC::Home, Ctrl::Off, "trbt"_av, "kbd_trbt_home.png");
    TEST_SCANCODE(KC::LeftArrow, Ctrl::Off, "trbt"_av, "kbd_trbt_home.png");

    TEST_SCANCODE(KC::Key_A, Ctrl::Off, "atrbt"_av, "kbd__trbt_insert_a.png");
    TEST_SCANCODE(KC::Key_B, Ctrl::Off, "abtrbt"_av, "kbd_a_trbt_insert_b.png");
    TEST_SCANCODE(KC::Key_C, Ctrl::Off, "abctrbt"_av, "kbd_ab_trbt_insert_c.png");
    TEST_SCANCODE(KC::Key_D, Ctrl::Off, "abcdtrbt"_av, "kbd_abc_trbt_insert_d.png");

    TEST_SCANCODE(KC::End, Ctrl::Off, "abcdtrbt"_av, "kbd_abcd_trbt_end.png");

    TEST_SCANCODE(KC::Space, Ctrl::Off, "abcdtrbt "_av, "kbd_abcdtrbt_insert_space.png");
    TEST_SCANCODE(KC::Key_A, Ctrl::Off, "abcdtrbt a"_av, "kbd_abcdtrbt._insert_a.png");
    TEST_SCANCODE(KC::Key_B, Ctrl::Off, "abcdtrbt ab"_av, "kbd_abcdtrbt.a_insert_b.png");

    TEST_SCANCODE(KC::LeftArrow, Ctrl::On, "abcdtrbt ab"_av, "kbd_abcdtrbt.ab_ctrl_left.png");
    TEST_SCANCODE(KC::LeftArrow, Ctrl::On, "abcdtrbt ab"_av, "kbd_abcdtrbt._ab_ctrl_left.png");
    TEST_SCANCODE(KC::RightArrow, Ctrl::On, "abcdtrbt ab"_av, "kbd__abcdtrbt.ab_ctrl_right.png");
    TEST_SCANCODE(KC::RightArrow, Ctrl::On, "abcdtrbt ab"_av, "kbd_abcdtrbt._ab_ctrl_right.png");
    TEST_SCANCODE(KC::Home, Ctrl::Off, "abcdtrbt ab"_av, "kbd_abcdtrbt.ab_home.png");

    TEST_SCANCODE(KC::Delete, Ctrl::Off, "bcdtrbt ab"_av, "kbd__abcdtrbt.ab_delete.png");
    TEST_SCANCODE(KC::Delete, Ctrl::On, "ab"_av, "kbd__bcdtrbt.ab_ctrl_delete.png");

    RED_CHECK(ctx.onsubmit.get_and_reset() == 0);
    TEST_SCANCODE(KC::Enter, Ctrl::Off, "ab"_av, "kbd__bcdtrbt.ab_ctrl_delete.png");
    RED_CHECK(ctx.onsubmit.get_and_reset() == 1);

    #undef TEST_SCANCODE
}

RED_AUTO_TEST_CASE(WidgetEditMouse)
{
    TestWidgetEditCtx ctx{
      .drawable = TestGraphic{55, 30},
    };

    auto edit = ctx.edit();

    edit.update_width(50);
    edit.init_focus();
    edit.rdp_input_invalidate(edit.get_rect());

    edit.set_text("abcdtrbi"_av, {WidgetEdit::Redraw::Yes});
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "edit_abcdtrbi.png");

    #define TEST_MOUSE(x, img_path)                                     \
        edit.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, x, 5); \
        RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH img_path)

    TEST_MOUSE(3, "mouse_abcdtrbi_3.png");
    TEST_MOUSE(1, "mouse_abcdtrbi_2.png");
    TEST_MOUSE(47, "mouse_abcdtrbi_47.png");
    TEST_MOUSE(49, "mouse_abcdtrbi_48.png");
    TEST_MOUSE(9, "mouse_abcdtrbi_10.png");
    TEST_MOUSE(25, "mouse_abcdtrbi_25.png");

    #undef TEST_MOUSE
}

RED_AUTO_TEST_CASE(WidgetEditSetText)
{
    TestWidgetEditCtx ctx{
      .drawable = TestGraphic{40, 84},
    };

    using CusorPosition = WidgetEdit::CusorPosition;

    struct D
    {
        WidgetEdit edit;
        CusorPosition cursor_pos;
    };

    auto text = "abcdefg"_av;
    auto text2 = "abcde"_av;

    auto make_edit = [&](){
        return WidgetEdit(ctx.drawable, ctx.font, ctx.copy_paste, text, ctx.colors, ctx.onsubmit);
    };

    WidgetEdit edits[]{
        make_edit(),
        make_edit(),
        make_edit(),
    };

    int16_t y = 0;

    for (auto & edit : edits) {
        edit.init_focus();
        edit.set_xy(0, y);
        y += 30;
    }

    // right part is not redrawing
    auto update_text = [](WidgetEdit& edit, chars_view text, CusorPosition cursor_position) {
        edit.set_text(text, {WidgetEdit::Redraw::No, cursor_position});
        edit.set_xy(edit.x(), edit.y());
        edit.update_width(40);
        edit.rdp_input_invalidate(edit.get_rect());
    };

    update_text(edits[0], text, CusorPosition::CursorToEnd);
    update_text(edits[1], text, CusorPosition::CursorToBegin);
    update_text(edits[2], text, CusorPosition::KeepCursorPosition);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "set_text_init.png");

    auto redraw = WidgetEdit::Redraw::Yes;

    edits[0].action_move_cursor_left(false, redraw);

    edits[1].action_move_cursor_right(false, redraw);

    edits[2].action_move_cursor_left(false, redraw);
    edits[2].action_move_cursor_left(false, redraw);
    edits[2].action_move_cursor_left(false, redraw);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "set_text_update_pos.png");

    update_text(edits[0], text2, CusorPosition::KeepCursorPosition);
    update_text(edits[1], text2, CusorPosition::KeepCursorPosition);
    update_text(edits[2], text2, CusorPosition::KeepCursorPosition);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "set_text_keep_pos.png");
}
