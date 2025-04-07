/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2014
 *   Author(s): Christophe Grosjean, Xiaopeng Zhou, Jonathan Poelen,
 *              Meng Tan, Jennifer Inthavong
 */

#include "configs/config.hpp"
#include "mod/internal/wait_mod.hpp"
#include "mod/internal/copy_paste.hpp"
#include "utils/sugar/to_sv.hpp"


using namespace std::chrono_literals;

WaitMod::WaitMod(
    WaitModVariables vars,
    EventContainer& events,
    gdi::GraphicApi & drawable,
    FrontAPI & front, uint16_t width, uint16_t height,
    Rect const widget_rect, chars_view caption, chars_view message,
    ClientExecute & rail_client_execute, Font const& font, Theme const& theme,
    CopyPaste& copy_paste, Translator tr, uint32_t flag
)
    : RailInternalModBase(
        drawable, width, height, rail_client_execute, font, theme,
        (flag & WidgetWait::SHOW_FORM) ? &copy_paste : nullptr)
    , language_button(
        vars.get<cfg::internal_mod::keyboard_layout_proposals>(), this->wait_widget,
        drawable, front, font, LanguageButton::Colors::from_theme(theme))
    , wait_widget(drawable, copy_paste, widget_rect,
        {
            .onaccept = [this]{ this->accepted(); },
            .onrefused = [this]{ this->refused(); },
            .onconfirm = [this]{ this->confirm(); },
            .onctrl_shift = [this]{ this->language_button.next_layout(); },
        },
        caption, message, &this->language_button,
        font, theme, tr, flag, vars.get<cfg::context::duration_max>())
    , vars(vars)
    , events_guard(events)
{
    this->screen.add_widget(this->wait_widget, WidgetComposite::HasFocus::Yes);
    this->screen.init_focus();
    this->screen.rdp_input_invalidate(this->screen.get_rect());

    this->events_guard.create_event_timeout("Wait Mod Timeout",
        600s, [this](Event&)
        {
            this->refused();
        }
    );
}

void WaitMod::confirm()
{
    this->vars.set_acl<cfg::context::waitinforeturn>("confirm");
    auto edits = this->wait_widget.get_edit_texts();
    this->vars.set_acl<cfg::context::comment>(to_sv(edits.comment));
    this->vars.set_acl<cfg::context::ticket>(to_sv(edits.ticket));
    this->vars.set_acl<cfg::context::duration>(to_sv(edits.duration));
    this->set_mod_signal(BACK_EVENT_NEXT);
}

// TODO ugly. The value should be pulled by authentifier when module is closed instead of being pushed to it by mod
void WaitMod::accepted()
{
    this->vars.set_acl<cfg::context::waitinforeturn>("backselector");
    this->set_mod_signal(BACK_EVENT_NEXT);
}

// TODO ugly. The value should be pulled by authentifier when module is closed instead of being pushed to it by mod
void WaitMod::refused()
{
    this->vars.set_acl<cfg::context::waitinforeturn>("exit");
    this->set_mod_signal(BACK_EVENT_NEXT);
}
