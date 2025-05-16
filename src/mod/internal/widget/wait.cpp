/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "mod/internal/widget/wait.hpp"
#include "keyboard/keymap.hpp"
#include "translation/trkeys.hpp"
#include "utils/sugar/chars_to_int.hpp"
#include "utils/theme.hpp"


struct WidgetWait::D
{
    // Group box
    static constexpr uint16_t border           = 6;
    static constexpr uint16_t text_margin      = 6;
    static constexpr uint16_t text_indentation = border + text_margin + 4;

    static WidgetEventNotifier check_form_confirmation_event(WidgetWait & w)
    {
        return [&w]{ w.check_form_confirmation(); };
    }
};

WidgetWait::WidgetWait(
    gdi::GraphicApi & drawable, CopyPaste & copy_paste, Rect const widget_rect,
    Events events, chars_view caption, chars_view text,
    Widget * extra_button,
    Font const & font, Theme const & theme, Translator tr,
    unsigned flags, std::chrono::minutes duration_max
)
    : WidgetComposite(drawable, Focusable::Yes, theme.global.bgcolor)
    , onaccept(events.onaccept)
    , onconfirm(events.onconfirm)
    , onrefused(events.onrefused)
    , onctrl_shift(events.onctrl_shift)
    , extra_button(extra_button)
    , border_color(theme.global.fgcolor)
    , font(font)
    , tr(tr)
    , widget_flags(flags)
    , duration_max(duration_max == duration_max.zero()
        ? std::chrono::minutes(60000)
        : duration_max)
    , caption(drawable, font, caption, WidgetLabel::Colors::from_theme(theme))
    , dialog(drawable, {font, text}, {.fg = theme.global.fgcolor, .bg = theme.global.bgcolor})
    , form{
        .warning_msg{
            drawable, font,
            ""_av,
            WidgetLabel::Colors::from_theme(theme)
        },
        .duration_label{
            drawable, font,
            (flags & DURATION_DISPLAY)
                ? tr((flags & DURATION_MANDATORY) ? trkeys::duration_r : trkeys::duration)
                : ""_zv,
            WidgetLabel::Colors::from_theme(theme)
        },
        .duration_edit{
            drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme),
            D::check_form_confirmation_event(*this)
        },
        .duration_format{
            drawable, font,
            (flags & DURATION_DISPLAY)
                ? tr(trkeys::note_duration_format)
                : ""_zv,
            WidgetLabel::Colors::from_theme(theme)
        },
        .ticket_label{
            drawable, font,
            (flags & TICKET_DISPLAY)
                ? tr((flags & TICKET_MANDATORY) ? trkeys::ticket_r : trkeys::ticket)
                : ""_zv,
            WidgetLabel::Colors::from_theme(theme)
        },
        .ticket_edit{
            drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme),
            D::check_form_confirmation_event(*this)
        },
        .comment_label{
            drawable, font,
            (flags & COMMENT_DISPLAY)
                ? tr((flags & COMMENT_MANDATORY) ? trkeys::comment_r : trkeys::comment)
                : ""_zv,
            WidgetLabel::Colors::from_theme(theme)
        },
        .comment_edit{
            drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme),
            D::check_form_confirmation_event(*this)
        },
        .notes{
            drawable, font,
            (flags & NOTE_DISPLAY)
                ? tr(trkeys::note_required)
                : ""_zv,
            WidgetLabel::Colors::from_theme(theme)
        },
        .confirm{
            drawable, font,
            (flags & SHOW_FORM)
                ? tr(trkeys::confirm)
                : ""_zv,
            WidgetButton::Colors::from_theme(theme),
            D::check_form_confirmation_event(*this)
        },
    }
    , goselector(drawable, font, tr(trkeys::back_selector),
                 WidgetButton::Colors::from_theme(theme), events.onaccept)
    , exit(drawable, font, tr(trkeys::exit), WidgetButton::Colors::from_theme(theme),
           events.onrefused)
{
    this->add_widget(this->caption);
    this->add_widget(this->dialog);

    if (flags & SHOW_FORM) {
        this->add_widget(this->form.warning_msg);

        if (flags & DURATION_DISPLAY) {
            this->add_widget(this->form.duration_label);
            this->add_widget(this->form.duration_edit);
            this->add_widget(this->form.duration_format);
        }
        if (flags & TICKET_DISPLAY) {
            this->add_widget(this->form.ticket_label);
            this->add_widget(this->form.ticket_edit);
        }
        if (flags & COMMENT_DISPLAY) {
            this->add_widget(this->form.comment_label);
            this->add_widget(this->form.comment_edit);
        }

        if (flags & (COMMENT_MANDATORY | TICKET_MANDATORY | DURATION_MANDATORY)) {
            this->add_widget(this->form.notes);
        }

        this->add_widget(this->form.confirm);
    }

    if (!(flags & HIDE_BACK_TO_SELECTOR)) {
        this->add_widget(this->goselector, (flags & SHOW_FORM) ? HasFocus::No : HasFocus::Yes);
    }
    this->add_widget(this->exit);

    if (extra_button) {
        this->add_widget(*extra_button);
    }

    this->move_size_widget(widget_rect.x, widget_rect.y, widget_rect.cx, widget_rect.cy);
}

