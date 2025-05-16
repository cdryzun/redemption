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
#include "mod/internal/button_state.hpp"
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
        Color fg;
        Color bg;
        Color placeholder;
        Color edit_fg;
        Color edit_bg;
        Color border;
        Color focus_border;
        Color cursor;
        Color password_toggle;

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
        uint16_t width;
        uint16_t edit_offset;
        bool label_as_placeholder;
    };

    WidgetEditValid(
        gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
        Options opts, Colors colors, WidgetEventNotifier onsubmit
    );

    ~WidgetEditValid();

    uint16_t label_width(bool is_placeholder) const noexcept;
    // 0 when Type != Text
    uint16_t text_width() const noexcept;

    void set_text(bytes_view text, TextOptions opts);

    [[nodiscard]] WidgetEdit::Text get_text() const;

    void update_layout(Layout data);

    void set_xy(int16_t x, int16_t y) override;

    void rdp_input_invalidate(Rect clip) override;

    void focus() override;

    void blur() override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void init_focus() override;

    Widget * widget_at_pos(int16_t x, int16_t y) override;

private:
    struct D;

    void draw_placeholder(Rect clip);
    void draw_button_zone(Rect clip);

    bool is_password_widget() const noexcept;
    bool is_text_widget() const noexcept;

    struct Label
    {
        Color bg_color;
        Color fg_color;
        Color placeholder_color;
        bool has_placeholder;
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
    ButtonState valid_pressed;
    ButtonState toggle_password_pressed;
    Color password_toggle_color;
    Label label;
    EditOrText edit_or_text;
};
