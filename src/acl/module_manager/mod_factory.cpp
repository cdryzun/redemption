/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Product name: redemption, a FLOSS RDP proxy
  Copyright (C) Wallix 2019
  Author(s): Christophe Grosjean

    ModFactory : Factory class used to instanciate BackEnd modules

*/

#include "acl/module_manager/mod_factory.hpp"
#include "acl/mod_pack.hpp"

#include "configs/config.hpp"
#include "translation/trkeys.hpp"
#include "translation/translation.hpp"
#include "translation/local_err_msg.hpp"
#include "utils/strutils.hpp"
#include "utils/load_theme.hpp"
#include "utils/log_siem.hpp"
#include "utils/error_message_ctx.hpp"
#include "mod/null/null.hpp"
#include "keyboard/keymap.hpp"
#include "core/client_info.hpp"

// Modules
#include "acl/module_manager/create_module_rdp.hpp"
#include "acl/module_manager/create_module_vnc.hpp"
#include "mod/internal/bouncer2_mod.hpp"
#include "mod/internal/replay_mod.hpp"
#include "mod/internal/widget_test_mod.hpp"
#include "mod/internal/test_card_mod.hpp"
#include "mod/internal/selector_mod.hpp"
#include "mod/internal/close_mod.hpp"
#include "mod/internal/interactive_target_mod.hpp"
#include "mod/internal/dialog_mod.hpp"
#include "mod/internal/wait_mod.hpp"
#include "mod/internal/transition_mod.hpp"
#include "mod/internal/login_mod.hpp"

namespace
{
    struct NoMod final : null_mod
    {
        bool is_up_and_running() const override { return false; }
    };

    inline NoMod no_mod;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}

ModFactory::ModFactory(
    EventContainer & events,
    CRef<TimeBase> time_base,
    CRef<ClientInfo> client_info,
    FrontAPI & front,
    gdi::GraphicApi & graphics,
    CRef<BGRPalette> palette,
    CRef<Font> glyphs,
    Inifile & ini,
    Keymap & keymap,
    Random & gen,
    CryptoContext & cctx,
    ErrorMessageCtx & err_msg_ctx,
    Translator translator,
    Translator log_translation
)
: events(events)
, client_info(client_info)
, front(front)
, graphics(graphics)
, ini(ini)
, glyphs(glyphs)
, keymap(keymap)
, gen(gen)
, cctx(cctx)
, err_msg_ctx(err_msg_ctx)
, log_tr(log_translation)
, file_system_license_store{ app_path(AppPath::License).to_string() }
, rail_client_execute(
    time_base, graphics, front,
    this->client_info.window_list_caps,
    ini.get<cfg::debug::mod_internal>() & 1)
, mod_wrapper(
    no_mod, time_base, palette, graphics,
    client_info, glyphs, rail_client_execute, ini, translator)
{
    ::load_theme(theme, ini);

    this->rail_client_execute.enable_remote_program(this->client_info.remote_program);
}

ModFactory::~ModFactory()
{
    if (&this->mod() != &no_mod){
        delete &this->mod();
    }
}

void ModFactory::disconnect()
{
    if (&mod() != &no_mod) {
        if (is_connected()) {
            auto const& sid = ini.get<cfg::context::session_id>();
            zstring_view message = err_msg_ctx.visit_msg(
                [](TrKey const* k, zstring_view msg){
                    return k ? MsgTranslationCatalog::default_catalog().msgid(*k) : msg;
                }
            );
            log_siem::target_disconnection(message, sid.c_str());
        }

        try {
            mod().disconnect();
        }
        catch (Error const& e) {
            LOG(LOG_ERR, "disconnect raised exception %d", static_cast<int>(e.id));
        }

        psocket_transport = nullptr;
        current_mod = ModuleName::UNKNOWN;
        auto * previous_mod = &mod();
        mod_wrapper.set_mod(no_mod, nullptr, false);
        delete previous_mod;
    }
}

static ModPack mod_pack_from_widget(mod_api* mod)
{
    return {mod, nullptr, nullptr, false};
}

