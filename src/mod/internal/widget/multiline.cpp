/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/multiline.hpp"
#include "gdi/graphic_api.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "utils/theme.hpp"


void MultiLineText::set_text(Font const & font, chars_view text)
{
    m_lines.set_text(font, text);
    m_cy_line = font.max_height();
}

void MultiLineText::update_dimension(unsigned preferred_max_width) noexcept
{
    m_lines.compute_lines(preferred_max_width);
}

void MultiLineText::reset() noexcept
{
    m_lines.clear_text();
    m_cy_line = 0;
}

Dimension MultiLineText::dimension() const noexcept
{
    auto n = m_lines.lines().size();
    auto sep = n ? n * line_sep() : 0;
    return Dimension{
        checked_int{m_lines.max_width()},
        checked_int{m_cy_line * n + sep},
    };
}

void MultiLineText::draw(gdi::GraphicApi& drawable, Data data)
{
    auto dim = dimension();
    auto rect = Rect(data.x, data.y, dim.w, dim.h);
    Rect rect_intersect = data.clip.intersect(rect);

    if (!rect_intersect.isempty()) {
        if (data.draw_bg_rect) {
            drawable.draw(
                RDPOpaqueRect(rect_intersect, data.bgcolor),
                rect, gdi::ColorCtx::depth24()
            );
        }

        auto h_line = checked_cast<uint16_t>(m_cy_line + line_sep());

        auto start = checked_cast<std::size_t>((rect_intersect.y - rect.y) / h_line);
        auto stop = checked_cast<std::size_t>(
            (rect_intersect.ebottom() - rect.y + h_line - 1) / h_line
        );

        rect.cy = h_line;
        rect.y += h_line * start;

        for (auto const& line : m_lines.lines().first(stop).drop_front(start)) {
            gdi::draw_text(
                drawable,
                rect.x,
                rect.y,
                m_cy_line,
                gdi::DrawTextPadding{},
                line,
                data.fgcolor,
                data.bgcolor,
                rect_intersect.intersect(rect)
            );

            rect.y += h_line;
        }
    }
}


WidgetMultiLine::Colors WidgetMultiLine::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.global.fgcolor,
        .bg = theme.global.bgcolor,
    };
}


WidgetMultiLine::WidgetMultiLine(gdi::GraphicApi & drawable, Colors colors)
    : Widget(drawable, Focusable::Yes)
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
