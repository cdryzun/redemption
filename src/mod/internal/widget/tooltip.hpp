/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "mod/internal/widget/multiline.hpp"

class WidgetTooltip : public Widget
{
public:
    struct Colors
    {
        Color fg;
        Color bg;
        Color border;

        static Colors from_theme(Theme const& theme) noexcept;
    };

    WidgetTooltip(gdi::GraphicApi & drawable, Font const & font,
                  unsigned max_width, chars_view text, Colors colors);

    Dimension get_optimal_dim() const override;

    void set_text(Font const & font, unsigned max_width, chars_view text);

    void rdp_input_invalidate(Rect clip) override;

private:
    struct D;
    friend D;

    Colors colors;
    MultiLineText desc;
};
