/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/colors.hpp"
#include "mod/internal/widget/widget.hpp"

class SubRegion;

void fill_region(gdi::GraphicApi & drawable, const SubRegion & region, Widget::Color bg_color);


class CompositeArray
{
public:
    CompositeArray();
    ~CompositeArray();

    void add(Widget & w);
    void remove(Widget const & w);

    void clear();

    Widget** begin();
    Widget** end();

private:
    void dealloc_table();

    int count = 0;
    int capacity;
    Widget ** p;
    Widget * fixed_table[16];
};  // class CompositeArray


class WidgetComposite : public Widget
{
    Widget * pressed = nullptr;

    Color bg_color;

    using FocusIndex = unsigned;
    static const FocusIndex invalid_focus_index = ~0u;

    unsigned current_focus_index = invalid_focus_index;

    CompositeArray widgets;

public:
    enum class HasFocus : bool
    {
        No,
        Yes,
    };

    enum class Redraw : bool
    {
        No,
        Yes,
    };

    WidgetComposite(gdi::GraphicApi & drawable, Focusable focusable, Color bg_color);

    ~WidgetComposite() override;

    void add_widget(Widget & w, HasFocus has_focus = HasFocus::No);
    void remove_widget(Widget & w);
    void clear_widget();

    void set_widget_focus(Widget & new_focused, Redraw redraw);

    void focus() override;
    void blur() override;

    void move_xy(int16_t x, int16_t y) override;

    NextFocusResult next_focus(FocusDirection dir, FocusStrategy strategy) final;

    Widget * widget_at_pos(int16_t x, int16_t y) override;

    void rdp_input_invalidate(Rect clip) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void init_focus() override;

    Color get_bg_color() const
    {
        return this->bg_color;
    }

protected:
    void invalidate_children(Rect clip);

    void draw_inner_free(Rect clip, Color bg_color);

    void move_children_xy(int16_t x, int16_t y);

    Widget * current_focus();

    FocusIndex widgets_len() /*const*/;
    FocusIndex widget_index_at_pos(int16_t x, int16_t y) /*const*/;
};
