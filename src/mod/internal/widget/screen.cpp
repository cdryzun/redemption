/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/RDP/rdp_pointer.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/draw_utils.hpp"
#include "mod/internal/widget/screen.hpp"
#include "utils/theme.hpp"


struct WidgetScreen::WidgetTooltip::D
{
    static const int border_width = 1;
    static const int h_border = border_width + 9;
    static const int w_border = border_width + 9;
};


WidgetScreen::WidgetTooltip::Colors
WidgetScreen::WidgetTooltip::Colors::from_theme(const Theme& theme) noexcept
{
    return Colors{
        .fg = theme.tooltip.fgcolor,
        .bg = theme.tooltip.bgcolor,
        .border = theme.tooltip.border_color,
    };
}


WidgetScreen::WidgetTooltip::WidgetTooltip(
    gdi::GraphicApi & drawable, Colors colors
)
    : Widget(drawable, Focusable::No)
    , colors(colors)
{
}

bool WidgetScreen::WidgetTooltip::has_text() const noexcept
{
    return !desc.lines().empty();
}

void WidgetScreen::WidgetTooltip::clear_text() noexcept
{
    desc.clear_text();
}

void WidgetScreen::WidgetTooltip::set_text(Font const & font, unsigned max_width, chars_view text)
{
    desc.set_text(font, text);
    desc.update_dimension(max_width);
    Dimension dim = desc.dimension();

    set_wh(dim.w + 2 * D::w_border,
           dim.h + 2 * D::h_border);
}

void WidgetScreen::WidgetTooltip::rdp_input_invalidate(Rect clip)
{
    auto rect = get_rect();
    Rect rect_intersect = clip.intersect(rect);

    if (!rect_intersect.isempty()) {
        drawable.draw(
            RDPOpaqueRect(rect, colors.bg),
            rect_intersect, gdi::ColorCtx::depth24()
        );

        desc.draw(drawable, {
            .x = checked_int{x() + D::w_border},
            .y = checked_int{y() + D::h_border},
            .clip = rect_intersect,
            .fgcolor = colors.fg,
            .bgcolor = colors.bg,
            .draw_bg_rect = false,
        });

        gdi_draw_border(
            drawable, colors.border, rect, D::border_width,
            rect_intersect, gdi::ColorCtx::depth24()
        );
    }
}


WidgetScreen::WidgetScreen(
    gdi::GraphicApi & drawable, uint16_t width, uint16_t height,
    Font const & font, Theme const& theme
)
    : WidgetComposite(drawable, Focusable::Yes, NamedBGRColor::BLACK)
    , current_over(nullptr)
    , font(font)
    , tooltip(drawable, WidgetTooltip::Colors::from_theme(theme))
{
    this->set_wh(width, height);
    this->set_xy(0, 0);
}

void WidgetScreen::show_tooltip(
    chars_view text, int x, int y,
    Rect const preferred_display_rect,
    Rect const mouse_area)
{
    if (text.empty()) {
        if (this->tooltip.has_text()) {
            this->tooltip.clear_text();
            this->remove_widget(this->tooltip);
            this->rdp_input_invalidate(this->tooltip.get_rect());
        }
    }
    else if (!this->tooltip.has_text()) {
        Rect display_rect = this->get_rect();
        if (!preferred_display_rect.isempty()) {
            display_rect = display_rect.intersect(preferred_display_rect);
        }

        this->tooltip_mouse_area = mouse_area;
        this->tooltip.set_text(this->font, this->cx(), text);

        int w = this->tooltip.cx();
        int h = this->tooltip.cy();
        int sw = display_rect.x + display_rect.cx;
        int sh = display_rect.y + display_rect.cy;
        int posx = ((x + w) > sw) ? (sw - w) : x;
        int posy = (y - h >= display_rect.y) ? (y - h) : (y + h > sh) ? (sh - h) : y;
        this->tooltip.set_xy(checked_int{posx}, checked_int{posy});

        this->add_widget(this->tooltip);
        this->tooltip.rdp_input_invalidate(this->tooltip.get_rect());
    }
}

void WidgetScreen::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    this->redo_mouse_pointer_change(x, y);

    if (this->tooltip.has_text()) {
        if (device_flags & MOUSE_FLAG_MOVE) {
            if (!this->tooltip_mouse_area.contains_pt(x, y)) {
                this->hide_tooltip();
            }
        }
        if (device_flags & MOUSE_FLAG_BUTTON1) {
            this->hide_tooltip();
        }
    }

    WidgetComposite::rdp_input_mouse(device_flags, x, y);
}

void WidgetScreen::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (this->tooltip.has_text()) {
        this->hide_tooltip();
    }
    WidgetComposite::rdp_input_scancode(flags, scancode, event_time, keymap);
}

void WidgetScreen::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (this->tooltip.has_text()) {
        this->hide_tooltip();
    }
    WidgetComposite::rdp_input_unicode(flag, unicode);
}

void WidgetScreen::allow_mouse_pointer_change(bool allow)
{
    this->allow_mouse_pointer_change_ = allow;
}

void WidgetScreen::redo_mouse_pointer_change(int x, int y)
{
    Widget * w = this;
    // last_widget_at_pos(x, y)
    {
        Widget * wnext;
        int count = 10;
        while (--count > 0 && (wnext = w->widget_at_pos(x, y))) {
            w = wnext;
        }
    }

    if (this->current_over != w){
        if (this->allow_mouse_pointer_change_) {
            switch (w ? w->pointer_flag : PointerType::Normal) {
            case PointerType::Edit:
                this->drawable.cached_pointer(PredefinedPointer::Edit);
            break;
            case PointerType::Custom: {
                if (auto const* cache_index = w->get_cache_pointer_index()) {
                    this->drawable.cached_pointer(*cache_index);
                    break;
                }
                [[fallthrough]];
            }
            case PointerType::Normal:
                this->drawable.cached_pointer(PredefinedPointer::Normal);
            }
        }
        this->current_over = w;
    }
}
