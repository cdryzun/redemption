/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/button.hpp"
#include "gdi/draw_utils.hpp"
#include "gdi/text_metrics.hpp"
#include "utils/theme.hpp"
#include "core/font.hpp"
#include "keyboard/keymap.hpp"


struct WidgetButton::D
{
    static const int x_text = 5;
    static const int y_text = 2;
    static const int border_len = 2;
};

WidgetButton::Colors WidgetButton::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .focus = {
            .fg = theme.global.fgcolor,
            .bg = theme.global.focus_color,
            .border = theme.global.fgcolor,
        },
        .blur = {
            .fg = theme.global.fgcolor,
            .bg = theme.global.bgcolor,
            .border = theme.global.fgcolor,
        },
    };
}

WidgetButton::Colors WidgetButton::Colors::no_border_from_theme(const Theme& theme) noexcept
{
    return {
        .focus = {
            .fg = theme.global.focus_color,
            .bg = theme.global.bgcolor,
            .border = theme.global.bgcolor,
        },
        .blur = {
            .fg = theme.global.fgcolor,
            .bg = theme.global.bgcolor,
            .border = theme.global.bgcolor,
        },
    };
}

WidgetButton::WidgetButton(
    gdi::GraphicApi & drawable, Font const & font,
    chars_view text, Colors colors, WidgetEventNotifier onsubmit)
: Widget(drawable, Focusable::Yes)
, h_text(font.max_height())
, colors(colors)
, onsubmit(onsubmit)
, button_text(font, text)
{
    set_wh(
        checked_int{button_text.width() + (D::border_len + D::x_text) * 2},
        checked_int{h_text + (D::border_len + D::y_text) * 2}
    );
}

void WidgetButton::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(get_rect());

    if (rect_intersect.isempty()) [[unlikely]] {
        return;
    }

    auto const current_colors = colors.current_colors(has_focus);
    int const is_pressed = button_state.is_pressed();
    int d = (current_colors.border == current_colors.bg) ? 0 : D::border_len;
    int padx = D::x_text + D::border_len - d;
    int pady = D::y_text + D::border_len - d;

    auto rect_text = get_rect();
    rect_text.x += d;
    rect_text.y += d;
    rect_text.cx -= d * 2;
    rect_text.cy -= d * 2;

    gdi::draw_text(
        drawable,
        x() + d,
        y() + d,
        h_text,
        gdi::DrawTextPadding{
            .top = checked_int(pady + is_pressed),
            .right = checked_int(padx - is_pressed),
            .bottom = checked_int(pady - is_pressed),
            .left = checked_int(padx + is_pressed),
        },
        button_text.fcs(),
        current_colors.fg,
        current_colors.bg,
        rect_text.intersect(rect_intersect)
    );

    if (d) {
        gdi_draw_border(
            drawable, current_colors.border, get_rect(),
            D::border_len, rect_intersect,
            gdi::ColorCtx::depth24()
        );
    }
}

void WidgetButton::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    button_state.update(
        get_rect(), x, y, device_flags,
        onsubmit,
        // TODO
        [this](Rect rect){ rdp_input_invalidate(rect); }
    );
}

bool WidgetButton::is_submit_event(const Keymap& keymap) noexcept
{
    auto kevt = keymap.last_kevent();
    return kevt == Keymap::KEvent::Enter
        || (kevt == Keymap::KEvent::KeyDown
            && keymap.last_decoded_keys().uchars[0] == ' ');
}

bool WidgetButton::is_submit_event(KbdFlags flag, uint16_t unicode) noexcept
{
    return !bool(flag & KbdFlags::Release) && unicode == ' ';
}

void WidgetButton::rdp_input_scancode(KbdFlags /*flags*/, Scancode /*scancode*/, uint32_t /*event_time*/, Keymap const& keymap)
{
    if (is_submit_event(keymap)) {
        this->onsubmit();
    }
}

void WidgetButton::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (is_submit_event(flag, unicode)) {
        this->onsubmit();
    }
    else {
        Widget::rdp_input_unicode(flag, unicode);
    }
}
