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
 *   Author(s): Christophe Grosjean, Meng Tan, Jennifer Inthavong
 */

#pragma once

#include "mod/internal/widget/composite.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/edit_valid.hpp"
#include "mod/internal/widget/help_icon.hpp"
#include "mod/internal/widget/image.hpp"
#include "mod/internal/widget/button.hpp"
#include "mod/internal/widget/vertical_scroll_text.hpp"
#include "translation/translation.hpp"
#include "mod/internal/widget/multiline.hpp"

class Theme;

class WidgetLogin : public WidgetComposite
{
public:
    struct Events
    {
        WidgetEventNotifier onsubmit;
        WidgetEventNotifier oncancel;
        WidgetEventNotifier onctrl_shift;
    };

    struct EditTexts
    {
        WidgetEdit::Text login;
        WidgetEdit::Text target;
        WidgetEdit::Text password;
    };

    WidgetLogin(
        gdi::GraphicApi & drawable, CopyPaste & copy_paste,
        WidgetTooltipShower & tooltip_shower,
        int16_t left, int16_t top, uint16_t width, uint16_t height,
        Events events, chars_view caption,
        chars_view login, chars_view password, chars_view target,
        chars_view label_text_login,
        chars_view label_text_password,
        chars_view label_text_target,
        chars_view label_error_message,
        chars_view login_message,
        Widget * extra_button,
        bool enable_target_field,
        Font const & font, Translator tr, Theme const & theme);

    void move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height);

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    EditTexts get_edit_texts() const noexcept;

private:

public:
    WidgetEventNotifier oncancel;
    WidgetEventNotifier onctrl_shift;
    WidgetTooltipShower & tooltip_shower;

    WidgetLabel        error_message_label;
    WidgetEditValid    login_edit;
    WidgetEditValid    password_edit;
    WidgetEditValid    target_edit;
    WidgetVerticalScrollText message_label;
    WidgetImage        img;
    WidgetLabel        version_label;
    WidgetHelpIcon     helpicon;
    Widget *       extra_button;

    Translator tr;

    bool show_target = false;
};
