/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/composite.hpp"
#include "keyboard/keymap.hpp"
#include "utils/out_param.hpp"
#include "utils/region.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "gdi/graphic_api.hpp"


void fill_region(gdi::GraphicApi & drawable, const SubRegion & region, Widget::Color bg_color)
{
    for (Rect const & rect : region.rects) {
        drawable.draw(RDPOpaqueRect(rect, bg_color), rect, gdi::ColorCtx::depth24());
    }
}


CompositeArray::CompositeArray()
: capacity(checked_int(std::size(fixed_table)))
, p(fixed_table)
{}

CompositeArray::~CompositeArray()
{
    dealloc_table();
}

void CompositeArray::dealloc_table()
{
    if (p != fixed_table) {
        delete[] p;
    }
}

Widget** CompositeArray::begin()
{
    return p;
}

Widget** CompositeArray::end()
{
    return p + count;
}

void CompositeArray::add(Widget & w)
{
    if (REDEMPTION_UNLIKELY(count == capacity)) {
        const std::size_t new_size = std::size_t(capacity) * 4;
        auto* new_array = new Widget*[new_size];
        std::copy(p, p + count, new_array);
        dealloc_table();
        p = new_array;
        capacity = checked_int(new_size);
    }

    p[count] = &w;
    count++;
}

void CompositeArray::remove(Widget const & w)
{
    for (int i = 0; i < count; ++i) {
        if (p[i] == &w) {
            std::copy(p + i + 1, p + count, p + i);
            --count;
            break;
        }
    }
}

void CompositeArray::clear()
{
    this->count = 0;
}


WidgetComposite::WidgetComposite(gdi::GraphicApi & drawable, Focusable focusable, Color bg_color)
    : Widget(drawable, focusable)
    , bg_color(bg_color)
{}

WidgetComposite::~WidgetComposite() = default;

void WidgetComposite::set_widget_focus(Widget & new_focused, Redraw redraw)
{
    auto * w = current_focus();
    if (&new_focused != w) {
        if (w) {
            if (redraw == Redraw::Yes) {
                w->blur();
            }
            else {
                w->has_focus = false;
            }
        }

        for (auto & curr : widgets) {
            if (curr == &new_focused) {
                current_focus_index = checked_int(&curr - widgets.begin());
                if (redraw == Redraw::Yes) {
                    curr->focus();
                }
                else {
                    curr->has_focus = true;
                }
                return;
            }
        }
    }
}

void WidgetComposite::focus()
{
    this->has_focus = true;
    if (auto * w = current_focus()) {
        w->focus();
    }
}

void WidgetComposite::blur()
{
    this->has_focus = false;
    if (auto * w = current_focus()) {
        w->blur();
    }
}

Widget * WidgetComposite::current_focus()
{
    return current_focus_index < widgets_len()
        ? *(widgets.begin() + current_focus_index)
        : nullptr;
}

WidgetComposite::FocusIndex WidgetComposite::widgets_len()
{
    return checked_int(widgets.end() - widgets.begin());
}

void WidgetComposite::init_focus()
{
    has_focus = true;
    if (auto * w = current_focus()) {
        w->init_focus();
    }
}

void WidgetComposite::add_widget(Widget & w, HasFocus has_focus)
{
    widgets.add(w);

    if (w.focusable == Focusable::Yes
     && (has_focus == HasFocus::Yes || current_focus_index == invalid_focus_index)
    ) {
        current_focus_index = widgets_len() - 1;
    }
}

void WidgetComposite::remove_widget(Widget & w)
{
    if (current_focus() == &w) {
        current_focus_index = invalid_focus_index;
    }
    if (pressed == &w) {
        pressed = nullptr;
    }
    widgets.remove(w);
}

void WidgetComposite::clear_widget()
{
    widgets.clear();
    current_focus_index = invalid_focus_index;
}

void WidgetComposite::invalidate_children(Rect clip)
{
    for (Widget* w : this->widgets) {
        Rect rect = clip.intersect(w->get_rect());
        if (!rect.isempty()) {
            w->rdp_input_invalidate(rect);
        }
    }
}

void WidgetComposite::draw_inner_free(Rect clip, Color bg_color)
{
    SubRegion region;
    region.add_rect(clip.intersect(this->get_rect()));

    for (Widget* w : this->widgets) {
        Rect rect = clip.intersect(w->get_rect());
        if (!rect.isempty()) {
            region.subtract_rect(rect);
        }
    }

    ::fill_region(this->drawable, region, bg_color);
}

void WidgetComposite::move_xy(int16_t x, int16_t y)
{
    this->set_xy(this->x() + x, this->y() + y);

    this->move_children_xy(x, y);
}

void WidgetComposite::move_children_xy(int16_t x, int16_t y)
{
    for (Widget* w : this->widgets) {
        w->move_xy(x, y);
    }
}

