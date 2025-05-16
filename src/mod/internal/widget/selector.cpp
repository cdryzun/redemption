/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "gdi/draw_utils.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text.hpp"
#include "keyboard/keymap.hpp"
#include "mod/internal/widget/selector.hpp"
#include "translation/translation.hpp"
#include "translation/trkeys.hpp"
#include "utils/sugar/buf_maker.hpp"
#include "utils/sugar/byte_copy.hpp"
#include "utils/theme.hpp"

#include <numeric>
#include <memory_resource>


struct WidgetSelector::D
{
    static constexpr uint16_t HORIZONTAL_MARGIN = 15;
    static constexpr uint16_t VERTICAL_MARGIN = 10;
    static constexpr uint16_t TEXT_MARGIN = 20;
    static constexpr uint16_t FILTER_VERTICAL_SEPARATOR = 4;
    static constexpr uint16_t FILTER_HORIZONTAL_SEPARATOR = 4;
    static constexpr uint16_t NAV_SEPARATOR = 15;
    static constexpr uint16_t HELP_ICON_SEPARATOR = 9;
    static constexpr uint16_t EXPANSION_BUTTON_SEPARATOR = 5;

    static constexpr uint16_t GRID_LABEL_X_PADDING = 5;
    static constexpr uint16_t GRID_LABEL_Y_PADDING = 3;
    static constexpr int NB_MAX_LINE = 1024;

    static constexpr std::array<int, 3> WEIGHTS { 20, 70, 10 };

    static int grid_line_height(uint16_t font_height)
    {
        return font_height + GRID_LABEL_Y_PADDING * 2;
    }

    static int grid_line_height(Font const& font)
    {
        return grid_line_height(font.max_height());
    }

    template<class T>
    static writable_array_view<T>
    allocate_view(std::pmr::monotonic_buffer_resource & mbr, std::size_t n)
    {
        auto * p = static_cast<T*>(mbr.allocate(n * sizeof(T), alignof(T)));
        return writable_array_view{p, n};
    }

    static uint16_t expansion_button_width(Font const& font)
    {
        int h = font.max_height();
        return checked_int{(h + 1) / 2 - 1};
    }

    static void expansion_event(WidgetSelector & selector, unsigned i)
    {
        selector.column_expansion_buttons[i].has_focus = false;
        selector.column_expansion_buttons[i].reset_state();
        selector.grid.has_focus = true;
        selector.grid.set_priority_column_index(static_cast<int>(i));
        selector.rearrange_grid();
        redraw(selector, selector.grid.ebottom());
    }

    static void redraw(WidgetSelector & selector, int16_t bottom)
    {
        int y = std::min(selector.header_labels.y(), selector.target_helpicon.y());
        selector.rdp_input_invalidate(Rect{
            selector.header_labels.x(),
            checked_int(y),
            selector.cx(),
            checked_int(bottom - y),
        });
    }
};


WidgetSelector::WidgetGrid::WidgetGrid(
    gdi::GraphicApi & drawable, const Theme& theme,
    TooltipShower tooltip_shower,
    std::array<uint16_t, nb_columns> columns_min_width,
    Events events
)
    : Widget(drawable, Focusable::No)
    , columns_min_width(columns_min_width)
    , colors{
        // yes, color are inverted...
        .lines = {{
            {
                .fg = theme.selector_line2.fgcolor,
                .bg = theme.selector_line2.bgcolor,
            },
            {
                .fg = theme.selector_line1.fgcolor,
                .bg = theme.selector_line1.bgcolor,
            },
        }},
        .focus = {
            .fg = theme.selector_focus.fgcolor,
            .bg = theme.selector_focus.bgcolor,
        },
        .selected = {
            .fg = theme.selector_selected.fgcolor,
            .bg = theme.selector_selected.bgcolor,
        },
    }
    , events(events)
    , tooltip_shower(tooltip_shower)
{
}

