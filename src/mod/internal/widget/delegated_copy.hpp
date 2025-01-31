/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Product name: redemption, a FLOSS RDP proxy
Copyright (C) Wallix 2021
Author(s): Proxies Team
*/

#pragma once

#include "mod/internal/button_state.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/event_notifier.hpp"


class Font;
class Theme;

class WidgetDelegatedCopy : public Widget
{
public:
    // TODO WidgetButton::Colors
    struct Colors
    {
        Color fg;
        Color bg;
        Color active_bg;

        static Colors from_theme(Theme const& theme) noexcept;
    };

    WidgetDelegatedCopy(
        gdi::GraphicApi & drawable, WidgetEventNotifier onsubmit,
        Colors colors, Font const & font);

    void rdp_input_invalidate(Rect clip) override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void focus(int reason) override;
    void blur() override;

private:
    ButtonState button_state;
    Colors colors;
    WidgetEventNotifier onsubmit;
};
