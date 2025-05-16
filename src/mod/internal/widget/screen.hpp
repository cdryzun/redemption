/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/composite.hpp"
#include "gdi/text.hpp"


class WidgetTooltip;
class Theme;
class Font;

class WidgetScreen final : public WidgetComposite, public WidgetTooltipShower
{
public:
    WidgetScreen(gdi::GraphicApi & drawable, uint16_t width, uint16_t height,
                 Font const & font, Theme const& theme);

    void show_tooltip(chars_view text, int x, int y,
                      Rect preferred_display_rect,
                      Rect mouse_area) override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void allow_mouse_pointer_change(bool allow);

    void redo_mouse_pointer_change(int x, int y);

private:
    class WidgetTooltip final : public Widget
    {
    public:
        struct Colors
        {
            Color fg;
            Color bg;
            Color border;

            static Colors from_theme(Theme const& theme) noexcept;
        };

        WidgetTooltip(gdi::GraphicApi & drawable, Colors colors);

        bool has_text() const noexcept;

        void clear_text() noexcept;

        void set_text(Font const & font, unsigned max_width, chars_view text);

        void rdp_input_invalidate(Rect clip) override;

    private:
        struct D;
        friend D;

        Colors colors;
        gdi::MultiLineText desc;
    };

    Rect tooltip_mouse_area;
    Widget * current_over;
    Font const & font;
    bool allow_mouse_pointer_change_ = true;
    WidgetTooltip tooltip;
};