bool WidgetSelector::WidgetGrid::set_devices(
    Font const & font,
    Translator tr,
    chars_view authorizations,
    chars_view targets,
    chars_view protocols,
    unsigned max_line
)
{
    h_line = font.max_height();

    set_focusable(Focusable::No);
    selected_line = -1;

    init_lines(font, authorizations, targets, protocols, max_line);

    if (!lines.empty()) {
        set_focusable(Focusable::Yes);

        selected_line = 0;
        optimal_columns_width.fill(0);

        for (auto const & line : lines) {
            for (unsigned i = 0; i < nb_columns; ++i) {
                optimal_columns_width[i] = std::max(optimal_columns_width[i], line.widths[i]);
            }
        }

        return true;
    }
    else {
        /*
         * Set a "not found" message
         */

        auto lines = D::allocate_view<Line>(mbr, 1);
        lines[0] = {};

        auto text = tr(trkeys::no_results);
        lines[0].fcs[1] = init_widget_text(
            D::allocate_view<FontCharView const*>(mbr, text.size()),
            OutParam{lines[0].widths[1]}, font, text
        );

        this->lines = lines;

        return false;
    }
}

WidgetSelector::SelectedLine WidgetSelector::WidgetGrid::get_selected_line() const noexcept
{
    if (selected_line < 0) {
        return {};
    }

    auto line = lines[checked_int(selected_line)];
    return {
        .authorization = line.authorization_text,
        .target = line.target_text,
    };
}

void WidgetSelector::WidgetGrid::init_lines(
    Font const & font,
    chars_view authorizations,
    chars_view targets,
    chars_view protocols,
    unsigned max_line
)
{
    auto is_sep = [](char c) { return c == '\x01' || c == '\n'; };

    /*
     * Count max rows
     */

    unsigned rows = 1;
    for (char c : authorizations) {
        if (is_sep(c)) {
            ++rows;
            if (rows == max_line) {
                break;
            }
        }
    }
    ++rows;
    rows = std::min(rows, max_line);

    /*
     * Reset Lines
     */

    mbr.release();
    this->lines = {};

    /*
     * Init Lines
     */

    auto lines = D::allocate_view<Line>(mbr, rows);

    auto next_item = [is_sep](chars_view & list) {
        for (char const& c : list) {
            if (is_sep(c)) {
                auto item = list.before(&c);
                list = list.after(&c);
                return item;
            }
        }
        return std::exchange(list, chars_view());
    };

    unsigned index = 0;
    for (; index < rows; ++index) {
        auto authorization = next_item(authorizations);
        if (authorization.empty()) {
            break;
        }
        auto target = next_item(targets);
        if (target.empty()) {
            break;
        }
        auto protocol = next_item(protocols);
        if (protocol.empty()) {
            break;
        }

        auto & line = lines[index];

        /*
         * Populate Line
         */

        auto authorization_text = D::allocate_view<char>(mbr, authorization.size());
        byte_copy(authorization_text.data(), authorization);
        line.authorization_text = authorization_text;

        auto target_text = D::allocate_view<char>(mbr, target.size());
        byte_copy(target_text.data(), target);
        line.target_text = target_text;

        chars_view const texts[] { authorization, target, protocol, };

        for (unsigned i = 0; i < nb_columns; ++i) {
            line.fcs[i] = init_widget_text(
                D::allocate_view<FontCharView const*>(mbr, texts[i].size()),
                OutParam{line.widths[i]}, font, texts[i]
            );
        }
    }

    this->lines = lines.first(index);
}

int WidgetSelector::WidgetGrid::column_width(unsigned i) const noexcept
{
    return columns_width[i] + D::GRID_LABEL_X_PADDING * 2;
}