Widget::NextFocusResult WidgetComposite::next_focus(FocusDirection dir, FocusStrategy strategy)
{
    auto first = widgets.begin();
    auto last = widgets.end();

    auto result = NextFocusResult::Unfocusable;

    if (current_focus() && strategy == Widget::FocusStrategy::Next)
    {
        auto * w = current_focus();
        switch (w->next_focus(dir, strategy))
        {
            case NextFocusResult::Unfocusable:
                break;
            case NextFocusResult::Focusable:
                result = NextFocusResult::Focusable;
                break;
            case NextFocusResult::Focused:
                return NextFocusResult::Focused;
        }

        if (current_focus_index < widgets_len())
        {
            auto idx = (dir == FocusDirection::Forward)
                ? current_focus_index + 1
                : current_focus_index;
            first += idx;
        }
    }
    else if (dir == FocusDirection::Backward)
    {
        first = widgets.end();
    }

    auto inc = 1;
    auto adjust = 0;
    if (dir == FocusDirection::Backward)
    {
        inc = -1;
        adjust = -1;
        last = widgets.begin();
    }

    for (; first != last; first += inc)
    {
        Widget* w = *(first + adjust);
        if (w->focusable == Widget::Focusable::Yes)
        {
            switch (w->next_focus(dir, Widget::FocusStrategy::Restart))
            {
                case NextFocusResult::Unfocusable:
                    break;
                case NextFocusResult::Focusable:
                case NextFocusResult::Focused: {
                    auto new_idx = checked_cast<FocusIndex>(first + adjust - widgets.begin());
                    if (current_focus_index != new_idx)
                    {
                        if (auto * old_w = current_focus())
                        {
                            old_w->blur();
                        }
                        current_focus_index = new_idx;
                        w->focus();
                    }
                    return NextFocusResult::Focused;
                }
            }
        }
    }

    return result;
}

WidgetComposite::FocusIndex WidgetComposite::widget_index_at_pos(int16_t x, int16_t y)
{
    if (!this->get_rect().contains_pt(x, y)) {
        return invalid_focus_index;
    }

    if (auto * w = current_focus()) {
        if (w->get_rect().contains_pt(x, y)) {
            return current_focus_index;
        }
    }

    Widget** first = this->widgets.begin();
    Widget** last = this->widgets.end();

    // Foreground widget is the last in the list.
    if (first != last) {
        do {
            --last;
            Widget* w = *last;
            if (w->get_rect().contains_pt(x, y)) {
                return checked_int{last - first};
            }
        } while (first != last);
    }

    return invalid_focus_index;
}

Widget * WidgetComposite::widget_at_pos(int16_t x, int16_t y)
{
    auto idx = widget_index_at_pos(x, y);
    return (idx < widgets_len()) ? this->widgets.begin()[idx] : nullptr;
}

void WidgetComposite::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect());

    if (!rect_intersect.isempty()) {
        this->draw_inner_free(rect_intersect, this->get_bg_color());
        this->invalidate_children(rect_intersect);
    }
}

void WidgetComposite::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    auto next_focus_impl = [this](FocusDirection dir){
        if (NextFocusResult::Focused != this->next_focus(dir, FocusStrategy::Next)) {
            (void)this->next_focus(dir, FocusStrategy::Restart);
        }
    };

    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
        case Keymap::KEvent::Tab:
            next_focus_impl(FocusDirection::Forward);
            break;

        case Keymap::KEvent::BackTab:
            next_focus_impl(FocusDirection::Backward);
            break;

        default:
            if (auto * w = current_focus()) {
                w->rdp_input_scancode(flags, scancode, event_time, keymap);
            }
            break;
    }
    REDEMPTION_DIAGNOSTIC_POP()
}

void WidgetComposite::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (auto * w = current_focus()) {
        w->rdp_input_unicode(flag, unicode);
    }
}

void WidgetComposite::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (device_flags & (MOUSE_FLAG_WHEEL | MOUSE_FLAG_HWHEEL)) {
        if (auto * w = current_focus()) {
            w->rdp_input_mouse(device_flags, x, y);
        }
        return;
    }

    auto idx = widget_index_at_pos(x, y);
    Widget * w = (idx < widgets_len()) ? this->widgets.begin()[idx] : nullptr;

    // Mouse clic release
    // w could be null if mouse is located at an empty space
    if (device_flags == MOUSE_FLAG_BUTTON1) {
        if (this->pressed && w != this->pressed) {
            this->pressed->rdp_input_mouse(device_flags, x, y);
        }
        this->pressed = nullptr;
    }

    if (w) {
        // get focus when mouse clic
        auto activate_focus
            = (device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN))
           && (w->focusable == Focusable::Yes);
        if (activate_focus) {
            pressed = w;
            if (auto * curr = current_focus()) {
                curr->blur();
            }
            current_focus_index = idx;
        }

        w->rdp_input_mouse(device_flags, x, y);

        // when not done in rdp_input_mouse
        if (activate_focus) {
            has_focus = true;
            if (activate_focus && !w->has_focus) {
                w->focus();
            }
        }
    }

    if (device_flags == MOUSE_FLAG_MOVE && this->pressed) {
        this->pressed->rdp_input_mouse(device_flags, x, y);
    }
}
