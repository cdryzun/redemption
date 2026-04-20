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
  Copyright (C) Wallix 2010
  Author(s): Christophe Grosjean, Javier Caverni, Xavier Dunat,
             Raphael Zhou, Meng Tan

  Manage Modules Life cycle : creation, destruction and chaining
  find out the next module to run from context reading
*/

#include "configs/config.hpp"
#include "core/client_info.hpp"
#include "mod/vnc/vnc.hpp"
#include "mod/internal/rail_module_host_mod.hpp"
#include "mod/tls_params_loader.hpp"
#include "transport/socket_transport.hpp"
#include "acl/module_manager/create_module_vnc.hpp"
#include "acl/module_manager/create_module_rail.hpp"
#include "acl/connect_to_target_host.hpp"
#include "utils/sugar/unique_fd.hpp"
#include "utils/sugar/split.hpp"
#include "utils/netutils.hpp"
#include "RAIL/client_execute.hpp"


namespace
{

struct VncData
{
    VncData(EventContainer & events,
        Inifile & ini, SocketTransport::Name name, unique_fd sck,
        SocketTransport::Verbose verbose)
    : events_guard(events)
    , socket_transport(name, std::move(sck),
                       ini.get<cfg::context::target_host>(),
                       checked_int(ini.get<cfg::context::target_port>()),
                       ini.get<cfg::all_target_mod::connection_establishment_timeout>(),
                       ini.get<cfg::all_target_mod::tcp_user_timeout>(),
                       std::chrono::milliseconds(ini.get<cfg::globals::mod_recv_timeout>()),
                       verbose)
    {}

    ~VncData() = default;

    SocketTransport& get_transport()
    {
        return this->socket_transport;
    }

private:
    EventsGuard events_guard;

    struct FinalSocketTransport final : SocketTransport
    {
        using SocketTransport::SocketTransport;
    };

    FinalSocketTransport socket_transport;
};


class ModVNCWithSocket final : public VncData, public mod_vnc
{
public:
    ModVNCWithSocket(
        Random & rand,
        gdi::GraphicApi & drawable,
        Inifile & ini,
        SocketTransport::Name name,
        unique_fd sck,
        SocketTransport::Verbose verbose,
        EventContainer& events,
        SessionLogApi& session_log,
        const ModTlsParams& tls_params,
        std::string_view force_authentication_method,
        const char* username,
        const char* password,
        FrontAPI& front,
        uint16_t front_width,
        uint16_t front_height,
        bool clipboard_up,
        bool clipboard_down,
        const char * encodings,
        VncClipboardEncoding clipboard_server_encoding_type,
        VncBogusClipboardInfiniteLoopStrategy bogus_clipboard_infinite_loop,
        KeyLayout const& layout,
        kbdtypes::KeyLocks locks,
        bool server_is_apple,
        bool send_alt_ksym,
        bool cursor_pseudo_encoding_supported,
        ClientExecute* rail_client_execute,
        VNCVerbose vnc_verbose)
    : VncData(events, ini, name, std::move(sck), verbose)
    , mod_vnc(
          this->get_transport(), rand, drawable,
          events, username, password, front, front_width, front_height,
          clipboard_up, clipboard_down, encodings,
          clipboard_server_encoding_type, bogus_clipboard_infinite_loop,
          layout, locks, server_is_apple, send_alt_ksym, cursor_pseudo_encoding_supported,
          rail_client_execute, vnc_verbose, session_log, tls_params,
          force_authentication_method)
    {}
};

} // anonymous namespace

ModPack create_mod_vnc(
    gdi::GraphicApi & drawable,
    Inifile& ini, FrontAPI& front, ClientInfo const& client_info,
    ClientExecute& rail_client_execute,
    KeyLayout const& layout,
    kbdtypes::KeyLocks locks,
    Ref<Font const> glyphs,
    Theme & theme,
    EventContainer& events,
    SessionLogApi& session_log,
    ErrorMessageCtx& err_msg_ctx,
    Random & rand
    )
{
    LOG(LOG_INFO, "ModuleManager::Creation of new mod 'VNC'");

    unique_fd client_sck = ini.get<cfg::context::tunneling_target_host>().empty()
        ? connect_to_target_host(
            ini, session_log, err_msg_ctx, trkeys::authentification_vnc_fail,
            ini.get<cfg::mod_vnc::enable_ipv6>(),
            ini.get<cfg::all_target_mod::connection_establishment_timeout>(),
            ini.get<cfg::all_target_mod::tcp_user_timeout>())
        : addr_connect(
            ini.get<cfg::context::tunneling_target_host>().c_str(),
            std::chrono::seconds(1), false);

    const auto vnc_verbose = safe_cast<VNCVerbose>(ini.get<cfg::debug::mod_vnc>());

    std::unique_ptr<RailModuleHostMod> host_mod {
        client_info.remote_program
        ? create_mod_rail(ini,
                          events,
                          drawable,
                          client_info,
                          rail_client_execute,
                          glyphs,
                          theme)
        : nullptr
    };

    auto enable_clipboard_upload = false;
    auto enable_clipboard_download = false;
    for (auto opt : split_with(ini.get<cfg::context::proxy_opt>(), ',')) {
        auto opt_name = opt.as<std::string_view>();
        if (opt_name == "VNC_CLIPBOARD_UP") enable_clipboard_upload = true;
        if (opt_name == "VNC_CLIPBOARD_DOWN") enable_clipboard_download = true;
    }

    auto new_mod = std::make_unique<ModVNCWithSocket>(
        rand,
        host_mod ? host_mod->proxy_gd() : drawable,
        ini,
        "VNC Target"_sck_name,
        std::move(client_sck),
        safe_cast<SocketTransport::Verbose>(ini.get<cfg::debug::sck_mod>()),
        events,
        session_log,
        ModTlsParamsLoader::vnc(ini),
        ini.get<cfg::mod_vnc::force_authentication_method>(),
        ini.get<cfg::globals::target_user>().c_str(),
        ini.get<cfg::context::target_password>().c_str(),
        front,
        client_info.screen_info.width,
        client_info.screen_info.height,
        enable_clipboard_upload,
        enable_clipboard_download,
        ini.get<cfg::mod_vnc::encodings>().c_str(),
        ini.get<cfg::vnc_clipboard::clipboard_encoding>(),
        ini.get<cfg::vnc_clipboard::bogus_infinite_loop_strategy>(),
        layout,
        locks,
        ini.get<cfg::mod_vnc::server_is_macos>(),
        ini.get<cfg::mod_vnc::server_unix_alt>(),
        ini.get<cfg::mod_vnc::support_cursor_pseudo_encoding>(),
        (client_info.remote_program ? &rail_client_execute : nullptr),
        vnc_verbose
    );

    auto* socket_transport = &new_mod->get_transport();

    if (!client_info.remote_program) {
        return ModPack{not_null_ptr{new_mod.release()}, nullptr, socket_transport, false};
    }

    host_mod->set_mod(std::move(new_mod));
    return ModPack{not_null_ptr{host_mod.release()}, nullptr, socket_transport, false};
}
