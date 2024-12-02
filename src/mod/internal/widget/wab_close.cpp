/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2013
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#include "mod/internal/widget/wab_close.hpp"
#include "utils/theme.hpp"
#include "utils/trkeys.hpp"
#include "utils/strutils.hpp"
#include "core/app_path.hpp"
#include "core/font.hpp"
#include "gdi/graphic_api.hpp"

WidgetWabClose::WidgetWabClose(
    gdi::GraphicApi & drawable,
    int16_t left, int16_t top, int16_t width, int16_t height,
    Events events, std::string diagnostic_text,
    chars_view username, chars_view target,
    bool showtimer, Font const & font, Theme const & theme,
    Translator tr, bool back_to_selector)
: WidgetComposite(drawable, Focusable::Yes)
, oncancel(events.oncancel)
, connection_closed_label(drawable, tr(trkeys::connection_closed),
                          theme.global.fgcolor, theme.global.bgcolor, font)
, separator(drawable, theme.global.separator_color)
, username_label(drawable, tr(trkeys::wab_close_username),
                 theme.global.fgcolor, theme.global.bgcolor, font)
, username_value(drawable, username,
                 theme.global.fgcolor, theme.global.bgcolor, font)
, target_label(drawable, tr(trkeys::wab_close_target),
               theme.global.fgcolor, theme.global.bgcolor, font)
, target_value(drawable, target,
               theme.global.fgcolor, theme.global.bgcolor, font)
, diagnostic_label(drawable, tr(trkeys::wab_close_diagnostic),
                   theme.global.fgcolor, theme.global.bgcolor, font)
, diagnostic_value(drawable, theme.global.fgcolor, theme.global.bgcolor, font)
, timeleft_label(drawable, tr(trkeys::wab_close_timeleft),
                theme.global.fgcolor, theme.global.bgcolor, font)
, timeleft_value(drawable, ""_av,
                 theme.global.fgcolor, theme.global.bgcolor, font)
, cancel(drawable, tr(trkeys::close),
         events.oncancel,
         theme.global.fgcolor, theme.global.bgcolor,
         theme.global.focus_color, 2, font, 6, 2)
, img(drawable,
      theme.enable_theme ? theme.logo_path.c_str() :
      app_path(AppPath::LoginWabBlue),
      theme.global.bgcolor)
, prev_time(0)
, tr(tr)
, showtimer(showtimer)
, font(font)
, back_to_selector_ctx{
    .onback_to_selector = events.onback_to_selector,
    .fgcolor = theme.global.fgcolor,
    .bgcolor = theme.global.bgcolor,
    .focus_color = theme.global.focus_color,
}
, back(back_to_selector ? make_back_to_selector() : std::unique_ptr<WidgetButton>())
, diagnostic_text(std::move(diagnostic_text))
{
    this->set_bg_color(theme.global.bgcolor);
    this->add_widget(this->img);

    this->add_widget(this->connection_closed_label);
    this->add_widget(this->separator);

    if (!username.empty()) {
        this->add_widget(this->username_label);
        this->add_widget(this->username_value);
        this->add_widget(this->target_label);
        this->add_widget(this->target_value);
    }

    this->add_widget(this->diagnostic_label);
    this->add_widget(this->diagnostic_value);

    if (showtimer) {
        this->add_widget(this->timeleft_label);
        this->add_widget(this->timeleft_value);
    }

    this->add_widget(this->cancel, HasFocus::Yes);

    if (this->back) {
        this->add_widget(*this->back);
    }

    this->move_size_widget(left, top, width, height);
}

std::unique_ptr<WidgetButton> WidgetWabClose::make_back_to_selector()
{
    return std::make_unique<WidgetButton>(
        drawable, tr(trkeys::back_selector),
        back_to_selector_ctx.onback_to_selector,
        back_to_selector_ctx.fgcolor,
        back_to_selector_ctx.bgcolor,
        back_to_selector_ctx.focus_color, 2, font,
        6, 2
    );
}

Rect WidgetWabClose::set_back_to_selector(bool back_to_selector)
{
    if (back_to_selector != bool(this->back)) {
        Rect updated_rect = this->cancel.get_rect();
        if (this->back) {
            updated_rect = updated_rect.disjunct(this->back->get_rect());
            this->remove_widget(*this->back);
            this->back.reset();

            this->cancel.set_xy(
                this->x() + (this->cx() - this->cancel.cx()) / 2,
                this->cancel.y()
            );
        }
        else {
            this->back = make_back_to_selector();
            this->back->set_wh(this->back->get_optimal_dim());

            this->add_widget(*this->back);

            this->cancel.set_xy(
                this->x() + (this->cx() - (this->cancel.cx() + this->back->cx() + 10)) / 2,
                this->cancel.y()
            );
            this->back->set_xy(
                this->cancel.x() + this->cancel.cx() + 10,
                this->cancel.y()
            );
        }

        if (this->img.y() == this->y()) {
            return this->get_rect();
        }

        if (this->back) {
            updated_rect = updated_rect.disjunct(this->back->get_rect());
        }
        return updated_rect.disjunct(this->cancel.get_rect());
    }

    return Rect();
}

