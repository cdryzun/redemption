/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1 of the License, or
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
 *   Copyright (C) Wallix 1010-2013
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#include "mod/internal/widget/password.hpp"
#include "mod/internal/widget/edit_valid.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/edit.hpp"
#include "mod/internal/widget/button.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "gdi/draw_utils.hpp"

namespace
{
    constexpr auto button_toggle_visibility_hidden = "◉"_av;  // u+000025c9
    constexpr auto button_toggle_visibility_visible = "◎"_av;  // u+000025ce
    constexpr auto button_valid = "➜"_av;  // u+0000279c

    constexpr uint16_t border_len = 1;


    void draw_rect(gdi::GraphicApi& gd, Rect clip, RDPColor color, int x, int y, int w, int h)
    {
        gd.draw(
            RDPOpaqueRect(
                Rect(
                    checked_int(x), checked_int(y),
                    checked_int(w), checked_int(h)
                ),
                color
            ),
            clip, gdi::ColorCtx::depth24()
        );
    };
}

WidgetEditValid::WidgetEditValid(
    gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
    Options opts, Colors colors, WidgetEventNotifier onsubmit
)
    : Widget(gd, Focusable::Yes)
    , button_next(gd, button_valid, onsubmit,
                  colors.bg, colors.focus_border, colors.focus_border, 1, font, 4, 1)
    , label(font, {.fg = NamedBGRColor::MEDIUM_GREY, .bg = colors.bg}, opts.label)
    , widget_password(opts.is_password
        ? new WidgetPassword(gd, font, copy_paste, colors, onsubmit)
        : nullptr)
    , editbox(opts.is_password
        ? widget_password
        : new WidgetEdit(gd, font, copy_paste, colors, onsubmit))
    , button_toggle_visibility(opts.is_password
        ? new WidgetButton(gd, button_toggle_visibility_hidden,
            [this] {
                // Switch the visibility state
                if(widget_password->password_is_visible()) {
                    button_toggle_visibility->set_text(button_toggle_visibility_hidden);
                } else {
                    button_toggle_visibility->set_text(button_toggle_visibility_visible);
                }
                auto rect = widget_password->get_rect();
                rect.cx--;
                widget_password->toggle_password_visibility(rect);
            },
            NamedBGRColor::MEDIUM_GREY, colors.bg, colors.focus_border, 0, font, 4, 2)
        : nullptr)
    , label_as_placeholder(false)
{}

void WidgetEditValid::init_focus()
{
    this->has_focus = true;
    this->editbox->init_focus();
}

Dimension WidgetEditValid::get_optimal_dim() const
{
    Dimension dim = this->button_next.get_optimal_dim();

    dim.h += 2 /* border */;

    return dim;
}

WidgetEditValid::~WidgetEditValid()
{
    delete this->editbox;
    delete this->button_toggle_visibility;
}

uint16_t WidgetEditValid::label_width() const noexcept
{
    return label.get_optimal_dim().w;
}

void WidgetEditValid::update_layout(Data data)
{
    label_as_placeholder = data.label_as_placeholder;

    button_next.set_wh(button_next.get_optimal_dim());
    if (button_toggle_visibility) {
        button_toggle_visibility->set_wh(button_toggle_visibility->get_optimal_dim());
    }

    int w = button_next.cx();
    if (!label_as_placeholder) {
        w += data.edit_offset;
    }
    if (button_toggle_visibility) {
        w += button_toggle_visibility->cx();
    }

    editbox->set_text(data.edit_text, WidgetEdit::TextOptions{
        .redraw = WidgetEdit::Redraw::No,
        .set_size = WidgetEdit::SetSize::optimal(
            checked_int(data.max_width ? data.max_width - w : 0)
        ),
        .cursor_position = data.cursor_position,
    });

    Widget::set_wh(checked_int(w + editbox->cx()), editbox->cy());

    Widget::set_xy(data.x, data.y);

    int16_t x_edit = data.x;
    int16_t x_label = data.x;
    if (label_as_placeholder) {
        x_label += 2;
    }
    else {
        x_edit += data.edit_offset;
    }

    editbox->set_xy(x_edit, data.y);

    int16_t x_button = editbox->eright() - border_len;
    if (button_toggle_visibility) {
        button_toggle_visibility->set_wh(button_toggle_visibility->cx(), cy() - border_len * 2);
        button_toggle_visibility->set_xy(x_button, data.y + border_len);
        x_button = button_toggle_visibility->eright();
    }
    button_next.set_xy(x_button, data.y + border_len);
    button_next.set_wh(button_next.cx(), editbox->cy() - border_len * 2);

    label.set_xy(x_label, data.y + (editbox->cy() - label.cy()) / 2);
}

WidgetEdit::Text WidgetEditValid::get_text() const
{
    return this->editbox->get_text();
}

void WidgetEditValid::set_xy(int16_t x, int16_t y)
{
    Widget::set_xy(x, y);

    int dx = this->x() - x;
    int dy = this->y() - y;

    auto set_xy = [=](auto& w){
        w.set_xy(checked_int(w.x() + dx), checked_int(w.y() + dy));
    };

    set_xy(*editbox);
    set_xy(button_next);
    set_xy(label);
    if (is_password_widget()) {
        set_xy(*button_toggle_visibility);
    }
}

