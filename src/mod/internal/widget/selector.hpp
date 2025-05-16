/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/composite.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/edit.hpp"
#include "mod/internal/widget/help_icon.hpp"
#include "mod/internal/widget/number_edit.hpp"
#include "mod/internal/widget/button.hpp"
#include "translation/translation.hpp"
#include "utils/monotonic_clock.hpp"

#include <array>

class Theme;
class Font;
class FontCharView;

class WidgetSelector final : public WidgetComposite
{
public:
    struct Texts
    {
        struct Headers
        {
            chars_view authorization;
            chars_view target;
            chars_view protocol;
        };

        Headers headers;
        chars_view device_name;
    };

    struct Events
    {
        WidgetEventNotifier onconnect;
        WidgetEventNotifier oncancel;

        WidgetEventNotifier onfilter;

        WidgetEventNotifier onfirst_page;
        WidgetEventNotifier onprev_page;
        WidgetEventNotifier oncurrent_page;
        WidgetEventNotifier onnext_page;
        WidgetEventNotifier onlast_page;

        WidgetEventNotifier onprev_page_on_grid;
        WidgetEventNotifier onnext_page_on_grid;

        WidgetEventNotifier onctrl_shift;
    };

    struct SelectedLine
    {
        chars_view authorization;
        chars_view target;

        explicit operator bool () const noexcept
        {
            return !authorization.empty();
        }
    };

    struct FilterTexts
    {
        WidgetEdit::Text authorization;
        WidgetEdit::Text target;
        WidgetEdit::Text protocol;
    };

    WidgetSelector(
        gdi::GraphicApi & drawable, Font const & font, CopyPaste & copy_paste,
        Theme const & theme, WidgetTooltipShower & tooltip_shower,
        Translator tr, Widget * extra_button, Rect rect,
        Texts texts, Events events
    );

    void move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height);

    uint16_t max_line_per_page() const;

    bool has_lines() const noexcept;

    struct Page
    {
        // elements splited with \x01 or \n
        chars_view authorizations;
        chars_view targets;
        chars_view protocols;
        unsigned current_page;
        unsigned number_of_page;
    };

    void set_page(Page page);

    SelectedLine selected_line() const noexcept;

    FilterTexts filter_texts() const noexcept;

    unsigned current_page() const noexcept;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

private:
    static const unsigned nb_columns = 3;

    struct D;

    void rearrange_grid();
    void move_and_resize_navigation_buttons();

    Font const& font() noexcept;

    using FontChars = array_view<FontCharView const*>;

    struct FgBgColors
    {
        Color fg;
        Color bg;
    };

    struct TooltipShower
    {
        TooltipShower(WidgetSelector & selector)
        : selector(selector)
        {}

        void show_tooltip(chars_view text, int x, int y, Rect const mouse_area);

    private:
        WidgetSelector & selector;
    };

    struct WidgetGrid final : Widget
    {
        using Widget::set_wh;

        struct Events
        {
            WidgetEventNotifier onsubmit;
            WidgetEventNotifier onprev_page;
            WidgetEventNotifier onnext_page;
        };

        WidgetGrid(
            gdi::GraphicApi & drawable, Theme const& theme,
            TooltipShower tooltip_shower,
            std::array<uint16_t, nb_columns> columns_min_width,
            Events events
        );

        std::size_t count_line() const noexcept { return lines.size(); }

        void set_priority_column_index(int i) noexcept { this->priority_column_index = i; }

        void rdp_input_invalidate(Rect clip) override;

        void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

        void rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y) override;

        void focus() override;
        void blur() override;

        bool set_devices(
            Font const& font,
            Translator tr,
            chars_view authorizations,
            chars_view targets,
            chars_view protocols,
            unsigned max_line
        );

        SelectedLine get_selected_line() const noexcept;

        int column_width(unsigned i) const noexcept;

        std::array<bool, WidgetSelector::nb_columns> arrange();

    private:
        struct Line
        {
            chars_view authorization_text;
            chars_view target_text;
            std::array<FontChars, nb_columns> fcs;
            std::array<uint16_t, nb_columns> widths;
        };

        struct Colors
        {
            std::array<FgBgColors, 2> lines;
            FgBgColors focus;
            FgBgColors selected;
        };

        void init_lines(
            Font const & font,
            chars_view authorizations,
            chars_view targets,
            chars_view protocols,
            unsigned max_line
        );

        void set_selected_line(int new_selected_line);

        void draw_line_at(int i, FgBgColors colors);

        void draw_line(
            Line const& line, FgBgColors colors,
            int py, uint16_t h_line, Rect clip
        );

        MonotonicTimePoint last_tick {};
        int priority_column_index = -1;
        int selected_line = -1;

        std::array<uint16_t, nb_columns> columns_min_width;
        std::array<uint16_t, nb_columns> columns_width {};
        std::array<uint16_t, nb_columns> optimal_columns_width {};

        uint16_t h_line = 0;
        Colors colors;

        array_view<Line> lines;
        FontChars empty_message;
        Events events;
        TooltipShower tooltip_shower;
        std::pmr::monotonic_buffer_resource mbr;
    };

    struct WidgetHeaders final : Widget
    {
        using Colors = FgBgColors;

        using Widget::set_wh;

        WidgetHeaders(
            gdi::GraphicApi & drawable, Font const & font, Texts::Headers headers,
            Colors colors
        );

        void rdp_input_invalidate(Rect clip) override;

        Colors colors;
        std::array<uint16_t, nb_columns> widths {};
        std::array<WidgetText<32>, nb_columns> labels;
    };

    struct WidgetExpansionButton final : WidgetButtonEvent
    {
        struct Colors
        {
            Color bg;
            Color focus_bg;
            Color border;
        };

        WidgetExpansionButton(
            gdi::GraphicApi & drawable, Theme const & theme,
            uint16_t width_height, WidgetEventNotifier onsubmit
        );

        void rdp_input_invalidate(Rect clip) override;

        using WidgetButtonEvent::reset_state;

        bool inserted = false;

    private:
        Colors colors;
    };


    Widget * extra_button;
    bool less_than_800;
    bool has_devices = false;
    int nb_max_line = 0;

    WidgetTooltipShower & tooltip_shower_parent;

    WidgetHelpIcon target_helpicon;

    WidgetEventNotifier oncancel;
    WidgetEventNotifier onctrl_shift;

    WidgetLabel device_label;

    WidgetHeaders header_labels;

    std::array<WidgetExpansionButton, nb_columns - 1> column_expansion_buttons;

    std::array<WidgetEdit, nb_columns> edit_filters;

    WidgetGrid grid;

    WidgetButton first_page;
    WidgetButton prev_page;
    WidgetNumberEdit current_page_edit;
    WidgetLabel number_page;
    WidgetButton next_page;
    WidgetButton last_page;
    WidgetButton logout;
    WidgetButton apply;
    WidgetButton connect;

    Translator tr;
};
