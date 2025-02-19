/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/callback.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"
#include "utils/rect.hpp"


struct ButtonState
{
    enum class State
    {
        Normal,
        Pressed,
    };

    enum class RedrawOnSubmit
    {
        No,
        Yes,
    };

    ButtonState() noexcept = default;
    ButtonState(State state) noexcept : _is_pressed(state == State::Pressed) {}

    State state() const noexcept
    {
        return _is_pressed ? State::Pressed : State::Normal;
    }

    bool is_pressed() const noexcept
    {
        return _is_pressed;
    }

    void pressed(bool is_pressed) noexcept
    {
        _is_pressed = is_pressed;
    }

    void toggle() noexcept
    {
        _is_pressed = !_is_pressed;
    }

    bool is_toggable(uint16_t device_flags) noexcept
    {
        constexpr auto down = MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN;
        constexpr auto up = MOUSE_FLAG_BUTTON1;
        auto flags = _is_pressed ? up : down;
        return device_flags == flags;

    }

    template<class OnSubmit, class OnChange>
    void update(
        Rect area, uint16_t x, uint16_t y, uint16_t device_flags,
        RedrawOnSubmit redraw_on_submit, OnSubmit&& onsubmit, OnChange&& onchange)
    {
        auto redraw = RedrawOnSubmit::Yes;
        if (is_toggable(device_flags) && area.contains_pt(checked_int(x), checked_int(y))) {
            toggle();
            if (device_flags == MOUSE_FLAG_BUTTON1) {
                onsubmit();
                redraw = redraw_on_submit;
            }
        }
        else if (device_flags == MOUSE_FLAG_BUTTON1) {
            pressed(false);
        }
        else {
            return;
        }

        if (redraw == RedrawOnSubmit::Yes) {
            onchange(area);
        }
    }

private:
    bool _is_pressed = false;
};
