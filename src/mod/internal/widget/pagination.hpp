/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/number_edit.hpp"

/**
 * Hoizontal pagination bar:
 *
 *  <<   <   n / NNN   >   >>
 */
struct WidgetPagination final : Widget
{
    using UpdatePageEvent = BasicFunction<void(uint32_t new_page)>;

    struct Colors
    {
        Color fg;
        Color bg;
        Color focus_fg;
        WidgetEdit::Colors edit;

        static Colors from_theme(Theme const& theme) noexcept;
    };

    struct Data
    {
        /// Value in [0..=total_page] with 0 for unspecified page.
        uint32_t current_page;
        uint32_t total_page;
    };

    enum class Cycle : bool
    {
        No,
        Yes,
    };

    enum class RedrawAfterEvent : bool
    {
        No,
        Yes,
    };

    enum class TriggerUpdatePageEvent : bool
    {
        No,
        Yes,
    };

    WidgetPagination(
        gdi::GraphicApi & gd,
        Font const & font,
        Colors colors,
        RedrawAfterEvent redraw_after_event,
        UpdatePageEvent update_page_event
    ) noexcept;

    /// Current page in [1..=total_page] or 0 for unspecified page.
    uint32_t current_page() const noexcept
    {
        return m_current;
    }

    uint32_t total_page() const noexcept
    {
        return m_total;
    }

    /// \return true when page is valid is not the current page.
    bool is_new_page(uint32_t page) const noexcept;

    bool set_page(uint32_t page, TriggerUpdatePageEvent trigger_event);
    bool prev_page(Cycle enable_cycle, TriggerUpdatePageEvent trigger_event);
    bool next_page(Cycle enable_cycle, TriggerUpdatePageEvent trigger_event);

    void update(Data data);

    NextFocusResult next_focus(FocusDirection dir, FocusStrategy strategy) override;

    void init_focus() override;
    void focus() override;
    void blur() override;

    enum class FocusElement : uint8_t
    {
        None,
        First,
        Prev,
        Edit,
        Next,
        Last,
    };

    void set_focus_elem(FocusElement elem);
    FocusElement get_focus_elem() const noexcept;

    bool is_on_edit(uint16_t x) const noexcept;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

    void rdp_input_scancode(
        KbdFlags flags, Scancode scancode,
        uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void submit();

    void rdp_input_invalidate(Rect r) override;

private:
    struct D;
    friend D;

    using FontCharPtr = FontCharView const*;

    enum class Item : uint8_t;

    int16_t m_offsets[6];
    // (x1,x2) of butttons
    int16_t m_bbox_offsets[4 * 2];
    uint16_t m_line_h;
    uint16_t m_label_w = 0;
    uint8_t m_label_total_len;
    Item m_focus_item {};
    Item m_pressed_item {};
    bool m_redraw_after_event;
    uint32_t m_current = 0;
    uint32_t m_total = 0;
    FontCharPtr m_chars[
        // buttons
        4
        // label
        + 2 // prefix "/ "
        + 10 // len of u32.max().to_str()
    ];
    UpdatePageEvent m_update_page_event;
    Colors m_colors;
    WidgetNumberEdit m_edit;
};
