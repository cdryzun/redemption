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
 *              Meng Tan, Jennifer Inthavong
 */

#include "core/font.hpp"
#include "core/app_path.hpp"
#include "mod/internal/widget/dialog.hpp"
#include "mod/internal/widget/password.hpp"
#include "mod/internal/widget/edit.hpp"
#include "mod/internal/copy_paste.hpp"
#include "utils/theme.hpp"
#include "keyboard/keymap.hpp"
#include "gdi/graphic_api.hpp"


WidgetDialogBase::WidgetDialogBase(
    gdi::GraphicApi & drawable, Rect widget_rect, Events events,
    chars_view caption, chars_view text, Widget * extra_button,
    Theme const & theme, Font const & font, chars_view ok_text,
    std::unique_ptr<WidgetButton> cancel_,
    WidgetEdit* challenge_,
    WidgetLink* link
)
    : WidgetComposite(drawable, Focusable::Yes, theme.global.bgcolor)
    , onctrl_shift(events.onctrl_shift)
    , title(drawable, font, caption, WidgetLabel::Colors::from_theme(theme))
    , separator(drawable, theme.global.separator_color)
    , dialog(drawable, text,
             theme.global.fgcolor, theme.global.bgcolor, theme.global.focus_color, font)
    , link(link)
    , challenge(challenge_)
    , ok(drawable, font, ok_text, WidgetButton::Colors::from_theme(theme), events.onsubmit)
    , cancel(std::move(cancel_))
    , img(drawable,
          theme.enable_theme ? theme.logo_path.c_str() :
          app_path(AppPath::LoginWabBlue),
          theme.global.bgcolor)
    , extra_button(extra_button)
    , oncancel(events.oncancel)
{
    this->add_widget(this->img);
    this->add_widget(this->title);
    this->add_widget(this->separator);
    this->add_widget(this->dialog);

    auto focused = HasFocus::Yes;

    if (link) {
        this->add_widget(link->show);
        this->add_widget(link->label);
        this->add_widget(link->copy, focused);
        focused = HasFocus::No;
    }

    if (this->challenge) {
        this->add_widget(*this->challenge, focused);
        this->add_widget(this->ok, HasFocus::No);
    }
    else {
        this->add_widget(this->ok, focused);
    }


    if (this->cancel) {
        this->add_widget(*this->cancel);
    }

    if (this->challenge && extra_button) {
        this->add_widget(*extra_button);
    }

    this->move_size_widget(widget_rect.x, widget_rect.y, widget_rect.cx, widget_rect.cy);
}

WidgetDialogBase::~WidgetDialogBase() = default;

void WidgetDialogBase::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->set_xy(left, top);
    this->set_wh(width, height);

    constexpr int MULTILINE_X_PADDING = 10;

    int16_t y            = top;
    int16_t total_height = 0;

    this->title.set_xy(left + (width - this->title.cx()) / 2, y);
    y            += this->title.cy();
    total_height += this->title.cy();

    int total_width = (width > 620) ? 600 : width - MULTILINE_X_PADDING * 2;
    auto scroll_text_contraints = WidgetVerticalScrollText::DimensionContraints{
        .width = {{
            .min = 0,
            .max = checked_int{
                this->challenge
                    ? total_width
                    : width * 4 / 5 - MULTILINE_X_PADDING * 2
            }
        }},
        .height = {{
            .min = 0,
            .max = checked_int{height / 2}
        }},
        .prefer_optimal_dimension = {{
            .horizontal = {{
                .with_scroll = !this->challenge,
                .without_scroll = true,
            }},
            .vertical = true,
        }},
    };

    this->dialog.update_dimension(scroll_text_contraints);

    if (!this->challenge) {
        total_width = std::max(this->dialog.cx() + MULTILINE_X_PADDING * 2, this->title.cx() + 0);
    }

    if (this->link) {
        this->link->show.update_dimension(scroll_text_contraints);
        total_width = std::max<int>(this->link->show.cx(), total_width);
    }

    this->separator.set_width(checked_int{total_width});
    this->separator.set_xy(left + (width - total_width) / 2, y + 3);

    y            += 10;
    total_height += 10;

    this->dialog.set_xy(this->separator.x() + (this->challenge ? 0 : MULTILINE_X_PADDING), y);

    y            += this->dialog.cy() + 10;
    total_height += this->dialog.cy() + 10;

    if (this->challenge) {
        this->challenge->update_width(checked_int{total_width - MULTILINE_X_PADDING * 2});
        this->challenge->set_xy(this->separator.x() + MULTILINE_X_PADDING, y);

        y            += this->challenge->cy() + 10;
        total_height += this->challenge->cy() + 10;
    }

    y += 5;

    if (this->link) {
        // to activate
        this->link->show.set_xy(this->dialog.x(), y);

        y            += this->link->show.cy() + 10;
        total_height += this->link->show.cy() + 10;

        y += 5;

        const auto label_dim = this->link->label.dimension();
        const auto button_dim_h = this->link->copy.cy();
        const auto msg_dim = this->link->copied_msg.dimension();
        const int dy = std::max(label_dim.h, button_dim_h);
        const int label_dy = (dy - label_dim.h) / 2;
        const int button_dy = (dy - button_dim_h) / 2;

        this->link->label.set_xy(this->dialog.x(), y + label_dy);

        this->link->copy.set_xy(this->link->label.x() + label_dim.w + 2, y + button_dy);

        y            += dy + 10;
        total_height += dy + 10;

        this->link->copied_msg.set_xy(left + (width - msg_dim.w) / 2, y + button_dy);

        y            += msg_dim.h + 10;
        total_height += msg_dim.h + 10;

        y += 5;
    }

    if (this->cancel) {
        this->cancel->set_xy(this->separator.x() + total_width - (this->cancel->cx() + 10), y);

        this->ok.set_xy(this->cancel->x() - (this->ok.cx() + 10), y);
    }
    else {
        this->ok.set_xy(this->separator.x() + total_width - (this->ok.cx() + 10), y);
    }

    total_height += this->ok.cy();

    this->move_children_xy(0, (height - total_height) / 2);
    if (this->link) {
        this->link->copied_msg.move_xy(0, (height - total_height) / 2);
    }

    this->img.set_xy(left + (width - this->img.cx()) / 2,
                        top + (3 * (height - total_height) / 2 - this->img.cy()) / 2 + total_height);
    if (this->img.y() + this->img.cy() > top + height) {
        this->img.set_xy(this->img.x(), top);
    }

    if (this->challenge && this->extra_button) {
        extra_button->set_xy(left + 60, top + height - 60);
    }
}

