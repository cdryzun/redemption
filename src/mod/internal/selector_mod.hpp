/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/config_access.hpp"
#include "mod/internal/rail_mod_base.hpp"
#include "mod/internal/widget/selector.hpp"
#include "mod/internal/widget/language_button.hpp"

namespace gdi
{
    class OsdApi;
}

class CopyPaste;

using SelectorModVariables = vcfg::variables<
    vcfg::var<cfg::globals::auth_user,                  vcfg::accessmode::ask | vcfg::accessmode::set | vcfg::accessmode::get>,
    vcfg::var<cfg::context::selector,                   vcfg::accessmode::ask | vcfg::accessmode::set>,
    vcfg::var<cfg::context::target_protocol,            vcfg::accessmode::ask | vcfg::accessmode::get>,
    vcfg::var<cfg::globals::target_device,              vcfg::accessmode::ask | vcfg::accessmode::get>,
    vcfg::var<cfg::globals::target_user,                vcfg::accessmode::ask | vcfg::accessmode::get>,
    vcfg::var<cfg::context::password,                   vcfg::accessmode::ask>,
    vcfg::var<cfg::context::selector_current_page,      vcfg::accessmode::is_asked | vcfg::accessmode::get | vcfg::accessmode::set>,
    vcfg::var<cfg::context::selector_number_of_pages,   vcfg::accessmode::is_asked | vcfg::accessmode::get>,
    vcfg::var<cfg::context::selector_lines_per_page,    vcfg::accessmode::get | vcfg::accessmode::set>,
    vcfg::var<cfg::context::selector_group_filter,      vcfg::accessmode::set>,
    vcfg::var<cfg::context::selector_device_filter,     vcfg::accessmode::set>,
    vcfg::var<cfg::context::selector_proto_filter,      vcfg::accessmode::set>,
    vcfg::var<cfg::internal_mod::keyboard_layout_proposals, vcfg::accessmode::get>,
    vcfg::var<cfg::globals::host,                       vcfg::accessmode::get>,
    vcfg::var<cfg::context::banner_type,                vcfg::accessmode::get>,
    vcfg::var<cfg::context::banner_message,             vcfg::accessmode::get | vcfg::accessmode::set>
>;

class SelectorMod : public RailInternalModBase
{
public:
    SelectorMod(
        SelectorModVariables ini,
        gdi::GraphicApi & drawable,
        gdi::OsdApi& osd,
        FrontAPI & front, uint16_t width, uint16_t height,
        Rect const widget_rect, ClientExecute & rail_client_execute,
        Font const& font, Theme const& theme, CopyPaste& copy_paste,
        Translator translator);

    void acl_update(AclFieldMask const& acl_fields) override;

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height) override;

private:
    void ask_page();

    Translator tr;
    SelectorModVariables ini;

    gdi::OsdApi& osd;

    unsigned current_page;
    unsigned number_page;

    unsigned selector_lines_per_page_saved = 0;

    LanguageButton language_button;

    WidgetSelector selector;
};
