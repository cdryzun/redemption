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
#include "utils/theme.hpp"


namespace
{
    constexpr int x_text = 5;
    constexpr int y_text = 3;
    constexpr int border_width = 2;
}

WidgetDelegatedCopy::Colors WidgetDelegatedCopy::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.global.fgcolor,
        .bg = theme.global.bgcolor,
        .active_bg = theme.global.focus_color,
    };
}

WidgetDelegatedCopy::WidgetDelegatedCopy(
    gdi::GraphicApi & drawable, WidgetEventNotifier onsubmit,
    Colors colors, Font const & font
)
    : WidgetButtonEvent(drawable, onsubmit, WidgetButtonEvent::RedrawOnSubmit::Yes)
    , colors(colors)
{
    auto const& glyph = font.item('E').view;
    set_wh(
        checked_int{glyph.width + 4 + (border_width + x_text) * 2},
        checked_int{glyph.height + 3 + (border_width + y_text) * 2}
    );
}

void WidgetDelegatedCopy::rdp_input_invalidate(Rect clip)
{
    Rect rect = clip.intersect(this->get_rect());

    if (rect.isempty()) {
        return ;
    }

    const auto color_ctx = gdi::ColorCtx::depth24();

    auto fg = colors.fg;
    auto bg = has_focus ? colors.active_bg : colors.bg;

    drawable.draw(RDPOpaqueRect(rect, bg), clip, color_ctx);

    gdi_draw_border(drawable, fg, rect.x, rect.y, rect.cx, rect.cy, border_width, clip, color_ctx);

    rect.x += border_width + x_text;
    rect.y += border_width + y_text;
    rect.cx -= (border_width + x_text) * 2;
    rect.cy -= (border_width + y_text) * 2;

    if (is_pressed()) {
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
