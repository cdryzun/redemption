/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2014
 *   Author(s): Christophe Grosjean, Raphael Zhou, Meng Tan
 *              Jennifer Inthavong
 */

#pragma once

#include "mod/internal/widget/composite.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/edit.hpp"
#include "mod/internal/widget/edit_valid.hpp"
#include "mod/internal/widget/widget_rect.hpp"

class Theme;
class WidgetButton;

class WidgetInteractiveTarget : public WidgetComposite
{
public:
    // ASK DEVICE YES/NO
    // ASK CRED : LOGIN+PASSWORD/PASSWORD/NO

    struct Events
    {
        WidgetEventNotifier onsubmit;
        WidgetEventNotifier oncancel;
        WidgetEventNotifier onctrl_shift;
    };

    WidgetInteractiveTarget(
        gdi::GraphicApi & drawable, CopyPaste & copy_paste,
        int16_t left, int16_t top, uint16_t width, uint16_t height,
        Events events,
        bool ask_device, bool ask_login, bool ask_password,
        Theme const & theme,
        chars_view caption,
        chars_view label_device, chars_view device_info,
        chars_view label_login, chars_view text_login,
        chars_view label_password,
        Font const & font,
        Widget * extra_button);

    void move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height);

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    WidgetEdit::Text get_edit_device() const noexcept { return device_edit.get_text(); }
    WidgetEdit::Text get_edit_login() const noexcept { return login_edit.get_text(); }
    WidgetEdit::Text get_edit_password() const noexcept { return password_edit.get_text(); }

private:
    WidgetEventNotifier oncancel;
    WidgetEventNotifier onctrl_shift;

    WidgetLabel        caption_label;
    WidgetRect         separator;

    WidgetLabel        device_info;

    WidgetEditValid    device_edit;
    WidgetEditValid    login_edit;
    WidgetEditValid    password_edit;

    Widget * extra_button;

    Color fgcolor;

    bool ask_device;
    bool ask_login;
    bool ask_password;
};
