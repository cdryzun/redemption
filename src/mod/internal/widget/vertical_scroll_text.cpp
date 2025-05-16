/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/draw_utils.hpp"
#include "mod/internal/widget/vertical_scroll_text.hpp"
#include "utils/utf.hpp"
#include "keyboard/keymap.hpp"

struct WidgetVerticalScrollText::D
{
    static constexpr uint32_t top_button_uc = 0x25B2; // ▲
    static constexpr uint32_t cursor_button_uc = 0x25A5; // ▥
    static constexpr uint32_t bottom_button_uc = 0x25BC; // ▼

    static constexpr int16_t scroll_sep = 4;

    static Dimension get_optimal_button_dim(const Font& font)
    {
        auto const& glyph = font.item(top_button_uc).view;
        return Dimension(glyph.width + 8, glyph.height + 12);
    }
};


WidgetVerticalScrollText::WidgetVerticalScrollText(
    gdi::GraphicApi& drawable, chars_view text,
    Color fg_color, Color bg_color, Color focus_color,
    Font const & font)
: Widget(drawable, Focusable::No)
, fg_color(fg_color)
, bg_color(bg_color)
, focus_color(focus_color)
, line_height(font.max_height())
, button_dim(D::get_optimal_button_dim(font))
, icon_top(&font.item(D::top_button_uc).view)
, icon_cursor(&font.item(D::cursor_button_uc).view)
, icon_bottom(&font.item(D::bottom_button_uc).view)
{
    multiline_text.set_text(font, text);
}

void WidgetVerticalScrollText::set_xy(int16_t x, int16_t y)
{
    Widget::set_xy(x, y);
}

void WidgetVerticalScrollText::update_dimension(DimensionContraints contraints)
{
    auto compute_dim = [this, &contraints](Dimension optimal_dim, Dimension max_dim){
        auto use_optimal = [this](DimensionContraints::ScrollState st){
            return has_scroll ? st.with_scroll : st.without_scroll;
        };

        uint16_t w = use_optimal(contraints.prefer_optimal_dimension.horizontal)
            ? std::max(std::min(optimal_dim.w, max_dim.w), contraints.width.min)
            : max_dim.w;
        uint16_t h = use_optimal(contraints.prefer_optimal_dimension.vertical)
            ? std::max(std::min(optimal_dim.h, max_dim.h), contraints.height.min)
            : max_dim.h;
        return Dimension{w, h};
    };

    this->has_scroll = false;
    this->set_focusable(Focusable::No);

    uint16_t const max_cx = contraints.width.max;
    uint16_t const max_cy = contraints.height.max;

    this->multiline_text.update_dimension(max_cx);
    auto const dim = this->multiline_text.dimension();

    if (max_cy >= dim.h) {
        this->total_h = 0;
        this->current_y = 0;
        Widget::set_wh(compute_dim(dim, {max_cx, max_cy}));
        return;
    }

    this->has_scroll = true;
    this->set_focusable(Focusable::Yes);

    uint16_t const scroll_w = this->button_dim.w + D::scroll_sep;
    uint16_t const new_cx = max_cx - scroll_w;
    this->multiline_text.update_dimension(new_cx);

    auto const optimal_dim = this->multiline_text.dimension();
    auto const new_dim = compute_dim(optimal_dim, {new_cx, max_cy});
    Widget::set_wh(new_dim.w + scroll_w, new_dim.h);

    int const text_h = optimal_dim.h;
    int const total_scroll_h = std::max(new_dim.h - this->button_dim.h * 2, 1);

    auto old_percent_scroll = this->total_h && this->current_y
        ? static_cast<double>(this->current_y) / total_h
        : 0.0;

    uint16_t const glyph_cy = this->multiline_text.line_height({.with_line_sep = true});
    this->page_h = std::max(new_dim.h / glyph_cy - 1, 1) * glyph_cy;
    this->total_h = text_h - this->page_h;
    this->cursor_button_h = std::max(
        checked_cast<uint16_t>(this->page_h * total_scroll_h / text_h),
        this->button_dim.h
    );
    this->scroll_h = std::max(total_scroll_h - this->cursor_button_h, 1);
    this->cursor_button_y = int16_t(this->button_dim.h - 1);

    this->current_y = std::min(
        static_cast<int>(old_percent_scroll * this->page_h) / this->page_h,
        this->total_h
    );
}

