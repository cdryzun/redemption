/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2012
 *   Author(s): Christophe Grosjean, Raphael Zhou, Jonathan Poelen,
 *              Meng Tan
 */

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text.hpp"
#include "gdi/draw_utils.hpp"
#include "mod/internal/widget/scroll.hpp"
#include "mod/internal/widget/button.hpp"

struct WidgetScrollBar::D
{
    static const uint32_t scroll_up_text = 0x25B2;  // ▲
    static const uint32_t scroll_down_text = 0x25BC;  // ▼
    // static const uint32_t scroll_vertical_cursor_text = 0x25A5;  // ▥

    static const uint32_t scroll_left_text = 0x25C0;  // ◀
    static const uint32_t scroll_right_text = 0x25B6;  // ▶
    // static const uint32_t scroll_horizontal_cursor_text = 0x25A4;  // ▤

    static const uint16_t x_text = 4;
    static const uint16_t y_text = 5;

    struct Rects
    {
        WidgetScrollBar & self;

        Rect const left_or_top_button = Rect(
            self.x(),
            self.y(),
            self.horizontal ? self.icon_width + D::x_text * 2 : self.cx(),
            self.horizontal ? self.cy() : self.icon_height + D::y_text * 2
        );

        int x_bar = self.horizontal ? self.icon_width + D::x_text * 2 : 0;
        int y_bar = self.horizontal ? 0 : self.icon_height + D::y_text * 2;

        Rect const right_or_bottom_button = left_or_top_button.offset(
            checked_int(self.horizontal ? self.cx() - x_bar : 0),
            checked_int(self.horizontal ? 0 : self.cy() - y_bar)
        );

        Rect const cursor_button = left_or_top_button.offset(
            checked_int(self.horizontal
                ? x_bar + (self.cx() - x_bar * 3) * self.current_value / self.max_value
                : 0
            ),
            checked_int(self.horizontal
                ? 0
                : y_bar + (self.cy() - y_bar * 3) * self.current_value / self.max_value
            )
        );

        Rect rect_bar() const
        {
            return Rect{
                checked_int(self.x() + x_bar),
                checked_int(self.y() + y_bar),
                checked_int(self.horizontal ? self.cx() - x_bar * 2 : self.cx()),
                checked_int(self.horizontal ? self.cy() : self.cy() - y_bar * 2),
            };
        }
    };
};

WidgetScrollBar::WidgetScrollBar(
    gdi::GraphicApi & drawable, Font const & font,
    ScrollDirection scroll_direction, Colors colors,
    WidgetEventNotifier onscroll)
: Widget(drawable, Focusable::No)
, onscroll(onscroll)
, horizontal(scroll_direction == ScrollDirection::Horizontal)
, colors(colors)
, top_left_fc(font.item(horizontal ? D::scroll_left_text : D::scroll_up_text).view)
, bottom_right_fc(font.item(horizontal ? D::scroll_right_text : D::scroll_down_text).view)
, h_text(font.max_height())
, icon_width{checked_int{
    std::max(top_left_fc.width + !horizontal, bottom_right_fc.width + !horizontal)
}}
, icon_height{std::max(top_left_fc.height, bottom_right_fc.height)}
{
    Widget::set_wh({
        checked_int(icon_width + D::x_text * 2),
        checked_int(icon_height + D::y_text * 2),
    });
}

void WidgetScrollBar::compute_step_value()
{
    auto len = this->horizontal
        ? cx() - (icon_width + D::x_text * 2) * 3
        : cy() - (icon_height + D::y_text * 2) * 3
        ;
    this->step_value = len / max_value;
}

unsigned int WidgetScrollBar::get_current_value() const
{
    return static_cast<unsigned int>(this->current_value);
}

void WidgetScrollBar::set_current_value(unsigned int cv)
{
    this->current_value = std::min(checked_cast<int>(cv), this->max_value);
}

void WidgetScrollBar::set_max_value(unsigned int maxvalue)
{
    this->max_value = static_cast<int>(maxvalue);

    this->compute_step_value();
}

