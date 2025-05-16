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
#include "gdi/text.hpp"
#include "utils/theme.hpp"


struct WidgetEditValid::D
{
    static const uint32_t button_toggle_visibility_hidden = 0x000025c9; // ◉
    static const uint32_t button_toggle_visibility_visible = 0x000025ce; // ◎
    static const uint32_t button_valid = 0x0000279c; // ➜

    static const uint16_t border_len = 1;
    static const uint16_t w_button_valid_padding = 5;
    static const uint16_t w_button_toggle_padding = 3;
    static const uint16_t x_placeholder = 3;

    static int button_valid_width(FontCharView const& fc)
    {
        return fc.boxed_width() + w_button_valid_padding * 2;
    }

    static int button_toggle_width(FontCharView const& fc)
    {
        return fc.boxed_width() + w_button_toggle_padding * 2;
    }

    template<class T>
    static void destruct(T & value) noexcept
    {
        value.~T();
    }
};


WidgetEditValid::NoEditableText::NoEditableText(const Font& font, chars_view text)
    : WidgetText(font, text)
    , _height(font.max_height())
    , _font(font)
    , text_buffer(truncatable_bounded_array_view(text))
{}

void WidgetEditValid::NoEditableText::set_text(bytes_view text)
{
    WidgetText::set_text(_font, text.as_chars());
    text_buffer = truncatable_bounded_array_view(text);
}


WidgetEditValid::Edit::Edit(
    gdi::GraphicApi& gd, const Font& font, CopyPaste& copy_paste,
    chars_view text, Type type, Colors colors, WidgetEventNotifier onsubmit
)
    : WidgetPassword(gd, font, copy_paste, colors, onsubmit)
{
    if (type == Type::Edit) {
        toggle_password_visibility(WidgetEdit::Redraw::No);
    }

    if (!text.empty()) {
        set_text(text, {WidgetEdit::Redraw::No});
    }
}


WidgetEditValid::WidgetEditValid(
    gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
    Options opts, Colors colors, WidgetEventNotifier onsubmit
)
    : Widget(gd, opts.type == Type::Text ? Focusable::No : Focusable::Yes)
    , buttons{
        .valid_text = opts.type != Type::Text
            ? &font.item(D::button_valid).view
            : nullptr,
        .visibility_hidden = opts.type == Type::Password
            ? &font.item(D::button_toggle_visibility_hidden).view
            : nullptr,
        .visibility_visible = opts.type == Type::Password
            ? &font.item(D::button_toggle_visibility_visible).view
            : nullptr,
    }
    , password_toggle_color(colors.password_toggle)
    , label{
        .bg_color = colors.bg,
        .fg_color = colors.fg,
        .placeholder_color = colors.placeholder,
        .has_placeholder = false,
        .text = {font, opts.label},
    }
    , edit_or_text(opts.type == Type::Text
        ? EditOrText{.text = {font, opts.edit}}
        : EditOrText{.edit = {
            gd, font, copy_paste,
            opts.edit, opts.type,
            {
                .fg = colors.edit_fg,
                .bg = colors.edit_bg,
                .border = colors.border,
                .focus_border = colors.focus_border,
                .cursor = colors.cursor,
            },
            onsubmit,
        }}
    )
{
    if (opts.type == Type::Text) {
        set_wh(edit_or_text.text.width(), edit_or_text.text.height());
    }
    else {
        set_wh(edit_or_text.edit.cx(), edit_or_text.edit.cy());
    }
}

WidgetEditValid::~WidgetEditValid()
{
    if (is_text_widget()) {
        D::destruct(edit_or_text.text);
    }
    else {
        D::destruct(edit_or_text.edit);
    }
}

void WidgetEditValid::init_focus()
{
    if (!is_text_widget()) {
        has_focus = true;
        edit_or_text.edit.init_focus();
    }
}

uint16_t WidgetEditValid::label_width(bool has_placeholder) const noexcept
{
    return label.text.width() + D::border_len * 2 + has_placeholder * D::x_placeholder;
}

uint16_t WidgetEditValid::text_width() const noexcept
{
    return is_text_widget() ? edit_or_text.text.width() + D::border_len * 2 : 0;
}

