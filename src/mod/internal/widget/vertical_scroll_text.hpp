/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/numerics/safe_conversions.hpp"
#include "mod/internal/widget/widget.hpp"
#include "gdi/text.hpp"


class Font;

class WidgetVerticalScrollText : public Widget
{
public:
    WidgetVerticalScrollText(
        gdi::GraphicApi& drawable, chars_view text,
        Color fg_color, Color bg_color, Color focus_color,
        Font const& font);

    void set_xy(int16_t x, int16_t y) override;

    struct DimensionContraints
    {
        struct Range
        {
            uint16_t min;
            uint16_t max;
        };

        // intermediate for initialization with integer or designated initializers or range:
        // update_dimension({5, ...});
        // update_dimension({{.min=5, .max=5, ...}});
        struct Length : Range
        {
            template<class T>
            constexpr Length(checked_int<T> n) noexcept
                : Length(static_cast<uint16_t>(n))
            {}

            constexpr Length(uint16_t n) noexcept : Range{n, n} {}
            constexpr Length(Range rng) noexcept : Range{rng} {}
        };


        struct ScrollStateBase
        {
            bool with_scroll;
            bool without_scroll;
        };

        // intermediate for initialization bool or designated initializers of pair:
        struct ScrollState : ScrollStateBase
        {
            constexpr ScrollState(bool enable) noexcept
                : ScrollStateBase{enable, enable}
            {}

            constexpr ScrollState(ScrollStateBase states) noexcept
                : ScrollStateBase{states}
            {}
        };


        struct PreferOptimalDimsBase
        {
            ScrollState horizontal;
            ScrollState vertical;
        };

        // intermediate for initialization bool or designated initializers of pair:
        // update_dimension({..., false});
        // update_dimension({..., {{.horizontal = false, .vertical = true}};});
        struct PreferOptimalDims : PreferOptimalDimsBase
        {
            constexpr PreferOptimalDims(bool prefer_optimal_dim) noexcept
                : PreferOptimalDimsBase{prefer_optimal_dim, prefer_optimal_dim}
            {}

            constexpr PreferOptimalDims(PreferOptimalDimsBase prefer_optimal_dim) noexcept
                : PreferOptimalDimsBase{prefer_optimal_dim}
            {}
        };

        Length width;
        Length height;
        PreferOptimalDims prefer_optimal_dimension = true;
    };

    void update_dimension(DimensionContraints contraints);

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_invalidate(Rect clip) override;

    void scroll_up();
    void scroll_down();

private:
    struct D;
    friend D;

    void _update_cursor_button_y_and_redraw(int new_y);
    bool _scroll_up();
    bool _scroll_down();

    Color const fg_color;
    Color const bg_color;
    Color const focus_color;

    uint16_t line_height;

    int16_t cursor_button_y = 0;

    int current_y = 0;
    int page_h = 0;
    int scroll_h = 0;
    int total_h = 0;

    int mouse_start_y = 0;
    int mouse_y = 0;

    Dimension const button_dim;
    uint16_t cursor_button_h;

    bool has_scroll = false;

    enum class ButtonType : unsigned char
    {
        None,

        Top,
        Cursor,
        Bottom,
    };

    ButtonType selected_button = ButtonType::None;

    gdi::MultiLineText multiline_text;
    FontCharView const* icon_top;
    FontCharView const* icon_cursor;
    FontCharView const* icon_bottom;
};