std::array<bool, WidgetSelector::nb_columns> WidgetSelector::WidgetGrid::arrange()
{
    int const columns_cx = cx() - D::GRID_LABEL_X_PADDING * 2 * int{nb_columns};

    auto sum = [](auto & cont){ return std::accumulate(cont.begin(), cont.end(), 0); };

    /*
     * Apply weight per column
     */

    bool is_optimal = true;
    int const total_weight = sum(D::WEIGHTS);
    for (unsigned i = 0; i < nb_columns; ++i) {
        columns_width[i] = checked_int(columns_cx * D::WEIGHTS[i] / total_weight);
        if (columns_width[i] < optimal_columns_width[i]) {
            is_optimal = false;
        }
    }
    columns_width.back() += columns_cx - sum(columns_width);

    std::array<bool, nb_columns> is_optimal_columns;

    /*
     * Check if all columns are optimal
     */

    if (is_optimal) {
        is_optimal_columns.fill(true);
    }

    /*
     * else: Resize columns
     */

    else if (
        int const total_optimal_width = sum(optimal_columns_width);
        total_optimal_width <= columns_cx
    ) {
        int const remaining_cx = columns_cx - total_optimal_width;

        for (unsigned i = 0; i < nb_columns; ++i) {
            columns_width[i] = checked_int(
                optimal_columns_width[i] + remaining_cx * D::WEIGHTS[i] / total_weight
            );
        }

        columns_width.back() += columns_cx - sum(columns_width);
        is_optimal_columns.fill(true);
    }

    /*
     * else: Set column min and push remaining pixel
     */

    else {
        columns_width = columns_min_width;
        int const min_cx = sum(columns_width);

        if (min_cx < columns_cx) {
            uint16_t const remaining_cx = checked_int(columns_cx - min_cx);

            unsigned const priority
                = (priority_column_index == -1)
                ? (optimal_columns_width[0] < optimal_columns_width[1] ? 1u : 0u)
                : checked_cast<unsigned>(priority_column_index);

            if (optimal_columns_width[priority] >= columns_width[priority] + remaining_cx) {
                columns_width[priority] += remaining_cx;
            }
            else {
                /*
                 * Dispatch remaining pixel
                 */

                columns_width[priority] = std::min(
                    checked_cast<uint16_t>(columns_width[priority] + remaining_cx),
                    optimal_columns_width[priority]
                );

                int partial_weight = 0;
                int unused_width = columns_cx;
                for (unsigned i = 0; i < nb_columns; ++i) {
                    if (columns_width[i] < optimal_columns_width[i]) {
                        partial_weight += optimal_columns_width[i];
                    }
                    unused_width -= columns_width[i];
                }

                for (unsigned i = 0; i < nb_columns; ++i) {
                    if (columns_width[i] < optimal_columns_width[i]) {
                        unused_width += columns_width[i];

                        columns_width[i] = checked_int(std::min(
                            columns_width[i] + columns_cx * partial_weight / total_weight,
                            std::min(unused_width, int{optimal_columns_width[i]})
                        ));

                        unused_width -= columns_width[i];
                    }
                }

                columns_width.back() += unused_width;
            }
        }

        auto is_resizable = [&](unsigned ignore_index){
            for (unsigned i = 0; i < nb_columns; ++i) {
                if (i != ignore_index && columns_min_width[i] != columns_width[i]) {
                    return true;
                }
            }
            return false;
        };

        for (unsigned i = 0; i < nb_columns; ++i) {
            is_optimal_columns[i] = (columns_width[i] >= optimal_columns_width[i])
                                 || !is_resizable(i);
        }
    }

    return is_optimal_columns;
}

void WidgetSelector::WidgetGrid::rdp_input_invalidate(Rect clip)
{
    clip = clip.intersect(get_rect());

    int h = D::grid_line_height(h_line);
    int py = y();
    int iline = (clip.y - py) / h;
    auto offset = std::min(lines.size(), static_cast<std::size_t>(iline));

    for (auto const& line : lines.from_offset(offset)) {
        auto line_colors
            = (iline == selected_line)
            ? (has_focus ? colors.focus : colors.selected)
            : colors.lines[checked_int(iline % 2)];
        ++iline;

        draw_line(line, line_colors, py, h_line, clip);

        py += h;

        if (py >= clip.ebottom()) {
            break;
        }
    }
}

void WidgetSelector::WidgetGrid::set_selected_line(int new_selected_line)
{
    if (new_selected_line != selected_line) {
        draw_line_at(selected_line, colors.lines[checked_int(selected_line % 2)]);
        draw_line_at(new_selected_line, colors.focus);
        selected_line = new_selected_line;
    }
}

void WidgetSelector::WidgetGrid::draw_line_at(int i, FgBgColors colors)
{
    int py = y() + D::grid_line_height(h_line) * i;
    draw_line(lines[checked_int(i)], colors, py, h_line, get_rect());
}