void WidgetDialogBase::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
        case Keymap::KEvent::Esc:
            this->oncancel();
            break;
        case Keymap::KEvent::LeftArrow:
        case Keymap::KEvent::UpArrow:
        case Keymap::KEvent::PgUp:
            this->dialog.scroll_up();
            break;
        case Keymap::KEvent::RightArrow:
        case Keymap::KEvent::DownArrow:
        case Keymap::KEvent::PgDown:
            this->dialog.scroll_down();
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

void WidgetDialogBase::show_copied_msg()
{
    if (this->link && !this->link->msg_showed) {
        this->link->msg_showed = true;
        this->add_widget(this->link->copied_msg);
        this->link->copied_msg.rdp_input_invalidate(this->link->copied_msg.get_rect());
    }
}

void WidgetDialogBase::focus_to_ok()
{
    this->set_widget_focus(this->ok, Redraw::Yes);
}


WidgetDialog::WidgetDialog(
    gdi::GraphicApi& drawable, Rect widget_rect,
    Events events,
    chars_view caption, chars_view text,
    const Theme& theme, const Font& font,
    chars_view ok_text, chars_view cancel_text)
: WidgetDialogBase(
    drawable, widget_rect,
    {.onsubmit = events.onsubmit, .oncancel = events.oncancel},
    caption, text, nullptr, theme, font, ok_text,
    !cancel_text.empty()
        ? std::make_unique<WidgetButton>(
            drawable, font, cancel_text, WidgetButton::Colors::from_theme(theme),
            events.oncancel
        )
        : std::unique_ptr<WidgetButton>(),
    nullptr, nullptr
)
{}


WidgetDialogWithChallenge::WidgetDialogWithChallenge(
    gdi::GraphicApi& drawable, Rect widget_rect,
    Events events,
    chars_view caption, chars_view text,
    Widget * extra_button,
    chars_view ok_text,
    Font const & font, Theme const & theme, CopyPaste & copy_paste,
    ChallengeOpt challenge_opt)
: WidgetDialogBase::WidgetChallenge{
    .challenge_edit = WidgetPassword{
        drawable, font, copy_paste,
        WidgetEdit::Colors::from_theme(theme), events.onsubmit
    },
}
, WidgetDialogBase(
    drawable, widget_rect, events,
    caption, text, extra_button, theme, font,
    ok_text,
    nullptr,
    [&]{
        if (ChallengeOpt::Echo == challenge_opt) {
            challenge_edit.toggle_password_visibility(WidgetEdit::Redraw::No);
        }
        return &challenge_edit;
    }(),
    nullptr
)
{}


WidgetDialogWithCopyableLink::WidgetDialogWithCopyableLink(
    gdi::GraphicApi & drawable, Rect widget_rect, Events events,
    chars_view caption, chars_view text,
    chars_view link_value, chars_view link_label,
    chars_view copied_msg_label, chars_view ok_text,
    Font const & font, Theme const & theme, CopyPaste & copy_paste
)
: WidgetDialogBase::WidgetLink{
    .show = WidgetVerticalScrollText(drawable, link_value,
                theme.global.fgcolor, theme.global.bgcolor, theme.global.focus_color,
                font),
    .copied_msg = WidgetLabel(drawable, font, copied_msg_label,
                              WidgetLabel::Colors::from_theme(theme)),
    .label = WidgetLabel(drawable, font, link_label, WidgetLabel::Colors::from_theme(theme)),
    .link_value = link_value.as<std::vector>(),
    .copy = WidgetDelegatedCopy(
        drawable, [this]{
            this->copy_paste.copy(this->link_value);
            this->show_copied_msg();
            this->focus_to_ok();
        },
        WidgetDelegatedCopy::Colors::from_theme(theme), font)
}
, WidgetDialogBase(
    drawable, widget_rect,
    {.onsubmit = events.onsubmit, .oncancel = events.oncancel},
    caption, text, nullptr, theme, font,
    ok_text, nullptr, nullptr, this
)
, copy_paste(copy_paste)
{
}
