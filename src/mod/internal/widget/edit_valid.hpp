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
#include "mod/internal/widget/button.hpp"
#include "mod/internal/widget/edit.hpp"
#include "mod/internal/widget/label.hpp"
#include "utils/colors.hpp"

class WidgetEdit;
class WidgetLabel;
class CopyPaste;
class WidgetPassword;

class WidgetEditValid : public Widget
{
public:
    using Colors = WidgetEdit::Colors;
    using CusorPosition = WidgetEdit::CusorPosition;

    struct Options
    {
        bool is_password;
        chars_view label;
    };

    struct Data
    {
        int16_t x;
        int16_t y;
        uint16_t edit_offset;
        chars_view edit_text;
        bool label_as_placeholder;
        uint16_t max_width;
        CusorPosition cursor_position = CusorPosition::CursorToEnd;
    };

    WidgetEditValid(
        gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
        Options opts, Colors colors, WidgetEventNotifier onsubmit
    );

    Dimension get_optimal_dim() const override;

    ~WidgetEditValid() override;

    uint16_t label_width() const noexcept;

    void update_layout(Data data);

    [[nodiscard]] WidgetEdit::Text get_text() const;

    void set_xy(int16_t x, int16_t y) override;

    void set_wh(uint16_t w, uint16_t h) override;

    using Widget::set_wh;

    void rdp_input_invalidate(Rect clip) override;

    void focus(int reason) override;

    void blur() override;

    Widget * widget_at_pos(int16_t x, int16_t y) override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    bool is_password_widget() noexcept;

    void init_focus() override;

private:
    WidgetButton button_next;
    WidgetText label;
    WidgetPassword * widget_password;
    WidgetEdit * editbox;
    WidgetButton * button_toggle_visibility;

    bool label_as_placeholder;

    Color border_none_color;
};