void WidgetSelector::WidgetGrid::draw_line(
    const Line& line, FgBgColors colors, int py, uint16_t h_line, Rect clip)
{
    /*
     * Draw background line
     */
    Rect rect{x(), checked_int(py), 0, 0};
    rect.cy = checked_int(D::grid_line_height(h_line));
    for (unsigned i = 0; i < nb_columns; ++i) {
        rect.cx += column_width(i);
    }
    drawable.draw(RDPOpaqueRect(rect, colors.bg), clip, gdi::ColorCtx::depth24());

    /*
     * Draw labels
     */
    int px = x();
    for (unsigned i = 0; i < nb_columns; ++i) {
        auto text_width = columns_width[i];

        gdi::draw_text(
            drawable,
            px + D::GRID_LABEL_X_PADDING,
            py + D::GRID_LABEL_Y_PADDING,
            h_line,
            gdi::DrawTextPadding{},
            line.fcs[i],
            colors.fg,
            colors.bg,
            clip.intersect(Rect{
                checked_int(px),
                checked_int(py),
                checked_int(text_width + D::GRID_LABEL_X_PADDING),
                checked_int(D::grid_line_height(h_line)),
            })
        );

        px += text_width + D::GRID_LABEL_X_PADDING * 2;
    }
}

void WidgetSelector::WidgetGrid::rdp_input_scancode(
    KbdFlags /*flags*/, Scancode /*scancode*/, uint32_t /*event_time*/, const Keymap& keymap)
{
    if (selected_line < -1) {
        return;
    }

    int new_selected_line = selected_line;
    int count_line = checked_cast<int>(lines.size());

    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
        case Keymap::KEvent::LeftArrow:
            events.onprev_page();
            return;

        case Keymap::KEvent::UpArrow:
            new_selected_line = (selected_line > 0) ? selected_line - 1 : count_line - 1;
            break;

        case Keymap::KEvent::RightArrow:
            events.onnext_page();
            return;

        case Keymap::KEvent::DownArrow:
            new_selected_line = (selected_line == count_line - 1) ? 0 : selected_line + 1;
            break;

        case Keymap::KEvent::End:
            new_selected_line = count_line - 1;
            break;

        case Keymap::KEvent::Home:
            new_selected_line = 0;
            break;

        case Keymap::KEvent::Enter:
            events.onsubmit();
            return;

        default:
            return;
    }
    REDEMPTION_DIAGNOSTIC_POP()

    set_selected_line(new_selected_line);
}

void WidgetSelector::WidgetGrid::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    using namespace std::chrono_literals;

    int h = D::grid_line_height(h_line);
    int new_selected_line = (y - this->y()) / h;
    int py = this->y() + new_selected_line * h;
    Rect rect(checked_int(this->x()), checked_int(py), this->cx(), checked_int(h));

    if (unsigned(new_selected_line) < lines.size() && rect.contains_pt(x, y)) {
        /*
         * Select line
         */
        if (device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN)) {
            auto now = MonotonicTimePoint::clock::now();
            if (new_selected_line != selected_line) {
                set_selected_line(new_selected_line);
                last_tick = MonotonicTimePoint();
                has_focus = true;
            }
            else if (now - last_tick <= MonotonicTimePoint::duration(700ms)) {
                events.onsubmit();
            }
            last_tick = now;

            if (!has_focus) {
                focus();
            }
        }

        /*
         * Show tooltip when column is truncated
         */
        else if (device_flags == MOUSE_FLAG_MOVE) {
            auto const& line = lines[checked_int(new_selected_line)];
            int px = this->x();
            // ignore last column (protocol)
            for (unsigned i = 0; i < nb_columns - 1; ++i) {
                int w = column_width(i);
                if (px <= x && x <= px + w && line.widths[i] > columns_width[i]) {
                    auto text = (i == 0) ? line.authorization_text : line.target_text;
                    tooltip_shower.show_tooltip(text, x, y, rect);
                    break;
                }
                px += w;
            }
        }
    }
}

