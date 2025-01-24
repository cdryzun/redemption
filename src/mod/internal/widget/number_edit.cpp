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

#include "mod/internal/widget/number_edit.hpp"
#include "keyboard/keymap.hpp"

namespace
{
    bool is_digit(uint32_t c)
    {
        return '0' <= c && c <= '9';
    }
}

void WidgetNumberEdit::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (keymap.last_kevent() == Keymap::KEvent::KeyDown) {
        auto c = keymap.last_decoded_keys().uchars[0];
        if (!is_digit(c)) {
            return ;
        }
    }
    WidgetEdit::rdp_input_scancode(flags, scancode, event_time, keymap);
}

void WidgetNumberEdit::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (bool(flag & KbdFlags::Release)) {
        return;
    }

    if (!is_digit(unicode)) {
        return ;
    }

    uint32_t uc = unicode;
    insert_chars({&uc, 1}, Redraw::Yes);
}

void WidgetNumberEdit::clipboard_insert_utf8(zstring_view text)
{
    uint32_t ucs[21];
    auto * p = ucs;

    auto ucs_text = bytes_view(text);
    // remove left spaces
    while (!ucs_text.empty()
      && (ucs_text.front() == ' '
       || ucs_text.front() == '\t'
       || ucs_text.front() == '\r'
    )) {
        ucs_text = ucs_text.drop_front(1);
    }

    // copy digits
    for (uint8_t c : ucs_text) {
        if (!is_digit(c) || p < std::end(ucs)) {
            return ;
        }
        *p++ = c;
    }

    insert_chars({ucs, p}, Redraw::Yes);
}
