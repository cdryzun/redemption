/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "gdi/text_metrics.hpp"

class Theme;

// TODO merge with gdi::MultiLineTextMetrics
class MultiLineText
{
public:
    MultiLineText() = default;

    void set_text(Font const & font, chars_view text);

    void update_dimension(unsigned preferred_max_width) noexcept;

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

    struct WithLineSep { bool with_line_sep; };

    uint16_t line_height(WithLineSep with_sep) noexcept
    {
        auto h = m_cy_line;
        if (with_sep.with_line_sep) {
            h += line_sep();
        }
        return h;
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

    struct TextData
    {
        Font const & font;
        chars_view text;
        // 0 means do not compute size
        unsigned max_width = 0;
    };

    WidgetMultiLine(gdi::GraphicApi & drawable, TextData text_data, Colors colors);

    WidgetMultiLine(gdi::GraphicApi & drawable, Colors colors);

    void set_text(Font const & font, unsigned max_width, chars_view text);

    void update_dimension(unsigned max_width);

    void rdp_input_invalidate(Rect clip) override;

private:
    Colors colors;
    MultiLineText multi_line;
};
