/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/composite.hpp"
#include "mod/internal/widget/label.hpp"
#include "utils/sugar/array_view.hpp"

class Font;

class WidgetGroupBox : public WidgetComposite
{
public:
    WidgetGroupBox( gdi::GraphicApi & drawable, chars_view text
                  , Color fgcolor, Color bgcolor, Font const & font);

    void rdp_input_invalidate(Rect clip) override;

private:
    Color fg_color;
    uint16_t line_height;
    WidgetText<127> caption;
};