void WidgetSelector::WidgetGrid::focus()
{
    if (!has_focus) {
        if (!lines.empty()) {
            draw_line_at(selected_line, colors.focus);
        }
        has_focus = true;
    }
}

void WidgetSelector::WidgetGrid::blur()
{
    if (has_focus) {
        if (!lines.empty()) {
            draw_line_at(selected_line, colors.selected);
        }
        has_focus = false;
    }
}


WidgetSelector::WidgetHeaders::WidgetHeaders(
    gdi::GraphicApi& drawable, const Font& font, Texts::Headers headers, Colors colors
)
    : Widget(drawable, Focusable::No)
    , colors(colors)
    , labels{{
        {font, headers.authorization},
        {font, headers.target},
        {font, headers.protocol},
    }}
{}

void WidgetSelector::WidgetHeaders::rdp_input_invalidate(Rect clip)
{
    int px = x();
    for (unsigned i = 0; i < nb_columns; ++i) {
        gdi::draw_text(
            drawable,
            px,
            y(),
            cy(),
            gdi::DrawTextPadding{
                .top = 0,
                .right = widths[i],
                .bottom = 0,
                .left = D::GRID_LABEL_X_PADDING,
            },
            labels[i].fcs(),
            colors.fg,
            colors.bg,
            Rect(checked_int(px), y(), widths[i], cy()).intersect(clip)
        );

        px += widths[i];
    }
}


WidgetSelector::WidgetExpansionButton::WidgetExpansionButton(
    gdi::GraphicApi& drawable, Theme const& theme,
    uint16_t width_height, WidgetEventNotifier onsubmit
)
    : WidgetButtonEvent(drawable, onsubmit, WidgetButtonEvent::RedrawOnSubmit::No)
    , colors{
        .bg = theme.global.bgcolor,
        .focus_bg = theme.global.focus_color,
        .border = theme.global.fgcolor,
    }
{
    set_wh(width_height, width_height);
}

void WidgetSelector::WidgetExpansionButton::rdp_input_invalidate(Rect clip)
{
    constexpr int border_len = 1;

    gdi_draw_border(
        drawable, colors.border, get_rect(),
        border_len, clip, gdi::ColorCtx::depth24()
    );

    auto rect = get_rect().shrink(border_len);
    auto bg = has_focus ? colors.focus_bg : colors.bg;
    drawable.draw(RDPOpaqueRect(rect, bg), clip, gdi::ColorCtx::depth24());
}

WidgetSelector::WidgetSelector(
    gdi::GraphicApi & drawable, Font const & font, CopyPaste & copy_paste,
    Theme const & theme, WidgetTooltipShower & tooltip_shower,
    Translator tr, Widget * extra_button, Rect rect,
    Texts texts, Events events
)
: WidgetComposite(drawable, Focusable::Yes, theme.global.bgcolor)
, extra_button(extra_button)
, less_than_800(rect.cx < 800)
, tooltip_shower_parent(tooltip_shower)
, target_helpicon(drawable, font, WidgetHelpIcon::Style::ThinText, {
    .fg = theme.global.fgcolor,
    .bg = theme.selector_label.bgcolor,
})
, oncancel(events.oncancel)
, onctrl_shift(events.onctrl_shift)
, device_label(drawable, font, texts.device_name, WidgetLabel::Colors::from_theme(theme))
, header_labels(drawable, font, texts.headers, {
    .fg = theme.global.fgcolor,
    .bg = theme.selector_label.bgcolor,
})
, column_expansion_buttons{[&]{
    auto hw = D::expansion_button_width(font);
    return decltype(column_expansion_buttons){
        WidgetExpansionButton{drawable, theme, hw, [this]{ D::expansion_event(*this, 0); } },
        WidgetExpansionButton{drawable, theme, hw, [this]{ D::expansion_event(*this, 1); } },
    };
}()}
, edit_filters{
    WidgetEdit{drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme), events.onfilter},
    WidgetEdit{drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme), events.onfilter},
    WidgetEdit{drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme), events.onfilter},
}
, grid(
    drawable, theme, *this,
    {
        checked_int(header_labels.labels[0].width() + D::expansion_button_width(font)
            + D::EXPANSION_BUTTON_SEPARATOR),
        checked_int(header_labels.labels[1].width() + D::expansion_button_width(font)
            + D::EXPANSION_BUTTON_SEPARATOR
            + D::HELP_ICON_SEPARATOR
            + target_helpicon.cx()),
        header_labels.labels[2].width(),
    },
    {
        .onsubmit = events.onconnect,
        .onprev_page = events.onprev_page_on_grid,
        .onnext_page = events.onnext_page_on_grid,
    }
)
//BEGIN WidgetPager
, first_page(drawable, font, "◀◂"_av, WidgetButton::Colors::no_border_from_theme(theme),
             events.onfirst_page)