void WidgetWait::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->set_xy(left, top);
    this->set_wh(width, height);

    constexpr uint8_t x_padding = 40;

    int y = 20;

    this->caption.set_xy(left + D::text_indentation, top);

    this->dialog.update_dimension(checked_int{width - x_padding * 2});
    this->dialog.set_xy(checked_int{left + x_padding}, checked_int{top + y + 10});

    y = this->dialog.y() + this->dialog.cy() + 20;

    if (widget_flags & SHOW_FORM) {
        int16_t label_x_offset = checked_int{left + x_padding};
        int16_t edit_x_offset = checked_int{
            std::max({
                this->form.duration_label.cx(),
                this->form.ticket_label.cx(),
                this->form.comment_label.cx()
            })
            + label_x_offset + 20
        };
        uint16_t edit_witdh = checked_int{width - edit_x_offset - label_x_offset};

        int h_sep = form.duration_edit.cy() + 8;

        form.warning_msg.set_xy(label_x_offset, top);

        int d = (form.duration_edit.cy() - form.warning_msg.cy()) / 2;

        if (widget_flags & DURATION_DISPLAY) {
            form.duration_label.set_xy(label_x_offset, checked_int{top + y + d});

            form.duration_edit.set_xy(edit_x_offset, checked_int(top + y));
            form.duration_edit.update_width(checked_int(
                edit_witdh - form.duration_format.cx() - 10
            ));

            form.duration_format.set_xy(
                checked_int{form.duration_edit.eright() + 10},
                checked_int{top + y + d}
            );

            y += h_sep;
        }

        if (widget_flags & TICKET_DISPLAY) {
            form.ticket_label.set_xy(label_x_offset, checked_int{top + y + d});

            form.ticket_edit.set_xy(edit_x_offset, checked_int(top + y));
            form.ticket_edit.update_width(edit_witdh);

            y += h_sep;
        }

        if (widget_flags & COMMENT_DISPLAY) {
            form.comment_label.set_xy(label_x_offset, checked_int{top + y + d});

            form.comment_edit.set_xy(edit_x_offset, checked_int(top + y));
            form.comment_edit.update_width(edit_witdh);

            y += h_sep;
        }

        if (widget_flags & NOTE_DISPLAY) {
            form.notes.set_xy(edit_x_offset, checked_int{top + y});
        }

        form.confirm.set_xy(
            checked_int{left + width - form.confirm.cx() - x_padding},
            checked_int{top + y + 10}
        );

        y = form.confirm.ebottom() + 20;
    }

    this->exit.set_xy(
        checked_int{left + width - x_padding - this->exit.cx()},
        checked_int{y}
    );

    if (!(widget_flags & HIDE_BACK_TO_SELECTOR)) {
        this->goselector.set_xy(
            checked_int{this->exit.x() - (this->goselector.cx() + 10)},
            checked_int{y}
        );
    }

    if (this->extra_button) {
        this->extra_button->set_xy(checked_int{left + 60}, checked_int{top + height - 60});
    }

    /*
     * center vertically
     */

    y += this->exit.cy() + 20;
    this->group_height = checked_int{y};

    this->move_xy(0, checked_int{(height - (y - top)) / 2});
    this->set_xy(left, top);
}

