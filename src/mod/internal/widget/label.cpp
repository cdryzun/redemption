/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "mod/internal/widget/label.hpp"
#include "gdi/text.hpp"
#include "utils/utf.hpp"
#include "utils/theme.hpp"


[[nodiscard]]
array_view<FontCharView const *> init_widget_text(
    writable_array_view<FontCharView const *> fcs,
    OutParam<uint16_t> width,
    Font const & font,
    chars_view text
)
{
    int w = 0;
    auto out = std::begin(fcs);
    auto end_out = std::end(fcs);
    utf8_for_each(
        text,
        [&](uint32_t uc) {
            auto const * fc = &font.item(uc).view;
            w += fc->boxed_width();
            *out++ = fc;
        },
        [](utf8_char_invalid) {},
        [](utf8_char_truncated) {},
        [&]{ return out < end_out; }
    );

    width.out_value = checked_int(w ? w + 1 : w);
    return fcs.before(out);
}


WidgetLabel::Colors WidgetLabel::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.global.fgcolor,
        .bg = theme.global.bgcolor,
    };
}

WidgetLabel::WidgetLabel(
    gdi::GraphicApi & drawable, Font const & font, chars_view text, Colors colors
)
    : Widget(drawable, Focusable::No)
    , colors(colors)
{
    set_text(font, text);
}

void WidgetLabel::set_text(Font const & font, chars_view text)
{
    text_label.set_text(font, text);
    set_wh(text_label.width(), font.max_height());
}

void WidgetLabel::set_text_and_redraw(const Font& font, chars_view text, Rect clip)
{
    auto old_width = cx();
    text_label.set_text(font, text);
    set_wh(text_label.width(), font.max_height());

    rdp_input_invalidate(clip);

    // refresh background when label width decrease
    if (old_width > cx()) {
        auto rect = get_rect();
        rect.x += cx();
        rect.cx = old_width - cx();
        rect = rect.intersect(clip);
        if (!rect.isempty()) {
            drawable.draw(RDPOpaqueRect(rect, colors.bg), rect, gdi::ColorCtx::depth24());
        }
    }
}

void WidgetLabel::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect());

    if (rect_intersect.isempty()) [[unlikely]] {
        return;
    }

    gdi::draw_text(
        drawable,
        x(),
        y(),
        cy(),
        gdi::DrawTextPadding{
          .top = 0,
          .right = cx(),
          .bottom = 0,
          .left = 0,
        },
        text_label.fcs(),
        colors.fg,
        colors.bg,
        rect_intersect
    );
}