void WidgetEditValid::set_text(bytes_view text, TextOptions opts)
{
    if (is_text_widget()) {
        edit_or_text.text.set_text(text);
    }
    else {
        edit_or_text.edit.set_text(text, opts);
    }
}

WidgetEdit::Text WidgetEditValid::get_text() const
{
    if (!is_text_widget()) {
        return edit_or_text.edit.get_text();
    }
    else {
        return edit_or_text.text.text_buffer;
    }

}

void WidgetEditValid::update_layout(Layout layout)
{
    label.has_placeholder = layout.label_as_placeholder;

    uint16_t widget_h;
    uint16_t widget_w;

    if (is_text_widget()) {
        edit_or_text.text.offset_x = label.has_placeholder ? 0 : layout.edit_offset;
        widget_h = edit_or_text.text.height();
        widget_w = edit_or_text.text.offset_x + edit_or_text.text.width() + D::border_len * 2;
    }
    else {
        int w = D::button_valid_width(*buttons.valid_text) + D::border_len * 2;
        if (!label.has_placeholder) {
            w += layout.edit_offset;
        }
        if (auto * fc = buttons.visibility_visible) {
            w += D::button_toggle_width(*fc);
        }

        auto edit_offset = label.has_placeholder ? uint16_t{} : layout.edit_offset;
        edit_or_text.edit.set_xy(checked_int{x() + edit_offset}, y());
        edit_or_text.edit.update_width(checked_int(layout.width - w));
        widget_h = edit_or_text.edit.cy();
        widget_w = layout.width;
    }

    Widget::set_wh(widget_w, widget_h);
}

void WidgetEditValid::set_xy(int16_t x, int16_t y)
{
    int dx = x - this->x();
    int dy = y - this->y();

    Widget::set_xy(x, y);

    if (!is_text_widget()) {
        edit_or_text.edit.set_xy(
            checked_int(edit_or_text.edit.x() + dx),
            checked_int(edit_or_text.edit.y() + dy)
        );
    }
}

void WidgetEditValid::rdp_input_invalidate(Rect clip)
{
    Rect rect = clip.intersect(this->get_rect());

    if (!rect.isempty()) {
        /*
         * No editable text
         */
        if (is_text_widget()) [[unlikely]] {
            uint16_t h_text = edit_or_text.text.height();
            int top_pad = (cy() - h_text) / 2;
            int bottom_pad = cy() - h_text - top_pad;
            auto draw_text = [&](int x, array_view<FontCharView const*> fcs, uint16_t left){
                gdi::draw_text(
                    drawable,
                    x,
                    y(),
                    h_text,
                    gdi::DrawTextPadding{
                        .top = checked_int(top_pad),
                        .right = cx(),
                        .bottom = checked_int(bottom_pad),
                        .left = left,
                    },
                    fcs,
                    label.fg_color,
                    label.bg_color,
                    rect
                );
            };

            draw_text(x() + edit_or_text.text.offset_x, edit_or_text.text.fcs(), 0);
            if (!label.has_placeholder) {
                rect = rect.intersect(
                    checked_int{x() + edit_or_text.text.offset_x},
                    checked_int{rect.ebottom()}
                );
                draw_text(x(), label.text.fcs(), 1);
            }

            return ;
        }

        /*
         * Edit text
         */

        edit_or_text.edit.rdp_input_invalidate(rect);

        // placeholder
        if (label.has_placeholder) {
            if (!this->edit_or_text.edit.has_text()) {
                draw_placeholder(rect);
            }
        }
        // label
        else {
            uint16_t h_text = edit_or_text.edit.get_font().max_height();
            uint16_t w_text = checked_int(edit_or_text.edit.x() - x());
            gdi::draw_text(
                drawable,
                x(),
                y(),
                h_text,
                gdi::DrawTextPadding{
                    .top = checked_int((cy() - h_text) / 2),
                    .right = cx(),
                    .bottom = cy(),
                    .left = 1,
                },
                label.text.fcs(),
                label.fg_color,
                label.bg_color,
                Rect(x(), y(), w_text, cy()).intersect(rect)
            );
        }

        draw_button_zone(rect);
    }
}