void WidgetEditValid::set_wh(uint16_t w, uint16_t h)
{
    Widget::set_wh(w, h);

    Dimension dim = this->button_next.get_optimal_dim();
    this->button_next.set_wh(dim.w, h - 2 /* 2 x border */);

    if (is_password_widget()) {
        this->button_toggle_visibility->set_wh(dim.w, h - 2 /* 2 x border */);
        this->editbox->set_wh(w - this->button_toggle_visibility->cx() - button_next.cx() - 2,
                                h - 2 /* 2 x border */);
        this->button_toggle_visibility->set_xy(this->editbox->eright(), this->button_toggle_visibility->y());
        this->button_next.set_xy(this->button_toggle_visibility->eright(), this->button_next.y());
    }
    else {
        this->editbox->set_wh(w - this->button_next.cx() - 2, h - 2 /* 2 x border */);
        this->button_next.set_xy(this->editbox->eright(), this->button_next.y());
    }
}

void WidgetEditValid::rdp_input_invalidate(Rect clip)
{
    Rect rect = clip.intersect(this->get_rect());

    if (!rect.isempty()) {
        this->editbox->rdp_input_invalidate(rect);

        if (this->label_as_placeholder) {
            if (!this->editbox->has_text()) {
                this->label.draw(drawable, rect);
                this->editbox->draw_current_cursor(clip);
            }
        }
        else {
            this->label.draw(drawable, rect);
        }

        RDPColor border_color;
        if (this->has_focus) {
            this->button_next.rdp_input_invalidate(rect);
            if (is_password_widget()) {
                this->button_toggle_visibility->rdp_input_invalidate(rect);
            }
            border_color = this->button_next.focus_color;
        }
        else {
            if (is_password_widget()) {
                this->drawable.draw(
                    RDPOpaqueRect(rect.intersect(this->button_toggle_visibility->get_rect()), this->button_toggle_visibility->bg_color),
                    rect, gdi::ColorCtx::depth24()
                );
            }
            this->drawable.draw(
                RDPOpaqueRect(rect.intersect(this->button_next.get_rect()), this->button_next.fg_color),
                rect, gdi::ColorCtx::depth24()
            );
            border_color = this->editbox->get_colors().border;
        }

        auto draw_border = [&](int x, int y, int w, int h) {
            draw_rect(drawable, clip, border_color, x, y, w, h);
        };

        int16_t xb = button_next.x() + border_len;
        int16_t yb = y();
        uint16_t wb = button_next.cx() + border_len;
        uint16_t hb = cy();

        if (button_toggle_visibility) {
            xb = button_toggle_visibility->x() + border_len;
            wb += button_toggle_visibility->cx();
        }

        // top
        draw_border(xb, yb, wb - border_len, border_len);
        // bottom
        draw_border(xb, checked_int(yb + hb - border_len), wb - border_len, border_len);
        // right
        draw_border(checked_int(xb + wb - border_len * 2), yb + border_len, border_len, hb - border_len * 2);
    }
}

void WidgetEditValid::focus(int reason)
{
    this->editbox->focus(reason);
    Widget::focus(reason);
}

void WidgetEditValid::blur()
{
    this->editbox->blur();
    Widget::blur();
}

Widget * WidgetEditValid::widget_at_pos(int16_t x, int16_t y)
{
    if (editbox->get_rect().contains_pt(x, y)) {
        return editbox;
    }
    if (is_password_widget() && button_toggle_visibility->get_rect().contains_pt(x, y)) {
        return button_toggle_visibility;
    }
    if (button_next.get_rect().contains_pt(x, y)) {
        return &button_next;
    }

    return nullptr;
}

void WidgetEditValid::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (bool(device_flags & MOUSE_FLAG_BUTTON1)) {
        auto update = [&](WidgetButton& btn){
            if (btn.get_rect().contains_pt(x,y)
             || (device_flags == MOUSE_FLAG_BUTTON1 && btn.state == WidgetButton::State::Pressed)
            ) {
                btn.rdp_input_mouse(device_flags, x, y);
            }
        };

        update(button_next);
        if (is_password_widget()) {
            update(*button_toggle_visibility);
        }
    }

    if (device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN)
     && editbox->get_rect().contains_pt(x,y)
    ) {
        editbox->rdp_input_mouse(device_flags, x, y);
    }
}

void WidgetEditValid::rdp_input_scancode(
    KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    bool has_char1 = !editbox->has_text();
    editbox->rdp_input_scancode(flags, scancode, event_time, keymap);

    if (label_as_placeholder) {
        bool has_char2 = !editbox->has_text();
        if (has_char1 != has_char2) {
            auto rect = editbox->get_rect();
            if (has_char1) {
                rect.cx--;
                editbox->rdp_input_invalidate(rect);
            }
            else {
                label.draw(drawable, label.get_rect());
                editbox->draw_current_cursor(rect);
            }
        }
    }
}

void WidgetEditValid::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    this->editbox->rdp_input_unicode(flag, unicode);
}

bool WidgetEditValid::is_password_widget() noexcept
{
    return button_toggle_visibility && widget_password;
}
