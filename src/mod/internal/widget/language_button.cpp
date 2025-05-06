/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/language_button.hpp"
#include "mod/internal/widget/button.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/front_api.hpp"
#include "core/font.hpp"
#include "keyboard/keylayouts.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text.hpp"
#include "gdi/draw_utils.hpp"
#include "utils/log.hpp"
#include "utils/theme.hpp"
#include "utils/sugar/split.hpp"
#include "utils/strutils.hpp"
#include "utils/txt2d_to_rects.hpp"


struct LanguageButton::D
{
    static constexpr auto kbd_icon_rects = TXT2D_TO_RECTS(
        // "            ##                                                  ",
        // "            ##                                                  ",
        // "            ########################################            ",
        // "                                                  ##            ",
        // "                                                  ##            ",
        "################################################################",
        "##------------------------------------------------------------##",
        "##------------------------------------------------------------##",
        "##----######----######----######----######----############----##",
        "##----######----######----######----######----############----##",
        "##----######----######----######----######----############----##",
        "##--------------------------------------------------######----##",
        "##--------------------------------------------------######----##",
        "##----############----######----######----######----######----##",
        "##----############----######----######----######----######----##",
        "##----############----######----######----######----######----##",
        "##------------------------------------------------------------##",
        "##------------------------------------------------------------##",
        "##----######----################################----######----##",
        "##----######----################################----######----##",
        "##----######----################################----######----##",
        "##------------------------------------------------------------##",
        "##------------------------------------------------------------##",
        "################################################################",
    );
    static constexpr uint16_t kbd_icon_cx = kbd_icon_rects.back().cx;
    static constexpr uint16_t kbd_icon_cy = kbd_icon_rects.back().ebottom();

    static constexpr uint16_t border_width = 2;
    static constexpr uint16_t icon_w_padding = 6;
    static constexpr uint16_t icon_right_padding = 5;
    static constexpr uint16_t icon_h_padding = 6;

    static uint16_t compute_inner_height(Font const & font)
    {
        return std::max(font.max_height(), kbd_icon_cy);
    }

    static Dimension compute_optimial_dim(Font const & font, WidgetText<64> & text)
    {
        uint16_t w = text.width()
                   + icon_w_padding * 2
                   + border_width * 2
                   + kbd_icon_cx
                   + icon_right_padding;
        uint16_t h = compute_inner_height(font)
                   + border_width * 2
                   + icon_h_padding * 2;
        return {w, h};
    }
};


LanguageButton::Colors LanguageButton::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.global.fgcolor,
        .bg = theme.global.bgcolor,
        .focus_bg = theme.global.focus_color,
    };
}


LanguageButton::LanguageButton(
    zstring_view enable_locales,
    Widget & parent_redraw,
    gdi::GraphicApi & drawable,
    FrontAPI & front,
    Font const & font,
    Colors colors
)
: WidgetButtonEvent(drawable, [this]{ next_layout(); }, WidgetButtonEvent::RedrawOnSubmit::Yes)
, colors{colors}
, font(font)
, front(front)
, parent_redraw(parent_redraw)
, front_layout(front.get_keylayout())
{
    locales.push_back(bool(front_layout.kbdid)
        ? Ref(front_layout)
        : Ref(default_layout()));

    for (auto locale : split_with(enable_locales, ',')) {
        auto const name = trim(locale);
        if (auto const* layout = find_layout_by_name(name)) {
            if (layout->kbdid != front_layout.kbdid) {
                locales.emplace_back(*layout);
            }
        }
        else {
            LOG(LOG_WARNING, "Layout \"%.*s\" not found.",
                static_cast<int>(name.size()), name.data());
        }
    }

    button_text.set_text(font, locales.front().get().name);

    set_wh(D::compute_optimial_dim(font, button_text));
}

void LanguageButton::next_layout()
{
    Rect rect = this->get_rect();

    this->selected_language = (this->selected_language + 1) % this->locales.size();
    KeyLayout const& layout = this->locales[this->selected_language];
    button_text.set_text(font, layout.name);

    Dimension dim = D::compute_optimial_dim(font, button_text);
    this->set_wh(dim);

    rect.cx = std::max(rect.cx, this->cx());
    rect.cy = std::max(rect.cy, this->cy());
    this->parent_redraw.rdp_input_invalidate(rect);

    front.set_keylayout(layout);
}

void LanguageButton::rdp_input_invalidate(Rect clip)
{
    auto h_text = font.max_height();

    int cy_inner = cy() - D::border_width * 2;
    int y_padding = (cy_inner - h_text) / 2;
    int pressed_pad = is_pressed();

    gdi_draw_border(
        drawable, colors.fg, get_rect(),
        D::border_width, clip,
        gdi::ColorCtx::depth24()
    );

    gdi::draw_text(
        drawable,
        x() + D::border_width,
        y() + D::border_width,
        h_text,
        gdi::DrawTextPadding{
            .top = checked_int(y_padding + pressed_pad),
            .right = checked_int(D::icon_w_padding - pressed_pad),
            .bottom = checked_int(cy_inner - h_text - y_padding - pressed_pad),
            .left = checked_int(D::icon_w_padding + D::kbd_icon_cx + D::icon_right_padding + pressed_pad),
        },
        button_text.fcs(),
        colors.fg,
        has_focus ? colors.focus_bg : colors.bg,
        clip.intersect(get_rect().shrink(D::border_width))
    );

    int ox = x() + D::icon_w_padding;
    int oy = y() + (cy() - D::compute_inner_height(font)) / 2;

    Rect rect_intersect = clip.intersect(Rect(
        checked_int(ox), checked_int(oy),
        D::kbd_icon_cx, D::kbd_icon_cy
    ));

    if (!rect_intersect.isempty()) {
        for (auto r : D::kbd_icon_rects) {
            r.x += ox;
            r.y += oy;
            drawable.draw(RDPOpaqueRect(r, colors.fg), rect_intersect, gdi::ColorCtx::depth24());
        }
    }
}