void WidgetEditValid::draw_button_zone(Rect clip)
{
    auto draw_rect = [this](Rect clip, RDPColor color, int x, int y, int w, int h) {
        drawable.draw(
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

    auto draw_border3 = [&](RDPColor color, int shring) {
        int16_t xb = checked_int(edit_or_text.edit.eright() - shring);
        int16_t yb = checked_int(y() + shring);
        uint16_t wb = checked_int(eright() - xb - shring);
        uint16_t hb = checked_int(cy() - shring * 2);

        // top
        draw_rect(clip, color, xb, yb, wb, D::border_len);
        // bottom
        draw_rect(clip, color, xb, checked_int(yb + hb - D::border_len), wb, D::border_len);
        // right
        draw_rect(clip, color, checked_int(xb + wb - D::border_len), yb + D::border_len, D::border_len, hb - D::border_len * 2);
    };

    RDPColor border_color;

    if (this->has_focus) {
        int const y_button = y() + D::border_len * 2;
        int const cy_button = cy() - D::border_len * 4;

        auto draw_button = [this, y_button, cy_button, clip](
            int x_button, bool is_pressed, FontCharView const* fc,
            RDPColor fg, RDPColor bg, int pad, int adjust_border
        ) {
            uint16_t h_text = edit_or_text.edit.get_font().max_height();
            int top_pad = (cy_button - h_text) / 2 + is_pressed;
            int bottom_pad = cy_button - h_text - top_pad;
            return gdi::draw_text(
                drawable,
                checked_int(x_button),
                y_button,
                h_text,
                gdi::DrawTextPadding{
                    .top = checked_int(top_pad),
                    .right = checked_int(pad - is_pressed),
                    .bottom = checked_int(bottom_pad),
                    .left = checked_int(pad + is_pressed - adjust_border),
                },
                array_view{&fc, 1},
                fg, bg, clip
            );
        };

        int x_button = edit_or_text.edit.eright();
        int adjust_border = D::border_len;
        if (auto * fc = buttons.visibility_visible) {
            draw_button(
                x_button - D::border_len,
                toggle_password_pressed.is_pressed(),
                edit_or_text.edit.password_is_visible()
                    ? buttons.visibility_visible
                    : buttons.visibility_hidden,
                password_toggle_color,
                edit_or_text.edit.get_colors().bg,
                D::w_button_toggle_padding, adjust_border
            );
            x_button += D::button_toggle_width(*fc) - D::border_len;
            adjust_border = 0;
        }
        draw_button(
            x_button,
            valid_pressed.is_pressed(),
            buttons.valid_text,
            edit_or_text.edit.get_colors().bg,
            edit_or_text.edit.get_colors().focus_border,
            D::w_button_valid_padding, adjust_border
        );

        draw_border3(edit_or_text.edit.get_colors().bg, D::border_len);

        border_color = edit_or_text.edit.get_colors().focus_border;
    }
    else {
        draw_rect(
            clip,
            edit_or_text.edit.get_colors().bg,
            checked_int(edit_or_text.edit.eright() - D::border_len),
            checked_int(y() + D::border_len),
            checked_int(eright() - edit_or_text.edit.eright()),
            checked_int(cy() - D::border_len * 2)
        );

        border_color = edit_or_text.edit.get_colors().border;
    }

    draw_border3(border_color, 0);
}

void WidgetEditValid::focus()
{
    if (!is_text_widget() && !has_focus){
        has_focus = true;
        edit_or_text.edit.focus();
        if (label.has_placeholder && !this->edit_or_text.edit.has_text()) {
            edit_or_text.edit.draw_current_cursor(get_rect());
        }
        draw_button_zone(get_rect());
    }
}

void WidgetEditValid::blur()
{
    if (!is_text_widget() && has_focus) {
        has_focus = false;
        toggle_password_pressed.pressed(false);
        valid_pressed.pressed(false);
        edit_or_text.edit.blur();
        if (label.has_placeholder && !this->edit_or_text.edit.has_text()) {
            edit_or_text.edit.hidden_current_cursor(get_rect());
        }
        draw_button_zone(get_rect());
    }
}

// TODO for edit pointer cursor only
Widget * WidgetEditValid::widget_at_pos(int16_t x, int16_t y)
{
    if (!is_text_widget() && edit_or_text.edit.get_rect().contains_pt(x, y)) {
        return &edit_or_text.edit;
    }
    return nullptr;
}

void WidgetEditValid::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (!is_text_widget() && bool(device_flags & MOUSE_FLAG_BUTTON1))
    {
        int sx = edit_or_text.edit.eright();
        int btn1 = sx;
        int btn2 = sx;

        bool force_focus = should_be_focused(device_flags);

        // toggle visibility button
        if (auto * fc = buttons.visibility_visible)
        {
            int w = D::button_toggle_width(*fc) - D::border_len;

            auto rect = Rect(
                checked_int(btn1),
                this->y(),
                checked_int(w),
                cy()
            );

            btn2 += D::button_toggle_width(*fc);

            auto redraw_behavior = force_focus
                ? ButtonState::Redraw::No
                : ButtonState::Redraw::OnSubmit;
            toggle_password_pressed.update(
                rect, x, y, device_flags, redraw_behavior,
                [this]{ edit_or_text.edit.toggle_password_visibility(WidgetEdit::Redraw::Yes); },
                [this](Rect rect){ rdp_input_invalidate(rect); }
            );
        }

        auto rect = Rect(
            checked_int(btn2),
            this->y(),
            checked_int(D::button_toggle_width(*buttons.valid_text) - D::border_len),
            cy()
        );

        auto redraw_behavior = force_focus
            ? ButtonState::Redraw::No
            : ButtonState::Redraw::OnSubmit;
        valid_pressed.update(
            rect, x, y, device_flags, redraw_behavior,
            [this]{ edit_or_text.edit.submit(); },
            [this](Rect rect){ rdp_input_invalidate(rect); }
        );

        if (force_focus) {
            focus();
        }

        if (sx > x && device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN)) {
            edit_or_text.edit.rdp_input_mouse(device_flags, x, y);
        }
    }
}

