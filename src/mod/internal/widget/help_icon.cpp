/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/help_icon.hpp"
#include "gdi/text.hpp"
#include "gdi/draw_utils.hpp"
#include "utils/theme.hpp"
#include "core/font.hpp"


struct WidgetHelpIcon::D
{
    struct PaddingAndBorder
    {
        uint16_t x_text;
        uint16_t y_text;
        uint16_t border_len;

        uint16_t x_pad() noexcept
        {
            return x_text + border_len;
        }

        uint16_t y_pad() noexcept
        {
            return y_text + border_len;
        }
    };

    static constexpr PaddingAndBorder button_def{
        .x_text = 5,
        .y_text = 2,
        .border_len = 2,
    };

    static constexpr PaddingAndBorder thin_text_def{
        .x_text = 2,
        .y_text = 0,
        .border_len = 1,
    };

    static PaddingAndBorder paddings_and_border(Style style) noexcept
    {
        return (style == Style::Button) ? D::button_def : D::thin_text_def;
    }
};


WidgetHelpIcon::WidgetHelpIcon(
    gdi::GraphicApi & drawable, const Font& font, Style style, Colors colors
)
    : Widget(drawable, Focusable::No)
    , fc(&font.item('?').view)
    , colors{colors}
    , style{style}
{
    auto paddings_and_border = D::paddings_and_border(style);
    set_wh(
        checked_int(fc->boxed_width() * 2 - fc->width + paddings_and_border.x_pad() * 2),
        checked_int(font.max_height() + paddings_and_border.y_pad() * 2)
    );
}

void WidgetHelpIcon::rdp_input_invalidate(Rect clip)
{
    auto paddings_and_border = D::paddings_and_border(style);
    auto x_pad = paddings_and_border.x_pad();
    auto y_pad = paddings_and_border.y_pad();
    gdi::draw_text(
        drawable,
        x(),
        y(),
        cy() - y_pad * 2,
        gdi::DrawTextPadding::Padding2{
          .top_bottom = y_pad,
          .left_right = x_pad,
        },
        {&fc, 1},
        colors.fg,
        colors.bg,
        get_rect().intersect(clip)
    );

    gdi_draw_border(
        drawable, colors.fg, get_rect(),
        paddings_and_border.border_len,
        clip, gdi::ColorCtx::depth24()
    );
}