void WidgetScrollBar::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect());

    if (!rect_intersect.isempty()) {
        D::Rects rects{*this};

        enum Orientation { LeftTop, RightBottom };

        auto draw_button = [&](Orientation orientation, bool has_focus, const FontCharView* fc) {
            auto fg = colors.fg;
            auto bg = has_focus ? colors.bg_focus : colors.bg;

            auto left_pad = icon_width - fc->width + 1 - orientation;
            auto right_pad = icon_width - fc->width + orientation;
            auto top_pad = icon_height - fc->height + 1 - orientation;
            auto bottom_pad = icon_height - fc->height + orientation;

            auto clip = orientation
                ? rects.right_or_bottom_button
                : rects.left_or_top_button;

            gdi::draw_text(
                drawable,
                clip.x - fc->offsetx - !horizontal,
                clip.y - fc->offsety,
                h_text,
                gdi::DrawTextPadding{
                    .top = checked_int(top_pad / 2 + D::y_text),
                    .right = checked_int(right_pad / 2 + D::x_text),
                    .bottom = checked_int(bottom_pad / 2 + D::y_text),
                    .left = checked_int(left_pad / 2 + D::x_text),
                },
                array_view{&fc, 1},
                fg,
                bg,
                clip.intersect(rect_intersect)
            );
        };

        // left / top button
        draw_button(LeftTop, selected_button == SelectedButton::LeftOrTop, &top_left_fc);

        // right / bottom button
        draw_button(RightBottom, selected_button == SelectedButton::RightOrBottom, &bottom_right_fc);

        // scroll bar
        drawable.draw(
            RDPOpaqueRect(rect_intersect.intersect(rects.rect_bar()), colors.bg),
            get_rect(),
            gdi::ColorCtx::depth24()
        );

        // scroll bar cursor
        drawable.draw(
            RDPOpaqueRect(rect_intersect.intersect(rects.cursor_button), colors.fg),
            get_rect(),
            gdi::ColorCtx::depth24()
        );
    }
}

void WidgetScrollBar::set_size(uint16_t width_or_height)
{
    Widget::set_wh(
        horizontal ? width_or_height : cx(),
        horizontal ? cy() : width_or_height
    );
    this->compute_step_value();
}

// RdpInput
void WidgetScrollBar::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN)) {
        this->mouse_down = true;

        D::Rects rects{*this};

        if (rects.left_or_top_button.contains_pt(x, y)) {
            this->selected_button = SelectedButton::LeftOrTop;

            const int old_value = this->current_value;

            this->current_value -= this->step_value;

            if (this->current_value < 0) {
                this->current_value = 0;
            }

            if (old_value != this->current_value) {
                this->onscroll();
            }
        }
        else if (rects.cursor_button.contains_pt(x, y)) {
            this->selected_button = SelectedButton::Cursor;

            this->old_mouse_x_or_y         = (this->horizontal ? x : y);
            this->old_cursor_button_x_or_y = (this->horizontal ? rects.cursor_button.x : rects.cursor_button.y);
        }
        if (rects.right_or_bottom_button.contains_pt(x, y)) {
            this->selected_button = SelectedButton::RightOrBottom;

            const int old_value = this->current_value;

            this->current_value += this->step_value;

            if (this->current_value > this->max_value) {
                this->current_value = this->max_value;
            }

            if (old_value != this->current_value) {
                this->onscroll();
            }
        }

        this->rdp_input_invalidate(this->get_rect());
        this->has_focus = true;
    }
    else if (device_flags == MOUSE_FLAG_BUTTON1) {
        this->mouse_down               = false;
        this->selected_button          = SelectedButton::None;
        this->old_mouse_x_or_y         = 0;
        this->old_cursor_button_x_or_y = 0;

        this->rdp_input_invalidate(this->get_rect());
    }
    else if (device_flags == MOUSE_FLAG_MOVE) {
        if (this->mouse_down && (SelectedButton::Cursor == this->selected_button)) {
            const int old_value = this->current_value;

            const int min_button_x_or_y = this->horizontal
                ? this->x() + icon_width + D::x_text * 2 - 1
                : this->y() + icon_height + D::y_text * 2 - 1
                ;
            const int max_button_x_or_y = min_button_x_or_y
                + (this->horizontal
                    ? this->cx() - icon_width * 3 - 2
                    : this->cy() - icon_height * 3 - 2
                );

            const int min_x_or_y = min_button_x_or_y + (this->old_mouse_x_or_y - this->old_cursor_button_x_or_y);
            const int max_x_or_y = max_button_x_or_y + (this->old_mouse_x_or_y - this->old_cursor_button_x_or_y);

            if ((this->horizontal ? x : y) < min_x_or_y) {
                this->current_value = 0;
            }
            else if ((this->horizontal ? x : y) >= max_x_or_y) {
                this->current_value = this->max_value;
            }
            else {
                this->current_value =
                    ((this->horizontal ? x : y) - min_x_or_y) * this->max_value / (max_x_or_y - min_x_or_y);
            }

            if (old_value != this->current_value) {
                this->onscroll();
            }

            this->rdp_input_invalidate(this->get_rect());
        }
    }
    else {
        this->Widget::rdp_input_mouse(device_flags, x, y);
    }
}
