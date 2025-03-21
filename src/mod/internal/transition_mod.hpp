/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/rail_mod_base.hpp"
#include "mod/internal/widget/label.hpp"

class TransitionMod : public RailInternalModBase
{
public:
    TransitionMod(
        chars_view message,
        gdi::GraphicApi & drawable,
        uint16_t width, uint16_t height,
        Rect const widget_rect, ClientExecute & rail_client_execute, Font const& font,
        Theme const& theme
    );

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_invalidate(Rect r) override;

    void acl_update(AclFieldMask const&/* acl_fields*/) override {}

private:
    WidgetText<127> ttmessage;
    gdi::GraphicApi & drawable;
    Rect widget_rect;
    uint16_t font_height;
    RDPColor fgcolor;
    RDPColor bgcolor;
    RDPColor border_color;
};