void WidgetVerticalScrollText::_update_cursor_button_y_and_redraw(int new_y)
{
    this->current_y = new_y;
    this->cursor_button_y = checked_int{
        this->scroll_h * new_y / this->total_h + this->button_dim.h
    };
    this->rdp_input_invalidate(this->get_rect());
}

void WidgetVerticalScrollText::scroll_down()
{
    if (!this->has_scroll) {
        return ;
    }
    this->_scroll_down();
}

void WidgetVerticalScrollText::scroll_up()
{
    if (!this->has_scroll) {
        return ;
    }
    this->_scroll_up();
}

bool WidgetVerticalScrollText::_scroll_down()
{
    const auto old_y = this->current_y;
    const auto new_y = std::min(this->current_y + this->page_h, this->total_h);
    if (old_y != new_y) {
        _update_cursor_button_y_and_redraw(new_y);
        return true;
    }
    return false;
}

bool WidgetVerticalScrollText::_scroll_up()
{
    const auto old_y = this->current_y;
    const auto new_y = std::max(this->current_y - this->page_h, 0);
    if (old_y != new_y) {
        _update_cursor_button_y_and_redraw(new_y);
        return true;
    }
    return false;
}

void WidgetVerticalScrollText::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (!this->has_scroll) {
        this->Widget::rdp_input_mouse(device_flags, x, y);
        return;
    }

    auto update_scroll_bar = [this]{
        this->rdp_input_invalidate(Rect{
            checked_int{this->x() + this->cx() - this->button_dim.w},
            this->y(),
            this->button_dim.w,
            this->cy()
        });
    };

    if (device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN)) {
        auto in_range = [](int x, int16_t rx, uint16_t rcx){
            return rx <= x && x < rx + rcx;
        };

        if (!in_range(x, checked_int{this->eright() - this->button_dim.w * 2}, this->cx())) {
            // outside
        }
        // cursor
        else if (in_range(y, this->y() + this->cursor_button_y, this->cursor_button_h)) {
            this->selected_button = ButtonType::Cursor;

            this->mouse_start_y = y;
            this->mouse_y = this->cursor_button_y - this->button_dim.w;

            update_scroll_bar();
        }
        // top
        else if (y < this->y() + this->cursor_button_y) {
            auto old = std::exchange(this->selected_button, ButtonType::Top);
            if (!this->_scroll_up() && old != ButtonType::Top) {
                this->rdp_input_invalidate(this->get_rect());
            }
        }
        // bottom
        else if (y > this->y() + this->cursor_button_y + this->button_dim.h) {
            auto old = std::exchange(this->selected_button, ButtonType::Bottom);
            if (!this->_scroll_down() && old != ButtonType::Bottom) {
                this->rdp_input_invalidate(this->get_rect());
            }
        }
    }
    else if (device_flags == MOUSE_FLAG_BUTTON1) {
        if (bool(this->selected_button)) {
            this->selected_button = ButtonType::None;
            update_scroll_bar();
        }
    }
    else if (device_flags == MOUSE_FLAG_MOVE) {
        if (ButtonType::Cursor == this->selected_button) {
            auto const delta = y - this->mouse_start_y;
            auto const cursor_y = this->mouse_y + delta;
            auto new_y = cursor_y * this->total_h / this->scroll_h;
            bool update = false;

            if (new_y <= 0) {
                new_y = 0;
                update = new_y != this->current_y;
            }
            else if (new_y >= this->total_h) {
                new_y = this->total_h;
                update = new_y != this->current_y;
            }
            else if (new_y != this->current_y) {
                update = std::abs(new_y - current_y) >= this->line_height;
            }

            if (update) {
                _update_cursor_button_y_and_redraw(new_y);
            }
        }
    }
    else if (device_flags & MOUSE_FLAG_WHEEL) {
        // auto delta = device_flags & 0xff;
        if (device_flags & MOUSE_FLAG_WHEEL_NEGATIVE) {
            this->_scroll_down();
        }
        else {
            this->_scroll_up();
        }
    }
    else {
        Widget::rdp_input_mouse(device_flags, x, y);
    }
}

