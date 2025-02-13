/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/selector.hpp"
#include "translation/translation.hpp"
#include "translation/trkeys.hpp"
#include "utils/theme.hpp"
#include "utils/sugar/buf_maker.hpp"
#include "gdi/draw_utils.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text_metrics.hpp"
#include "keyboard/keymap.hpp"
#include "core/font.hpp"


WidgetSelector::temporary_number_of_page::temporary_number_of_page(chars_view s)
    : len(s.size() + 1)
{
    this->buffer[0] = '/';
    memcpy(this->buffer + 1, s.data(), std::min(s.size(), std::size(buffer) - 1));
}

struct WidgetSelector::WidgetHelpIcon::D
{
    static const int border_len = 1;
    static const int pad_x = 2;

    static int right_pad(const FontCharView * fc)
    {
        return pad_x + border_len + fc->offsetx + fc->incby - fc->width;
    }
};

WidgetSelector::WidgetHelpIcon::WidgetHelpIcon(
    gdi::GraphicApi & drawable, const Font& font, const Theme& theme
)
    : Widget(drawable, Focusable::No)
    , fc(&font.item('?').view)
    , fg(theme.global.fgcolor)
    , bg(theme.selector_label.bgcolor)
{
    set_wh(
        checked_int(fc->boxed_width() + D::border_len + D::pad_x + D::right_pad(fc)),
        checked_int(font.max_height() + D::border_len * 2)
    );
}

void WidgetSelector::WidgetHelpIcon::rdp_input_invalidate(Rect clip)
{
    gdi::draw_text(
        drawable,
        x(),
        y(),
        cy() - D::border_len * 2,
        gdi::DrawTextPadding{
          .top = D::border_len,
          .right = checked_int(D::right_pad(fc)),
          .bottom = D::border_len,
          .left = D::border_len + D::pad_x,
        },
        {&fc, 1},
        fg,
        bg,
        get_rect().intersect(clip)
    );

    gdi_draw_border(drawable, fg, get_rect(), D::border_len, clip, gdi::ColorCtx::depth24());
}

WidgetSelector::WidgetSelector(
    gdi::GraphicApi & drawable, CopyPaste & copy_paste,
    WidgetTooltipShower & tooltip_shower,
    chars_view device_name,
    int16_t left, int16_t top, uint16_t width, uint16_t height,
    Events events,
    chars_view current_page,
    chars_view number_of_page,
    Widget * extra_button,
    WidgetSelectorParams const & selector_params,
    Font const & font, Theme const & theme, Translator tr)
: WidgetComposite(drawable, Focusable::Yes)
, tooltip_shower(*this)
, tooltip_shower_parent(tooltip_shower)
, onconnect(events.onconnect)
, oncancel(events.oncancel)
, onctrl_shift(events.onctrl_shift)
, less_than_800(width < 800)
, nb_columns(std::min(selector_params.nb_columns, WidgetSelectorParams::nb_max_columns))
, device_label(drawable, font, device_name, WidgetLabel::Colors::from_theme(theme))
, header_labels{
    WidgetLabel{
        drawable, font, selector_params.label[0],
        {.fg = theme.selector_label.fgcolor, .bg = theme.selector_label.bgcolor}
    },
    WidgetLabel{
        drawable, font, selector_params.label[1],
        {.fg = theme.selector_label.fgcolor, .bg = theme.selector_label.bgcolor}
    },
    WidgetLabel{
        drawable, font, selector_params.label[2],
        {.fg = theme.selector_label.fgcolor, .bg = theme.selector_label.bgcolor}
    }
}
, column_expansion_buttons{
    WidgetButton{
        drawable, font, ""_av, WidgetButton::Colors::from_theme(theme),
        [this]{
            this->priority_column_index = 0;
            this->rearrange();
            this->rdp_input_invalidate(this->get_rect());
        },
    },
    WidgetButton{
        drawable, font, ""_av, WidgetButton::Colors::from_theme(theme),
        [this]{
            this->priority_column_index = 1;
            this->rearrange();
            this->rdp_input_invalidate(this->get_rect());
        },
    },
    WidgetButton{
        drawable, font, ""_av, WidgetButton::Colors::from_theme(theme),
        [this]{
            this->priority_column_index = 2;
            this->rearrange();
            this->rdp_input_invalidate(this->get_rect());
        },
    }
}
, edit_filters{
    WidgetEdit{drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme), events.onfilter},
    WidgetEdit{drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme), events.onfilter},
    WidgetEdit{drawable, font, copy_paste, WidgetEdit::Colors::from_theme(theme), events.onfilter},
}
, selector_lines(drawable, tooltip_shower,
                 [this]{ this->ask_for_connection(); },
                 0, this->nb_columns,
                 theme.selector_line1.bgcolor,
                 theme.selector_line1.fgcolor,
                 theme.selector_line2.bgcolor,
                 theme.selector_line2.fgcolor,
                 theme.selector_focus.bgcolor,
                 theme.selector_focus.fgcolor,
                 theme.selector_selected.bgcolor,
                 theme.selector_selected.fgcolor,
                 font, 2)