, prev_page(drawable, font, "◀"_av, WidgetButton::Colors::no_border_from_theme(theme),
            events.onprev_page)
, current_page_edit(drawable, font, copy_paste,
                    WidgetEdit::Colors::from_theme(theme), events.oncurrent_page)
, number_page(drawable, font, "/XX"_av, WidgetLabel::Colors::from_theme(theme))
, next_page(drawable, font, "▶"_av, WidgetButton::Colors::no_border_from_theme(theme),
            events.onnext_page)
, last_page(drawable, font, "▸▶"_av, WidgetButton::Colors::no_border_from_theme(theme),
            events.onlast_page)
//END WidgetPager
, logout(drawable, font, tr(trkeys::logout), WidgetButton::Colors::from_theme(theme),
         events.oncancel)
, apply(drawable, font, tr(trkeys::filter), WidgetButton::Colors::from_theme(theme),
        events.onfilter)
, connect(drawable, font, tr(trkeys::connect), WidgetButton::Colors::from_theme(theme),
          events.onconnect)
, tr(tr)
{
    this->add_widget(this->device_label);
    this->add_widget(this->header_labels);

    for (auto & w : edit_filters) {
        this->add_widget(w);
    }

    this->add_widget(this->apply);
    this->add_widget(this->grid);

    this->add_widget(this->first_page);
    this->add_widget(this->prev_page);
    this->add_widget(this->current_page_edit);
    this->add_widget(this->number_page);
    this->add_widget(this->next_page);
    this->add_widget(this->last_page);
    this->add_widget(this->logout);
    this->add_widget(this->connect);
    this->add_widget(this->target_helpicon);

    if (extra_button) {
        this->add_widget(*extra_button);
    }

    this->move_size_widget(rect.x, rect.y, rect.cx, rect.cy);
}

Font const & WidgetSelector::font() noexcept
{
    return current_page_edit.get_font();
}

uint16_t WidgetSelector::max_line_per_page() const
{
    return checked_int(nb_max_line);
}

void WidgetSelector::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->set_xy(left, top);
    this->set_wh(width, height);

    this->device_label.set_xy(left + D::TEXT_MARGIN, top + D::VERTICAL_MARGIN);

    this->less_than_800 = (this->cx() < 800);

    this->move_and_resize_navigation_buttons();

    // filter button position
    this->apply.set_xy(
        checked_int(left + this->cx() - (this->apply.cx() + D::TEXT_MARGIN)),
        checked_int(top + D::VERTICAL_MARGIN)
    );

    // grid headers
    int16_t grid_x = left + (this->less_than_800 ? 0 : D::HORIZONTAL_MARGIN);
    int16_t labels_y = this->device_label.ebottom() + D::HORIZONTAL_MARGIN;
    this->header_labels.set_xy(grid_x, labels_y);

    // help icon
    uint16_t h_text = font().max_height();
    this->target_helpicon.set_xy(
        0,
        checked_int{labels_y + (h_text - this->target_helpicon.cy()) / 2}
    );

    // filters
    int16_t filters_y = checked_int(labels_y + h_text + D::FILTER_VERTICAL_SEPARATOR);
    for (auto & edit : this->edit_filters) {
        edit.set_xy(0, filters_y);
    }

    // grid
    auto line_height = D::grid_line_height(font());
    this->grid.set_xy(grid_x, this->edit_filters[0].ebottom() + D::FILTER_VERTICAL_SEPARATOR);
    nb_max_line = std::max(0, current_page_edit.y() - grid.y() - D::VERTICAL_MARGIN) / line_height;
    nb_max_line = std::min(D::NB_MAX_LINE, nb_max_line);
    if (nb_max_line == 0) {
        ++nb_max_line;
    }
    this->grid.set_wh(
        checked_int(this->cx() - (this->less_than_800 ? 0 : 30)),
        checked_int(
            std::min(checked_cast<int>(grid.count_line()), nb_max_line)
            * line_height
        )
    );

    this->header_labels.set_wh(grid.cx(), h_text);

    this->rearrange_grid();
}