struct ModFactory::Impl
{
    static void set_mod(ModFactory& self, ModuleName name, ModPack mod_pack, bool enable_osd)
    {
        self.keymap.reset_decoded_keys();

        self.mod_wrapper.clear_osd_message(false);

        if (&self.mod() != &no_mod){
            delete &self.mod();
        }

        self.current_mod = name;
        self.psocket_transport = mod_pack.psocket_transport;
        self.rail_client_execute.enable_remote_program(self.client_info.remote_program);
        self.mod_wrapper.set_mod(*mod_pack.mod, mod_pack.winapi, enable_osd);

        if (ModuleName::RDP != name
         && self.rail_client_execute.is_rail_enabled() && !self.rail_client_execute.is_ready()
        ) {
            bool can_resize_hosted_desktop = mod_pack.can_resize_hosted_desktop
                                          && self.ini.get<cfg::remote_program::allow_resize_hosted_desktop>()
                                          && !self.ini.get<cfg::globals::is_rec>();
            self.rail_client_execute.ready(*mod_pack.mod, self.glyphs, can_resize_hosted_desktop);
        }
    }

    static ModPack create_close_mod(ModFactory& self, bool back_to_selector)
    {
        auto tr_optional = [&](TrKey const* k){
            return k ? self.tr(*k) : ""_av;
        };

        std::string message;

        self.err_msg_ctx.visit_msg([&](TrKey const* k, chars_view extra_msg2) {
            auto local_err = LocalErrMsg::from_error_id(self.error_id);
            auto info = tr_optional(local_err.extra_msg);
            auto msg1 = tr_optional(local_err.msg);
            auto msg2 = tr_optional(k);

            if (msg1.empty() && msg2.empty() && extra_msg2.empty()) {
                msg2 = self.tr(trkeys::connection_ended);
            }

            auto msg1_sep = (!msg1.empty() && (!msg2.empty() || !extra_msg2.empty() || !info.empty())) ? "\n\n"_av : ""_av;
            auto msg2_sep = (!msg2.empty() && !extra_msg2.empty()) ? " "_av : ""_av;
            auto info_sep = (!info.empty() && (!msg2.empty() || !extra_msg2.empty())) ? "\n\n"_av : ""_av;

            str_assign(message, msg1, msg1_sep, msg2, msg2_sep, extra_msg2, info_sep, info);
        });

        self.err_msg_ctx.clear();
        self.error_id = NO_ERROR;

        auto new_mod = new CloseMod(
            std::move(message),
            self.ini,
            self.events,
            self.graphics,
            self.client_info.screen_info.width,
            self.client_info.screen_info.height,
            self.rail_client_execute.adjust_rect(self.client_info.get_widget_rect()),
            self.rail_client_execute,
            self.glyphs,
            self.theme,
            self.get_translator(),
            back_to_selector
        );
        return mod_pack_from_widget(new_mod);
    }

    static ModPack create_dialog(ModFactory& self, chars_view ok_text, chars_view cancel, chars_view caption)
    {
        auto new_mod = new DialogMod(
            self.ini,
            self.graphics,
            self.client_info.screen_info.width,
            self.client_info.screen_info.height,
            self.rail_client_execute.adjust_rect(self.client_info.get_widget_rect()),
            caption,
            self.ini.get<cfg::context::message>(),
            ok_text,
            cancel,
            self.rail_client_execute,
            self.glyphs,
            self.theme
        );
        return mod_pack_from_widget(new_mod);
    }

    static CopyPaste& copy_paste(ModFactory& self)
    {
        if (!self.copy_paste_ptr) {
            const auto verbosity = self.ini.get<cfg::debug::mod_internal>();
            self.copy_paste_ptr = std::make_unique<CopyPaste>(verbosity != 0);
            self.copy_paste_ptr->ready(self.front);
        }
        return *self.copy_paste_ptr;
    }
};


bool ModFactory::update_close_mod(ModuleName name)
{
    if (name != ModuleName::close && name != ModuleName::close_back) {
        return false;
    }

    if (current_mod != ModuleName::close && current_mod != ModuleName::close_back) {
        return false;
    }

    if (name == ModuleName::close_back) {
        LOG(LOG_INFO, "----------------------- create_close_mod_back_to_selector() from close_mod -----------------");
    }
    else {
        LOG(LOG_INFO, "----------------------- create_close_mod() from close_mod_back_to_selector -----------------");
    }

    if (current_mod != name) {
        current_mod = name;
        auto& close_box = static_cast<CloseMod&>(mod());
        auto updated_rect = close_box.set_back_to_selector(name == ModuleName::close_back);
        close_box.rdp_input_invalidate(updated_rect);
    }

    return true;
}

void ModFactory::create_mod_bouncer()
{
    auto new_mod = new Bouncer2Mod(
        this->graphics,
        this->events,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height);
    Impl::set_mod(*this, ModuleName::bouncer2, mod_pack_from_widget(new_mod), true);
}