void WidgetWait::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect());

    if (!rect_intersect.isempty()) {
        this->draw_inner_free(rect_intersect, this->get_bg_color());

        auto draw_rect = [&](int x, int y, int w, int h){
            auto rect = Rect(
                checked_int{x},
                checked_int{y},
                checked_int{w},
                checked_int{h}
            );
            this->drawable.draw(
                RDPOpaqueRect(rect, this->border_color),
                rect_intersect, gdi::ColorCtx::depth24()
            );
        };

        auto wlabel = D::text_margin * 2 + caption.cx();
        auto y = this->caption.y() + this->caption.cy() / 2;
        auto gcy = this->group_height - this->caption.cy() / 2 - D::border;
        auto gcx = this->cx() - D::border * 2 + 1;
        auto px = this->x() + D::border - 1;

        // Top line (left to text)
        draw_rect(px, y, D::text_indentation - D::text_margin - D::border + 2, 1);
        // Top line (right to text)
        auto right_cx = gcx + 1 - wlabel - 4;
        if (right_cx > 0) {
            draw_rect(px + wlabel + 3, y, right_cx, 1);
        }

        // Bottom line
        draw_rect(px, y + gcy, gcx + 1, 1);

        // Left border
        draw_rect(px, y + 1, 1, gcy - 1);

        // Right Border
        draw_rect(px + gcx, y, 1, gcy);

        this->invalidate_children(rect_intersect);
    }
}

void WidgetWait::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
        case Keymap::KEvent::Esc:
            if (widget_flags & HIDE_BACK_TO_SELECTOR) {
                this->onrefused();
            }
            else {
                this->onaccept();
            }
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

void WidgetWait::check_form_confirmation()
{
    auto set_warning_buffer = [this](
        WidgetEdit& focused_edit,
        auto k, TrKey field, auto... args
    ) {
        this->form.warning_msg.set_text_and_redraw(
            font,
            Translator::FmtMsg<256>(tr, k, tr(field).c_str(), args...),
            get_rect()
        );
        this->set_widget_focus(focused_edit, Redraw::Yes);
    };

    auto has_flags = [this](unsigned m){
        return (widget_flags & m) == m;
    };

    if (has_flags(DURATION_DISPLAY | DURATION_MANDATORY)
     && !form.duration_edit.has_text()
    ) {
        set_warning_buffer(form.duration_edit, trkeys::fmt_field_required, trkeys::duration);
        return;
    }

    if (has_flags(DURATION_DISPLAY)
     && form.duration_edit.has_text()
    ) {
        std::chrono::minutes res = check_duration(form.duration_edit.get_text().c_str());
        // res is duration in minutes.
        if (res <= res.zero() || res > this->duration_max) {
            if (res <= res.zero()) {
                set_warning_buffer(
                    form.duration_edit, trkeys::fmt_invalid_format, trkeys::duration
                );
            }
            else {
                set_warning_buffer(
                    form.duration_edit, trkeys::fmt_toohigh_duration, trkeys::duration,
                    int(this->duration_max.count())
                );
            }
            return;
        }
    }

    if (has_flags(TICKET_DISPLAY | TICKET_MANDATORY)
     && !form.ticket_edit.has_text()
    ) {
        set_warning_buffer(form.ticket_edit, trkeys::fmt_field_required, trkeys::ticket);
        return;
    }

    if (has_flags(COMMENT_DISPLAY | COMMENT_MANDATORY)
     && !form.comment_edit.has_text()
    ) {
        set_warning_buffer(form.comment_edit, trkeys::fmt_field_required, trkeys::comment);
        return;
    }

    onconfirm();
}

WidgetWait::EditTexts WidgetWait::get_edit_texts() const noexcept
{
    return {
        .comment = form.comment_edit.get_text(),
        .ticket = form.ticket_edit.get_text(),
        .duration = form.duration_edit.get_text(),
    };
}