void WidgetSelector::move_and_resize_navigation_buttons()
{
    // Navigation buttons
    int nav_bottom_y = this->cy() - (this->connect.cy() + D::VERTICAL_MARGIN);
    int nav_top_y = this->y() + nav_bottom_y - (this->last_page.cy() + D::VERTICAL_MARGIN);
    int nav_offset_x = this->eright();

    if (extra_button) {
        extra_button->set_xy(
            checked_int(this->x() + (this->less_than_800 ? 30 : 60)),
            checked_int((this->cy() - nav_top_y - D::VERTICAL_MARGIN - extra_button->cy()) / 2
                        + nav_top_y)
        );
    }

    // ">>" button
    nav_offset_x -= (this->last_page.cx() + D::TEXT_MARGIN);
    this->last_page.set_xy(checked_int(nav_offset_x), checked_int(nav_top_y));

    // ">" button
    nav_offset_x -= (this->next_page.cx() + D::NAV_SEPARATOR);
    this->next_page.set_xy(checked_int(nav_offset_x), checked_int(nav_top_y));

    // "/ N" button
    nav_offset_x -= (this->number_page.cx() + D::NAV_SEPARATOR);
    this->number_page.set_xy(
        checked_int(nav_offset_x),
        checked_int(nav_top_y + (this->next_page.cy() - this->number_page.cy()) / 2)
    );

    // edit page button
    int edit_cx
        = checked_cast<int>(number_page.text_label.fcs().size())
        * this->font().item('0').view.boxed_width()
        + this->current_page_edit.x_padding();
    nav_offset_x -= edit_cx;
    this->current_page_edit.set_xy(
        checked_int(nav_offset_x),
        checked_int(nav_top_y + (this->next_page.cy() - this->current_page_edit.cy()) / 2)
    );
    this->current_page_edit.update_width(checked_int(edit_cx));

    // "<" button
    nav_offset_x -= (this->prev_page.cx() + D::NAV_SEPARATOR);
    this->prev_page.set_xy(checked_int(nav_offset_x), checked_int(nav_top_y));

    // "<<" button
    nav_offset_x -= (this->first_page.cx() + D::NAV_SEPARATOR);
    this->first_page.set_xy(checked_int(nav_offset_x), checked_int(nav_top_y));

    // "connect" button
    int nav_w = this->last_page.eright() - this->first_page.x();
    this->connect.set_xy(
        checked_int(this->last_page.eright() - nav_w/4 - this->connect.cx()/2),
        checked_int(this->y() + nav_bottom_y)
    );
    // "logout" button
    this->logout.set_xy(
        checked_int(this->first_page.x() + nav_w/4 - this->logout.cx()/2),
        checked_int(this->y() + nav_bottom_y)
    );
}