//BEGIN WidgetPager
, first_page(drawable, font, "◀◂"_av, WidgetButton::Colors::no_border_from_theme(theme),
             events.onfirst_page)
, prev_page(drawable, font, "◀"_av, WidgetButton::Colors::no_border_from_theme(theme),
            events.onprev_page)
, current_page(drawable, font, copy_paste, current_page,
               WidgetEdit::Colors::from_theme(theme),
               events.oncurrent_page)
, number_page(drawable, font,
              !number_of_page.empty() ? temporary_number_of_page(number_of_page) : "/XXX"_av,
              WidgetLabel::Colors::from_theme(theme))
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
, target_helpicon(drawable, font, theme)
, tr(tr)
, font(font)
, left(left)
, top(top)
, extra_button(extra_button)
{
    this->set_bg_color(theme.global.bgcolor);
    this->add_widget(this->device_label);

    for (int i = 0; i < this->nb_columns; i++) {
        this->weight[i] = selector_params.weight[i];
        this->label[i] = selector_params.label[i];
        this->add_widget(this->header_labels[i]);
        this->add_widget(this->edit_filters[i]);

        this->column_expansion_buttons[i].set_wh(8, 8);
    }

    this->add_widget(this->apply);
    this->add_widget(this->selector_lines, HasFocus::Yes);

    this->add_widget(this->first_page);
    this->add_widget(this->prev_page);
    this->add_widget(this->current_page);
    this->add_widget(this->number_page);
    this->add_widget(this->next_page);
    this->add_widget(this->last_page);
    this->add_widget(this->logout);
    this->add_widget(this->connect);
    this->add_widget(this->target_helpicon);

    number_page.set_wh(number_page.get_optimal_dim());

    if (extra_button) {
        this->add_widget(*extra_button);
    }

    this->move_size_widget(left, top, width, height);
}

void WidgetSelector::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->set_xy(left, top);
    this->set_wh(width, height);

    this->left = left;
    this->top  = top;

    Dimension dim = this->device_label.get_optimal_dim();
    this->device_label.set_wh(dim);
    this->device_label.set_xy(left + TEXT_MARGIN, top + VERTICAL_MARGIN);


    for (int i = 0; i < this->nb_columns; i++) {
        dim = this->header_labels[i].get_optimal_dim();
        this->header_labels[i].set_wh(dim);

        dim = this->edit_filters[i].get_optimal_dim();
        this->edit_filters[i].set_wh(dim);
    }

    dim = this->current_page.get_optimal_dim();
    this->current_page.set_wh(this->first_page.cy() + 2, dim.h);

    this->less_than_800 = (this->cx() < 800);

    this->selector_lines.set_wh(width - (this->less_than_800 ? 0 : 30),
        this->selector_lines.cy());
    std::fill(this->current_columns_width, this->current_columns_width + this->selector_lines.get_nb_columns(), 0);

    if (this->extra_button) {
        this->extra_button->set_xy(left + 60, top + height - 60);
    }

    this->rearrange();
}

constexpr uint16_t COLUMN_EXPANSION_BUTTON_PLACE_HOLDER = 18;

