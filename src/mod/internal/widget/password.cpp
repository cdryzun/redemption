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
 *   Copyright (C) Wallix 2010-2013
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#include "mod/internal/widget/password.hpp"
#include "mod/internal/copy_paste.hpp"
#include "keyboard/keymap.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text_metrics.hpp"
#include "utils/utf.hpp"

WidgetPasswordFont::WidgetPasswordFont(Font const& font)
    : shadow_font(nullptr, 0, 0, font.max_height(), nullptr, font.item('*').view)
{}

WidgetPassword::WidgetPassword(
    gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
    Colors colors, WidgetEventNotifier onsubmit
)
    : WidgetPasswordFont(font)
    , WidgetEdit(gd, this->shadow_font, copy_paste, colors, onsubmit)
    , font(font)
{}

WidgetPassword::WidgetPassword(
    gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
    chars_view text, uint16_t max_width,
    Colors colors, WidgetEventNotifier onsubmit
)
    : WidgetPasswordFont(font)
    , WidgetEdit(gd, this->shadow_font, copy_paste, text, max_width, colors, onsubmit)
    , font(font)
{}

void WidgetPassword::toggle_password_visibility(Rect rect)
{
    is_password_visible = !is_password_visible;
    this->set_font(is_password_visible ? this->font : this->shadow_font);
    this->rdp_input_invalidate(rect);
}

void WidgetPassword::rdp_input_scancode(
    KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {

    case Keymap::KEvent::LeftArrow:
    case Keymap::KEvent::UpArrow:
        if (keymap.is_ctrl_pressed()) {
            action_move_cursor_to_begin_of_line(Redraw::Yes);
        }
        else {
            action_move_cursor_left(false, Redraw::Yes);
        }
        break;

    case Keymap::KEvent::RightArrow:
    case Keymap::KEvent::DownArrow:
        if (keymap.is_ctrl_pressed()) {
            action_move_cursor_to_end_of_line(Redraw::Yes);
        }
        else {
            action_move_cursor_right(false, Redraw::Yes);
        }
        break;

    case Keymap::KEvent::Paste:
        copy_paste.paste(*this);
        break;

    case Keymap::KEvent::Copy:
        break;

    case Keymap::KEvent::Cut:
        if (has_text()) {
            set_text(""_av, {Redraw::Yes});
        }
        break;

    default:
        WidgetEdit::rdp_input_scancode(flags, scancode, event_time, keymap);
    }
    REDEMPTION_DIAGNOSTIC_POP()
}
