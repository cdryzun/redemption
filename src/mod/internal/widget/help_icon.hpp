/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/widget.hpp"

class Font;
class Theme;
class FontCharView;

struct WidgetHelpIcon : Widget
{
    struct Colors
    {
        Color fg;
        Color bg;
    };

    enum class Style
    {
        Button,
        ThinText,
    };

    WidgetHelpIcon(gdi::GraphicApi & drawable, Font const & font, Style style, Colors colors);

    void rdp_input_invalidate(Rect clip) override;

    struct D;
    friend struct D;

private:
    FontCharView const * fc;
    Colors colors;
    Style style;
};