void WidgetSelector::rearrange_grid()
{
    auto is_optimal_columns = grid.arrange();

    /*
     * Header and edit positioning
     */

    int offset = this->less_than_800 ? 0 : D::HORIZONTAL_MARGIN;
    offset += this->x();

    for (unsigned i = 0; i < nb_columns; ++i) {
        int width = grid.column_width(i);
        header_labels.widths[i] = checked_int(width);
        edit_filters[i].set_xy(
            checked_int(offset + (i ? D::FILTER_VERTICAL_SEPARATOR / 2 : 0)),
            edit_filters[i].y()
        );
        edit_filters[i].update_width(checked_int(
            width - (
                i == 1  // is target column
                    ? D::FILTER_HORIZONTAL_SEPARATOR
                    : D::FILTER_HORIZONTAL_SEPARATOR / 2
            )
        ));

        // insert/remove expansion button
        // protocol column is ignored
        if (i < column_expansion_buttons.size()) {
            auto & button = column_expansion_buttons[i];
            if (is_optimal_columns[i]) {
                if (button.inserted) {
                    button.inserted = false;
                    remove_widget(button);
                }
            }
            else if (!button.inserted) {
                button.set_xy(
                    checked_int(offset + width - button.cx() - D::GRID_LABEL_X_PADDING),
                    checked_int(header_labels.y() + (header_labels.cy() - button.cy()) / 2)
                );
                button.inserted = true;
                button.reset_state();
                add_widget(button);
            }
        }

        offset += width;
    }

    this->target_helpicon.set_xy(
        checked_int(header_labels.x()
                  + header_labels.widths[0]
                  + header_labels.labels[1].width()
                  + D::GRID_LABEL_X_PADDING
                  + D::HELP_ICON_SEPARATOR),
        this->target_helpicon.y()
    );
}

bool WidgetSelector::has_lines() const noexcept
{
    return has_devices;
}

void WidgetSelector::set_page(Page page)
{
    current_page_edit.set_text(
        int_to_decimal_chars(page.current_page),
        {WidgetEdit::Redraw::No}
    );

    number_page.set_text(
        font(),
        static_str_concat('/', int_to_decimal_chars(page.number_of_page))
    );

    has_devices = grid.set_devices(
        font(), tr,
        page.authorizations,
        page.targets,
        page.protocols,
        max_line_per_page()
    );

    grid.set_wh(grid.cx(), checked_int(grid.count_line() * D::grid_line_height(font())));

    // focus to grid without redraw
    if (has_devices) {
        set_widget_focus(grid, Redraw::No);
    }
    else {
        grid.has_focus = false;
    }

    move_and_resize_navigation_buttons();

    rearrange_grid();

    D::redraw(*this, connect.ebottom());
}

WidgetSelector::SelectedLine WidgetSelector::selected_line() const noexcept
{
    return grid.get_selected_line();
}

WidgetSelector::FilterTexts WidgetSelector::filter_texts() const noexcept
{
    return {
        .authorization = edit_filters[0].get_text(),
        .target = edit_filters[1].get_text(),
        .protocol = edit_filters[2].get_text(),
    };
}

unsigned WidgetSelector::current_page() const noexcept
{
    return current_page_edit.get_text_as_uint();
}

void WidgetSelector::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
        case Keymap::KEvent::Esc:
            this->oncancel();
            break;

        case Keymap::KEvent::Ctrl:
        case Keymap::KEvent::Shift:
            if (this->extra_button
                && keymap.is_shift_pressed()
                && keymap.is_ctrl_pressed())
            {
                this->onctrl_shift();
            }
            break;

        case Keymap::KEvent::KeyDown:
            // shortcut for resize column expansion
            if (grid.has_focus && grid.count_line()) {
                auto expand = [&](unsigned i) {
                    if (column_expansion_buttons[i].inserted) {
                        D::expansion_event(*this, i);
                    }
                };
                switch (scancode) {
                    case Scancode::Digit1: expand(0); break;
                    case Scancode::Digit2: expand(1); break;
                    default: grid.rdp_input_scancode(flags, scancode, event_time, keymap);
                }
                return;
            }
            [[fallthrough]];

        default:
            WidgetComposite::rdp_input_scancode(flags, scancode, event_time, keymap);
            break;
    }
    REDEMPTION_DIAGNOSTIC_POP()
}

void WidgetSelector::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (device_flags == MOUSE_FLAG_MOVE && this->target_helpicon.get_rect().contains_pt(x, y)) {
        TooltipShower(*this).show_tooltip(
            tr(trkeys::target_accurate_filter_help),
            x, y, this->target_helpicon.get_rect()
        );
    }

    WidgetComposite::rdp_input_mouse(device_flags, x, y);
}

void WidgetSelector::TooltipShower::show_tooltip(
    chars_view text, int x, int y, Rect const mouse_area)
{
    this->selector.tooltip_shower_parent.show_tooltip(text, x, y, this->selector.get_rect(), mouse_area);
}
