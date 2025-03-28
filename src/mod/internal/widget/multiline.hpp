/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "gdi/text.hpp"

class Theme;

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
    gdi::MultiLineText multi_line;
};
