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

#include "mod/internal/widget/form.hpp"
#include "utils/sugar/chars_to_int.hpp"
#include "translation/trkeys.hpp"
#include "utils/theme.hpp"

using namespace std::chrono_literals;

namespace
{
    WidgetEventNotifier check_confirmation_event(WidgetForm & w)
    {
        return [&w]{ w.check_confirmation(); };
    }
}

WidgetForm::WidgetForm(
    gdi::GraphicApi & drawable, CopyPaste & copy_paste,
    int16_t left, int16_t top, int16_t width, int16_t height,
    Events events,
    Font const & font, Theme const & theme, Translator tr,
    unsigned flags, std::chrono::minutes duration_max
)
    : WidgetForm(drawable, copy_paste, events,
                 font, theme, tr, flags, duration_max)
{
    this->move_size_widget(left, top, width, height);
}

WidgetForm::WidgetForm(
    gdi::GraphicApi & drawable, CopyPaste & copy_paste,
    Events events,
    Font const & font, Theme const & theme, Translator tr,
    unsigned flags, std::chrono::minutes duration_max
)
    : WidgetComposite(drawable, Focusable::Yes)
    , events(events)
    , font(font)
    , tr(tr)
    , flags(flags)
    , duration_max(duration_max == 0min ? 60000min : duration_max)
    , warning_msg(drawable, font, ""_av, WidgetLabel::Colors::from_theme(theme))
    , duration_label(drawable, font,
                     tr((flags & DURATION_MANDATORY) ? trkeys::duration_r : trkeys::duration),
                     WidgetLabel::Colors::from_theme(theme))
    , duration_edit(drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme),
                    check_confirmation_event(*this))
    , duration_format(drawable, font, tr(trkeys::note_duration_format),
                      WidgetLabel::Colors::from_theme(theme))
    , ticket_label(drawable, font,
                   tr((flags & TICKET_MANDATORY) ? trkeys::ticket_r : trkeys::ticket),
                   WidgetLabel::Colors::from_theme(theme))
    , ticket_edit(drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme),
                  check_confirmation_event(*this))
    , comment_label(drawable, font,
                    tr((flags & COMMENT_MANDATORY) ? trkeys::comment_r : trkeys::comment),
                    WidgetLabel::Colors::from_theme(theme))
    , comment_edit(drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme),
                   check_confirmation_event(*this))
    , notes(drawable, font, tr(trkeys::note_required), WidgetLabel::Colors::from_theme(theme))
    , confirm(drawable, font, tr(trkeys::confirm), WidgetButton::Colors::from_theme(theme),
              [this]{ return this->check_confirmation(); })
{
    this->set_bg_color(theme.global.bgcolor);

    this->add_widget(this->warning_msg);

    if (this->flags & DURATION_DISPLAY) {
        this->add_widget(this->duration_label);
        this->add_widget(this->duration_edit);
        this->add_widget(this->duration_format);
    }
    if (this->flags & TICKET_DISPLAY) {
        this->add_widget(this->ticket_label);
        this->add_widget(this->ticket_edit);
    }
    if (this->flags & COMMENT_DISPLAY) {
        this->add_widget(this->comment_label);
        this->add_widget(this->comment_edit);
    }

    if (this->flags & (COMMENT_MANDATORY | TICKET_MANDATORY | DURATION_MANDATORY)) {
        this->add_widget(this->notes);
    }

    this->add_widget(this->confirm);
}

void WidgetForm::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->set_xy(left, top);
    this->set_wh(width, height);

    uint16_t labelmaxwidth = 0;

    if (this->flags & DURATION_DISPLAY) {
        labelmaxwidth = std::max(labelmaxwidth, this->duration_label.cx());
    }

    if (this->flags & TICKET_DISPLAY) {
        labelmaxwidth = std::max(labelmaxwidth, this->ticket_label.cx());
    }

    if (this->flags & COMMENT_DISPLAY) {
        labelmaxwidth = std::max(labelmaxwidth, this->comment_label.cx());
    }

    constexpr uint8_t x_padding = 20;
    constexpr uint8_t h_sep = 32;

    this->warning_msg.set_wh(width - labelmaxwidth - x_padding, this->warning_msg.cy());
    this->warning_msg.set_xy(left + labelmaxwidth + x_padding, top);

    int y = 20;
    int d = (duration_edit.cy() - this->warning_msg.cy()) / 2;

    if (this->flags & DURATION_DISPLAY) {
        this->duration_label.set_xy(left, top + y + d);

        duration_edit.set_xy(
            checked_int(left + labelmaxwidth + x_padding),
            checked_int(top + y)
        );
        duration_edit.update_width(checked_int(
            (width - labelmaxwidth - x_padding) - this->duration_format.cx() - x_padding
        ));

        this->duration_format.set_xy(this->duration_edit.eright() + 10, top + y + d);

        y += h_sep;
    }

    if (this->flags & TICKET_DISPLAY) {
        this->ticket_label.set_xy(left, top + y + d);

        ticket_edit.set_xy(
            checked_int(left + labelmaxwidth + x_padding),
            checked_int(top + y)
        );
        ticket_edit.update_width(checked_int(width - labelmaxwidth - x_padding));

        y += h_sep;
    }

    if (this->flags & COMMENT_DISPLAY) {
        this->comment_label.set_xy(left, top + y + d);

        comment_edit.set_xy(
            checked_int(left + labelmaxwidth + x_padding),
            checked_int(top + y)
        );
        comment_edit.update_width(checked_int(width - labelmaxwidth - x_padding));

        y += h_sep;
    }

    if (this->flags & (COMMENT_MANDATORY | TICKET_MANDATORY | DURATION_MANDATORY)) {
        this->notes.set_wh(width - labelmaxwidth - x_padding, this->notes.cy());
        this->notes.set_xy(left + labelmaxwidth + x_padding, top + y);
    }

    this->confirm.set_xy(left + width - this->confirm.cx(), top + y + 10);
}

