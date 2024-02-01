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
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "utils/sugar/array_view.hpp"


class Font;
class FontCharView;
namespace gdi
{
    class ColorCtx;
}

class WidgetText
{
public:
    struct Colors
    {
        Widget::Color fg;
        Widget::Color bg;
    };

    WidgetText(Font const & font, Colors colors, chars_view text);

    void set_xy(int16_t x, int16_t y) noexcept
    {
        rect.x = x;
        rect.y = y;
    }

    Dimension get_optimal_dim() const noexcept { return {rect.cx, rect.cy}; }
    Rect get_rect() const noexcept { return rect; }

    int16_t x() const noexcept { return rect.x; }
    int16_t y() const noexcept { return rect.y; }
    uint16_t cx() const noexcept { return rect.cx; }
    uint16_t cy() const noexcept { return rect.cy; }

    void draw(gdi::GraphicApi & drawable, Rect clip);

private:
    using FontCharPtr = FontCharView const *;
    static const size_t buffer_size = 255;

    Rect rect;
    Colors colors;
    size_t fc_buffer_len = 0;
    FontCharPtr fc_buffer[buffer_size];
};

class WidgetLabel : public Widget
{
public:
    WidgetLabel(gdi::GraphicApi & drawable, chars_view text,
                Color fgcolor, Color bgcolor, Font const & font,
                int xtext = 0, int ytext = 0); /*NOLINT*/

    void set_text(chars_view text);

    [[nodiscard]] chars_view get_text() const;

    void rdp_input_invalidate(Rect clip) override;

    static void draw(Rect const clip, Rect const rect, gdi::GraphicApi& drawable,
                     chars_view text, Color fgcolor, Color bgcolor, gdi::ColorCtx color_ctx,
                     Font const & font, int xtext, int ytext);

    Dimension get_optimal_dim() const override;

    static Dimension get_optimal_dim(Font const & font, chars_view text, int xtext, int ytext);

    void set_color(Color bg_color, Color fg_color) override;

public:
    static const size_t buffer_size = 256;

    char buffer[buffer_size];

    int x_text;
    int y_text;
    Color bg_color;
    Color fg_color;

private:
    Font const * font;
};
