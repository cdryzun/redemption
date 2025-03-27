/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "acl/acl_field_mask.hpp"
#include "mod/internal/selector_mod.hpp"
#include "mod/internal/copy_paste.hpp"
#include "configs/config.hpp"
#include "core/font.hpp"
#include "gdi/osd_api.hpp"
#include "keyboard/keymap.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/sugar/chars_to_int.hpp"
#include "utils/sugar/to_sv.hpp"
#include "utils/snprintf_av.hpp"
#include "translation/trkeys.hpp"

namespace
{
    constexpr uint16_t nb_max_row = 1024;
} // namespace


SelectorMod::SelectorMod(
    SelectorModVariables ini,
    gdi::GraphicApi & drawable,
    gdi::OsdApi& osd,
    FrontAPI & front, uint16_t width, uint16_t height,
    Rect const widget_rect, ClientExecute & rail_client_execute,
    Font const& font, Theme const& theme, CopyPaste& copy_paste,
    Translator tr
)
    : RailInternalModBase(drawable, width, height, rail_client_execute, font, theme, &copy_paste)
    , tr(tr)
    , ini(ini)
    , osd(osd)
    , current_page(0)
    , number_page(0)
    , language_button(
        ini.get<cfg::internal_mod::keyboard_layout_proposals>(),
        this->selector, drawable, front, font, LanguageButton::Colors::from_theme(theme))
    , selector(
        drawable, font, copy_paste, theme, this->screen, tr,
        &this->language_button, widget_rect,
        {
            .headers = {
                .authorization = tr(trkeys::authorization),
                .target = tr(trkeys::target),
                .protocol = tr(trkeys::protocol),
            },
            .device_name = SNPrintf<256>(
                "%s@%s",
                ini.get<cfg::globals::auth_user>().c_str(),
                ini.get<cfg::globals::host>().c_str()
            ),
        },
        {
            .onconnect = [this]{
                char buffer[1024] = {};
                if (auto line = selector.selected_line()) {
                    chars_view target = line.target;
                    chars_view groups = line.authorization;
                    snprintf(buffer, sizeof(buffer), "%.*s:%.*s:%s",
                             static_cast<int>(target.size()), target.data(),
                             static_cast<int>(groups.size()), groups.data(),
                             this->ini.get<cfg::globals::auth_user>().c_str());
                    this->ini.set_acl<cfg::globals::auth_user>(buffer);
                    this->ini.ask<cfg::globals::target_user>();
                    this->ini.ask<cfg::globals::target_device>();
                    this->ini.ask<cfg::context::target_protocol>();

                    this->set_mod_signal(BACK_EVENT_NEXT);
                }
            },

            .oncancel = [this]{
                this->ini.ask<cfg::globals::auth_user>();
                this->ini.ask<cfg::context::password>();
                this->ini.set<cfg::context::selector>(false);
                this->set_mod_signal(BACK_EVENT_NEXT);
            },

            .onfilter = [this]{ this->ask_page(); },

            .onfirst_page = [this]{
                if (this->current_page > 1) {
                    this->current_page = 1;
                    this->ask_page();
                }
            },

            .onprev_page = [this]{
                if (this->current_page > 1) {
                    --this->current_page;
                    this->ask_page();
                }
            },

            .oncurrent_page = [this]{
                unsigned page = this->selector.current_page();
                if (page != this->current_page) {
                    this->current_page = page;
                    this->ask_page();
                }
            },

            .onnext_page = [this]{
                if (this->current_page < this->number_page) {
                    ++this->current_page;
                    this->ask_page();
                }
            },

            .onlast_page = [this]{
                if (this->current_page < this->number_page) {
                    this->current_page = this->number_page;
                    this->ask_page();
                }
            },

            .onprev_page_on_grid = [this] {
                if (this->current_page > 1) {
                    --this->current_page;
                    this->ask_page();
                }
                else if (this->current_page == 1 && this->number_page > 1) {
                    this->current_page = this->number_page;
                    this->ask_page();
                }
            },

            .onnext_page_on_grid = [this] {
                if (this->current_page < this->number_page) {
                    ++this->current_page;
                    this->ask_page();
                }
                else if (this->current_page == this->number_page && this->number_page > 1) {
                    this->current_page = 1;
                    this->ask_page();
                }
            },

            .onctrl_shift = [this]{ this->language_button.next_layout(); },
        }
    )
{
    this->screen.add_widget(this->selector, WidgetComposite::HasFocus::Yes);
    this->screen.init_focus();

    this->selector_lines_per_page_saved = std::min(this->selector.max_line_per_page(), nb_max_row);
    this->ini.set_acl<cfg::context::selector_lines_per_page>(this->selector_lines_per_page_saved);
    this->selector.rdp_input_invalidate(this->selector.get_rect());
    this->ask_page();
}

void SelectorMod::acl_update(AclFieldMask const& acl_fields)
{
    if (acl_fields.has<cfg::globals::target_user>()) {
        current_page = ini.get<cfg::context::selector_current_page>();
        number_page = ini.get<cfg::context::selector_number_of_pages>();
        selector.set_page({
            .authorizations = ini.get<cfg::globals::target_user>(),
            .targets = ini.get<cfg::globals::target_device>(),
            .protocols = ini.get<cfg::context::target_protocol>(),
            .current_page = current_page,
            .number_of_page = number_page,
        });
    }

    if (ini.get<cfg::context::banner_message>().empty()) {
        // Show OSD banner message only after primary auth

        gdi::OsdMsgUrgency omu = gdi::OsdMsgUrgency::NORMAL;

        switch (ini.get<cfg::context::banner_type>())
        {
            case BannerType::info: omu = gdi::OsdMsgUrgency::INFO; break;
            case BannerType::warn: omu = gdi::OsdMsgUrgency::WARNING; break;
            case BannerType::alert: omu = gdi::OsdMsgUrgency::ALERT; break;
        }

        osd.display_osd_message(ini.get<cfg::context::banner_message>(), omu);
        ini.set<cfg::context::banner_message>("");
    }
}

void SelectorMod::ask_page()
{
    this->ini.set_acl<cfg::context::selector_current_page>(this->current_page);

    auto texts = this->selector.filter_texts();

    this->ini.set_acl<cfg::context::selector_group_filter>(to_sv(texts.authorization));
    this->ini.set_acl<cfg::context::selector_device_filter>(to_sv(texts.target));
    this->ini.set_acl<cfg::context::selector_proto_filter>(to_sv(texts.protocol));

    this->ini.ask<cfg::globals::target_user>();
    this->ini.ask<cfg::globals::target_device>();
    this->ini.ask<cfg::context::selector>();
}

void SelectorMod::rdp_input_scancode(
    KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    RailModBase::check_alt_f4(keymap);

    this->screen.rdp_input_scancode(flags, scancode, event_time, keymap);
}

void SelectorMod::move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height)
{
    this->selector.move_size_widget(left, top, width, height);

    uint16_t selector_lines_per_page = this->selector.max_line_per_page();

    if (this->selector_lines_per_page_saved != selector_lines_per_page) {
        this->selector_lines_per_page_saved = selector_lines_per_page;
        this->ini.set_acl<cfg::context::selector_lines_per_page>(selector_lines_per_page);
        this->ask_page();
    }

    this->selector.rdp_input_invalidate(this->selector.get_rect());
}
