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

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "mod/internal/widget/wait.hpp"
#include "keyboard/keymap.hpp"

#include "translation/translation.hpp"
#include "translation/trkeys.hpp"
#include "utils/theme.hpp"


struct WidgetWait::D
{
    static constexpr unsigned HIDE_BACK_TO_SELECTOR = 0x10000;

    // Group box
    static constexpr uint16_t border           = 6;
    static constexpr uint16_t text_margin      = 6;
    static constexpr uint16_t text_indentation = border + text_margin + 4;
};

WidgetWait::WidgetWait(
    gdi::GraphicApi & drawable, CopyPaste & copy_paste, Rect const widget_rect,
    Events events, chars_view caption, chars_view text,
    Widget * extra_button,
    Font const & font, Theme const & theme, Translator tr,
    bool showform, unsigned flags, std::chrono::minutes duration_max
)
    : WidgetComposite(drawable, Focusable::Yes)
    , onaccept(events.onaccept)
    , onrefused(events.onrefused)
    , onctrl_shift(events.onctrl_shift)
    , extra_button(extra_button)
    , font(font)
    , border_color(theme.global.fgcolor)
    , hasform(showform)
    , hide_back_to_selector(flags & D::HIDE_BACK_TO_SELECTOR)
    , message_dialog(av_auto_cast{text})
    , caption(drawable, font, caption, WidgetLabel::Colors::from_theme(theme))
    , dialog(drawable, {.fg = theme.global.fgcolor, .bg = theme.global.bgcolor})
    , form(drawable, copy_paste, {events.onconfirm, events.onrefused},
           font, theme, tr, flags & ~D::HIDE_BACK_TO_SELECTOR, duration_max)
    , goselector(drawable, font, tr(trkeys::back_selector),
                 WidgetButton::Colors::from_theme(theme), events.onaccept)
    , exit(drawable, font, tr(trkeys::exit), WidgetButton::Colors::from_theme(theme),
           events.onrefused)
{
    this->set_bg_color(theme.global.bgcolor);
    this->add_widget(this->caption);
    this->add_widget(this->dialog);

    if (showform) {
        this->add_widget(this->form, HasFocus::Yes);
    }

    if (!this->hide_back_to_selector) {
        this->add_widget(this->goselector, showform ? HasFocus::No : HasFocus::Yes);
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

    int y = 20;

    this->caption.set_xy(left + D::text_indentation, top);

    this->dialog.set_text(this->font, width - 80, this->message_dialog);
    this->dialog.set_xy(left + 40, top + y + 10);

    y = this->dialog.y() + this->dialog.cy() + 20;

    if (this->hasform) {
        this->form.move_size_widget(left + 40, y, width - 80, 156);

        y = this->form.ebottom() + 10;
    }

    this->exit.set_xy(left + width - 40 - this->exit.cx(), y);

    if (!this->hide_back_to_selector) {
        this->goselector.set_xy(this->exit.x() - (this->goselector.cx() + 10), y);
    }

    if (this->extra_button) {
        this->extra_button->set_xy(left + 60, top + height - 60);
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
            if (this->hide_back_to_selector) {
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
