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
 *   Author(s): Christophe Grosjean, Xiaopeng Zhou, Jonathan Poelen,
 *              Meng Tan, Jennifer Inthavong
 */

#include "configs/config.hpp"
#include "mod/internal/copy_paste.hpp"
#include "mod/internal/dialog_mod.hpp"
#include "mod/internal/widget/edit.hpp"
#include "translation/translation.hpp"
#include "translation/trkeys.hpp"
#include "utils/sugar/to_sv.hpp"

namespace
{

void init_dialog_mod(WidgetScreen& screen, Widget& dialog_widget)
{
    screen.add_widget(dialog_widget, WidgetComposite::HasFocus::Yes);
    screen.init_focus();
    screen.rdp_input_invalidate(screen.get_rect());
}

} // anonymous namespace

DialogMod::DialogMod(
    DialogModVariables vars,
    gdi::GraphicApi & drawable,
    uint16_t width, uint16_t height,
    Rect const widget_rect, chars_view caption, chars_view message,
    chars_view ok_text, chars_view cancel_text,
    ClientExecute & rail_client_execute,
    Font const& font, Theme const& theme
)
    : RailInternalModBase(drawable, width, height, rail_client_execute, font, theme, nullptr)
    , dialog_widget(
        drawable, widget_rect,
        !cancel_text.empty()
            ? WidgetDialog::Events{
                .onsubmit = [this]{
                    this->vars.set_acl<cfg::context::accept_message>(true);
                    this->set_mod_signal(BACK_EVENT_NEXT);
                },
                .oncancel = [this]{
                    this->vars.set_acl<cfg::context::accept_message>(false);
                    this->set_mod_signal(BACK_EVENT_NEXT);
                },
            }
            : WidgetDialog::Events{
                .onsubmit = [this]{
                    this->vars.set_acl<cfg::context::display_message>(true);
                    this->set_mod_signal(BACK_EVENT_NEXT);
                },
                .oncancel = [this]{
                    this->vars.set_acl<cfg::context::display_message>(false);
                    this->set_mod_signal(BACK_EVENT_NEXT);
                },
            },
        caption, message, theme, font,
        ok_text,
        cancel_text)
    , vars(vars)
{
    init_dialog_mod(this->screen, this->dialog_widget);
}


DialogWithChallengeMod::DialogWithChallengeMod(
    DialogWithChallengeModVariables vars,
    gdi::GraphicApi & drawable,
    FrontAPI & front, uint16_t width, uint16_t height,
    Rect const widget_rect, chars_view caption, chars_view message,
    ClientExecute & rail_client_execute, Font const& font,
    Theme const& theme, CopyPaste& copy_paste, Translator tr,
    ChallengeOpt challenge
)
    : RailInternalModBase(drawable, width, height, rail_client_execute, font, theme, &copy_paste)
    , language_button(
        vars.get<cfg::internal_mod::keyboard_layout_proposals>(), this->dialog_widget,
        drawable, front, font, LanguageButton::Colors::from_theme(theme))
    , dialog_widget(
        drawable, widget_rect,
        WidgetDialogWithChallenge::Events{
            .onsubmit = [this]{
                this->vars.set_acl<cfg::context::password>(to_sv(this->dialog_widget.get_chalenge()));
                this->set_mod_signal(BACK_EVENT_NEXT);
            },
            .oncancel = [this]{
                this->vars.set_acl<cfg::context::password>("");
                this->set_mod_signal(BACK_EVENT_NEXT);
            },
            .onctrl_shift = [this]{ this->language_button.next_layout(); },
        },
        caption, message,
        &this->language_button,
        tr(trkeys::OK),
        font, theme, copy_paste, challenge)
    , vars(vars)
{
    init_dialog_mod(this->screen, this->dialog_widget);
}


WidgetDialogWithCopyableLinkMod::WidgetDialogWithCopyableLinkMod(
    WidgetDialogWithCopyableLinkModVariables vars,
    gdi::GraphicApi & drawable,
    uint16_t width, uint16_t height,
    Rect const widget_rect, chars_view caption, chars_view message,
    chars_view link_value, chars_view link_label, chars_view copied_msg_label,
    ClientExecute & rail_client_execute, Font const& font, Theme const& theme,
    CopyPaste& copy_paste, Translator tr
)
    : RailInternalModBase(drawable, width, height, rail_client_execute, font, theme, &copy_paste)
    , dialog_widget(
        drawable, widget_rect,
        {
            .onsubmit = [this]{
                this->vars.set_acl<cfg::context::display_message>(true);
                this->set_mod_signal(BACK_EVENT_NEXT);
            },
            .oncancel = [this]{
                this->vars.set_acl<cfg::context::display_message>(false);
                this->set_mod_signal(BACK_EVENT_NEXT);
            }
        },
        caption, message, link_value, link_label, copied_msg_label,
        tr(trkeys::OK),
        font, theme, copy_paste)
    , vars(vars)
{
    init_dialog_mod(this->screen, this->dialog_widget);
}
