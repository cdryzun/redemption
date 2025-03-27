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
 *   Copyright (C) Wallix 2010-2013
 *   Author(s): Christophe Grosjean, Xiaopeng Zhou, Jonathan Poelen, Meng Tan,
 *              Jennifer Inthavong
 */

#include "configs/config.hpp"
#include "mod/internal/login_mod.hpp"
#include "mod/internal/copy_paste.hpp"
#include "main/version.hpp"
#include "utils/strutils.hpp"
#include "translation/trkeys.hpp"
#include "utils/sugar/to_sv.hpp"

#include <string>
#include <utility>


namespace
{
    inline std::pair<std::string_view, std::string_view>
    rpartition(std::string_view text)
    {
        size_t sep_pos = text.find_last_of(":+");
        if (sep_pos != std::string::npos) {
            return std::make_pair(
                text.substr(0, sep_pos),
                text.substr(sep_pos + 1)
            );
        }

        return std::make_pair(std::string_view(), text);
    }

    inline void
    update_target_login(std::string& auth_user, chars_view login, std::string_view target)
    {
        size_t sep_pos = target.find_last_of("+:");
        char sep = (sep_pos != std::string::npos) ? target[sep_pos] : ':';
        str_assign(auth_user, target, sep, login);
    }
} // anonymous namespace

LoginMod::LoginMod(
    LoginModVariables vars,
    EventContainer & events,
    chars_view username, chars_view password,
    gdi::GraphicApi & drawable, FrontAPI & front, uint16_t width, uint16_t height,
    Rect const widget_rect, ClientExecute & rail_client_execute, Font const& font,
    Theme const& theme, CopyPaste& copy_paste, Translator tr
)
    : RailInternalModBase(drawable, width, height, rail_client_execute, font, theme, &copy_paste)
    , events_guard(events)
    , language_button(
        vars.get<cfg::internal_mod::keyboard_layout_proposals>(),
        this->login, drawable, front, font, LanguageButton::Colors::from_theme(theme))
    , login([&]{
        chars_view target;
        chars_view login;
        if (vars.get<cfg::internal_mod::enable_target_field>()) {
            auto pair = rpartition(av_auto_cast{username});
            target = pair.first;
            login = pair.second;
        } else {
            login = username;
        }

        return WidgetLogin(
            drawable, copy_paste, this->screen,
            widget_rect.x, widget_rect.y, widget_rect.cx, widget_rect.cy,
            {
                .onsubmit = [this]{
                    auto edits = this->login.get_edit_texts();
                    if (edits.target.empty()) {
                        this->vars.set_acl<cfg::globals::auth_user>(to_sv(edits.login));
                    }
                    else {
                        this->vars.update_acl<cfg::globals::auth_user>([&](std::string& auth_user) {
                            update_target_login(auth_user, edits.login, to_sv(edits.target));
                        });
                    }
                    this->vars.ask<cfg::context::selector>();
                    this->vars.ask<cfg::globals::target_user>();
                    this->vars.ask<cfg::globals::target_device>();
                    this->vars.ask<cfg::context::target_protocol>();
                    this->vars.set_acl<cfg::context::password>(to_sv(edits.password));
                    this->set_mod_signal(BACK_EVENT_NEXT);
                },
                .oncancel = [this]{ this->set_mod_signal(BACK_EVENT_STOP); },
                .onctrl_shift = [this] { this->language_button.next_layout(); },
            },
            "Redemption " VERSION ""_av,
            login, password, target,
            tr(trkeys::login),
            tr(trkeys::password),
            tr(trkeys::optional_target),
            vars.get<cfg::context::opt_message>(),
            vars.get<cfg::context::login_message>(),
            &this->language_button,
            vars.get<cfg::internal_mod::enable_target_field>(),
            font, tr, theme
        );
    }())
    , vars(vars)
{
    if (vars.get<cfg::globals::authentication_timeout>().count()) {
        LOG(LOG_INFO, "LoginMod: Ending session in %u seconds",
            static_cast<unsigned>(vars.get<cfg::globals::authentication_timeout>().count()));
    }
    this->screen.add_widget(this->login, WidgetComposite::HasFocus::Yes);
    this->screen.init_focus();

    this->screen.rdp_input_invalidate(this->screen.get_rect());

    if (vars.get<cfg::globals::authentication_timeout>().count()) {
        this->events_guard.create_event_timeout(
            "Log Box Timeout",
            vars.get<cfg::globals::authentication_timeout>(),
            [this](Event&e)
            {
                e.garbage = true;
                this->set_mod_signal(BACK_EVENT_STOP);
            });
    }
}
