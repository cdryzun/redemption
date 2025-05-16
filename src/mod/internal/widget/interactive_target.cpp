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

#include "mod/internal/widget/interactive_target.hpp"
#include "keyboard/keymap.hpp"
#include "utils/theme.hpp"
#include "utils/strutils.hpp"

namespace
{
    WidgetEventNotifier next_focus_event(WidgetInteractiveTarget & w)
    {
        return WidgetEventNotifier([&w]{
            (void)w.next_focus(Widget::FocusDirection::Forward, Widget::FocusStrategy::Next);
        });
    }
}

WidgetInteractiveTarget::WidgetInteractiveTarget(
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
    Widget * extra_button
)
    : WidgetComposite(drawable, Focusable::Yes, theme.global.bgcolor)
    , oncancel(events.oncancel)
    , onctrl_shift(events.onctrl_shift)
    , caption_label(drawable, font, caption, WidgetLabel::Colors::from_theme(theme))
    , separator(drawable, theme.global.separator_color)
    , device_info(
        drawable, font, ask_device ? device_info : ""_av,
        // TODO BUG error context should be transfered instead of detected with string prefix (incompatible with translation)
        {
            .fg = ask_device && (utils::starts_with(device_info, "Error:"_av)
                              || utils::starts_with(device_info, "Erreur:"_av))
                ? theme.global.error_color
                : theme.global.fgcolor,
            .bg =  theme.global.bgcolor
        }
    )
    , device_edit(
        drawable, font, copy_paste,
        {
            .type = ask_device
                ? WidgetEditValid::Type::Edit
                : WidgetEditValid::Type::Text,
            .label = label_device,
            .edit = ask_device ? ""_av : device_info,
        },
        WidgetEditValid::Colors::from_theme(theme),
        (ask_login || ask_password)
            ? next_focus_event(*this)
            : events.onsubmit
    )
    , login_edit(
        drawable, font, copy_paste,
        {
            .type = ask_login
                ? WidgetEditValid::Type::Edit
                : WidgetEditValid::Type::Text,
            .label = label_login,
            .edit = ask_login ? ""_av : text_login,
        },
        WidgetEditValid::Colors::from_theme(theme),
        next_focus_event(*this)
    )
    , password_edit(
        drawable, font, copy_paste,
        {
            .type = WidgetEditValid::Type::Password,
            .label = label_password,
        },
        WidgetEditValid::Colors::from_theme(theme),
        !(ask_login || ask_password)
            ? next_focus_event(*this)
            : events.onsubmit
    )
    , extra_button(extra_button)
    , fgcolor(theme.global.fgcolor)
    , ask_device(ask_device)
    , ask_login(ask_login)
    , ask_password(ask_login || ask_password)
{
    HasFocus device_has_focus = HasFocus::No;
    HasFocus login_has_focus = HasFocus::No;
    HasFocus password_has_focus = HasFocus::No;

    if (this->ask_device) {
        device_has_focus = HasFocus::Yes;
    }
    else if (this->ask_login) {
        login_has_focus = HasFocus::Yes;
    }
    else {
        password_has_focus = HasFocus::Yes;
    }

    this->add_widget(this->caption_label);
    this->add_widget(this->separator);

    this->add_widget(this->device_edit, device_has_focus);
    if (ask_device) {
        this->add_widget(this->device_info);
    }
    this->add_widget(this->login_edit, login_has_focus);
    if (this->ask_password) {
        this->add_widget(this->password_edit, password_has_focus);
    }

    if (extra_button) {
        this->add_widget(*extra_button);
    }

    this->move_size_widget(left, top, width, height);
}

void WidgetInteractiveTarget::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->set_xy(left, top);
    this->set_wh(width, height);

    // Center bloc positioning
    // Device, Login and Password boxes
    bool const is_placeholder = false;

    uint16_t offset_edit = device_edit.label_width(is_placeholder);
    offset_edit = std::max(offset_edit, login_edit.label_width(is_placeholder));
    if (ask_password) {
        offset_edit = std::max(offset_edit, password_edit.label_width(is_placeholder));
    }
    offset_edit += 20;

    uint16_t edit_w = 400;
    edit_w = std::max(edit_w, device_edit.text_width());
    edit_w = std::max(edit_w, login_edit.text_width());
    if (ask_device) {
        edit_w = std::max(edit_w, device_info.cx());
    }
    if (ask_password) {
        edit_w = std::max(edit_w, password_edit.text_width());
    }

    uint16_t cbloc_w = std::max(
        this->caption_label.cx(),
        checked_cast<uint16_t>(edit_w + offset_edit)
    );

    constexpr int y_sep = 20;

    const int cbloc_h =
        this->caption_label.cy() + y_sep +
        y_sep +
        this->device_edit.cy() + y_sep +
        (this->ask_device ? this->device_info.cy() + y_sep - 10 : 0) +
        this->login_edit.cy() +
        (this->ask_password ? this->password_edit.cy() + y_sep : 0);

    const int16_t x_cbloc = checked_int(left + (width  - cbloc_w) / 2);
    const int y_cbloc = (height - cbloc_h) / 3;

    int y = y_cbloc;

    this->caption_label.set_xy(left + (width - this->caption_label.cx()) / 2, top + y);
    y = this->caption_label.ebottom() + y_sep;

    this->separator.set_xy(x_cbloc, y);
    this->separator.set_width(cbloc_w);
    y = this->separator.ebottom() + y_sep;

    auto update_edit_layout = [&](WidgetEditValid & w){
        w.set_xy(checked_int(x_cbloc), checked_int(y));
        w.update_layout({
            .width = cbloc_w,
            .edit_offset = offset_edit,
            .label_as_placeholder = false,
        });
        return w.ebottom() + y_sep;
    };

    y = update_edit_layout(device_edit);

    if (this->ask_device) {
        this->device_info.set_xy(x_cbloc + offset_edit, y - 10);

        y = this->device_info.ebottom() + y_sep;
    }

    y = update_edit_layout(login_edit);

    if (this->ask_password) {
        update_edit_layout(password_edit);
    }

    if (this->extra_button) {
        this->extra_button->set_xy(left + 60, top + height - 60);
    }
}

void WidgetInteractiveTarget::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
        case Keymap::KEvent::Esc:
            this->oncancel();
            break;

        case Keymap::KEvent::Ctrl:
        case Keymap::KEvent::Shift:
            if (this->extra_button
                && keymap.is_shift_pressed()
                && keymap.is_ctrl_pressed())
            {
                this->onctrl_shift();
            }
            break;

        default:
            WidgetComposite::rdp_input_scancode(flags, scancode, event_time, keymap);
            break;
    }
    REDEMPTION_DIAGNOSTIC_POP()
}