void WidgetSelector::rearrange()
{
    ColumnWidthStrategy column_width_strategies[WidgetSelectorParams::nb_max_columns];
    bool column_width_is_optimal[WidgetSelectorParams::nb_max_columns];

    for (int i = 0; i < this->nb_columns; i++) {
        gdi::TextMetrics tm(this->font, this->header_labels[i].get_text());
        column_width_strategies[i] = { static_cast<uint16_t>(tm.width + 5 + COLUMN_EXPANSION_BUTTON_PLACE_HOLDER), this->weight[i] };

        column_width_is_optimal[i] = false;
    }

    BufMaker<128, uint16_t> rows_height_buffer;
    auto rows_height = rows_height_buffer.dyn_array(this->selector_lines.get_nb_rows());

    compute_format(this->selector_lines, column_width_strategies, this->priority_column_index,
                   rows_height.data(), this->current_columns_width, column_width_is_optimal);
    apply_format(this->selector_lines, rows_height.data(), this->current_columns_width);

    {
        // filter button position
        this->apply.set_xy(this->left + this->cx() - (this->apply.cx() + TEXT_MARGIN),
                            this->top + VERTICAL_MARGIN);
    }

    {
        // labels and filters position
        uint16_t offset = this->less_than_800 ? 0 : HORIZONTAL_MARGIN;
        uint16_t labels_y = this->device_label.ebottom() + HORIZONTAL_MARGIN;
        uint16_t filters_y = labels_y + this->header_labels[0].cy()
            + FILTER_SEPARATOR;

        for (std::size_t i = 0; i < this->nb_columns; ++i) {
            this->header_labels[i].set_wh(
                this->current_columns_width[i] + this->selector_lines.border * 2,
                this->header_labels[i].cy());
            this->header_labels[i].set_xy(this->left + offset, labels_y);
            this->edit_filters[i].set_xy(this->header_labels[i].x(), filters_y);
            this->edit_filters[i].set_wh(
                this->header_labels[i].cx() - ((i == this->nb_columns-1) ? 0 : FILTER_SEPARATOR),
                this->edit_filters[i].cy());
            offset += this->header_labels[i].cx();

            bool contains_widget = this->contains_widget(this->column_expansion_buttons[i]);
            if (column_width_is_optimal[i]) {
                if (contains_widget) {
                    this->remove_widget(this->column_expansion_buttons[i]);
                }
            }
            else {
                if (!contains_widget) {
                    this->add_widget(this->column_expansion_buttons[i]);
                }

                this->column_expansion_buttons[i].set_xy(this->left + offset - 15, labels_y + 5);
            }
        }

        WidgetLabel& target_header_label = this->header_labels[IDX_TARGET];

        this->target_helpicon.set_xy(target_header_label.x()
                                     + target_header_label.get_optimal_dim().w + 4,
                                     labels_y - WidgetHelpIcon::D::border_len);
    }

    {
        // selector list position
        this->selector_lines.set_xy(this->left + (this->less_than_800 ? 0 : HORIZONTAL_MARGIN),
                                    this->edit_filters[0].ebottom() + FILTER_SEPARATOR);
    }
    {
        // Navigation buttons
        uint16_t nav_bottom_y = this->cy() - (this->connect.cy() + VERTICAL_MARGIN);
        this->connect.set_xy(this->connect.x(), this->top + nav_bottom_y);
        this->logout.set_xy(this->logout.x(), this->top + nav_bottom_y);

        uint16_t nav_top_y = this->connect.y() - (this->last_page.cy() + VERTICAL_MARGIN);
        this->last_page.set_xy(this->last_page.x(), nav_top_y);
        this->next_page.set_xy(this->next_page.x(), nav_top_y);
        this->number_page.set_xy(this->number_page.x(),
            nav_top_y + (this->next_page.cy() - this->number_page.cy()) / 2);
        this->current_page.set_xy(this->current_page.x(),
            nav_top_y + (this->next_page.cy() - this->current_page.cy()) / 2);
        this->prev_page.set_xy(this->prev_page.x(), nav_top_y);
        this->first_page.set_xy(this->first_page.x(), nav_top_y);

        uint16_t nav_offset_x = this->cx() - (this->last_page.cx() + TEXT_MARGIN);
        this->last_page.set_xy(this->left + nav_offset_x, this->last_page.y());

        nav_offset_x -= (this->next_page.cx() + NAV_SEPARATOR);
        this->next_page.set_xy(this->left + nav_offset_x, this->next_page.y());

        nav_offset_x -= (this->number_page.cx() + NAV_SEPARATOR);
        this->number_page.set_xy(this->left + nav_offset_x, this->number_page.y());

        nav_offset_x -= this->current_page.cx();
        this->current_page.set_xy(this->left + nav_offset_x, this->current_page.y());

        nav_offset_x -= (this->prev_page.cx() + NAV_SEPARATOR);
        this->prev_page.set_xy(this->left + nav_offset_x, this->prev_page.y());

        nav_offset_x -= (this->first_page.cx() + NAV_SEPARATOR);
        this->first_page.set_xy(this->left + nav_offset_x, this->first_page.y());

        int nav_w = this->last_page.eright() - this->first_page.x();
        this->connect.set_xy(this->last_page.eright() - nav_w/4 - this->connect.cx()/2,
            this->connect.y());
        this->logout.set_xy(this->first_page.x() + nav_w/4 - this->logout.cx()/2,
            this->logout.y());
    }
}

void WidgetSelector::ask_for_connection()
{
    this->onconnect();
}


void WidgetSelector::add_device(array_view<chars_view> entries)
{
    this->selector_lines.add_line(entries);
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

        default:
            WidgetComposite::rdp_input_scancode(flags, scancode, event_time, keymap);
            break;
    }
    REDEMPTION_DIAGNOSTIC_POP()
}

void WidgetSelector::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (device_flags == MOUSE_FLAG_MOVE && this->target_helpicon.get_rect().contains_pt(x, y)) {
        auto rect = this->get_rect();
        // exclude title bar when remoteapp
        rect.y += 30;
        rect.cy -= 30;
        this->tooltip_shower.show_tooltip(
            this->tr(trkeys::target_accurate_filter_help),
            x, y, rect, this->target_helpicon.get_rect()
        );
    }

    WidgetComposite::rdp_input_mouse(device_flags, x, y);
}

void WidgetSelector::TooltipShower::show_tooltip(
    chars_view text, int x, int y,
    Rect const preferred_display_rect,
    Rect const mouse_area)
{
    (void)preferred_display_rect;
    this->selector.tooltip_shower_parent.show_tooltip(text, x, y, this->selector.get_rect(), mouse_area);
}
