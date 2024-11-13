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

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "mod/internal/widget/delegated_copy.hpp"
#include "gdi/draw_utils.hpp"


namespace
{
    constexpr int XTEXT = 5;
    constexpr int YTEXT = 3;
    constexpr int BORDER_WIDTH = 2;
}


WidgetDelegatedCopy::WidgetDelegatedCopy(
    gdi::GraphicApi & drawable, WidgetEventNotifier onsubmit,
    Color fgcolor, Color bgcolor, Color activecolor, Font const & font
)
    : WidgetButton(
        drawable, ""_av, onsubmit, fgcolor, bgcolor,
        activecolor, BORDER_WIDTH, font, XTEXT, YTEXT)
    , optimal_glyph_dim(get_optimal_dim(font))
{
}

void WidgetDelegatedCopy::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect());

    if (!rect_intersect.isempty()) {
        this->draw(
            clip, this->get_rect(), this->drawable, this->has_focus,
            this->fg_color, this->bg_color, this->focus_color,
            this->state
        );
    }
}

void WidgetDelegatedCopy::draw(
    Rect clip, Rect rect, gdi::GraphicApi & drawable, bool has_focus,
    Color fg, Color bg, Color focus_color, State state)
{
    const auto color_ctx = gdi::ColorCtx::depth24();

    drawable.draw(RDPOpaqueRect(rect, has_focus ? focus_color : bg), clip, color_ctx);

    gdi_draw_border(drawable, fg, rect.x, rect.y, rect.cx, rect.cy, BORDER_WIDTH, clip, color_ctx);

    rect.x += BORDER_WIDTH + XTEXT;
    rect.y += BORDER_WIDTH + YTEXT;
    rect.cx -= (BORDER_WIDTH + XTEXT) * 2;
    rect.cy -= (BORDER_WIDTH + YTEXT) * 2;

    if (state == State::Pressed) {
        rect.x++;
        rect.y++;
    }

    gdi_draw_border(drawable, fg, rect.x, rect.y, rect.cx, rect.cy, 1, clip, color_ctx);

    auto drawRect = [&](int16_t x, int16_t y, uint16_t w, uint16_t h){
        drawable.draw(RDPOpaqueRect(Rect(x, y, w, h), fg), clip, color_ctx);
    };

    // clip
    const int16_t d = ((rect.cx - 2) / 4) + /* border=*/1;
    drawRect(rect.x + d, rect.y, rect.cx - d * 2, 3);
    drawRect(rect.x + 2, rect.y + (rect.cy - 6) / 3 + 3, rect.cx - 4, 1);
    drawRect(rect.x + 2, rect.y + (rect.cy - 6) / 3 * 2 + 4, rect.cx - 4, 1);
}

Dimension WidgetDelegatedCopy::get_optimal_dim() const
{
    return this->optimal_glyph_dim;
}

Dimension WidgetDelegatedCopy::get_optimal_dim(Font const & font)
{
    auto const& glyph = font.item('E').view;
    return Dimension{
        checked_int{glyph.width + 4 + (BORDER_WIDTH + XTEXT) * 2},
        checked_int{glyph.height + 3 + (BORDER_WIDTH + YTEXT) * 2},
    };
}
