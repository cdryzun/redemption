/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/front_api.hpp"
#include "mod/internal/widget_test_mod.hpp"
#include "utils/theme.hpp"


// Pimpl
struct WidgetTestMod::WidgetTestModPrivate
{
    WidgetTestModPrivate(
        uint16_t width, uint16_t height, gdi::GraphicApi & gd, EventContainer & /*events*/,
        FrontAPI & front, Font const& font, Theme const & theme)
    // : gd(gd)
    // , front(front)
    // , events_guard(events)
    // , screen(gd, width, height, font, theme)
    // , copy_paste(true)
    {
        (void)width;
        (void)height;
        (void)gd;
        (void)front;
        (void)font;
        (void)theme;
    }

    // gdi::GraphicApi & gd;
    // FrontAPI & front;
    // // EventRef timer;
    // // EventsGuard events_guard;
    // WidgetScreen screen;
    // CopyPaste copy_paste;
};

WidgetTestMod::WidgetTestMod(
    gdi::GraphicApi & gd,
    EventContainer & events,
    FrontAPI & front, uint16_t width, uint16_t height,
    ClientExecute & rail_client_execute, Font const & font,
    Theme const & theme, CopyPaste& copy_paste)
: RailInternalModBase(gd, width, height, rail_client_execute, font, theme, &copy_paste)
, d(std::make_unique<WidgetTestModPrivate>(width, height, gd, events, front, font, theme))
{
    (void)front.server_resize({width, height, BitsPerPixel{8}});
}

WidgetTestMod::~WidgetTestMod() = default;

void WidgetTestMod::rdp_input_invalidate(Rect clip)
{
    RailInternalModBase::rdp_input_invalidate(clip);
}

void WidgetTestMod::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    RailInternalModBase::rdp_input_mouse(device_flags, x, y);
}

void WidgetTestMod::rdp_input_scancode(
    KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (pressed_scancode(flags, scancode) == Scancode::Esc) {
        this->set_mod_signal(BACK_EVENT_STOP);
    }
    else {
        RailInternalModBase::rdp_input_scancode(flags, scancode, event_time, keymap);
    }
}

void WidgetTestMod::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    RailInternalModBase::rdp_input_unicode(flag, unicode);
}

void WidgetTestMod::rdp_input_synchronize(KeyLocks locks)
{
    RailInternalModBase::rdp_input_synchronize(locks);
}
