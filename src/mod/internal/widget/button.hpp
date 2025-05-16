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
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#pragma once

#include "mod/internal/button_state.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/event_notifier.hpp"

class Font;
class Theme;

class WidgetButtonEvent : public Widget
{
public:
    static bool is_submit_event(Keymap const& keymap) noexcept;
    static bool is_submit_event(KbdFlags flag, uint16_t unicode) noexcept;

    enum class RedrawOnSubmit : uint8_t
    {
        No,
        Yes,
    };

    WidgetButtonEvent(
        gdi::GraphicApi & drawable, WidgetEventNotifier onsubmit,
        RedrawOnSubmit redraw_on_submit
    );

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) final;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) final;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) final;

    void trigger();

protected:
    bool is_pressed() noexcept;
    void reset_state() noexcept;

private:
    ButtonState::Redraw redraw_on_submit;
    ButtonState button_state;
    WidgetEventNotifier onsubmit;
};

class WidgetButton final : public WidgetButtonEvent
{
public:
    struct Colors
    {
        struct Box
        {
            Color fg;
            Color bg;
            Color border;
        };

        Box focus;
        Box blur;

        Box current_colors(bool has_focus) const noexcept
        {
            return has_focus ? focus : blur;
        }

        static Colors from_theme(Theme const& theme) noexcept;
        static Colors no_border_from_theme(Theme const& theme) noexcept;
    };

    WidgetButton(gdi::GraphicApi & drawable, Font const & font,
                 chars_view text, Colors colors, WidgetEventNotifier onsubmit);

    void rdp_input_invalidate(Rect clip) override;

private:
    struct D;
    friend D;

    Colors colors;
    WidgetText<64> button_text;
};
