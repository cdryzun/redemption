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
 *   Copyright (C) Wallix 2010-2021
 *   Author(s): Proxies Team
 */

#pragma once

#include "mod/internal/widget/multiline.hpp"


class Font;

class WidgetVerticalScrollText : public Widget
{
public:
    WidgetVerticalScrollText(
        gdi::GraphicApi& drawable, chars_view text,
        Color fg_color, Color bg_color, Color focus_color,
        Font const& font);

    void set_xy(int16_t x, int16_t y) override;

    void set_wh(uint16_t w, uint16_t h) override;

    using Widget::set_wh;

    struct DimensionContraints
    {
        struct Range
        {
            uint16_t min;
            uint16_t max;
        };

        // intermediate for designated initializers of range or initialization with integer:
        // update_dimension({5, ...});
        // update_dimension({{.min=5, .max=5, ...}});
        struct Length : Range
        {
            constexpr Length(uint16_t n) noexcept : Range{n, n} {}
            constexpr Length(Range rng) noexcept : Range{rng} {}
        };

        struct PreferOptimalDimsBase
        {
            bool horizontal;
            bool vertical;
        };

        // intermediate for designated initializers of pair or initialization with bool:
        // update_dimension({..., false});
        // update_dimension({..., {{.horizontal = false, .vertical = true}};});
        struct PreferOptimalDims : PreferOptimalDimsBase
        {
            constexpr PreferOptimalDims(bool prefer_optimal_dim) noexcept
                : PreferOptimalDimsBase{prefer_optimal_dim, prefer_optimal_dim}
            {}

            constexpr PreferOptimalDims(PreferOptimalDimsBase prefer_optimal_dim) noexcept
                : PreferOptimalDimsBase{prefer_optimal_dim}
            {}
        };

        Length width;
        Length height;
        PreferOptimalDims prefer_optimal_dimension = true;
    };

    void update_dimension(DimensionContraints contraints);

    Dimension get_optimal_dim() const override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_invalidate(Rect clip) override;

    void scroll_up();
    void scroll_down();

    chars_view get_text() const
    {
        return text;
    }

private:
    struct D;
    friend D;

    void _update_cursor_button_y_and_redraw(int new_y);
    bool _scroll_up();
    bool _scroll_down();

    Color const fg_color;
    Color const bg_color;
    Color const focus_color;

    Font const & font;

    int16_t cursor_button_y = 0;

    int current_y = 0;
    int page_h = 0;
    int scroll_h = 0;
    int total_h = 0;

    int mouse_start_y = 0;
    int mouse_y = 0;

    Dimension const button_dim;
    uint16_t cursor_button_h;

    bool has_scroll = false;

    enum class ButtonType : unsigned char
    {
        None,

        Top,
        Cursor,
        Bottom,
    };

    ButtonType selected_button = ButtonType::None;

    std::string text;
    MultiLineText multiline_text;
};