void WidgetEditValid::rdp_input_scancode(
    KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (!is_text_widget()) {
        bool has_char1 = !edit_or_text.edit.has_text();
        edit_or_text.edit.rdp_input_scancode(flags, scancode, event_time, keymap);

        if (label.has_placeholder) {
            bool has_char2 = !edit_or_text.edit.has_text();
            if (has_char1 != has_char2) {
                auto rect = edit_or_text.edit.get_rect();
                if (has_char1) {
                    rect.cx--;
                    edit_or_text.edit.rdp_input_invalidate(rect);
                }
                else {
                    draw_placeholder(rect);
                }
            }
        }
    }
}

void WidgetEditValid::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (!is_text_widget()) {
        edit_or_text.edit.rdp_input_unicode(flag, unicode);
    }
}

void WidgetEditValid::draw_placeholder(Rect clip)
{
    assert(!is_text_widget());
    auto& font = edit_or_text.edit.get_font();
    auto h_text = font.max_height();
    auto edit_rect = edit_or_text.edit.get_rect();
    edit_rect.x += D::border_len + 1/*cursor width*/;
    edit_rect.y += D::border_len;
    edit_rect.cx -= D::border_len * 2 - 1;
    edit_rect.cy -= D::border_len * 2;
    gdi::draw_text(
        drawable,
        x() + D::x_placeholder,
        y() + (edit_or_text.edit.cy() - h_text) / 2,
        h_text,
        gdi::DrawTextPadding::Padding(0),
        label.text.fcs(),
        label.placeholder_color,
        edit_or_text.edit.get_colors().bg,
        edit_rect.intersect(clip)
    );

    if (this->has_focus) {
        edit_or_text.edit.draw_current_cursor(clip);
    }
}

WidgetEditValid::Colors WidgetEditValid::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.global.fgcolor,
        .bg = theme.global.bgcolor,
        .placeholder = theme.edit.placeholder_color,
        .edit_fg = theme.edit.fgcolor,
        .edit_bg = theme.edit.bgcolor,
        .border = theme.edit.border_color,
        .focus_border = theme.edit.focus_border_color,
        .cursor = theme.edit.cursor_color,
        .password_toggle = theme.edit.password_toggle_color,
    };
}

bool WidgetEditValid::is_password_widget() const noexcept
{
    return buttons.visibility_visible;
}

bool WidgetEditValid::is_text_widget() const noexcept
{
    return !buttons.valid_text;
}


