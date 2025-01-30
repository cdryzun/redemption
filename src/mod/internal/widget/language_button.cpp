/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2015
 *   Author(s): Christophe Grosjean, Xiaopeng Zhou, Jonathan Poelen,
 *              Meng Tan, Jennifer Inthavong
 */

#include "mod/internal/widget/language_button.hpp"
#include "mod/internal/widget/button.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/front_api.hpp"
#include "core/font.hpp"
#include "keyboard/keylayouts.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text_metrics.hpp"
#include "gdi/draw_utils.hpp"
#include "utils/log.hpp"
#include "utils/theme.hpp"
#include "utils/sugar/split.hpp"
#include "utils/strutils.hpp"
#include "utils/txt2d_to_rects.hpp"


namespace
{
    constexpr auto kbd_icon_rects = TXT2D_TO_RECTS(
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
    constexpr uint16_t kbd_icon_cx = kbd_icon_rects.back().cx;
    constexpr uint16_t kbd_icon_cy = kbd_icon_rects.back().ebottom();

    constexpr uint16_t button_border = 2;
    constexpr uint16_t icon_w_padding = 6;
    constexpr uint16_t icon_right_padding = 5;
    constexpr uint16_t icon_h_padding = 6;

    uint16_t compute_inner_height(Font const & font)
    {
        return std::max(font.max_height(), kbd_icon_cy);
    }

    Dimension compute_optimial_dim(Font const & font, WidgetText<64> & text)
    {
        uint16_t w = text.width()
                   + icon_w_padding * 2
                   + button_border * 2
                   + kbd_icon_cx
                   + icon_right_padding;
        uint16_t h = compute_inner_height(font)
                   + button_border * 2
                   + icon_h_padding * 2;
        return {w, h};
    }
} // namespace

LanguageButton::LanguageButton(
    zstring_view enable_locales,
    Widget & parent_redraw,
    gdi::GraphicApi & drawable,
    FrontAPI & front,
    Font const & font,
    Theme const & theme
)
: Widget(drawable, Focusable::Yes)
, colors{
    .fg = theme.global.fgcolor,
    .bg = theme.global.bgcolor,
    .focus_bg = theme.global.focus_color,
}
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

    set_wh(compute_optimial_dim(font, button_text));
}

void LanguageButton::next_layout()
{
    Rect rect = this->get_rect();

    this->selected_language = (this->selected_language + 1) % this->locales.size();
    KeyLayout const& layout = this->locales[this->selected_language];
    button_text.set_text(font, layout.name);

    Dimension dim = compute_optimial_dim(font, button_text);
    this->set_wh(dim);

    rect.cx = std::max(rect.cx, this->cx());
    rect.cy = std::max(rect.cy, this->cy());
    this->parent_redraw.rdp_input_invalidate(rect);

    front.set_keylayout(layout);
}

void LanguageButton::rdp_input_invalidate(Rect clip)
{
    auto h_text = font.max_height();

    int cy_inner = cy() - button_border * 2;
    int y_padding = (cy_inner - h_text) / 2;
    int is_pressed = button_state.is_pressed();

    gdi_draw_border(
        drawable, colors.fg, get_rect(),
        button_border, clip,
        gdi::ColorCtx::depth24()
    );

    gdi::draw_text(
        drawable,
        x() + button_border,
        y() + button_border,
        h_text,
        gdi::DrawTextPadding{
            .top = checked_int(y_padding + is_pressed),
            .right = checked_int(icon_w_padding - is_pressed),
            .bottom = checked_int(cy_inner - h_text - y_padding - is_pressed),
            .left = checked_int(icon_w_padding + kbd_icon_cx + icon_right_padding + is_pressed),
        },
        button_text.fcs(),
        colors.fg,
        has_focus ? colors.focus_bg : colors.bg,
        clip.intersect(get_rect().shrink(button_border))
    );

    int ox = x() + icon_w_padding;
    int oy = y() + (cy() - compute_inner_height(font)) / 2;

    Rect rect_intersect = clip.intersect(Rect(
        checked_int(ox), checked_int(oy),
        kbd_icon_cx, kbd_icon_cy
    ));

    if (!rect_intersect.isempty()) {
        for (auto r : kbd_icon_rects) {
            r.x += ox;
            r.y += oy;
            drawable.draw(RDPOpaqueRect(r, colors.fg), rect_intersect, gdi::ColorCtx::depth24());
        }
    }
}

void LanguageButton::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    button_state.update(
        get_rect(), x, y, device_flags,
        [this]{ next_layout(); },
        // TODO
        [this](Rect rect){ rdp_input_invalidate(rect); }
    );
}

void LanguageButton::rdp_input_scancode(KbdFlags /*flags*/, Scancode /*scancode*/, uint32_t /*event_time*/, const Keymap& keymap)
{
    if (WidgetButton::is_submit_event(keymap)) {
        next_layout();
    }
}

void LanguageButton::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (WidgetButton::is_submit_event(flag, unicode)) {
        next_layout();
    }
}

void LanguageButton::focus(int reason)
{
    (void)reason;
    if (!has_focus){
        has_focus = true;
        // TODO
        rdp_input_invalidate(get_rect());
    }
}

void LanguageButton::blur()
{
    if (has_focus) {
        has_focus = false;
        button_state.pressed(false);
        // TODO
        rdp_input_invalidate(get_rect());
    }
}