void WidgetWabClose::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->set_wh(width, height);
    this->set_xy(left, top);

    int16_t y = 10;

    Dimension dim = this->connection_closed_label.get_optimal_dim();
    this->connection_closed_label.set_wh(dim);
    this->connection_closed_label.set_xy(
        left + (this->cx() - this->connection_closed_label.cx()) / 2, top + y);
    y += this->connection_closed_label.cy();

    const bool is_short_screen = (width < 600);

    this->separator.set_wh(is_short_screen ? width : std::max(600, width / 3 * 2), 2);
    this->separator.set_xy(left + (this->cx() - this->separator.cx()) / 2, top + y + 3);
    y += 30;

    const uint16_t dx = is_short_screen ? 0 : (width - this->separator.cx()) / 2;

    uint16_t x = 0;

    if (!this->username_value.get_text().empty()) {
        dim = this->username_label.get_optimal_dim();
        this->username_label.set_wh(dim);
        this->username_label.set_xy(left + dx, this->username_label.y());
        x = std::max<uint16_t>(this->username_label.cx(), x);

        dim = this->target_label.get_optimal_dim();
        this->target_label.set_wh(dim);
        this->target_label.set_xy(left + dx, this->target_label.y());
        x = std::max<uint16_t>(this->target_label.cx(), x);
    }

    dim = this->diagnostic_label.get_optimal_dim();
    this->diagnostic_label.set_wh(dim);
    this->diagnostic_label.set_xy(left + dx, this->diagnostic_label.y());
    x = std::max<uint16_t>(this->diagnostic_label.cx(), x);

    if (this->showtimer) {
        dim = this->timeleft_label.get_optimal_dim();
        this->timeleft_label.set_wh(dim);
        this->timeleft_label.set_xy(left + dx, this->timeleft_label.y());
        x = std::max<uint16_t>(this->timeleft_label.cx(), x);
    }

    x += 10;

    if (!this->username_value.get_text().empty()) {
        this->username_label.set_xy(this->username_label.x(), top + y);

        dim = this->username_value.get_optimal_dim();
        this->username_value.set_wh(dim);
        this->username_value.set_xy(x + this->diagnostic_label.x(), top + y);

        y += this->username_label.cy() + 20;

        this->target_label.set_xy(this->target_label.x(), top + y);

        dim = this->target_value.get_optimal_dim();
        this->target_value.set_wh(dim);
        this->target_value.set_xy(x + this->diagnostic_label.x(), top + y);

        y += this->target_label.cy() + 20;
    }

    this->diagnostic_label.set_xy(this->diagnostic_label.x(), top + y);

    const bool short_diag = this->diagnostic_label.cx() > this->cx() - (x + 10)
                         || width < 400;

    this->diagnostic_value.set_text(gdi::MultiLineTextMetrics(
        this->font, this->diagnostic_text,
        short_diag ? this->cx() : this->separator.cx() - x
    ));
    dim = this->diagnostic_value.get_optimal_dim();
    this->diagnostic_value.set_wh(dim);

    if (short_diag) {
        y += this->diagnostic_label.cy() + 10;

        this->diagnostic_value.set_xy(0, top + y);
        y += this->diagnostic_value.cy() + 20;
    }
    else {
        this->diagnostic_value.set_xy(x + this->diagnostic_label.x(), top + y);
        y += std::max(this->diagnostic_value.cy(), this->diagnostic_label.cy()) + 20;
    }

    if (this->showtimer) {
        this->timeleft_label.set_xy(this->timeleft_label.x(), top + y);

        dim = this->timeleft_value.get_optimal_dim();
        this->timeleft_value.set_wh(dim);
        this->timeleft_value.set_xy(x + this->diagnostic_label.x(), top + y);

        y += this->timeleft_label.cy() + 20;
    }

    dim = this->cancel.get_optimal_dim();
    this->cancel.set_wh(dim);

    if (this->back) {
        dim = this->back->get_optimal_dim();
        this->back->set_wh(dim);
    }

    int const back_width = this->back ? this->back->cx() + 10 : 0;

    this->cancel.set_xy(left + (this->cx() - (this->cancel.cx() + back_width)) / 2,
                        top + y);

    if (this->back) {
        this->back->set_xy(this->cancel.x() + this->cancel.cx() + 10, top + y);
    }

    y += this->cancel.cy();

    this->move_children_xy(0, (height - y) / 2);

    dim = this->img.get_optimal_dim();
    this->img.set_wh(dim);

    this->img.set_xy(left + (this->cx() - this->img.cx()) / 2,
                     top + (3*(height - y) / 2 - this->img.cy()) / 2 + y);
    if (this->img.y() + this->img.cy() > top + height) {
        this->img.set_xy(this->img.x(), top);
    }
}

std::chrono::seconds WidgetWabClose::refresh_timeleft(std::chrono::seconds remaining)
{
    long tl = remaining.count();
    bool seconds = true;
    if (tl > 60) {
        seconds = false;
        tl = tl / 60;
    }
    if (this->prev_time != tl) {
        using TrFmt = Translator::FmtMsg<256>;
        auto duration = static_cast<int>(tl);
        auto text = seconds
            ? TrFmt(tr, trkeys::close_box_second_timer, duration)
            : TrFmt(tr, trkeys::close_box_minute_timer, duration);

        Rect old = this->timeleft_value.get_rect();
        this->timeleft_value.set_text(nullptr);
        this->rdp_input_invalidate(old);
        this->timeleft_value.set_text(text);

        Dimension dim = this->timeleft_value.get_optimal_dim();
        this->timeleft_value.set_wh(dim);

        this->rdp_input_invalidate(this->timeleft_value.get_rect());

        this->prev_time = tl;
    }

    return std::chrono::seconds(seconds ? 1 : 60);
}

void WidgetWabClose::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (pressed_scancode(flags, scancode) == Scancode::Esc) {
        oncancel();
    }
    else {
        WidgetComposite::rdp_input_scancode(flags, scancode, event_time, keymap);
    }
}