namespace
{
    char const* consume_spaces(char const* s) noexcept
    {
        while (*s ==  ' ') {
            ++s;
        }
        return s;
    }

    // parse " *\d+h *\d+m| *\d+[hm]" or returns 0
    std::chrono::minutes check_duration(const char * duration)
    {
        auto chars_res = decimal_chars_to_int<unsigned>(consume_spaces(duration));
        if (chars_res.ec == std::errc()) {
            unsigned long minutes = 0;

            if (*chars_res.ptr == 'h') {
                minutes = chars_res.val * 60;
                duration = consume_spaces(chars_res.ptr + 1);

                if (*duration) {
                    chars_res = decimal_chars_to_int<unsigned>(duration);
                    if (chars_res.ec != std::errc()) {
                        return std::chrono::minutes(0);
                    }
                    duration = chars_res.ptr + 1;
                }
            }

            if (*chars_res.ptr == 'm') {
                minutes += chars_res.val;
                duration = chars_res.ptr + 1;
            }

            if (*duration == '\0') {
                return std::chrono::minutes(minutes);
            }
        }

        return std::chrono::minutes(0);
    }
} // anonymous namespace

void WidgetForm::check_confirmation()
{
    auto set_warning_buffer = [this](auto k, TrKey field, auto... args) {
        this->warning_msg.set_text(font, Translator::FmtMsg<256>(tr, k, tr(field).c_str(), args...));
    };

    auto has_flags = [this](unsigned m){
        return (this->flags & m) == m;
    };

    if (has_flags(DURATION_DISPLAY | DURATION_MANDATORY)
     && !this->duration_edit.has_text()
    ) {
        set_warning_buffer(trkeys::fmt_field_required, trkeys::duration);
        this->set_widget_focus(this->duration_edit, focus_reason_mousebutton1);
        this->rdp_input_invalidate(this->get_rect());
        return;
    }

    if (has_flags(DURATION_DISPLAY)
     && this->duration_edit.has_text()
    ) {
        std::chrono::minutes res = check_duration(this->duration_edit.get_text().c_str());
        // res is duration in minutes.
        if (res <= 0min || res > this->duration_max) {
            if (res <= 0min) {
                set_warning_buffer(trkeys::fmt_invalid_format, trkeys::duration);
            }
            else {
                set_warning_buffer(trkeys::fmt_toohigh_duration, trkeys::duration,
                    int(this->duration_max.count()));
            }
            this->set_widget_focus(this->duration_edit, focus_reason_mousebutton1);
            this->rdp_input_invalidate(this->get_rect());
            return;
        }
    }

    if (has_flags(TICKET_DISPLAY | TICKET_MANDATORY)
     && !this->ticket_edit.has_text()
    ) {
        set_warning_buffer(trkeys::fmt_field_required, trkeys::ticket);
        this->set_widget_focus(this->ticket_edit, focus_reason_mousebutton1);
        this->rdp_input_invalidate(this->get_rect());
        return;
    }

    if (has_flags(COMMENT_DISPLAY | COMMENT_MANDATORY)
     && !this->comment_edit.has_text()
    ) {
        set_warning_buffer(trkeys::fmt_field_required, trkeys::comment);
        this->set_widget_focus(this->comment_edit, focus_reason_mousebutton1);
        this->rdp_input_invalidate(this->get_rect());
        return;
    }

    this->events.submit();
}

void WidgetForm::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (pressed_scancode(flags, scancode) == Scancode::Esc) {
        this->events.cancel();
    }
    else {
        WidgetComposite::rdp_input_scancode(flags, scancode, event_time, keymap);
    }
}

WidgetForm::EditTexts WidgetForm::get_edit_texts() const noexcept
{
    return {
        .comment = comment_edit.get_text(),
        .ticket = ticket_edit.get_text(),
        .duration = duration_edit.get_text(),
    };
}
