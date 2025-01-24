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
#include "utils/out_param.hpp"


class Font;
class FontCharView;
namespace gdi
{
    class ColorCtx;
}

[[nodiscard]]
array_view<FontCharView const *> init_widget_text(
    writable_array_view<FontCharView const *> fcs,
    OutParam<uint16_t> width,
    Font const & font,
    chars_view text
);

template<std::size_t BufSize>
class WidgetText
{
public:
    using FontCharPtr = FontCharView const *;

    WidgetText(Font const & font, chars_view text)
    {
        set_text(font, text);
    }

    void set_text(Font const & font, chars_view text)
    {
        _fc_buffer_len = checked_int(
            init_widget_text(make_writable_array_view(_fc_buffer), OutParam(_width), font, text)
            .size()
        );
    }

    uint16_t width() const noexcept
    {
        return _width;
    }

    array_view<FontCharPtr> fcs() const noexcept
    {
        return {_fc_buffer, _fc_buffer_len};
    }

private:
    uint16_t _width;
    unsigned _fc_buffer_len = 0;
    FontCharPtr _fc_buffer[BufSize];
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
