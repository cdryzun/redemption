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
 *   Copyright (C) Wallix 2010-2012
 *   Author(s): Christophe Grosjean, Raphael Zhou, Jonathan Poelen,
 *              Meng Tan
 */

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "mod/internal/widget/event_notifier.hpp"


class Font;
class FontCharView;

class WidgetScrollBar : public Widget
{
public:
    enum class ScrollDirection : bool
    {
        Horizontal,
        Vertical,
    };

    struct Colors
    {
        Color fg;
        Color bg;
        Color bg_focus;
    };

    WidgetScrollBar(gdi::GraphicApi & drawable, Font const & font,
                    ScrollDirection scroll_direction, Colors colors,
                    WidgetEventNotifier onscroll);

    [[nodiscard]] unsigned int get_current_value() const;

    void set_current_value(unsigned int cv);

    void set_max_value(unsigned int maxvalue);

    // Widget

    void rdp_input_invalidate(Rect clip) override;

    void set_size(uint16_t width_or_height);

    using Widget::set_wh;

    // RdpInput
    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

private:
    struct D;
    friend D;

    void compute_step_value();

    WidgetEventNotifier onscroll;
    const bool horizontal;

    Colors colors;

    FontCharView const & top_left_fc;
    FontCharView const & bottom_right_fc;
    uint16_t h_text;

    int current_value = 0;
    int max_value     = 100;

    int step_value = 1;

    bool mouse_down = false;

    enum class SelectedButton
    {
        None,

        LeftOrTop,
        Cursor,
        RightOrBottom,
    };

    SelectedButton selected_button = SelectedButton::None;

    int old_mouse_x_or_y         = 0;
    int old_cursor_button_x_or_y = 0;

    const uint8_t icon_width;
    const uint8_t icon_height;
};