void ModFactory::create_mod_replay()
{
    auto new_mod = new ReplayMod(
        this->events,
        this->graphics,
        this->front,
        str_concat(
            this->ini.get<cfg::mod_replay::replay_path>().as_string(),
            this->ini.get<cfg::globals::target_user>(),
            ".mwrm"_av
        ),
        // this->client_info.screen_info.width,
        // this->client_info.screen_info.height,
        this->err_msg_ctx,
        !this->ini.get<cfg::mod_replay::on_end_of_data>(),
        this->ini.get<cfg::mod_replay::replay_on_loop>(),
        this->ini.get<cfg::audit::play_video_with_corrupted_bitmap>(),
        safe_cast<FileToGraphicVerbose>(this->ini.get<cfg::debug::capture>())
    );
    Impl::set_mod(*this, ModuleName::autotest, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_widget_test_mod()
{
    auto new_mod = new WidgetTestMod(
        this->graphics,
        this->events,
        this->front,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute,
        this->glyphs,
        this->theme,
        Impl::copy_paste(*this)
    );
    Impl::set_mod(*this, ModuleName::widgettest, mod_pack_from_widget(new_mod), true);
}

void ModFactory::create_test_card_mod()
{
    auto new_mod = new TestCardMod(
        this->graphics,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->glyphs
    );
    Impl::set_mod(*this, ModuleName::card, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_selector_mod()
{
    auto new_mod = new SelectorMod(
        this->ini,
        this->mod_wrapper.get_graphics(),
        this->mod_wrapper,
        this->front,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute.adjust_rect(this->client_info.get_widget_rect()),
        this->rail_client_execute,
        this->glyphs,
        this->theme,
        Impl::copy_paste(*this),
        this->get_translator()
    );
    Impl::set_mod(*this, ModuleName::selector, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_close_mod()
{
    LOG(LOG_INFO, "----------------------- create_close_mod() -----------------");

    bool back_to_selector = false;
    Impl::set_mod(*this, ModuleName::close, Impl::create_close_mod(*this, back_to_selector), false);
}

void ModFactory::create_close_mod_back_to_selector()
{
    LOG(LOG_INFO, "----------------------- create_close_mod_back_to_selector() -----------------");

    bool back_to_selector = true;
    Impl::set_mod(*this, ModuleName::close_back, Impl::create_close_mod(*this, back_to_selector), false);
}

void ModFactory::create_interactive_target_mod()
{
    auto new_mod = new InteractiveTargetMod(
        this->ini,
        this->graphics,
        this->front,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute.adjust_rect(this->client_info.get_widget_rect()),
        this->rail_client_execute,
        this->glyphs,
        this->theme,
        Impl::copy_paste(*this),
        this->get_translator()
    );
    Impl::set_mod(*this, ModuleName::interactive_target, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_valid_message_mod()
{
    chars_view ok_text = tr(trkeys::accepted);
    chars_view cancel = tr(trkeys::refused);
    chars_view caption = tr(trkeys::information);
    auto mod_pack = Impl::create_dialog(*this, ok_text, cancel, caption);
    Impl::set_mod(*this, ModuleName::valid, mod_pack, false);
}

void ModFactory::create_display_message_mod()
{
    chars_view ok_text = tr(trkeys::OK);
    chars_view cancel = nullptr;
    chars_view caption = tr(trkeys::information);
    auto mod_pack = Impl::create_dialog(*this, ok_text, cancel, caption);
    Impl::set_mod(*this, ModuleName::confirm, mod_pack, false);
}

void ModFactory::create_dialog_challenge_mod()
{
    chars_view caption = tr(trkeys::challenge);
    const auto challenge = this->ini.get<cfg::context::authentication_challenge>()
        ? DialogWithChallengeMod::ChallengeOpt::Echo
        : DialogWithChallengeMod::ChallengeOpt::Hide;
    auto new_mod = new DialogWithChallengeMod(
        this->ini,
        this->graphics,
        this->front,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute.adjust_rect(this->client_info.get_widget_rect()),
        caption,
        this->ini.get<cfg::context::message>(),
        this->rail_client_execute,
        this->glyphs,
        this->theme,
        Impl::copy_paste(*this),
        this->get_translator(),
        challenge
    );
    Impl::set_mod(*this, ModuleName::challenge, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_display_link_mod()
{
    auto new_mod = new WidgetDialogWithCopyableLinkMod(
        this->ini,
        this->graphics,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute.adjust_rect(this->client_info.get_widget_rect()),
        tr(trkeys::link_caption),
        this->ini.get<cfg::context::message>(),
        this->ini.get<cfg::context::display_link>(),
        tr(trkeys::link_label),
        tr(trkeys::link_copied),
        this->rail_client_execute,
        this->glyphs,
        this->theme,
        Impl::copy_paste(*this),
        this->get_translator()
    );
    Impl::set_mod(*this, ModuleName::link_confirm, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_wait_info_mod()
{
    LOG(LOG_INFO, "ModuleManager::Creation of internal module 'Wait Info Message'");
    uint flag = this->ini.get<cfg::context::formflag>();
    auto new_mod = new WaitMod(
        this->ini,
        this->events,
        this->graphics,
        this->front,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute.adjust_rect(this->client_info.get_widget_rect()),
        tr(trkeys::information),
        this->ini.get<cfg::context::message>(),
        this->rail_client_execute,
        this->glyphs,
        this->theme,
        Impl::copy_paste(*this),
        this->get_translator(),
        flag
    );
    Impl::set_mod(*this, ModuleName::waitinfo, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_transition_mod()
{
    auto new_mod = new TransitionMod(
        tr(trkeys::wait_msg),
        this->graphics,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute.adjust_rect(this->client_info.get_widget_rect()),
        this->rail_client_execute,
        this->glyphs,
        this->theme
    );
    Impl::set_mod(*this, ModuleName::transitory, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_login_mod()
{
    LOG(LOG_INFO, "ModuleManager::Creation of internal module 'Login'");
    char username_buffer[255];
    chars_view username;
    if (!this->ini.is_asked<cfg::globals::auth_user>()){
        if (this->ini.is_asked<cfg::globals::target_user>()
         || this->ini.is_asked<cfg::globals::target_device>()){
            username = this->ini.get<cfg::globals::auth_user>();
        }
        else {
            // TODO check this! Assembling parts to get user login with target is not obvious method used below il likely to show @: if target fields are empty
            int n = snprintf( username_buffer, std::size(username_buffer), "%s@%s:%s%s%s"
                            , this->ini.get<cfg::globals::target_user>().c_str()
                            , this->ini.get<cfg::globals::target_device>().c_str()
                            , this->ini.get<cfg::context::target_protocol>().c_str()
                            , this->ini.get<cfg::context::target_protocol>().empty() ? "" : ":"
                            , this->ini.get<cfg::globals::auth_user>().c_str()
                            );
            auto len = std::min(static_cast<std::size_t>(n), std::size(username_buffer));
            username = {username_buffer, len};
        }
    }

    auto new_mod = new LoginMod(
        this->ini,
        this->events,
        username,
        ""_av, // password
        this->graphics,
        this->front,
        this->client_info.screen_info.width,
        this->client_info.screen_info.height,
        this->rail_client_execute.adjust_rect(this->client_info.get_widget_rect()),
        this->rail_client_execute,
        this->glyphs,
        this->theme,
        Impl::copy_paste(*this),
        this->get_translator()
    );
    Impl::set_mod(*this, ModuleName::waitinfo, mod_pack_from_widget(new_mod), false);
}

void ModFactory::create_rdp_mod(
    SessionLogApi& session_log,
    PerformAutomaticReconnection perform_automatic_reconnection)
{
    this->copy_paste_ptr.reset();
    auto mod_pack = create_mod_rdp(
        this->mod_wrapper.get_graphics(),
        this->mod_wrapper,
        this->redir_info,
        this->ini,
        this->front,
        this->client_info,
        this->rail_client_execute,
        this->keymap.locks(),
        this->glyphs, this->theme,
        this->events,
        session_log,
        this->err_msg_ctx,
        this->get_translator(),
        this->file_system_license_store,
        this->gen,
        this->cctx,
        this->server_auto_reconnect_packet,
        perform_automatic_reconnection);
    Impl::set_mod(*this, ModuleName::RDP, mod_pack, true);
}

void ModFactory::create_vnc_mod(SessionLogApi& session_log)
{
    this->copy_paste_ptr.reset();
    auto mod_pack = create_mod_vnc(
        this->mod_wrapper.get_graphics(),
        this->ini,
        this->front, this->client_info,
        this->rail_client_execute,
        this->keymap.layout(),
        this->keymap.locks(),
        this->glyphs, this->theme,
        this->events,
        session_log,
        err_msg_ctx,
        this->gen);
    Impl::set_mod(*this, ModuleName::VNC, mod_pack, true);
}
