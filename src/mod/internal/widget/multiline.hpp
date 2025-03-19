/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "gdi/text_metrics.hpp"

class Theme;

class MultiLineText
{
public:
    MultiLineText() = default;

    MultiLineText(Font const & font, unsigned max_width, chars_view text);

    void set_text(Font const & font, unsigned max_width, chars_view text);
    // TODO update_dimension(unsigned max_width)

    void reset() noexcept;

    Dimension dimension() const noexcept;

    struct Data
    {
        int16_t x;
        int16_t y;
        Rect clip;
        RDPColor fgcolor;
        RDPColor bgcolor;
        bool draw_bg_rect;
    };

    void draw(gdi::GraphicApi & drawable, Data data);

    array_view<gdi::MultiLineTextMetrics::Line> lines() const noexcept
    {
        return m_lines.lines();
    }

    uint16_t max_width() const noexcept
    {
        return m_lines.max_width();
    }

    static uint16_t line_sep() noexcept
    {
        return 1;
    }

private:
    uint16_t m_cy_line = 0;
    gdi::MultiLineTextMetrics m_lines;
};


class WidgetMultiLine : public Widget
{
public:
    struct Colors
    {
        Color fg;
        Color bg;

        static Colors from_theme(Theme const& theme) noexcept;
    };

    WidgetMultiLine(gdi::GraphicApi & drawable, Font const & font,
                    unsigned max_width, chars_view text, Colors colors);

    WidgetMultiLine(gdi::GraphicApi & drawable, Colors colors);

    void set_text(Font const & font, unsigned max_width, chars_view text);
    // TODO update_dimension(unsigned max_width)

    void rdp_input_invalidate(Rect clip) override;

private:
    Colors colors;
    MultiLineText multi_line;
};
