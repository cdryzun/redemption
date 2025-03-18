/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "utils/bitmap.hpp"

class WidgetImage : public Widget
{
public:
    /// \param bg_color is used with a transparent image
    WidgetImage(gdi::GraphicApi& drawable, const char *filename, Color bg_color);

    void rdp_input_invalidate(Rect clip) override;

private:
    Bitmap bmp;
    Bitmap last_bmp;
    Rect last_rect;
};
