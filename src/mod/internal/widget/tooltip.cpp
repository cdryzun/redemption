/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/tooltip.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "gdi/draw_utils.hpp"


struct WidgetTooltip::D
{
    static const int border_width = 1;
    static const int h_border = border_width + 9;
    static const int w_border = border_width + 9;
};

WidgetTooltip::WidgetTooltip(
    gdi::GraphicApi & drawable, Font const & font,
    unsigned max_width, chars_view text, Colors colors
)
    : Widget(drawable, Focusable::No)
    , colors(colors)
    , desc(font, max_width, text)
{
}

Dimension WidgetTooltip::get_optimal_dim() const
{
    Dimension dim = desc.dimension();

    dim.w += 2 * D::w_border;
    dim.h += 2 * D::h_border;

    return dim;
}

void WidgetTooltip::set_text(Font const & font, unsigned max_width, chars_view text)
{
    desc.set_text(font, max_width, text);
    Dimension dim = desc.dimension();

    set_wh(dim.w + 2 * D::w_border,
           dim.h + 2 * D::h_border);
}

void WidgetTooltip::rdp_input_invalidate(Rect clip)
{
    auto rect = get_rect();
    Rect rect_intersect = clip.intersect(rect);

    if (!rect_intersect.isempty()) {
        drawable.draw(
            RDPOpaqueRect(rect, colors.bg),
            rect_intersect, gdi::ColorCtx::depth24()
        );

        desc.draw(drawable, {
            .x = checked_int{x() + D::w_border},
            .y = checked_int{y() + D::h_border},
            .clip = rect_intersect,
            .fgcolor = colors.fg,
            .bgcolor = colors.bg,
            .draw_bg_rect = false,
        });

        gdi_draw_border(
            drawable, colors.border, rect, D::border_width,
            rect_intersect, gdi::ColorCtx::depth24()
        );
    }
}
