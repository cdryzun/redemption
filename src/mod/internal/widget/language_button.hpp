/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/button.hpp"
#include "keyboard/keylayout.hpp"
#include "utils/ref.hpp"

#include <vector>

class FrontAPI;
class Theme;

class LanguageButton : public WidgetButtonEvent
{
public:
    struct Colors
    {
        Color fg;
        Color bg;
        Color focus_bg;

        static Colors from_theme(Theme const& theme) noexcept;
    };

    LanguageButton(
        zstring_view enable_locales,
        Widget & parent,
        gdi::GraphicApi & drawable,
        FrontAPI & front,
        Font const & font,
        Colors colors
    );

    void next_layout();

    void rdp_input_invalidate(Rect clip) override;

private:
    struct D;

    unsigned selected_language = 0;
    Colors colors;
    Font const & font;
    FrontAPI & front;
    Widget & parent_redraw;
    std::vector<CRef<KeyLayout>> locales;
    KeyLayout front_layout;
    WidgetText<64> button_text;
};
