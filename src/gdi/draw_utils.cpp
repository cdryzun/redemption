/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gdi/draw_utils.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"


void gdi_draw_border(
    gdi::GraphicApi& drawable, RDPColor color,
    int16_t x, int16_t y, uint16_t cx, uint16_t cy,
    uint16_t border_width, Rect clip, gdi::ColorCtx color_ctx)
{
    auto draw = [&](Rect rect){
        drawable.draw(RDPOpaqueRect(rect, color), clip, color_ctx);
    };

    auto const bw = border_width;

    // top
    draw(Rect(x, y, cx, bw));
    // left
    draw(Rect(x, checked_int(y + bw), bw, cy - bw * 2));
    // right
    draw(Rect(checked_int(x + cx - bw), checked_int(y + bw), bw, cy - bw * 2));
    // bottom
    draw(Rect(x, checked_int(y + cy - bw), cx, bw));
}
