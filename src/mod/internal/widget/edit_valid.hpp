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

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "mod/internal/widget/password.hpp"
#include "mod/internal/widget/label.hpp"
#include "utils/colors.hpp"

class WidgetEdit;
class WidgetLabel;
class CopyPaste;
class WidgetPassword;

class WidgetEditValid : public Widget
{
public:
    using TextOptions = WidgetEdit::TextOptions;

    struct Colors
    {
        // TODO remove default constructor of Color
        Color fg;
        Color bg;
        Color placeholder = NamedBGRColor::MEDIUM_GREY; // TODO remove default
        Color edit_fg;
        Color edit_bg;
        Color border = edit_bg; // TODO remove default
        Color focus_border;
        Color cursor = Widget::Color(0x888888); // TODO remove default
        Color password_toggle = NamedBGRColor::MEDIUM_GREY; // TODO remove default

        static Colors from_theme(Theme const& theme) noexcept;
    };

    enum class Type
    {
        Text,
        Edit,
        Password,
    };

    struct Options
    {
        Type type;
        chars_view label;
        chars_view edit = ""_av;
    };

    struct Layout
    {
        int16_t x;
        int16_t y;
        uint16_t width;
        uint16_t edit_offset;
        bool label_as_placeholder;
    };

    WidgetEditValid(
        gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
        Options opts, Colors colors, WidgetEventNotifier onsubmit
    );

    ~WidgetEditValid();

    Dimension get_optimal_dim() const override;

    uint16_t label_width(bool is_placeholder) const noexcept;

    void set_text(bytes_view text, TextOptions opts);

    void update_layout(Layout data);

    [[nodiscard]] WidgetEdit::Text get_text() const;

    void set_xy(int16_t x, int16_t y) override;

    void set_wh(uint16_t w, uint16_t h) override;

    using Widget::set_wh;

    void rdp_input_invalidate(Rect clip) override;

    void focus(int reason) override;

    void blur() override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void init_focus() override;

private:
    void draw_placeholder(Rect clip);

    bool is_password_widget() const noexcept;
    bool is_text_widget() const noexcept;

    struct Label
    {
        Color bg_color;
        Color fg_color;
        Color placeholder_color;
        bool is_placeholder;
        WidgetText<128> text;
    };

    struct Buttons
    {
        FontCharView const * valid_text;  // nullptr with type == Type::Text
        FontCharView const * visibility_hidden;  // nullptr with type != Type::Password
        FontCharView const * visibility_visible;  // nullptr with type != Type::Password
    };

    struct NoEditableText : WidgetText<WidgetPassword::max_capacity>
    {
        NoEditableText(Font const& font, chars_view text);

        uint16_t height() const noexcept { return _height; }

        void set_text(bytes_view text);

        uint16_t offset_x = 0;
        uint16_t _height;
        Font const& _font;
        WidgetPassword::Text text_buffer;
    };

    struct Edit : WidgetPassword
    {
        Edit(
            gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
            chars_view text, Type type, Colors colors, WidgetEventNotifier onsubmit
        );
    };

    union EditOrText
    {
        NoEditableText text;
        Edit edit;

        ~EditOrText() {}
    };

    Buttons buttons;
    bool valid_pressed = false;
    bool toggle_password_pressed = false;
    Color password_toggle_color;
    Label label;
    EditOrText edit_or_text;
};
