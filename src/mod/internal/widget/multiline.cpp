/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/multiline.hpp"
#include "gdi/graphic_api.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "utils/theme.hpp"


WidgetMultiLine::Colors WidgetMultiLine::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.global.fgcolor,
        .bg = theme.global.bgcolor,
    };
}


WidgetMultiLine::WidgetMultiLine(gdi::GraphicApi & drawable, Colors colors)
    : Widget(drawable, Focusable::No)
    , colors(colors)
{}

WidgetMultiLine::WidgetMultiLine(
    gdi::GraphicApi & drawable, TextData text_data, Colors colors
)
    : WidgetMultiLine(drawable, colors)
{
    if (text_data.max_width) {
        set_text(text_data.font, text_data.max_width, text_data.text);
    }
    else {
        multi_line.set_text(text_data.font, text_data.text);
    }
}

void WidgetMultiLine::set_text(Font const & font, unsigned max_width, chars_view text)
{
    multi_line.set_text(font, text);
    update_dimension(max_width);
}

void WidgetMultiLine::update_dimension(unsigned max_width)
{
    multi_line.update_dimension(max_width);
    set_wh(multi_line.dimension());
}

void WidgetMultiLine::rdp_input_invalidate(Rect clip)
{
    multi_line.draw(drawable, {
        .x = x(),
        .y = y(),
        .clip = clip.intersect(get_rect()),
        .fgcolor = colors.fg,
        .bgcolor = colors.bg,
        .draw_bg_rect = true,
    });
}