void WidgetVerticalScrollText::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (!this->has_scroll) {
        Widget::rdp_input_scancode(flags, scancode, event_time, keymap);
        return ;
    }

    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
        case Keymap::KEvent::LeftArrow:
        case Keymap::KEvent::UpArrow:
        case Keymap::KEvent::PgUp:
            this->_scroll_up();
            break;

        case Keymap::KEvent::RightArrow:
        case Keymap::KEvent::DownArrow:
        case Keymap::KEvent::PgDown:
            this->_scroll_down();
            break;

        default:
            Widget::rdp_input_scancode(flags, scancode, event_time, keymap);
            break;
    }
    REDEMPTION_DIAGNOSTIC_POP()
}

void WidgetVerticalScrollText::rdp_input_invalidate(Rect clip)
{
    auto const rect = this->get_rect();
    Rect const rect_intersect = clip.intersect(rect);

    if (!rect_intersect.isempty()) {
        auto opaque_rect = [this, &rect_intersect](int x, int y, int cx, int cy, RDPColor color){
            this->drawable.draw(
                RDPOpaqueRect(
                    Rect{checked_int{x}, checked_int{y}, checked_int{cx}, checked_int{cy}},
                    color
                ),
                rect_intersect,
                gdi::ColorCtx::depth24()
            );
        };

        opaque_rect(rect.x, rect.y, rect.cx, rect.cy, this->bg_color);

        if (this->has_scroll) {
            auto const bw = this->button_dim.w;
            auto const bh = this->button_dim.h;
            auto const rx = rect.x;
            auto const ry = rect.y;
            auto const rw = rect.cx;
            auto const rh = rect.cy;
            auto const sx = checked_cast<int16_t>(rx + rw - bw);
            auto const cy = checked_cast<int16_t>(this->cursor_button_y + ry);

            auto draw_button_borders = [this, sx, bw, rect_intersect](int16_t y, uint16_t bh){
                gdi_draw_border(drawable, fg_color, sx, y, bw, bh, 2, rect_intersect, gdi::ColorCtx::depth24());
            };

            auto draw_text_button = [&](
                FontCharView const* fc, ButtonType button_type, int16_t y, uint16_t bh, uint16_t dy
            ){
                bool const has_focus = (this->selected_button == button_type);
                auto const bg = has_focus ? this->focus_color : this->bg_color;
                if (has_focus) {
                    opaque_rect(sx + 2, y + 2, bw - 4, bh - 4, bg);
                }

                gdi::draw_text(
                    drawable,
                    sx + 2,
                    y + dy + 2,
                    this->line_height,
                    gdi::DrawTextPadding{
                        .top = 0,
                        .right = cx(),
                        .bottom = 0,
                        .left = 0,
                    },
                    {&fc, 1},
                    this->fg_color,
                    bg,
                    rect_intersect
                );
            };

            draw_text_button(this->icon_top, ButtonType::Top, ry, bh, 0);
            draw_text_button(this->icon_cursor, ButtonType::Cursor, cy,
                             this->cursor_button_h, (this->cursor_button_h - bh) / 2);
            draw_text_button(this->icon_bottom, ButtonType::Bottom,
                             checked_int{ry + rh - bh}, bh, 0);

            // left scroll border
            opaque_rect(sx,          ry + bh, 1, rh - bh * 2, this->fg_color);
            // right scroll border
            opaque_rect(sx + bw - 1, ry + bh, 1, rh - bh * 2, this->fg_color);

            // top button
            draw_button_borders(ry, bh);
            // cursor button
            draw_button_borders(cy, this->cursor_button_h);
            // bottom button
            draw_button_borders(checked_int{ry + rh - bh}, bh);
        }

        if (!this->has_scroll
         || (this->x() <= rect_intersect.x
          && rect_intersect.x <= this->x() + this->cx() - this->button_dim.w)
        ) {
            multiline_text.draw(drawable, {
                .x = x(),
                .y = checked_int{y() - current_y},
                .clip = rect_intersect,
                .fgcolor = fg_color,
                .bgcolor = bg_color,
                .draw_bg_rect = false,
            });
        }
    }
}
